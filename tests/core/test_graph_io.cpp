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

#include <cassert>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "grafite/core/graph.hpp"

namespace {

class TestAudioNode final : public grafite::Node {
public:
    TestAudioNode(
        grafite::ChannelCount input_channels,
        grafite::ChannelCount output_channels
    )
    {
        (void)add_audio_input(input_channels);
        (void)add_audio_output(output_channels);
    }

    void process(
        const grafite::ProcessContext& context
    ) noexcept override
    {
        (void)context;
    }
};

class CaptureAudioNode final : public grafite::Node {
public:
    CaptureAudioNode()
        : input_(add_audio_input(1))
    {
    }

    void process(
        const grafite::ProcessContext& context
    ) noexcept override
    {
        const auto input = context.audio_input(input_);
        frames_ = context.frames();
        for (grafite::FrameCount i = 0; i < frames_; ++i) {
            samples_[i] = input(0, i);
        }
    }

    [[nodiscard]]
    grafite::Sample sample(grafite::FrameCount index) const noexcept
    {
        return samples_[index];
    }

    [[nodiscard]]
    grafite::FrameCount frames() const noexcept
    {
        return frames_;
    }

private:
    grafite::AudioInputIndex input_;
    grafite::Sample samples_[256]{};
    grafite::FrameCount frames_ = 0;
};

template <typename Exception, typename Function>
void expect_exception(Function&& function)
{
    bool caught = false;

    try {
        std::forward<Function>(function)();
    } catch (const Exception&) {
        caught = true;
    }

    assert(caught);
}

void test_port_declarations()
{
    static_assert(
        !std::is_same_v<
            grafite::GraphAudioInputId,
            grafite::GraphAudioOutputId
        >
    );

    grafite::Graph graph;

    const auto input = graph.add_audio_input("Guitar", 1);
    const auto output = graph.add_audio_output("Main", 2);

    assert(input.value != grafite::invalid_graph_port_id);
    assert(output.value != grafite::invalid_graph_port_id);
    assert(input.value != output.value);

    assert(graph.audio_ports().size() == 2);
    assert(graph.audio_ports()[0].name == "Guitar");
    assert(graph.audio_ports()[0].channels == 1);
    assert(
        graph.audio_ports()[0].direction ==
        grafite::GraphPortDirection::input
    );

    assert(graph.audio_ports()[1].name == "Main");
    assert(graph.audio_ports()[1].channels == 2);
    assert(
        graph.audio_ports()[1].direction ==
        grafite::GraphPortDirection::output
    );

    expect_exception<std::invalid_argument>([&] {
        (void)graph.add_audio_input("", 1);
    });

    expect_exception<std::invalid_argument>([&] {
        (void)graph.add_audio_output("Invalid", 0);
    });

    expect_exception<std::invalid_argument>([&] {
        (void)graph.add_audio_input("Guitar", 1);
    });

    // Names may be reused in opposite directions.
    (void)graph.add_audio_output("Guitar", 1);
}

void test_external_connections()
{
    grafite::Graph graph;

    const auto input = graph.add_audio_input("Input", 1);
    const auto output = graph.add_audio_output("Output", 2);

    auto& first = graph.add_node<TestAudioNode>(1, 2);
    auto& second = graph.add_node<TestAudioNode>(1, 2);

    graph.connect(
        input,
        first,
        grafite::AudioInputIndex{0}
    );

    graph.connect(
        input,
        second,
        grafite::AudioInputIndex{0}
    );

    graph.connect(
        first,
        grafite::AudioOutputIndex{0},
        output
    );

    assert(graph.audio_input_connections().size() == 2);
    assert(graph.audio_output_connections().size() == 1);

    const auto& first_connection =
        graph.audio_input_connections()[0];

    assert(first_connection.source.value == input.value);
    assert(first_connection.destination.node_id == first.id());
    assert(
        first_connection.destination.port_id ==
        first.port_id(grafite::AudioInputIndex{0})
    );

    const auto& output_connection =
        graph.audio_output_connections()[0];

    assert(output_connection.destination.value == output.value);
    assert(output_connection.source.node_id == first.id());
    assert(
        output_connection.source.port_id ==
        first.port_id(grafite::AudioOutputIndex{0})
    );

    // An external output accepts only one producer.
    expect_exception<std::invalid_argument>([&] {
        graph.connect(
            second,
            grafite::AudioOutputIndex{0},
            output
        );
    });
}

void test_invalid_external_connections()
{
    grafite::Graph graph;

    const auto mono_input = graph.add_audio_input("Mono", 1);
    const auto stereo_output =
        graph.add_audio_output("Stereo", 2);

    auto& mono = graph.add_node<TestAudioNode>(1, 1);
    auto& stereo = graph.add_node<TestAudioNode>(2, 2);

    // Channel counts must match.
    expect_exception<std::invalid_argument>([&] {
        graph.connect(
            mono_input,
            stereo,
            grafite::AudioInputIndex{0}
        );
    });

    expect_exception<std::invalid_argument>([&] {
        graph.connect(
            mono,
            grafite::AudioOutputIndex{0},
            stereo_output
        );
    });

    // Unknown external port identifiers are rejected.
    expect_exception<std::invalid_argument>([&] {
        graph.connect(
            grafite::GraphAudioInputId{9999},
            mono,
            grafite::AudioInputIndex{0}
        );
    });

    expect_exception<std::invalid_argument>([&] {
        graph.connect(
            stereo,
            grafite::AudioOutputIndex{0},
            grafite::GraphAudioOutputId{9999}
        );
    });

    // Out-of-range processing indices are rejected.
    expect_exception<std::out_of_range>([&] {
        graph.connect(
            mono_input,
            mono,
            grafite::AudioInputIndex{1}
        );
    });

    expect_exception<std::out_of_range>([&] {
        graph.connect(
            stereo,
            grafite::AudioOutputIndex{1},
            stereo_output
        );
    });

    // Nodes from other graphs cannot be connected.
    grafite::Graph other_graph;
    auto& foreign = other_graph.add_node<TestAudioNode>(1, 2);

    expect_exception<std::invalid_argument>([&] {
        graph.connect(
            mono_input,
            foreign,
            grafite::AudioInputIndex{0}
        );
    });

    expect_exception<std::invalid_argument>([&] {
        graph.connect(
            foreign,
            grafite::AudioOutputIndex{0},
            stereo_output
        );
    });
}

void test_input_connection_exclusivity()
{
    // External connection first, internal connection second.
    {
        grafite::Graph graph;

        const auto input = graph.add_audio_input("Input", 1);

        auto& source = graph.add_node<TestAudioNode>(1, 1);
        auto& destination = graph.add_node<TestAudioNode>(1, 1);

        graph.connect(
            input,
            destination,
            grafite::AudioInputIndex{0}
        );

        expect_exception<std::invalid_argument>([&] {
            graph.connect(
                source,
                grafite::AudioOutputIndex{0},
                destination,
                grafite::AudioInputIndex{0}
            );
        });

        assert(graph.audio_input_connections().size() == 1);
        assert(graph.connections().empty());
    }

    // Internal connection first, external connection second.
    {
        grafite::Graph graph;

        const auto input = graph.add_audio_input("Input", 1);

        auto& source = graph.add_node<TestAudioNode>(1, 1);
        auto& destination = graph.add_node<TestAudioNode>(1, 1);

        graph.connect(
            source,
            grafite::AudioOutputIndex{0},
            destination,
            grafite::AudioInputIndex{0}
        );

        expect_exception<std::invalid_argument>([&] {
            graph.connect(
                input,
                destination,
                grafite::AudioInputIndex{0}
            );
        });

        assert(graph.connections().size() == 1);
        assert(graph.audio_input_connections().empty());
    }

    // Duplicate external connections are rejected.
    {
        grafite::Graph graph;

        const auto input = graph.add_audio_input("Input", 1);
        auto& destination = graph.add_node<TestAudioNode>(1, 1);

        graph.connect(
            input,
            destination,
            grafite::AudioInputIndex{0}
        );

        expect_exception<std::invalid_argument>([&] {
            graph.connect(
                input,
                destination,
                grafite::AudioInputIndex{0}
            );
        });
    }
}

void test_external_input_processing()
{
    grafite::Graph graph;
    const auto input_id = graph.add_audio_input("Input", 1);
    auto& sink = graph.add_node<CaptureAudioNode>();
    graph.connect(input_id, sink, grafite::AudioInputIndex{0});

    grafite::ProcessSpec spec;
    spec.sample_rate = 48000.0;
    spec.max_block_size = 256;

    auto compiled = std::move(graph).compile(spec);
    auto input = compiled.audio_input_buffer(input_id);
    assert(input.channels() == 1);
    assert(input.frames() == 256);

    compiled.clear_audio_inputs(128);
    input(0, 0) = 0.25F;
    input(0, 127) = 0.75F;
    compiled.process(128, 0);

    const auto& captured =
        static_cast<const CaptureAudioNode&>(compiled.node(0));
    assert(captured.frames() == 128);
    assert(captured.sample(0) == 0.25F);
    assert(captured.sample(1) == 0.0F);
    assert(captured.sample(127) == 0.75F);

    compiled.clear_audio_inputs(64);
    input(0, 1) = 0.5F;
    compiled.process(64, 128);
    assert(captured.frames() == 64);
    assert(captured.sample(0) == 0.0F);
    assert(captured.sample(1) == 0.5F);
    assert(captured.sample(63) == 0.0F);

    expect_exception<std::invalid_argument>([&] {
        (void)compiled.audio_input_buffer(
            grafite::GraphAudioInputId{9999}
        );
    });
}

class ConstantAudioNode final : public grafite::Node {
public:
    ConstantAudioNode()
        : output_(add_audio_output(1))
    {
    }

    void process(
        const grafite::ProcessContext& context
    ) noexcept override
    {
        auto output = context.audio_output(output_);
        for (grafite::FrameCount i = 0; i < context.frames(); ++i) {
            output(0, i) = 0.625F;
        }
    }

private:
    grafite::AudioOutputIndex output_;
};

void test_external_output_processing()
{
    grafite::Graph graph;
    const auto output_id = graph.add_audio_output("Main", 1);
    const auto silent_id = graph.add_audio_output("Unused", 2);
    auto& source = graph.add_node<ConstantAudioNode>();
    graph.connect(source, grafite::AudioOutputIndex{0}, output_id);

    grafite::ProcessSpec spec;
    spec.sample_rate = 48000.0;
    spec.max_block_size = 256;

    auto compiled = std::move(graph).compile(spec);
    auto output = compiled.audio_output_buffer(output_id);
    auto silent = compiled.audio_output_buffer(silent_id);
    assert(output.channels() == 1);
    assert(output.frames() == 256);
    assert(silent.channels() == 2);

    compiled.process(128, 0);
    assert(output(0, 0) == 0.625F);
    assert(output(0, 127) == 0.625F);
    assert(silent(0, 0) == 0.0F);
    assert(silent(1, 127) == 0.0F);

    compiled.process(64, 128);
    assert(output(0, 0) == 0.625F);
    assert(output(0, 63) == 0.625F);
    assert(silent(0, 63) == 0.0F);

    expect_exception<std::invalid_argument>([&] {
        (void)compiled.audio_output_buffer(
            grafite::GraphAudioOutputId{9999}
        );
    });
}

class GainAudioNode final : public grafite::Node {
public:
    explicit GainAudioNode(grafite::Sample gain)
        : input_(add_audio_input(2)),
          output_(add_audio_output(2)),
          gain_(gain)
    {
    }

    void process(
        const grafite::ProcessContext& context
    ) noexcept override
    {
        const auto input = context.audio_input(input_);
        auto output = context.audio_output(output_);

        for (grafite::ChannelCount channel = 0;
             channel < 2; ++channel) {
            for (grafite::FrameCount frame = 0;
                 frame < context.frames(); ++frame) {
                output(channel, frame) =
                    input(channel, frame) * gain_;
            }
        }
    }

private:
    grafite::AudioInputIndex input_;
    grafite::AudioOutputIndex output_;
    grafite::Sample gain_;
};

void test_external_audio_end_to_end()
{
    grafite::Graph graph;
    const auto input_id = graph.add_audio_input("Stereo In", 2);
    const auto output_id = graph.add_audio_output("Stereo Out", 2);
    const auto silent_id = graph.add_audio_output("Unconnected", 1);

    auto& first = graph.add_node<GainAudioNode>(0.5F);
    auto& second = graph.add_node<GainAudioNode>(0.25F);
    graph.connect(input_id, first, grafite::AudioInputIndex{0});
    graph.connect(
        first,
        grafite::AudioOutputIndex{0},
        second,
        grafite::AudioInputIndex{0}
    );
    graph.connect(second, grafite::AudioOutputIndex{0}, output_id);

    grafite::ProcessSpec spec;
    spec.sample_rate = 48000.0;
    spec.max_block_size = 256;

    auto compiled = std::move(graph).compile(spec);
    auto input = compiled.audio_input_buffer(input_id);
    const auto output = compiled.audio_output_buffer(output_id);
    const auto silent = compiled.audio_output_buffer(silent_id);

    assert(input.channels() == 2);
    assert(output.channels() == 2);
    assert(input.channel_stride() == 256);
    assert(output.channel_stride() == 256);

    // Process two variable-size blocks through both DSP nodes.
    const grafite::FrameCount block_sizes[] = {128, 64};
    grafite::FrameIndex position = 0;

    for (const auto frames : block_sizes) {
        compiled.clear_audio_inputs(frames);

        // Leave some samples unwritten to verify silence propagation.
        for (grafite::FrameCount frame = 0;
             frame < frames / 2; ++frame) {
            input(0, frame) = 0.8F;
            input(1, frame) = -0.4F;
        }

        compiled.process(frames, position);

        for (grafite::FrameCount frame = 0;
             frame < frames; ++frame) {
            const bool written = frame < frames / 2;
            assert(output(0, frame) == (written ? 0.1F : 0.0F));
            assert(output(1, frame) == (written ? -0.05F : 0.0F));
            assert(silent(0, frame) == 0.0F);
        }

        position += frames;
    }
}

}  // namespace

int main()
{
    test_port_declarations();
    test_external_connections();
    test_invalid_external_connections();
    test_input_connection_exclusivity();
    test_external_input_processing();
    test_external_output_processing();
    test_external_audio_end_to_end();

    return 0;
}
