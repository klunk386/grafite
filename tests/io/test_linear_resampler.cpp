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
#include <stdexcept>

#include "grafite/io/linear_resampler.hpp"

int main()
{
    using grafite::Sample;
    using grafite::io::LinearResampler;

    // Interpolation is independent of a ring buffer or audio backend.
    LinearResampler resampler(48000.0, 96000.0);
    assert(std::abs(resampler.ratio() - 0.5) < 1e-12);
    resampler.reset(0.0F, 2.0F);

    Sample next = 4.0F;
    auto pull = [&next](Sample& sample) noexcept {
        sample = next;
        next += 2.0F;
        return true;
    };

    Sample output = -1.0F;
    assert(resampler.render(pull, output) && output == 0.0F);
    assert(resampler.render(pull, output) && output == 1.0F);
    // Crossing a block boundary must preserve interpolation phase.
    assert(resampler.render(pull, output) && output == 2.0F);
    assert(resampler.render(pull, output) && output == 3.0F);

    // Source exhaustion is signalled without allocating or blocking.
    LinearResampler exhausted(48000.0, 24000.0);
    exhausted.reset(1.0F, 2.0F);
    auto no_input = [](Sample&) noexcept { return false; };
    assert(exhausted.render(no_input, output) && output == 1.0F);
    assert(!exhausted.render(no_input, output));

    // A new timeline can be established after an input gap.
    exhausted.reset(5.0F, 6.0F);
    assert(exhausted.render(no_input, output) && output == 5.0F);

    bool rejected = false;
    try {
        LinearResampler invalid(48000.0, 0.0);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}
