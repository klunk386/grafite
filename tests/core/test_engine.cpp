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

    return 0;
}