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
#include <utility>
#include <vector>

#include "grafite/core/engine.hpp"
#include "grafite/core/graph.hpp"
#include "grafite/io/audio_ring_buffer.hpp"
#include "grafite/io/engine_audio_processor.hpp"
#include "grafite/io/ring_buffer_engine_processor.hpp"
#include "grafite/io/ring_buffer_playback_processor.hpp"
#include "wav_gain_processor.hpp"

int main()
{
    grafite::io::AudioRingBuffer ring(1, 16);
    grafite::AudioBuffer samples(1, 8);
    for (std::size_t i = 0; i < 8; ++i) {
        samples(0, i) = static_cast<float>(i + 1);
    }
    assert(ring.write(samples.view(), 8) == 8);

    grafite::io::RingBufferPlaybackProcessor source(
        ring, 48000.0, 48000.0, 2, 1.0F
    );
    grafite::Graph graph;
    const auto input_id = graph.add_audio_input("Input", 2);
    const auto output_id = graph.add_audio_output("Output", 2);
    auto& gain = graph.add_node<grafite::examples::GainNode>(2, 0.25F);
    graph.connect(input_id, gain, grafite::AudioInputIndex{0});
    graph.connect(gain, grafite::AudioOutputIndex{0}, output_id);

    grafite::ProcessSpec spec;
    spec.sample_rate = 48000.0;
    spec.max_block_size = 4;
    grafite::Engine engine(std::move(graph).compile(spec));
    grafite::io::EngineAudioProcessor dsp(
        engine,
        std::vector<grafite::GraphAudioInputId>{input_id},
        std::vector<grafite::GraphAudioOutputId>{output_id}
    );
    grafite::io::RingBufferEngineProcessor processor(source, dsp, 4);
    grafite::AudioBuffer output(2, 4);
    std::array<grafite::AudioBufferView, 1> outputs{output.view()};
    processor.process({}, outputs, 4);

    for (std::size_t i = 0; i < 4; ++i) {
        const float expected = 0.25F * static_cast<float>(i + 1);
        assert(std::abs(output(0, i) - expected) < 1e-6F);
        assert(std::abs(output(1, i) - expected) < 1e-6F);
    }
}
