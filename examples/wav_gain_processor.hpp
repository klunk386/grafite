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

#ifndef GRAFITE_EXAMPLES_WAV_GAIN_PROCESSOR_HPP
#define GRAFITE_EXAMPLES_WAV_GAIN_PROCESSOR_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "grafite/core/engine.hpp"
#include "grafite/core/graph.hpp"
#include "grafite/io/engine_audio_processor.hpp"
#include "grafite/io/file_audio_backend.hpp"

namespace grafite::examples {

/// Simple gain node used to demonstrate external graph audio I/O.
class GainNode final : public Node {
public:
    GainNode(ChannelCount channels, Sample gain)
        : input_(add_audio_input(channels)),
          output_(add_audio_output(channels)),
          gain_(gain)
    {
    }

    void process(const ProcessContext& context) noexcept override
    {
        const auto input = context.audio_input(input_);
        const auto output = context.audio_output(output_);

        for (ChannelCount ch = 0; ch < input.channels(); ++ch) {
            for (FrameCount frame = 0; frame < context.frames(); ++frame) {
                output(ch, frame) = input(ch, frame) * gain_;
            }
        }
    }

private:
    AudioInputIndex input_;
    AudioOutputIndex output_;
    Sample gain_;
};

/// Processes a PCM16 WAV through an Engine with a single gain node.
///
/// The file reader and writer operate outside the real-time DSP path.
/// The graph is compiled once and processes variable-sized audio blocks.
inline void process_wav_gain(
    const std::string& input_path,
    const std::string& output_path,
    Sample gain
)
{
    if (!std::isfinite(gain)) {
        throw std::invalid_argument("Gain must be finite");
    }

    const auto input_file = std::filesystem::absolute(input_path);
    const auto output_file = std::filesystem::absolute(output_path);

    if (input_file.lexically_normal() ==
        output_file.lexically_normal() ||
        (std::filesystem::exists(output_file) &&
         std::filesystem::equivalent(input_file, output_file))) {
        throw std::invalid_argument(
            "Input and output WAV paths must differ"
        );
    }

    io::FileAudioBackend backend(input_path);

    Graph graph;
    const auto input_id = graph.add_audio_input(
        "Input", backend.channels()
    );
    const auto output_id = graph.add_audio_output(
        "Output", backend.channels()
    );
    auto& gain_node = graph.add_node<GainNode>(
        backend.channels(), gain
    );

    graph.connect(input_id, gain_node, AudioInputIndex{0});
    graph.connect(gain_node, AudioOutputIndex{0}, output_id);

    ProcessSpec spec;
    spec.sample_rate = backend.sample_rate();
    spec.max_block_size = backend.block_size();

    Engine engine(std::move(graph).compile(spec));
    io::EngineAudioProcessor processor(
        engine,
        std::vector<GraphAudioInputId>{input_id},
        std::vector<GraphAudioOutputId>{output_id}
    );

    backend.run(processor, output_path);
}

}  // namespace grafite::examples

#endif  // GRAFITE_EXAMPLES_WAV_GAIN_PROCESSOR_HPP
