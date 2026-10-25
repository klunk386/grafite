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

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>

#include <iostream>

int main()
{
    AudioComponentDescription description{};
    description.componentType = kAudioUnitType_Output;
    description.componentSubType = kAudioUnitSubType_DefaultOutput;
    description.componentManufacturer = kAudioUnitManufacturer_Apple;

    const auto component = AudioComponentFindNext(nullptr, &description);
    if (component == nullptr) {
        std::cerr << "Default CoreAudio output component unavailable\n";
        return 1;
    }

    AudioComponentInstance unit = nullptr;
    if (AudioComponentInstanceNew(component, &unit) != noErr) {
        std::cerr << "Cannot create CoreAudio output unit\n";
        return 1;
    }

    const auto status = AudioComponentInstanceDispose(unit);
    if (status != noErr) {
        std::cerr << "Cannot dispose CoreAudio output unit\n";
        return 1;
    }

    return 0;
}
