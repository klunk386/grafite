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

#include <chrono>
#include <cmath>
#include <exception>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>

#include "grafite/core/engine.hpp"
#include "grafite/core/graph.hpp"
#include "grafite/io/coreaudio_output_backend.hpp"
#include "grafite/io/engine_audio_processor.hpp"

namespace {

class SineNode final : public grafite::Node {
public:
    SineNode()
        : output_(add_audio_output(2))
    {
    }

    void prepare(const grafite::ProcessSpec& spec) override
    {
        sample_rate_ = spec.sample_rate;
        phase_ = 0.0;
    }

    void process(const grafite::ProcessContext& context) noexcept override
    {
        constexpr double pi = 3.14159265358979323846;
        constexpr double frequency = 440.0;
        constexpr float amplitude = 0.1F;
        const double increment = 2.0 * pi * frequency / sample_rate_;
        const auto output = context.audio_output(output_);

        for (grafite::FrameCount frame = 0;
             frame < context.frames(); ++frame) {
            const auto value = static_cast<float>(
                amplitude * std::sin(phase_)
            );
            output(0, frame) = value;
            output(1, frame) = value;
            phase_ += increment;
            if (phase_ >= 2.0 * pi) {
                phase_ -= 2.0 * pi;
            }
        }
    }

private:
    grafite::AudioOutputIndex output_;
    double sample_rate_ = 48000.0;
    double phase_ = 0.0;
};

}  // namespace

int main()
{
    try {
        constexpr double sample_rate = 48000.0;
        constexpr grafite::FrameCount block_size = 256;

        grafite::Graph graph;
        const auto output_id = graph.add_audio_output("Stereo", 2);
        auto& oscillator = graph.add_node<SineNode>();
        graph.connect(
            oscillator,
            grafite::AudioOutputIndex{0},
            output_id
        );

        grafite::ProcessSpec spec;
        spec.sample_rate = sample_rate;
        spec.max_block_size = block_size;

        grafite::Engine engine(std::move(graph).compile(spec));
        grafite::io::EngineAudioProcessor processor(
            engine,
            {},
            std::vector<grafite::GraphAudioOutputId>{output_id}
        );

        grafite::io::CoreAudioOutputBackend backend(
            processor, sample_rate, block_size
        );

        std::cout << "Playing a quiet 440 Hz sine for 3 seconds...\n";
        backend.start();
        std::this_thread::sleep_for(std::chrono::seconds(3));
        backend.stop();
    } catch (const std::exception& error) {
        std::cerr << "CoreAudio example failed: "
                  << error.what() << '\n';
        return 1;
    }

    return 0;
}
