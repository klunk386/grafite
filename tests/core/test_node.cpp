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

#include "grafite/core/node.hpp"

namespace {

class TestNode final : public grafite::Node {
public:
    TestNode()
        : audio_input_a_(add_audio_input(2)),
          event_input_(add_event_input()),
          audio_output_(add_audio_output(2)),
          event_output_(add_event_output()),
          audio_input_b_(add_audio_input(1))
    {
    }

    [[nodiscard]]
    grafite::AudioInputIndex audio_input_a() const noexcept
    {
        return audio_input_a_;
    }

    [[nodiscard]]
    grafite::AudioInputIndex audio_input_b() const noexcept
    {
        return audio_input_b_;
    }

    [[nodiscard]]
    grafite::AudioOutputIndex audio_output() const noexcept
    {
        return audio_output_;
    }

    [[nodiscard]]
    grafite::EventInputIndex event_input() const noexcept
    {
        return event_input_;
    }

    [[nodiscard]]
    grafite::EventOutputIndex event_output() const noexcept
    {
        return event_output_;
    }

    void process(
        const grafite::ProcessContext& context
    ) noexcept override
    {
        const auto input = context.audio_input(audio_input_a_);
        auto output = context.audio_output(audio_output_);

        const auto frames = context.frames();

        for (grafite::FrameCount frame = 0;
             frame < frames;
             ++frame) {
            output(0, frame) = input(0, frame);
            output(1, frame) = input(1, frame);
        }
    }

private:
    grafite::AudioInputIndex audio_input_a_;
    grafite::EventInputIndex event_input_;
    grafite::AudioOutputIndex audio_output_;
    grafite::EventOutputIndex event_output_;
    grafite::AudioInputIndex audio_input_b_;
};


void test_default_identity()
{
    const TestNode node;

    assert(node.id() == grafite::invalid_node_id);
}


void test_port_declaration()
{
    const TestNode node;
    const auto& ports = node.ports();

    assert(ports.size() == 5);

    assert(ports[0].id() == 1);
    assert(ports[0].is_audio());
    assert(ports[0].is_input());
    assert(ports[0].channels() == 2);

    assert(ports[1].id() == 2);
    assert(ports[1].is_event());
    assert(ports[1].is_input());
    assert(ports[1].channels() == 0);

    assert(ports[2].id() == 3);
    assert(ports[2].is_audio());
    assert(ports[2].is_output());
    assert(ports[2].channels() == 2);

    assert(ports[3].id() == 4);
    assert(ports[3].is_event());
    assert(ports[3].is_output());
    assert(ports[3].channels() == 0);

    assert(ports[4].id() == 5);
    assert(ports[4].is_audio());
    assert(ports[4].is_input());
    assert(ports[4].channels() == 1);
}


void test_processing_indices()
{
    const TestNode node;

    assert(node.audio_input_a().value == 0);
    assert(node.audio_input_b().value == 1);

    assert(node.audio_output().value == 0);

    assert(node.event_input().value == 0);
    assert(node.event_output().value == 0);

    assert(
        node.port_id(node.audio_input_a()) == 1
    );

    assert(
        node.port_id(node.event_input()) == 2
    );

    assert(
        node.port_id(node.audio_output()) == 3
    );

    assert(
        node.port_id(node.event_output()) == 4
    );

    assert(
        node.port_id(node.audio_input_b()) == 5
    );
}


void test_processing()
{
    TestNode node;

    grafite::AudioBuffer input_a(2, 128);
    grafite::AudioBuffer input_b(1, 128);
    grafite::AudioBuffer output(2, 128);

    input_a(0, 32) = 0.25f;
    input_a(1, 32) = -0.25f;

    const auto& const_input_a = input_a;
    const auto& const_input_b = input_b;

    std::array<grafite::ConstAudioBufferView, 2> audio_inputs{
        const_input_a.view(),
        const_input_b.view()
    };

    std::array<grafite::AudioBufferView, 1> audio_outputs{
        output.view()
    };

    const grafite::ProcessContext context{
        128,
        0,
        audio_inputs,
        audio_outputs,
        {},
        {}
    };

    node.process(context);

    assert(output(0, 32) == 0.25f);
    assert(output(1, 32) == -0.25f);
}

}  // namespace


int main()
{
    test_default_identity();
    test_port_declaration();
    test_processing_indices();
    test_processing();

    return 0;
}