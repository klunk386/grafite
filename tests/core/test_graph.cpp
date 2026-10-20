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

#include "grafite/core/graph.hpp"

namespace {

class AudioSourceNode final : public grafite::Node {
public:
    explicit AudioSourceNode(
        grafite::ChannelCount channels
    )
        : output_(add_audio_output(channels))
    {
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
    grafite::AudioOutputIndex output_;
};


class AudioSinkNode final : public grafite::Node {
public:
    explicit AudioSinkNode(
        grafite::ChannelCount channels
    )
        : input_(add_audio_input(channels))
    {
    }

    [[nodiscard]]
    grafite::AudioInputIndex input() const noexcept
    {
        return input_;
    }

    void process(
        const grafite::ProcessContext&
    ) noexcept override
    {
    }

private:
    grafite::AudioInputIndex input_;
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
        const grafite::ProcessContext&
    ) noexcept override
    {
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
        const grafite::ProcessContext&
    ) noexcept override
    {
    }

private:
    grafite::EventInputIndex input_;
};


class LoopNode final : public grafite::Node {
public:
    LoopNode()
        : input_(add_audio_input(2)),
          output_(add_audio_output(2))
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


void test_add_nodes()
{
    grafite::Graph graph;

    auto& source =
        graph.add_node<AudioSourceNode>(2);

    auto& sink =
        graph.add_node<AudioSinkNode>(2);

    assert(source.id() == 1);
    assert(sink.id() == 2);

    assert(graph.nodes().size() == 2);

    assert(graph.node(1) == &source);
    assert(graph.node(2) == &sink);
    assert(graph.node(3) == nullptr);
}


void test_audio_connection()
{
    grafite::Graph graph;

    auto& source =
        graph.add_node<AudioSourceNode>(2);

    auto& sink =
        graph.add_node<AudioSinkNode>(2);

    graph.connect(
        source,
        source.output(),
        sink,
        sink.input()
    );

    assert(graph.connections().size() == 1);

    const auto& connection =
        graph.connections()[0];

    assert(connection.source.node_id == source.id());

    assert(
        connection.source.port_id ==
        source.port_id(source.output())
    );

    assert(
        connection.destination.node_id ==
        sink.id()
    );

    assert(
        connection.destination.port_id ==
        sink.port_id(sink.input())
    );
}


void test_event_connection()
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

    assert(graph.connections().size() == 1);

    const auto& connection =
        graph.connections()[0];

    assert(connection.source.node_id == source.id());

    assert(
        connection.source.port_id ==
        source.port_id(source.output())
    );

    assert(
        connection.destination.node_id ==
        sink.id()
    );

    assert(
        connection.destination.port_id ==
        sink.port_id(sink.input())
    );
}


void test_audio_fan_out()
{
    grafite::Graph graph;

    auto& source =
        graph.add_node<AudioSourceNode>(2);

    auto& sink_a =
        graph.add_node<AudioSinkNode>(2);

    auto& sink_b =
        graph.add_node<AudioSinkNode>(2);

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

    assert(graph.connections().size() == 2);
}


void test_reject_duplicate_destination()
{
    grafite::Graph graph;

    auto& source_a =
        graph.add_node<AudioSourceNode>(2);

    auto& source_b =
        graph.add_node<AudioSourceNode>(2);

    auto& sink =
        graph.add_node<AudioSinkNode>(2);

    graph.connect(
        source_a,
        source_a.output(),
        sink,
        sink.input()
    );

    bool rejected = false;

    try {
        graph.connect(
            source_b,
            source_b.output(),
            sink,
            sink.input()
        );
    } catch (const std::invalid_argument&) {
        rejected = true;
    }

    assert(rejected);
    assert(graph.connections().size() == 1);
}


void test_reject_channel_mismatch()
{
    grafite::Graph graph;

    auto& source =
        graph.add_node<AudioSourceNode>(1);

    auto& sink =
        graph.add_node<AudioSinkNode>(2);

    bool rejected = false;

    try {
        graph.connect(
            source,
            source.output(),
            sink,
            sink.input()
        );
    } catch (const std::invalid_argument&) {
        rejected = true;
    }

    assert(rejected);
    assert(graph.connections().empty());
}


void test_reject_foreign_node()
{
    grafite::Graph graph_a;
    grafite::Graph graph_b;

    auto& source =
        graph_a.add_node<AudioSourceNode>(2);

    auto& sink =
        graph_b.add_node<AudioSinkNode>(2);

    bool rejected = false;

    try {
        graph_a.connect(
            source,
            source.output(),
            sink,
            sink.input()
        );
    } catch (const std::invalid_argument&) {
        rejected = true;
    }

    assert(rejected);
    assert(graph_a.connections().empty());
}


void test_reject_self_connection()
{
    grafite::Graph graph;

    auto& node =
        graph.add_node<LoopNode>();

    bool rejected = false;

    try {
        graph.connect(
            node,
            node.output(),
            node,
            node.input()
        );
    } catch (const std::invalid_argument&) {
        rejected = true;
    }

    assert(rejected);
    assert(graph.connections().empty());
}


void test_reject_invalid_audio_source_index()
{
    grafite::Graph graph;

    auto& source =
        graph.add_node<AudioSourceNode>(2);

    auto& sink =
        graph.add_node<AudioSinkNode>(2);

    bool rejected = false;

    try {
        graph.connect(
            source,
            grafite::AudioOutputIndex{999},
            sink,
            sink.input()
        );
    } catch (const std::out_of_range&) {
        rejected = true;
    }

    assert(rejected);
    assert(graph.connections().empty());
}


void test_reject_invalid_audio_destination_index()
{
    grafite::Graph graph;

    auto& source =
        graph.add_node<AudioSourceNode>(2);

    auto& sink =
        graph.add_node<AudioSinkNode>(2);

    bool rejected = false;

    try {
        graph.connect(
            source,
            source.output(),
            sink,
            grafite::AudioInputIndex{999}
        );
    } catch (const std::out_of_range&) {
        rejected = true;
    }

    assert(rejected);
    assert(graph.connections().empty());
}


void test_reject_invalid_event_source_index()
{
    grafite::Graph graph;

    auto& source =
        graph.add_node<EventSourceNode>();

    auto& sink =
        graph.add_node<EventSinkNode>();

    bool rejected = false;

    try {
        graph.connect(
            source,
            grafite::EventOutputIndex{999},
            sink,
            sink.input()
        );
    } catch (const std::out_of_range&) {
        rejected = true;
    }

    assert(rejected);
    assert(graph.connections().empty());
}


void test_reject_invalid_event_destination_index()
{
    grafite::Graph graph;

    auto& source =
        graph.add_node<EventSourceNode>();

    auto& sink =
        graph.add_node<EventSinkNode>();

    bool rejected = false;

    try {
        graph.connect(
            source,
            source.output(),
            sink,
            grafite::EventInputIndex{999}
        );
    } catch (const std::out_of_range&) {
        rejected = true;
    }

    assert(rejected);
    assert(graph.connections().empty());
}

}  // namespace


int main()
{
    test_add_nodes();

    test_audio_connection();
    test_event_connection();

    test_audio_fan_out();

    test_reject_duplicate_destination();
    test_reject_channel_mismatch();
    test_reject_foreign_node();
    test_reject_self_connection();

    test_reject_invalid_audio_source_index();
    test_reject_invalid_audio_destination_index();

    test_reject_invalid_event_source_index();
    test_reject_invalid_event_destination_index();

    return 0;
}