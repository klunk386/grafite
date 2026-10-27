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

#ifndef GRAFITE_IO_MULTICHANNEL_LINEAR_RESAMPLER_HPP
#define GRAFITE_IO_MULTICHANNEL_LINEAR_RESAMPLER_HPP

#include <cmath>
#include <stdexcept>

#include "grafite/core/audio_buffer.hpp"

namespace grafite::io {

/// Stateful planar linear SRC with one shared phase for all channels.
///
/// Pull is bool(AudioBufferView), supplying exactly one complete source
/// frame atomically. All memory is allocated at construction. A failed
/// pull invalidates the interpolation state; call reset() before reuse.
/// This is a nominal-rate diagnostic SRC, not a band-limited converter.
class MultichannelLinearResampler {
public:
    MultichannelLinearResampler(
        ChannelCount channels, double input_rate, double output_rate
    )
        : step_(input_rate / output_rate),
          current_(channels, 1),
          next_(channels, 1)
    {
        if (channels == 0 || !std::isfinite(input_rate) ||
            !std::isfinite(output_rate) || input_rate <= 0.0 ||
            output_rate <= 0.0 || !std::isfinite(step_) ||
            step_ <= 0.0 || step_ > 16.0) {
            throw std::invalid_argument("Invalid multichannel SRC settings");
        }
    }

    [[nodiscard]] ChannelCount channels() const noexcept
    {
        return current_.channels();
    }

    [[nodiscard]] double ratio() const noexcept
    {
        return step_;
    }

    /// Seeds the interpolation window using two complete source frames.
    void reset(
        ConstAudioBufferView first, ConstAudioBufferView second
    ) noexcept
    {
        for (ChannelCount ch = 0; ch < channels(); ++ch) {
            current_(ch, 0) = first(ch, 0);
            next_(ch, 0) = second(ch, 0);
        }
        phase_ = 0.0;
        valid_ = true;
    }

    /// Produces one frame; no channel advances independently.
    /// On exhaustion, the caller must reseed with reset().
    template <typename Pull>
    [[nodiscard]] bool render(
        Pull&& pull, AudioBufferView output
    ) noexcept
    {
        if (!valid_) {
            return false;
        }
        while (phase_ >= 1.0) {
            // The next complete source frame is acquired before shifting
            // the interpolation window. Pull must be all-or-nothing.
            if (!pull(current_.view())) {
                valid_ = false;
                return false;
            }
            for (ChannelCount ch = 0; ch < channels(); ++ch) {
                const Sample old_next = next_(ch, 0);
                next_(ch, 0) = current_(ch, 0);
                current_(ch, 0) = old_next;
            }
            phase_ -= 1.0;
        }
        for (ChannelCount ch = 0; ch < channels(); ++ch) {
            output(ch, 0) = static_cast<Sample>(
                current_(ch, 0) +
                (next_(ch, 0) - current_(ch, 0)) * phase_
            );
        }
        phase_ += step_;
        return true;
    }

private:
    double step_;
    double phase_ = 0.0;
    AudioBuffer current_;
    AudioBuffer next_;
    bool valid_ = false;
};

}  // namespace grafite::io

#endif  // GRAFITE_IO_MULTICHANNEL_LINEAR_RESAMPLER_HPP
