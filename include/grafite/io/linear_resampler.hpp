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

#ifndef GRAFITE_IO_LINEAR_RESAMPLER_HPP
#define GRAFITE_IO_LINEAR_RESAMPLER_HPP

#include <cmath>
#include <stdexcept>
#include <utility>

#include "grafite/core/types.hpp"

namespace grafite::io {

/// Stateful, single-channel linear sample-rate converter.
///
/// Pull is a callable bool(Sample&) that supplies the next source sample.
/// No ownership of the source or transport is assumed. A false return
/// indicates insufficient input; the caller decides how to recover.
/// The source must remain sequential across render() calls.
/// Not band-limited: intended for initial integration and diagnostics.
class LinearResampler {
public:
    LinearResampler(double input_rate, double output_rate)
        : step_(input_rate / output_rate)
    {
        if (!std::isfinite(input_rate) || !std::isfinite(output_rate) ||
            input_rate <= 0.0 || output_rate <= 0.0 ||
            !std::isfinite(step_) || step_ <= 0.0 || step_ > 16.0) {
            throw std::invalid_argument("Invalid resampling rates");
        }
    }

    /// Reinitializes the interpolation timeline from two source samples.
    /// Called outside the DSP timeline after startup or an input gap.
    void reset(Sample first, Sample second) noexcept
    {
        current_ = first;
        next_ = second;
        phase_ = 0.0;
    }

    /// Renders one output sample, preserving phase across block boundaries.
    ///
    /// On input exhaustion, returns false. The caller must reset() after
    /// sufficient new source data is available.
    template <typename Pull>
    [[nodiscard]] bool render(Pull&& pull, Sample& output) noexcept
    {
        while (phase_ >= 1.0) {
            current_ = next_;
            if (!pull(next_)) {
                return false;
            }
            phase_ -= 1.0;
        }
        output = static_cast<Sample>(
            current_ + (next_ - current_) * phase_
        );
        phase_ += step_;
        return true;
    }

    [[nodiscard]] double ratio() const noexcept
    {
        return step_;
    }

private:
    double step_;
    double phase_ = 0.0;
    Sample current_ = 0.0F;
    Sample next_ = 0.0F;
};

}  // namespace grafite::io

#endif  // GRAFITE_IO_LINEAR_RESAMPLER_HPP
