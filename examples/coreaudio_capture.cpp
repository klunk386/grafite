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
#include <atomic>
#include <cstdint>
#include <chrono>
#include <cmath>
#include <exception>
#include <iostream>
#include <thread>

#include "grafite/io/coreaudio_input_backend.hpp"

namespace {

class LevelMeter final : public grafite::io::AudioInputSink {
public:
    void process_input(
        grafite::ConstAudioBufferView input,
        grafite::FrameCount frames
    ) noexcept override
    {
        double energy = 0.0;
        float peak = 0.0F;
        for (grafite::FrameCount frame = 0; frame < frames; ++frame) {
            const float sample = input(0, frame);
            energy += static_cast<double>(sample) * sample;
            peak = std::max(peak, std::abs(sample));
        }
        energy_.fetch_add(energy, std::memory_order_relaxed);
        frames_.fetch_add(frames, std::memory_order_relaxed);
        float previous = peak_.load(std::memory_order_relaxed);
        while (previous < peak &&
               !peak_.compare_exchange_weak(
                   previous, peak, std::memory_order_relaxed
               )) {
        }
    }

    void report() const
    {
        const auto frames = frames_.load(std::memory_order_relaxed);
        const auto energy = energy_.load(std::memory_order_relaxed);
        const auto peak = peak_.load(std::memory_order_relaxed);
        const auto rms = frames == 0 ? 0.0 :
            std::sqrt(energy / static_cast<double>(frames));
        std::cout << "Captured frames: " << frames
                  << "\nRMS: " << rms << "\nPeak: " << peak << '\n';
    }

private:
    std::atomic<double> energy_{0.0};
    std::atomic<std::uint64_t> frames_{0};
    std::atomic<float> peak_{0.0F};
};

}  // namespace

int main()
{
    try {
        LevelMeter meter;
        grafite::io::CoreAudioInputBackend backend(
            meter, 48000.0, 512
        );
        std::cout << "Capturing microphone for 5 seconds...\n";
        backend.start();
        std::this_thread::sleep_for(std::chrono::seconds(5));
        backend.stop();
        meter.report();
    } catch (const std::exception& error) {
        std::cerr << "CoreAudio capture failed: "
                  << error.what() << '\n';
        return 1;
    }
    return 0;
}
