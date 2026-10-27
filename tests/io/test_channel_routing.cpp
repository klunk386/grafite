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
#include <stdexcept>
#include <vector>

#include "grafite/io/channel_routing.hpp"

int main()
{
    using grafite::io::ChannelRouting;
    grafite::AudioBuffer source(4, 2);
    for (grafite::ChannelCount ch = 0; ch < 4; ++ch) {
        for (grafite::FrameCount frame = 0; frame < 2; ++frame) {
            source(ch, frame) = static_cast<float>(10 * ch + frame);
        }
    }

    ChannelRouting identity({0, 1, 2, 3});
    identity.validate(4);
    grafite::AudioBuffer copied(4, 2);
    for (grafite::FrameCount frame = 0; frame < 2; ++frame) {
        identity.route(source.view(), copied.view(), frame, frame);
    }
    for (grafite::ChannelCount ch = 0; ch < 4; ++ch) {
        for (grafite::FrameCount frame = 0; frame < 2; ++frame) {
            assert(copied(ch, frame) == source(ch, frame));
        }
    }

    ChannelRouting swap({1, 0});
    swap.validate(4);
    grafite::AudioBuffer stereo(2, 2);
    swap.route(source.view(), stereo.view(), 1, 0);
    assert(stereo(0, 0) == 11.0F);
    assert(stereo(1, 0) == 1.0F);

    ChannelRouting duplicate({2, 2, 2});
    duplicate.validate(4);
    grafite::AudioBuffer triplicate(3, 2);
    duplicate.route(source.view(), triplicate.view(), 0, 1);
    for (grafite::ChannelCount ch = 0; ch < 3; ++ch) {
        assert(triplicate(ch, 1) == 20.0F);
    }

    bool rejected = false;
    try {
        ChannelRouting empty(std::vector<grafite::ChannelCount>{});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    rejected = false;
    try {
        ChannelRouting invalid({0, 4});
        invalid.validate(4);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}
