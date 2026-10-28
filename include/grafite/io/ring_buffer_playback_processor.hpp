// =============================================================================
// GRAFITE - General-purpose Real-time Audio Framework
//
// A modular C++ framework for real-time and offline audio/DSP processing.
// GRAFITE is designed as a flexible foundation for building modular,
// reusable, and interoperable audio applications and processing components.
//
// Copyright (c) 2026 GRAFITE contributors.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// This file is part of GRAFITE and is distributed under the terms of the
// GNU Lesser General Public License. See the LICENSE file for details.
// =============================================================================

#ifndef GRAFITE_IO_RING_BUFFER_PLAYBACK_PROCESSOR_HPP
#define GRAFITE_IO_RING_BUFFER_PLAYBACK_PROCESSOR_HPP

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cmath>
#include <optional>
#include <utility>
#include <stdexcept>

#include "grafite/io/audio_processor.hpp"
#include "grafite/io/audio_ring_buffer.hpp"
#include "grafite/io/channel_routing.hpp"
#include "grafite/io/fifo_drift_controller.hpp"
#include "grafite/io/multichannel_linear_resampler.hpp"

namespace grafite::io {

/// Experimental multichannel FIFO playback with configurable channel routing.
///
/// Single consumer only. Output is silent until the FIFO reaches the startup
/// threshold. Optional bounded FIFO feedback compensates small clock drift.
/// This diagnostic linear SRC is not a production band-limited converter.
/// All buffers and state are preallocated; process() is real-time safe.
class RingBufferPlaybackProcessor final : public AudioProcessor {
public:
    RingBufferPlaybackProcessor(
        AudioRingBuffer& ring,
        double input_rate,
        double output_rate,
        FrameCount startup_frames,
        float gain = 0.02F,
        ChannelRouting routing = ChannelRouting({0, 0}),
        std::optional<FifoDriftController> drift = std::nullopt
    )
        : ring_(ring),
          resampler_(ring.channels(), input_rate, output_rate),
          first_(ring.channels(), 1),
          second_(ring.channels(), 1),
          rendered_(ring.channels(), 1),
          startup_frames_(startup_frames),
          output_rate_(output_rate),
          gain_(gain),
          routing_(std::move(routing)),
          drift_(std::move(drift))
    {
        if (startup_frames < 2 || startup_frames > ring.capacity() ||
            !std::isfinite(gain) || gain < 0.0F || gain > 1.0F) {
            throw std::invalid_argument("Invalid ring playback configuration");
        }
        routing_.validate(ring.channels());
        if (drift_ && drift_->nominal_ratio() != resampler_.ratio()) {
            throw std::invalid_argument("Mismatched nominal SRC ratio");
        }
    }

    /// Cumulative diagnostics; snapshots can change while audio is running.
    /// Startup silence is excluded from underflow accounting.
    [[nodiscard]] std::uint64_t underflow_events() const noexcept
    {
        return underflow_events_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::uint64_t underflow_frames() const noexcept
    {
        return underflow_frames_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::uint64_t recovery_events() const noexcept
    {
        return recovery_events_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] FrameCount peak_occupancy() const noexcept
    {
        return peak_occupancy_.load(std::memory_order_relaxed);
    }

    /// Thread-safe snapshot of the last bounded drift correction.
    [[nodiscard]] double drift_correction_ppm() const noexcept
    {
        return static_cast<double>(
            drift_millippm_snapshot_.load(std::memory_order_relaxed)
        ) * 0.001;
    }

    void process(
        std::span<const ConstAudioBufferView>,
        std::span<AudioBufferView> outputs,
        FrameCount frames
    ) noexcept override
    {
        if (outputs.size() != 1 ||
            outputs[0].channels() != routing_.output_channels() ||
            outputs[0].frames() < frames) {
            return;
        }
        if (frames == 0) {
            return;
        }
        auto output = outputs[0];
        // The observation is advisory: producer and consumer advance
        // independently. It is not an instantaneous high-water mark.
        const auto occupancy = ring_.available_read();
        auto peak = peak_occupancy_.load(std::memory_order_relaxed);
        while (occupancy > peak &&
               !peak_occupancy_.compare_exchange_weak(
                   peak, occupancy, std::memory_order_relaxed
               )) {
        }

        // Sample once per callback, not once per output frame. This
        // avoids responding to individual FIFO reads as clock drift.
        if (drift_ && active_) {
            const bool accepted = resampler_.set_ratio(
                drift_->update(
                    occupancy, static_cast<double>(frames) / output_rate_
                )
            );
            (void)accepted;  // Configuration guarantees a valid ratio.
            drift_millippm_snapshot_.store(
                static_cast<std::int32_t>(
                    std::round(drift_->correction_ppm() * 1000.0)
                ),
                std::memory_order_relaxed
            );
        }

        std::uint64_t missing = 0;
        for (FrameCount i = 0; i < frames; ++i) {
            if (!active_ && ring_.available_read() >= startup_frames_) {
                active_ = take(first_.view()) &&
                          take(second_.view());
                if (active_) {
                    resampler_.reset(first_.view(), second_.view());
                    if (drift_) {
                        drift_->reset();
                        const bool accepted = resampler_.set_ratio(
                            drift_->nominal_ratio()
                        );
                        (void)accepted;
                        drift_millippm_snapshot_.store(
                            0.0, std::memory_order_relaxed
                        );
                    }
                    if (has_started_) {
                        recovery_events_.fetch_add(
                            1, std::memory_order_relaxed
                        );
                    }
                    has_started_ = true;
                }
            }

            bool produced = false;
            if (active_) {
                produced = resampler_.render(
                    [this](AudioBufferView frame) noexcept {
                        return take(frame);
                    },
                    rendered_.view()
                );
                if (!produced) {
                    active_ = false;
                    if (drift_) {
                        drift_->reset();
                        drift_millippm_snapshot_.store(
                            0.0, std::memory_order_relaxed
                        );
                    }
                    underflow_events_.fetch_add(
                        1, std::memory_order_relaxed
                    );
                }
            }
            if (!produced) {
                rendered_.clear();
                if (has_started_) {
                    ++missing;
                }
            } else {
                for (ChannelCount ch = 0; ch < rendered_.channels(); ++ch) {
                    rendered_(ch, 0) *= gain_;
                }
            }
            routing_.route(rendered_.view(), output, 0, i);
        }
        if (missing != 0) {
            underflow_frames_.fetch_add(
                missing, std::memory_order_relaxed
            );
        }
    }

private:
    bool take(AudioBufferView frame) noexcept
    {
        return ring_.read(frame, 1) == 1;
    }

    AudioRingBuffer& ring_;
    MultichannelLinearResampler resampler_;
    AudioBuffer first_;
    AudioBuffer second_;
    AudioBuffer rendered_;
    FrameCount startup_frames_;
    double output_rate_;
    float gain_;
    ChannelRouting routing_;
    std::optional<FifoDriftController> drift_;
    bool active_ = false;
    bool has_started_ = false;
    std::atomic<std::uint64_t> underflow_events_{0};
    std::atomic<std::uint64_t> underflow_frames_{0};
    std::atomic<std::uint64_t> recovery_events_{0};
    std::atomic<FrameCount> peak_occupancy_{0};
    std::atomic<std::int32_t> drift_millippm_snapshot_{0};
};

}  // namespace grafite::io

#endif  // GRAFITE_IO_RING_BUFFER_PLAYBACK_PROCESSOR_HPP
