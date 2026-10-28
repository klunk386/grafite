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

#ifndef GRAFITE_IO_FIFO_DRIFT_CONTROLLER_HPP
#define GRAFITE_IO_FIFO_DRIFT_CONTROLLER_HPP

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "grafite/core/types.hpp"

namespace grafite::io {

/// Bounded proportional controller for nominal SRC clock correction.
///
/// Occupancy above target increases source consumption per output frame.
/// update() is allocation-free and must run on the audio consumer thread.
/// Optional exponential smoothing reduces callback-level FIFO jitter.
/// The smoothing factor is per update, not normalized to elapsed time.
class FifoDriftController {
public:
    FifoDriftController(
        double nominal_ratio,
        FrameCount target_frames,
        double max_correction_ppm = 200.0,
        double proportional_ppm_per_frame = 0.01,
        double occupancy_alpha = 1.0
    )
        : nominal_ratio_(nominal_ratio),
          target_frames_(target_frames),
          max_correction_ppm_(max_correction_ppm),
          proportional_ppm_per_frame_(proportional_ppm_per_frame),
          occupancy_alpha_(occupancy_alpha)
    {
        if (!std::isfinite(nominal_ratio) || nominal_ratio <= 0.0 ||
            nominal_ratio > 16.0 || target_frames == 0 ||
            !std::isfinite(max_correction_ppm) ||
            max_correction_ppm <= 0.0 || max_correction_ppm >= 1e6 ||
            !std::isfinite(proportional_ppm_per_frame) ||
            proportional_ppm_per_frame <= 0.0 ||
            !std::isfinite(occupancy_alpha) ||
            occupancy_alpha <= 0.0 || occupancy_alpha > 1.0 ||
            nominal_ratio * (1.0 + max_correction_ppm * 1e-6) > 16.0) {
            throw std::invalid_argument("Invalid FIFO drift controller");
        }
    }

    /// Computes source frames per output frame from a FIFO snapshot.
    [[nodiscard]] double update(FrameCount occupancy) noexcept
    {
        const double observed = static_cast<double>(occupancy);
        if (!initialized_) {
            filtered_occupancy_ = observed;
            initialized_ = true;
        } else {
            filtered_occupancy_ += occupancy_alpha_ *
                                   (observed - filtered_occupancy_);
        }
        const double error = filtered_occupancy_ -
                             static_cast<double>(target_frames_);
        correction_ppm_ = std::clamp(
            error * proportional_ppm_per_frame_,
            -max_correction_ppm_, max_correction_ppm_
        );
        return nominal_ratio_ * (1.0 + correction_ppm_ * 1e-6);
    }

    /// Disables the correction without changing the nominal ratio.
    void reset() noexcept
    {
        correction_ppm_ = 0.0;
        filtered_occupancy_ = 0.0;
        initialized_ = false;
    }

    [[nodiscard]] double filtered_occupancy() const noexcept
    {
        return filtered_occupancy_;
    }

    [[nodiscard]] double nominal_ratio() const noexcept
    {
        return nominal_ratio_;
    }

    [[nodiscard]] double correction_ppm() const noexcept
    {
        return correction_ppm_;
    }

    [[nodiscard]] FrameCount target_frames() const noexcept
    {
        return target_frames_;
    }

private:
    double nominal_ratio_;
    FrameCount target_frames_;
    double max_correction_ppm_;
    double proportional_ppm_per_frame_;
    double occupancy_alpha_;
    double correction_ppm_ = 0.0;
    double filtered_occupancy_ = 0.0;
    bool initialized_ = false;
};

}  // namespace grafite::io

#endif  // GRAFITE_IO_FIFO_DRIFT_CONTROLLER_HPP
