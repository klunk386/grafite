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
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <limits>

#include "grafite/event/midi_event.hpp"
#include "grafite/core/compiled_graph.hpp"
#include "grafite/core/graph.hpp"

namespace {

class TestNode final : public grafite::Node {
public:
    TestNode()
        : input_(add_audio_input(1)),
          output_(add_audio_output(1))
    {
    }

    [[nodiscard]]
    grafite::AudioInputIndex input() const noexcept
    {
        return input_;
    }

    [[nodiscard]]
    grafite::AudioOutputIndex output() const noexcept
    {
        return output_;
    }

    void process(
        const grafite::ProcessContext&
    ) noexcept override
    {
    }

private:
    grafite::AudioInputIndex input_;
    grafite::AudioOutputIndex output_;
};


class AudioSourceNode final : public grafite::Node {
public:
    AudioSourceNode()
        : output_(add_audio_output(1))
    {
    }

    [[nodiscard]]
    grafite::AudioOutputIndex output() const noexcept
    {
        return output_;
    }

    void process(
        const grafite::ProcessContext& context
    ) noexcept override
    {
        auto output = context.audio_output(output_);

        for (grafite::FrameCount frame = 0;
             frame < context.frames();
             ++frame) {
            output(0, frame) = 0.25F;
        }
    }

private:
    grafite::AudioOutputIndex output_;
};


class AudioSinkNode final : public grafite::Node {
public:
    AudioSinkNode()
        : input_(add_audio_input(1))
    {
    }

    [[nodiscard]]
    grafite::AudioInputIndex input() const noexcept
    {
        return input_;
    }

    void process(
        const grafite::ProcessContext& context
    ) noexcept override
    {
        const auto input = context.audio_input(input_);

        first_sample_ =
            context.frames() == 0
                ? 0.0F
                : input(0, 0);

        processed_frames_ = context.frames();
        frame_position_ = context.frame_position();
    }

    [[nodiscard]]
    grafite::Sample first_sample() const noexcept
    {
        return first_sample_;
    }

    [[nodiscard]]
    grafite::FrameCount processed_frames() const noexcept
    {
        return processed_frames_;
    }

    [[nodiscard]]
    grafite::FrameIndex frame_position() const noexcept
    {
        return frame_position_;
    }

private:
    grafite::AudioInputIndex input_;

    grafite::Sample first_sample_ = 0.0F;
    grafite::FrameCount processed_frames_ = 0;
    grafite::FrameIndex frame_position_ = 0;
};


class EventSourceNode final : public grafite::Node {
public:
    EventSourceNode()
        : output_(add_event_output())
    {
    }

    [[nodiscard]]
    grafite::EventOutputIndex output() const noexcept
    {
        return output_;
    }

    void process(
        const grafite::ProcessContext& context
    ) noexcept override
    {
        auto output = context.event_output(output_);

        const auto event =
            grafite::event::MidiEvent::make(
                3,
                0x90,
                60,
                100
            );

        const bool pushed = output.push(event);
        (void)pushed;
    }

private:
    grafite::EventOutputIndex output_;
};


class EventSinkNode final : public grafite::Node {
public:
    EventSinkNode()
        : input_(add_event_input())
    {
    }

    [[nodiscard]]
    grafite::EventInputIndex input() const noexcept
    {
        return input_;
    }

    void process(
        const grafite::ProcessContext& context
    ) noexcept override
    {
        const auto input = context.event_input(input_);

        event_count_ = input.size();

        if (!input.empty()) {
            const auto& event = input[0];

            frame_offset_ = event.frame_offset();

            if (grafite::event::MidiEvent::is_midi(event)) {
                status_ =
                    grafite::event::MidiEvent::byte(
                        event,
                        0
                    );

                note_ =
                    grafite::event::MidiEvent::byte(
                        event,
                        1
                    );

                velocity_ =
                    grafite::event::MidiEvent::byte(
                        event,
                        2
                    );
            }
        }
    }

    [[nodiscard]]
    std::size_t event_count() const noexcept
    {
        return event_count_;
    }

    [[nodiscard]]
    grafite::FrameCount frame_offset() const noexcept
    {
        return frame_offset_;
    }

    [[nodiscard]]
    std::uint8_t status() const noexcept
    {
        return status_;
    }

    [[nodiscard]]
    std::uint8_t note() const noexcept
    {
        return note_;
    }

    [[nodiscard]]
    std::uint8_t velocity() const noexcept
    {
        return velocity_;
    }

private:
    grafite::EventInputIndex input_;

    std::size_t event_count_ = 0;
    grafite::FrameCount frame_offset_ = 0;

    std::uint8_t status_ = 0;
    std::uint8_t note_ = 0;
    std::uint8_t velocity_ = 0;
};


class IntermittentAudioSourceNode final : public grafite::Node {
public:
    IntermittentAudioSourceNode()
        : output_(add_audio_output(1))
    {
    }

    [[nodiscard]]
    grafite::AudioOutputIndex output() const noexcept
    {
        return output_;
    }

    void process(
        const grafite::ProcessContext& context
    ) noexcept override
    {
        ++process_count_;

        if (process_count_ != 1) {
            return;
        }

        auto output = context.audio_output(output_);

        for (grafite::FrameCount frame = 0;
             frame < context.frames();
             ++frame) {
            output(0, frame) = 0.5F;
        }
    }

private:
    grafite::AudioOutputIndex output_;
    std::size_t process_count_ = 0;
};


class IntermittentEventSourceNode final : public grafite::Node {
public:
    IntermittentEventSourceNode()
        : output_(add_event_output())
    {
    }

    [[nodiscard]]
    grafite::EventOutputIndex output() const noexcept
    {
        return output_;
    }

    void process(
        const grafite::ProcessContext& context
    ) noexcept override
    {
        ++process_count_;

        if (process_count_ != 1) {
            return;
        }

        auto output = context.event_output(output_);

        const auto event =
            grafite::event::MidiEvent::make(
                4,
                0x90,
                64,
                100
            );

        const bool pushed = output.push(event);
        (void)pushed;
    }

private:
    grafite::EventOutputIndex output_;
    std::size_t process_count_ = 0;
};


class PrepareTestNode final : public grafite::Node {
public:
    void prepare(
        const grafite::ProcessSpec& spec
    ) override
    {
        ++prepare_count_;
        prepared_spec_ = spec;
    }

    void process(
        const grafite::ProcessContext&
    ) noexcept override
    {
    }

    [[nodiscard]]
    std::size_t prepare_count() const noexcept
    {
        return prepare_count_;
    }

    [[nodiscard]]
    const grafite::ProcessSpec& prepared_spec() const noexcept
    {
        return prepared_spec_;
    }

private:
    std::size_t prepare_count_ = 0;
    grafite::ProcessSpec prepared_spec_;
};


grafite::ProcessSpec make_spec()
{
    grafite::ProcessSpec spec;

    spec.sample_rate = 48000.0;
    spec.max_block_size = 512;

    return spec;
}


void test_empty_graph()
{
    grafite::Graph graph;

    auto compiled =
        std::move(graph).compile(make_spec());

    assert(compiled.node_count() == 0);
    assert(compiled.spec().sample_rate == 48000.0);
    assert(compiled.spec().max_block_size == 512);
}


void test_single_node()
{
    grafite::Graph graph;

    auto& node =
        graph.add_node<TestNode>();

    const grafite::NodeId node_id = node.id();

    auto compiled =
        std::move(graph).compile(make_spec());

    assert(compiled.node_count() == 1);
    assert(compiled.node(0).id() == node_id);
}


void test_linear_order()
{
    grafite::Graph graph;

    auto& first =
        graph.add_node<TestNode>();

    auto& second =
        graph.add_node<TestNode>();

    auto& third =
        graph.add_node<TestNode>();

    const grafite::NodeId first_id = first.id();
    const grafite::NodeId second_id = second.id();
    const grafite::NodeId third_id = third.id();

    graph.connect(
        first,
        first.output(),
        second,
        second.input()
    );

    graph.connect(
        second,
        second.output(),
        third,
        third.input()
    );

    auto compiled =
        std::move(graph).compile(make_spec());

    assert(compiled.node_count() == 3);

    assert(compiled.node(0).id() == first_id);
    assert(compiled.node(1).id() == second_id);
    assert(compiled.node(2).id() == third_id);
}


void test_reorders_nodes()
{
    grafite::Graph graph;

    auto& destination =
        graph.add_node<TestNode>();

    auto& middle =
        graph.add_node<TestNode>();

    auto& source =
        graph.add_node<TestNode>();

    const grafite::NodeId destination_id =
        destination.id();

    const grafite::NodeId middle_id =
        middle.id();

    const grafite::NodeId source_id =
        source.id();

    graph.connect(
        source,
        source.output(),
        middle,
        middle.input()
    );

    graph.connect(
        middle,
        middle.output(),
        destination,
        destination.input()
    );

    auto compiled =
        std::move(graph).compile(make_spec());

    assert(compiled.node_count() == 3);

    assert(compiled.node(0).id() == source_id);
    assert(compiled.node(1).id() == middle_id);
    assert(compiled.node(2).id() == destination_id);
}


void test_branching_graph()
{
    grafite::Graph graph;

    auto& destination =
        graph.add_node<TestNode>();

    auto& branch_a =
        graph.add_node<TestNode>();

    auto& branch_b =
        graph.add_node<TestNode>();

    auto& source =
        graph.add_node<TestNode>();

    const grafite::NodeId destination_id =
        destination.id();

    const grafite::NodeId branch_a_id =
        branch_a.id();

    const grafite::NodeId branch_b_id =
        branch_b.id();

    const grafite::NodeId source_id =
        source.id();

    graph.connect(
        source,
        source.output(),
        branch_a,
        branch_a.input()
    );

    graph.connect(
        source,
        source.output(),
        branch_b,
        branch_b.input()
    );

    graph.connect(
        branch_a,
        branch_a.output(),
        destination,
        destination.input()
    );

    bool rejected_second_source = false;

    try {
        graph.connect(
            branch_b,
            branch_b.output(),
            destination,
            destination.input()
        );
    } catch (const std::invalid_argument&) {
        rejected_second_source = true;
    }

    assert(rejected_second_source);

    auto compiled =
        std::move(graph).compile(make_spec());

    assert(compiled.node_count() == 4);

    const std::size_t not_found =
        compiled.node_count();

    std::size_t source_position = not_found;
    std::size_t branch_a_position = not_found;
    std::size_t branch_b_position = not_found;
    std::size_t destination_position = not_found;

    for (std::size_t index = 0;
         index < compiled.node_count();
         ++index) {
        const auto id = compiled.node(index).id();

        if (id == source_id) {
            source_position = index;
        } else if (id == branch_a_id) {
            branch_a_position = index;
        } else if (id == branch_b_id) {
            branch_b_position = index;
        } else if (id == destination_id) {
            destination_position = index;
        }
    }

    assert(source_position != not_found);
    assert(branch_a_position != not_found);
    assert(branch_b_position != not_found);
    assert(destination_position != not_found);

    assert(source_position < branch_a_position);
    assert(source_position < branch_b_position);
    assert(branch_a_position < destination_position);
}


void test_cycle_rejected()
{
    grafite::Graph graph;

    auto& first =
        graph.add_node<TestNode>();

    auto& second =
        graph.add_node<TestNode>();

    auto& third =
        graph.add_node<TestNode>();

    graph.connect(
        first,
        first.output(),
        second,
        second.input()
    );

    graph.connect(
        second,
        second.output(),
        third,
        third.input()
    );

    graph.connect(
        third,
        third.output(),
        first,
        first.input()
    );

    bool rejected = false;

    try {
        auto compiled =
            std::move(graph).compile(make_spec());

        (void)compiled;
    } catch (const std::invalid_argument&) {
        rejected = true;
    }

    assert(rejected);
}


void test_disconnected_nodes()
{
    grafite::Graph graph;

    auto& first =
        graph.add_node<TestNode>();

    auto& second =
        graph.add_node<TestNode>();

    const grafite::NodeId first_id = first.id();
    const grafite::NodeId second_id = second.id();

    auto compiled =
        std::move(graph).compile(make_spec());

    assert(compiled.node_count() == 2);

    assert(compiled.node(0).id() == first_id);
    assert(compiled.node(1).id() == second_id);
}

void test_audio_runtime_binding()
{
    grafite::Graph graph;

    auto& source =
        graph.add_node<AudioSourceNode>();

    auto& sink =
        graph.add_node<AudioSinkNode>();

    graph.connect(
        source,
        source.output(),
        sink,
        sink.input()
    );

    auto compiled =
        std::move(graph).compile(make_spec());

    compiled.process(128, 1024);

    assert(compiled.node_count() == 2);

    const auto& compiled_sink =
        static_cast<const AudioSinkNode&>(
            compiled.node(1)
        );

    assert(compiled_sink.first_sample() == 0.25F);
    assert(compiled_sink.processed_frames() == 128);
    assert(compiled_sink.frame_position() == 1024);
}


void test_unconnected_audio_input_is_silent()
{
    grafite::Graph graph;

    graph.add_node<AudioSinkNode>();

    auto compiled =
        std::move(graph).compile(make_spec());

    compiled.process(64, 0);

    const auto& sink =
        static_cast<const AudioSinkNode&>(
            compiled.node(0)
        );

    assert(sink.first_sample() == 0.0F);
    assert(sink.processed_frames() == 64);
}


void test_event_runtime_binding()
{
    grafite::Graph graph;

    auto& source =
        graph.add_node<EventSourceNode>();

    auto& sink =
        graph.add_node<EventSinkNode>();

    graph.connect(
        source,
        source.output(),
        sink,
        sink.input()
    );

    auto compiled =
        std::move(graph).compile(make_spec());

    compiled.process(128, 0);

    const auto& compiled_sink =
        static_cast<const EventSinkNode&>(
            compiled.node(1)
        );

    assert(compiled_sink.event_count() == 1);
    assert(compiled_sink.frame_offset() == 3);
    assert(compiled_sink.status() == 0x90);
    assert(compiled_sink.note() == 60);
    assert(compiled_sink.velocity() == 100);
}

void test_unconnected_event_input_is_empty()
{
    grafite::Graph graph;

    graph.add_node<EventSinkNode>();

    auto compiled =
        std::move(graph).compile(make_spec());

    compiled.process(128, 0);

    const auto& sink =
        static_cast<const EventSinkNode&>(
            compiled.node(0)
        );

    assert(sink.event_count() == 0);
}

void test_audio_fan_out_runtime_binding()
{
    grafite::Graph graph;

    auto& source =
        graph.add_node<AudioSourceNode>();

    auto& sink_a =
        graph.add_node<AudioSinkNode>();

    auto& sink_b =
        graph.add_node<AudioSinkNode>();

    graph.connect(
        source,
        source.output(),
        sink_a,
        sink_a.input()
    );

    graph.connect(
        source,
        source.output(),
        sink_b,
        sink_b.input()
    );

    auto compiled =
        std::move(graph).compile(make_spec());

    compiled.process(128, 0);

    const auto& compiled_sink_a =
        static_cast<const AudioSinkNode&>(
            compiled.node(1)
        );

    const auto& compiled_sink_b =
        static_cast<const AudioSinkNode&>(
            compiled.node(2)
        );

    assert(compiled_sink_a.first_sample() == 0.25F);
    assert(compiled_sink_b.first_sample() == 0.25F);
}

void test_consecutive_blocks()
{
    grafite::Graph graph;

    auto& source =
        graph.add_node<AudioSourceNode>();

    auto& sink =
        graph.add_node<AudioSinkNode>();

    graph.connect(
        source,
        source.output(),
        sink,
        sink.input()
    );

    auto compiled =
        std::move(graph).compile(make_spec());

    compiled.process(128, 0);

    const auto& compiled_sink =
        static_cast<const AudioSinkNode&>(
            compiled.node(1)
        );

    assert(compiled_sink.first_sample() == 0.25F);
    assert(compiled_sink.processed_frames() == 128);
    assert(compiled_sink.frame_position() == 0);

    compiled.process(64, 128);

    assert(compiled_sink.first_sample() == 0.25F);
    assert(compiled_sink.processed_frames() == 64);
    assert(compiled_sink.frame_position() == 128);
}

void test_audio_output_cleared_between_blocks()
{
    grafite::Graph graph;

    auto& source =
        graph.add_node<IntermittentAudioSourceNode>();

    auto& sink =
        graph.add_node<AudioSinkNode>();

    graph.connect(
        source,
        source.output(),
        sink,
        sink.input()
    );

    auto compiled =
        std::move(graph).compile(make_spec());

    compiled.process(128, 0);

    const auto& compiled_sink =
        static_cast<const AudioSinkNode&>(
            compiled.node(1)
        );

    assert(compiled_sink.first_sample() == 0.5F);

    compiled.process(128, 128);

    assert(compiled_sink.first_sample() == 0.0F);
}

void test_event_output_cleared_between_blocks()
{
    grafite::Graph graph;

    auto& source =
        graph.add_node<IntermittentEventSourceNode>();

    auto& sink =
        graph.add_node<EventSinkNode>();

    graph.connect(
        source,
        source.output(),
        sink,
        sink.input()
    );

    auto compiled =
        std::move(graph).compile(make_spec());

    compiled.process(128, 0);

    const auto& compiled_sink =
        static_cast<const EventSinkNode&>(
            compiled.node(1)
        );

    assert(compiled_sink.event_count() == 1);

    compiled.process(128, 128);

    assert(compiled_sink.event_count() == 0);
}

void test_node_prepare()
{
    grafite::Graph graph;

    graph.add_node<PrepareTestNode>();

    auto spec = make_spec();
    spec.max_events_per_port = 123;

    auto compiled =
        std::move(graph).compile(spec);

    const auto& node =
        static_cast<const PrepareTestNode&>(
            compiled.node(0)
        );

    assert(node.prepare_count() == 1);

    assert(
        node.prepared_spec().sample_rate ==
        spec.sample_rate
    );

    assert(
        node.prepared_spec().max_block_size ==
        spec.max_block_size
    );

    assert(
        node.prepared_spec().max_events_per_port ==
        spec.max_events_per_port
    );
}

void test_compiled_graph_move()
{
    grafite::Graph graph;

    auto& source =
        graph.add_node<AudioSourceNode>();

    auto& sink =
        graph.add_node<AudioSinkNode>();

    graph.connect(
        source,
        source.output(),
        sink,
        sink.input()
    );

    auto compiled =
        std::move(graph).compile(make_spec());

    auto moved =
        std::move(compiled);

    moved.process(128, 256);

    assert(moved.node_count() == 2);

    const auto& moved_sink =
        static_cast<const AudioSinkNode&>(
            moved.node(1)
        );

    assert(moved_sink.first_sample() == 0.25F);
    assert(moved_sink.processed_frames() == 128);
    assert(moved_sink.frame_position() == 256);
}

void test_invalid_process_spec_rejected()
{
    {
        grafite::Graph graph;

        auto spec = make_spec();
        spec.sample_rate = 0.0;

        bool rejected = false;

        try {
            auto compiled =
                std::move(graph).compile(spec);

            (void)compiled;
        } catch (const std::invalid_argument&) {
            rejected = true;
        }

        assert(rejected);
    }

    {
        grafite::Graph graph;

        auto spec = make_spec();
        spec.sample_rate = -48000.0;

        bool rejected = false;

        try {
            auto compiled =
                std::move(graph).compile(spec);

            (void)compiled;
        } catch (const std::invalid_argument&) {
            rejected = true;
        }

        assert(rejected);
    }

    {
        grafite::Graph graph;

        auto spec = make_spec();
        spec.sample_rate =
            std::numeric_limits<double>::quiet_NaN();

        bool rejected = false;

        try {
            auto compiled =
                std::move(graph).compile(spec);

            (void)compiled;
        } catch (const std::invalid_argument&) {
            rejected = true;
        }

        assert(rejected);
    }

    {
        grafite::Graph graph;

        auto spec = make_spec();
        spec.sample_rate =
            std::numeric_limits<double>::infinity();

        bool rejected = false;

        try {
            auto compiled =
                std::move(graph).compile(spec);

            (void)compiled;
        } catch (const std::invalid_argument&) {
            rejected = true;
        }

        assert(rejected);
    }

    {
        grafite::Graph graph;

        auto spec = make_spec();
        spec.max_block_size = 0;

        bool rejected = false;

        try {
            auto compiled =
                std::move(graph).compile(spec);

            (void)compiled;
        } catch (const std::invalid_argument&) {
            rejected = true;
        }

        assert(rejected);
    }
}

}  // namespace


int main()
{
    test_empty_graph();
    test_single_node();

    test_linear_order();
    test_reorders_nodes();
    test_branching_graph();

    test_cycle_rejected();
    test_disconnected_nodes();

    test_audio_runtime_binding();
    test_unconnected_audio_input_is_silent();

    test_event_runtime_binding();
    test_unconnected_event_input_is_empty();

    test_audio_fan_out_runtime_binding();

    test_consecutive_blocks();

    test_audio_output_cleared_between_blocks();
    test_event_output_cleared_between_blocks();

    test_node_prepare();
    test_compiled_graph_move();

    test_invalid_process_spec_rejected();

    return 0;
}