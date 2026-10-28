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

#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "grafite/io/fifo_drift_controller.hpp"

int main()
{
    using grafite::io::FifoDriftController;
    FifoDriftController controller(48000.0 / 44100.0, 4800, 200.0, 1.0);
    const double nominal = controller.nominal_ratio();
    assert(controller.target_frames() == 4800);
    assert(controller.update(4800) == nominal);
    assert(controller.correction_ppm() == 0.0);

    const double high = controller.update(4900);
    assert(high > nominal);
    assert(std::abs(controller.correction_ppm() - 100.0) < 1e-9);
    const double low = controller.update(4700);
    assert(low < nominal);
    assert(std::abs(controller.correction_ppm() + 100.0) < 1e-9);

    assert(controller.update(1000000) > nominal);
    assert(controller.correction_ppm() == 200.0);
    assert(controller.update(0) < nominal);
    assert(controller.correction_ppm() == -200.0);
    assert(std::abs(controller.update(4800) - nominal) < 1e-12);
    assert(controller.update(4900) > nominal);
    controller.reset();
    assert(controller.correction_ppm() == 0.0);

    // EWMA initialization uses the first observation, without ramp-up.
    FifoDriftController smoothed(1.0, 100, 200.0, 1.0, 0.25);
    assert(smoothed.update(100) == 1.0);
    assert(smoothed.filtered_occupancy() == 100.0);
    assert(smoothed.update(140) > 1.0);
    assert(smoothed.filtered_occupancy() == 110.0);
    assert(smoothed.correction_ppm() == 10.0);
    assert(smoothed.update(100) > 1.0);
    assert(smoothed.filtered_occupancy() == 107.5);
    assert(smoothed.correction_ppm() == 7.5);
    smoothed.reset();
    assert(smoothed.filtered_occupancy() == 0.0);
    assert(smoothed.update(80) < 1.0);
    assert(smoothed.filtered_occupancy() == 80.0);
    assert(smoothed.correction_ppm() == -20.0);

    // Equivalent elapsed time gives the same EWMA for different blocks.
    FifoDriftController timed_a(1.0, 100, 200.0, 1.0, 1.0, 0.1);
    FifoDriftController timed_b(1.0, 100, 200.0, 1.0, 1.0, 0.1);
    assert(timed_a.update(100, 0.01) == 1.0);
    assert(timed_b.update(100, 0.01) == 1.0);
    assert(timed_a.update(200, 0.02) > 1.0);
    assert(timed_b.update(200, 0.01) > 1.0);
    assert(timed_b.update(200, 0.01) > 1.0);
    assert(std::abs(
        timed_a.filtered_occupancy() -
        timed_b.filtered_occupancy()
    ) < 1e-12);
    const double unchanged = timed_a.filtered_occupancy();
    assert(timed_a.update(0, 0.0) > 1.0);
    assert(timed_a.filtered_occupancy() == unchanged);
    assert(timed_a.update(
        0, std::numeric_limits<double>::quiet_NaN()
    ) > 1.0);
    assert(timed_a.filtered_occupancy() == unchanged);

    bool rejected = false;
    try {
        FifoDriftController invalid(0.0, 4800);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
    rejected = false;
    try {
        FifoDriftController invalid(1.0, 0);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
    rejected = false;
    try {
        FifoDriftController invalid(
            std::numeric_limits<double>::quiet_NaN(), 4800
        );
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    rejected = false;
    try {
        FifoDriftController invalid(1.0, 100, 200.0, 1.0, 0.0);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
    rejected = false;
    try {
        FifoDriftController invalid(1.0, 100, 200.0, 1.0, 1.5);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
    rejected = false;
    try {
        FifoDriftController invalid(1.0, 100, 200.0, 1.0, 1.0, -0.1);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}
