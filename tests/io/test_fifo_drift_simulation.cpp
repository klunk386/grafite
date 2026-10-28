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

#include <algorithm>
#include <cassert>
#include <cmath>

#include "grafite/io/fifo_drift_controller.hpp"

namespace {

void simulate(double source_clock_ppm)
{
    constexpr double sample_rate = 48000.0;
    constexpr double callback_frames = 256.0;
    constexpr double elapsed = callback_frames / sample_rate;
    constexpr double target = 4800.0;
    constexpr int callbacks = 30000;

    grafite::io::FifoDriftController controller(
        1.0, 4800, 300.0, 2.0, 1.0, 0.1
    );
    double occupancy = target;
    double uncorrected = target;
    double largest_error = 0.0;
    for (int i = 0; i < callbacks; ++i) {
        const auto measured = static_cast<grafite::FrameCount>(
            std::round(occupancy)
        );
        const double ratio = controller.update(measured, elapsed);
        // A producer clock faster by +ppm adds frames per output block.
        occupancy += callback_frames * (
            1.0 + source_clock_ppm * 1e-6 - ratio
        );
        uncorrected += callback_frames * source_clock_ppm * 1e-6;
        largest_error = std::max(largest_error, std::abs(occupancy - target));
        assert(occupancy > 0.0 && occupancy < 9600.0);
        assert(std::abs(controller.correction_ppm()) <= 300.0);
    }
    // The proportional controller has a nonzero steady-state error.
    // Compare bounded residual against uncorrected clock divergence.
    assert(std::abs(occupancy - target) < 80.0);
    assert(std::abs(uncorrected - target) > 700.0);
    assert(largest_error < 250.0);
    const double expected_ppm = source_clock_ppm;
    assert(std::abs(controller.correction_ppm() - expected_ppm) < 10.0);
}

}  // namespace

int main()
{
    simulate(100.0);
    simulate(-100.0);
}
