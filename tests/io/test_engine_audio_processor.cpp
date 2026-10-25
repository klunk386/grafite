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
#include <utility>
#include <vector>

#include "grafite/core/graph.hpp"
#include "grafite/io/engine_audio_processor.hpp"

namespace {

class GainNode final : public grafite::Node {
public:
    GainNode()
        : input_(add_audio_input(2)),
          output_(add_audio_output(2))
    {
    }

    void process(const grafite::ProcessContext& context) noexcept override
    {
        const auto input = context.audio_input(input_);
        const auto output = context.audio_output(output_);

        for (grafite::ChannelCount ch = 0; ch < 2; ++ch) {
            for (grafite::FrameCount i = 0; i < context.frames(); ++i) {
                output(ch, i) = input(ch, i) * 0.5F;
            }
        }
    }

private:
    grafite::AudioInputIndex input_;
    grafite::AudioOutputIndex output_;
};

void test_backend_independent_processing()
{
    grafite::Graph graph;
    const auto input_id = graph.add_audio_input("Input", 2);
    const auto output_id = graph.add_audio_output("Output", 2);
    auto& gain = graph.add_node<GainNode>();
    graph.connect(input_id, gain, grafite::AudioInputIndex{0});
    graph.connect(gain, grafite::AudioOutputIndex{0}, output_id);

    grafite::ProcessSpec spec;
    spec.sample_rate = 48000.0;
    spec.max_block_size = 8;

    grafite::Engine engine(std::move(graph).compile(spec));
    grafite::io::EngineAudioProcessor adapter(
        engine,
        std::vector<grafite::GraphAudioInputId>{input_id},
        std::vector<grafite::GraphAudioOutputId>{output_id}
    );

    grafite::io::AudioProcessor& processor = adapter;

    grafite::AudioBuffer source(2, 8);
    grafite::AudioBuffer destination(2, 8);
    source(0, 0) = 0.75F;
    source(1, 0) = -0.5F;
    source(0, 2) = 1.0F;
    destination(0, 7) = 0.875F;

    const std::array<grafite::ConstAudioBufferView, 1> inputs{
        source.view()
    };
    const std::array<grafite::AudioBufferView, 1> outputs{
        destination.view()
    };

    processor.process(inputs, outputs, 3);
    assert(destination(0, 0) == 0.375F);
    assert(destination(1, 0) == -0.25F);
    assert(destination(0, 2) == 0.5F);
    assert(destination(0, 7) == 0.875F);
    assert(engine.frame_position() == 3);

    source.clear();
    source(1, 0) = 0.25F;
    processor.process(inputs, outputs, 1);
    assert(destination(0, 0) == 0.0F);
    assert(destination(1, 0) == 0.125F);
    assert(engine.frame_position() == 4);

    processor.process(inputs, outputs, 0);
    assert(engine.frame_position() == 4);
    assert(destination(1, 0) == 0.125F);
}

void test_unconnected_output_is_silent()
{
    grafite::Graph graph;
    const auto output_id = graph.add_audio_output("Silent", 1);
    grafite::ProcessSpec spec;
    spec.sample_rate = 48000.0;
    spec.max_block_size = 4;

    grafite::Engine engine(std::move(graph).compile(spec));
    grafite::io::EngineAudioProcessor adapter(
        engine,
        {},
        std::vector<grafite::GraphAudioOutputId>{output_id}
    );

    grafite::AudioBuffer destination(1, 4);
    destination(0, 0) = 1.0F;
    const std::array<grafite::AudioBufferView, 1> outputs{
        destination.view()
    };

    adapter.process({}, outputs, 4);
    assert(destination(0, 0) == 0.0F);
    assert(engine.frame_position() == 4);
}

}  // namespace

int main()
{
    test_backend_independent_processing();
    test_unconnected_output_is_silent();
    return 0;
}
