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
#include <utility>

#include "grafite/core/engine.hpp"
#include "grafite/core/graph.hpp"

namespace {

class TrackingNode final : public grafite::Node {
public:
    void process(
        const grafite::ProcessContext& context
    ) noexcept override
    {
        ++process_count_;
        last_frames_ = context.frames();
        last_frame_position_ = context.frame_position();
    }

    void reset() noexcept override
    {
        ++reset_count_;
    }

    [[nodiscard]]
    std::size_t process_count() const noexcept
    {
        return process_count_;
    }

    [[nodiscard]]
    std::size_t reset_count() const noexcept
    {
        return reset_count_;
    }

    [[nodiscard]]
    grafite::FrameCount last_frames() const noexcept
    {
        return last_frames_;
    }

    [[nodiscard]]
    grafite::FrameIndex last_frame_position() const noexcept
    {
        return last_frame_position_;
    }

private:
    std::size_t process_count_ = 0;
    std::size_t reset_count_ = 0;

    grafite::FrameCount last_frames_ = 0;
    grafite::FrameIndex last_frame_position_ = 0;
};


grafite::ProcessSpec make_spec()
{
    grafite::ProcessSpec spec;

    spec.sample_rate = 48000.0;
    spec.max_block_size = 512;
    spec.max_events_per_port = 128;

    return spec;
}


grafite::Engine make_engine()
{
    grafite::Graph graph;

    graph.add_node<TrackingNode>();

    auto compiled =
        std::move(graph).compile(make_spec());

    return grafite::Engine(
        std::move(compiled)
    );
}


void test_initial_state()
{
    auto engine = make_engine();

    assert(engine.spec().sample_rate == 48000.0);
    assert(engine.spec().max_block_size == 512);
    assert(engine.spec().max_events_per_port == 128);

    assert(engine.frame_position() == 0);
}


void test_process_advances_timeline()
{
    auto engine = make_engine();

    engine.process(128);

    assert(engine.frame_position() == 128);
}


void test_variable_block_sizes()
{
    auto engine = make_engine();

    engine.process(128);
    assert(engine.frame_position() == 128);

    engine.process(64);
    assert(engine.frame_position() == 192);

    engine.process(256);
    assert(engine.frame_position() == 448);
}


void test_process_context_timeline()
{
    grafite::Graph graph;

    graph.add_node<TrackingNode>();

    auto compiled =
        std::move(graph).compile(make_spec());

    grafite::Engine engine(
        std::move(compiled)
    );

    engine.process(128);
    engine.process(64);

    const auto& node =
        static_cast<const TrackingNode&>(
            engine.graph().node(0)
        );

    assert(node.process_count() == 2);
    assert(node.last_frames() == 64);
    assert(node.last_frame_position() == 128);
}


void test_zero_frame_block_is_ignored()
{
    auto engine = make_engine();

    engine.process(0);

    assert(engine.frame_position() == 0);
}


void test_reset_restarts_timeline()
{
    auto engine = make_engine();

    engine.process(256);

    assert(engine.frame_position() == 256);

    engine.reset();

    assert(engine.frame_position() == 0);
}


void test_reset_propagates_to_nodes()
{
    grafite::Graph graph;

    graph.add_node<TrackingNode>();

    auto compiled =
        std::move(graph).compile(make_spec());

    grafite::Engine engine(
        std::move(compiled)
    );

    engine.reset();

    const auto& node =
        static_cast<const TrackingNode&>(
            engine.graph().node(0)
        );

    assert(node.reset_count() == 1);
}


void test_engine_move()
{
    auto engine = make_engine();

    engine.process(128);

    auto moved =
        std::move(engine);

    assert(moved.frame_position() == 128);

    moved.process(64);

    assert(moved.frame_position() == 192);
}

class StereoGainNode final : public grafite::Node {
public:
    StereoGainNode()
        : input_(add_audio_input(2)),
          output_(add_audio_output(2))
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
                    input(channel, frame) * 0.5F;
            }
        }
    }

private:
    grafite::AudioInputIndex input_;
    grafite::AudioOutputIndex output_;
};


void test_external_audio_io()
{
    grafite::Graph graph;

    const auto input_id = graph.add_audio_input("Input", 2);
    const auto output_id = graph.add_audio_output("Output", 2);
    auto& gain = graph.add_node<StereoGainNode>();

    graph.connect(input_id, gain, grafite::AudioInputIndex{0});
    graph.connect(gain, grafite::AudioOutputIndex{0}, output_id);

    auto compiled = std::move(graph).compile(make_spec());
    grafite::Engine engine(std::move(compiled));

    auto input = engine.audio_input_buffer(input_id);
    const auto output = engine.audio_output_buffer(output_id);

    assert(input.channels() == 2);
    assert(output.channels() == 2);
    assert(input.frames() == 512);
    assert(output.frames() == 512);

    engine.clear_audio_inputs(128);
    input(0, 0) = 0.5F;
    input(1, 0) = -0.5F;
    input(0, 127) = 1.0F;
    engine.process(128);

    assert(output(0, 0) == 0.25F);
    assert(output(1, 0) == -0.25F);
    assert(output(0, 1) == 0.0F);
    assert(output(0, 127) == 0.5F);
    assert(engine.frame_position() == 128);

    engine.clear_audio_inputs(64);
    input(1, 1) = 0.25F;
    engine.process(64);

    assert(output(0, 0) == 0.0F);
    assert(output(1, 1) == 0.125F);
    assert(output(0, 63) == 0.0F);
    assert(engine.frame_position() == 192);
}

void test_external_audio_views_after_engine_move()
{
    grafite::Graph graph;

    const auto input_id = graph.add_audio_input("Input", 2);
    const auto output_id = graph.add_audio_output("Output", 2);
    const auto silent_id = graph.add_audio_output("Silent", 1);
    auto& gain = graph.add_node<StereoGainNode>();

    graph.connect(input_id, gain, grafite::AudioInputIndex{0});
    graph.connect(gain, grafite::AudioOutputIndex{0}, output_id);

    auto compiled = std::move(graph).compile(make_spec());
    grafite::Engine engine(std::move(compiled));

    auto input = engine.audio_input_buffer(input_id);
    const auto output = engine.audio_output_buffer(output_id);
    const auto silent = engine.audio_output_buffer(silent_id);

    auto* const input_data = input.channel(0);
    const auto* const output_data = output.channel(0);
    const auto* const silent_data = silent.channel(0);

    // Cached views must survive Engine move construction.
    grafite::Engine moved(std::move(engine));

    assert(moved.audio_input_buffer(input_id).channel(0) == input_data);
    assert(moved.audio_output_buffer(output_id).channel(0) == output_data);
    assert(moved.audio_output_buffer(silent_id).channel(0) == silent_data);

    moved.clear_audio_inputs(64);
    input(0, 0) = 0.75F;
    input(1, 1) = -0.5F;
    moved.process(64);

    assert(output(0, 0) == 0.375F);
    assert(output(1, 1) == -0.25F);
    assert(output(0, 1) == 0.0F);
    assert(silent(0, 0) == 0.0F);
    assert(moved.frame_position() == 64);

    // Move assignment must preserve views of the incoming graph.
    auto destination = make_engine();
    destination = std::move(moved);

    assert(destination.audio_input_buffer(input_id).channel(0) ==
           input_data);
    assert(destination.audio_output_buffer(output_id).channel(0) ==
           output_data);
    assert(destination.audio_output_buffer(silent_id).channel(0) ==
           silent_data);

    destination.clear_audio_inputs(32);
    input(0, 0) = -1.0F;
    destination.process(32);

    assert(output(0, 0) == -0.5F);
    assert(output(1, 1) == 0.0F);
    assert(silent(0, 0) == 0.0F);
    assert(destination.frame_position() == 96);
}

void test_external_audio_reset_contract()
{
    grafite::Graph graph;

    const auto input_id = graph.add_audio_input("Input", 2);
    const auto output_id = graph.add_audio_output("Output", 2);
    const auto silent_id = graph.add_audio_output("Silent", 1);
    auto& gain = graph.add_node<StereoGainNode>();

    graph.connect(input_id, gain, grafite::AudioInputIndex{0});
    graph.connect(gain, grafite::AudioOutputIndex{0}, output_id);

    auto compiled = std::move(graph).compile(make_spec());
    grafite::Engine engine(std::move(compiled));

    auto input = engine.audio_input_buffer(input_id);
    const auto output = engine.audio_output_buffer(output_id);
    const auto silent = engine.audio_output_buffer(silent_id);

    engine.clear_audio_inputs(128);
    input(0, 0) = 0.75F;
    input(1, 127) = -0.5F;
    engine.process(128);

    assert(output(0, 0) == 0.375F);
    assert(output(1, 127) == -0.25F);
    assert(engine.frame_position() == 128);

    engine.reset();

    // Reset restarts the timeline without changing graph-owned buffers.
    assert(engine.frame_position() == 0);
    assert(engine.audio_input_buffer(input_id).channel(0) ==
           input.channel(0));
    assert(engine.audio_output_buffer(output_id).channel(0) ==
           output.channel(0));
    assert(engine.audio_output_buffer(silent_id).channel(0) ==
           silent.channel(0));

    // The caller clears each new block; reset does not do so implicitly.
    engine.clear_audio_inputs(64);
    input(0, 1) = -1.0F;
    engine.process(64);

    assert(output(0, 0) == 0.0F);
    assert(output(0, 1) == -0.5F);
    assert(output(1, 1) == 0.0F);
    assert(silent(0, 0) == 0.0F);
    assert(engine.frame_position() == 64);
}

void test_empty_graph_and_zero_frame_contract()
{
    grafite::Graph graph;
    auto compiled = std::move(graph).compile(make_spec());
    grafite::Engine engine(std::move(compiled));

    assert(engine.graph().node_count() == 0);
    engine.clear_audio_inputs(0);
    engine.process(0);
    assert(engine.frame_position() == 0);

    engine.clear_audio_inputs(1);
    engine.process(1);
    assert(engine.frame_position() == 1);

    engine.process(512);
    assert(engine.frame_position() == 513);

    engine.reset();
    assert(engine.frame_position() == 0);
}


void test_zero_frame_preserves_external_audio()
{
    grafite::Graph graph;

    const auto input_id = graph.add_audio_input("Input", 2);
    const auto output_id = graph.add_audio_output("Output", 2);
    auto& gain = graph.add_node<StereoGainNode>();

    graph.connect(input_id, gain, grafite::AudioInputIndex{0});
    graph.connect(gain, grafite::AudioOutputIndex{0}, output_id);

    auto compiled = std::move(graph).compile(make_spec());
    grafite::Engine engine(std::move(compiled));
    auto input = engine.audio_input_buffer(input_id);
    const auto output = engine.audio_output_buffer(output_id);

    engine.clear_audio_inputs(64);
    input(0, 0) = 0.5F;
    engine.process(64);
    assert(output(0, 0) == 0.25F);

    input(0, 0) = -0.75F;
    engine.clear_audio_inputs(0);
    engine.process(0);

    assert(engine.frame_position() == 64);
    assert(input(0, 0) == -0.75F);
    assert(output(0, 0) == 0.25F);
}


}  // namespace


int main()
{
    test_initial_state();

    test_process_advances_timeline();
    test_variable_block_sizes();
    test_process_context_timeline();

    test_zero_frame_block_is_ignored();

    test_reset_restarts_timeline();
    test_reset_propagates_to_nodes();

    test_engine_move();
    test_external_audio_io();
    test_external_audio_views_after_engine_move();
    test_external_audio_reset_contract();
    test_empty_graph_and_zero_frame_contract();
    test_zero_frame_preserves_external_audio();

    return 0;
}