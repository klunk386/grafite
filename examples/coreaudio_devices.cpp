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

#include <CoreAudio/CoreAudio.h>

#include <iostream>

namespace {

bool inspect_device(
    AudioObjectPropertySelector selector,
    const char* label
)
{
    AudioObjectPropertyAddress address{
        selector,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    AudioDeviceID device = kAudioObjectUnknown;
    UInt32 size = sizeof(device);
    const OSStatus status = AudioObjectGetPropertyData(
        kAudioObjectSystemObject, &address, 0, nullptr, &size, &device
    );
    if (status != noErr || size != sizeof(device) ||
        device == kAudioObjectUnknown) {
        std::cerr << label << ": unavailable (OSStatus "
                  << status << ")\n";
        return false;
    }

    std::cout << label << " device ID: " << device << '\n';

    address.mSelector = kAudioDevicePropertyNominalSampleRate;
    Float64 rate = 0.0;
    size = sizeof(rate);
    if (AudioObjectGetPropertyData(
            device, &address, 0, nullptr, &size, &rate
        ) == noErr && size == sizeof(rate)) {
        std::cout << "  nominal sample rate: " << rate << " Hz\n";
    } else {
        std::cout << "  nominal sample rate: unavailable\n";
    }

    address.mSelector = kAudioDevicePropertyBufferFrameSize;
    UInt32 frames = 0;
    size = sizeof(frames);
    if (AudioObjectGetPropertyData(
            device, &address, 0, nullptr, &size, &frames
        ) == noErr && size == sizeof(frames)) {
        std::cout << "  device buffer size: " << frames << " frames\n";
    } else {
        std::cout << "  device buffer size: unavailable\n";
    }
    return true;
}

}  // namespace

int main()
{
    const bool input = inspect_device(
        kAudioHardwarePropertyDefaultInputDevice, "Default input"
    );
    const bool output = inspect_device(
        kAudioHardwarePropertyDefaultOutputDevice, "Default output"
    );
    return input && output ? 0 : 1;
}
