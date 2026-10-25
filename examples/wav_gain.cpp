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

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include "wav_gain_processor.hpp"

int main(int argc, char* argv[])
{
    if (argc != 4) {
        std::cerr << "Usage: grafite_wav_gain INPUT.wav OUTPUT.wav GAIN\n";
        return 1;
    }

    try {
        std::size_t consumed = 0;
        const auto gain = std::stof(argv[3], &consumed);
        if (consumed != std::string(argv[3]).size()) {
            throw std::invalid_argument("Invalid gain value");
        }

        grafite::examples::process_wav_gain(
            argv[1], argv[2], gain
        );
    } catch (const std::exception& error) {
        std::cerr << "WAV processing failed: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
