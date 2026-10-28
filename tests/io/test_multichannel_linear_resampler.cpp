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

#include <array>
#include <cassert>
#include <cmath>
#include <stdexcept>

#include "grafite/io/multichannel_linear_resampler.hpp"

int main()
{
    using grafite::io::MultichannelLinearResampler;
    MultichannelLinearResampler src(2, 48000.0, 96000.0);
    assert(src.channels() == 2);
    grafite::AudioBuffer first(2, 1);
    grafite::AudioBuffer second(2, 1);
    first(0, 0) = 0.0F;
    first(1, 0) = 100.0F;
    second(0, 0) = 2.0F;
    second(1, 0) = 102.0F;
    src.reset(first.view(), second.view());

    grafite::AudioBuffer output(2, 1);
    grafite::FrameCount next_frame = 2;
    auto pull = [&](grafite::AudioBufferView frame) noexcept {
        frame(0, 0) = 2.0F * static_cast<float>(next_frame);
        frame(1, 0) = frame(0, 0) + 100.0F;
        ++next_frame;
        return true;
    };
    for (int i = 0; i < 8; ++i) {
        assert(src.render(pull, output.view()));
        assert(std::abs(output(0, 0) - i) < 1e-6F);
        assert(std::abs(output(1, 0) - (100 + i)) < 1e-6F);
    }

    // Failure invalidates the whole frame, not an individual channel.
    auto exhausted = [](grafite::AudioBufferView) noexcept {
        return false;
    };
    assert(!src.render(exhausted, output.view()));
    assert(!src.render(pull, output.view()));
    src.reset(first.view(), second.view());
    assert(src.render(pull, output.view()));
    assert(output(0, 0) == 0.0F);
    assert(output(1, 0) == 100.0F);

    // Decimation by four must pull multiple complete frames per output.
    MultichannelLinearResampler decimator(2, 48000.0, 12000.0);
    decimator.reset(first.view(), second.view());
    grafite::FrameCount index = 2;
    auto decimate_pull = [&](grafite::AudioBufferView frame) noexcept {
        frame(0, 0) = 2.0F * static_cast<float>(index);
        frame(1, 0) = frame(0, 0) + 100.0F;
        ++index;
        return true;
    };
    for (int i = 0; i < 4; ++i) {
        assert(decimator.render(decimate_pull, output.view()));
        assert(output(0, 0) == 4.0F * i);
        assert(output(1, 0) == 100.0F + 4.0F * i);
    }

    // Failed pulls must not produce a partial multichannel output.
    MultichannelLinearResampler limited(2, 48000.0, 12000.0);
    limited.reset(first.view(), second.view());
    assert(limited.render(decimate_pull, output.view()));
    output(0, 0) = -999.0F;
    output(1, 0) = -999.0F;
    assert(!limited.render(
        [](grafite::AudioBufferView) noexcept { return false; },
        output.view()
    ));
    assert(output(0, 0) == -999.0F);
    assert(output(1, 0) == -999.0F);

    // Non-integer ratio and variable render groupings share one timeline.
    MultichannelLinearResampler fractional(2, 48000.0, 44100.0);
    fractional.reset(first.view(), second.view());
    grafite::FrameCount fractional_index = 2;
    auto fractional_pull = [&](grafite::AudioBufferView frame) noexcept {
        frame(0, 0) = 2.0F * static_cast<float>(fractional_index);
        frame(1, 0) = frame(0, 0) + 100.0F;
        ++fractional_index;
        return true;
    };
    for (int i = 0; i < 30; ++i) {
        assert(fractional.render(fractional_pull, output.view()));
        const float expected = static_cast<float>(
            i * 48000.0 / 44100.0 * 2.0
        );
        assert(std::abs(output(0, 0) - expected) < 1e-4F);
        assert(std::abs(output(1, 0) - (100.0F + expected)) < 1e-4F);
    }

    bool rejected = false;
    try {
        MultichannelLinearResampler invalid(0, 48000.0, 44100.0);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}
