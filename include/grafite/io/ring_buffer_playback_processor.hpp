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
#include <array>
#include <cmath>
#include <stdexcept>

#include "grafite/io/audio_processor.hpp"
#include "grafite/io/audio_ring_buffer.hpp"
#include "grafite/io/linear_resampler.hpp"

namespace grafite::io {

/// Experimental mono FIFO -> stereo playback with nominal linear resampling.
///
/// Single consumer only. Output is silent until the FIFO reaches the startup
/// threshold. No drift correction: separate hardware clocks will eventually
/// produce overrun/underrun. This is a diagnostic, not a production SRC.
/// All buffers and state are preallocated; process() is real-time safe.
class RingBufferPlaybackProcessor final : public AudioProcessor {
public:
    RingBufferPlaybackProcessor(
        AudioRingBuffer& ring,
        double input_rate,
        double output_rate,
        FrameCount startup_frames,
        float gain = 0.02F
    )
        : ring_(ring),
          resampler_(input_rate, output_rate),
          startup_frames_(startup_frames),
          gain_(gain)
    {
        if (ring.channels() != 1 ||
            startup_frames < 2 || startup_frames > ring.capacity() ||
            !std::isfinite(gain) || gain < 0.0F || gain > 1.0F) {
            throw std::invalid_argument("Invalid ring playback configuration");
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

    void process(
        std::span<const ConstAudioBufferView>,
        std::span<AudioBufferView> outputs,
        FrameCount frames
    ) noexcept override
    {
        if (outputs.size() != 1 || outputs[0].channels() != 2 ||
            outputs[0].frames() < frames) {
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

        std::uint64_t missing = 0;
        for (FrameCount i = 0; i < frames; ++i) {
            float value = 0.0F;
            if (!active_) {
                if (ring_.available_read() >= startup_frames_) {
                    active_ = take(current_) && take(next_);
                    if (active_) {
                        resampler_.reset(current_, next_);
                    }
                    if (active_ && has_started_) {
                        recovery_events_.fetch_add(
                            1, std::memory_order_relaxed
                        );
                    }
                    if (active_) {
                        has_started_ = true;
                    }
                }
            }
            if (active_) {
                if (!resampler_.render(
                        [this](Sample& sample) noexcept {
                            return take(sample);
                        },
                        value
                    )) {
                    active_ = false;
                    underflow_events_.fetch_add(
                        1, std::memory_order_relaxed
                    );
                    value = 0.0F;
                } else {
                    value *= gain_;
                }
            }
            if (!active_ && has_started_) {
                ++missing;
            }
            output(0, i) = value;
            output(1, i) = value;
        }
        if (missing != 0) {
            underflow_frames_.fetch_add(
                missing, std::memory_order_relaxed
            );
        }
    }

private:
    bool take(float& value) noexcept
    {
        std::array<float, 1> sample{};
        AudioBufferView view(sample.data(), 1, 1, 1);
        if (ring_.read(view, 1) != 1) {
            return false;
        }
        value = sample[0];
        return true;
    }

    AudioRingBuffer& ring_;
    LinearResampler resampler_;
    FrameCount startup_frames_;
    float gain_;
    float current_ = 0.0F;
    float next_ = 0.0F;
    bool active_ = false;
    bool has_started_ = false;
    std::atomic<std::uint64_t> underflow_events_{0};
    std::atomic<std::uint64_t> underflow_frames_{0};
    std::atomic<std::uint64_t> recovery_events_{0};
    std::atomic<FrameCount> peak_occupancy_{0};
};

}  // namespace grafite::io

#endif  // GRAFITE_IO_RING_BUFFER_PLAYBACK_PROCESSOR_HPP
