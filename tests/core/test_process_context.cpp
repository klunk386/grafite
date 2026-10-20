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

#include "grafite/core/process_context.hpp"
#include "grafite/event/midi_event.hpp"

namespace {

void test_empty_context()
{
    const grafite::ProcessContext context;

    assert(context.frames() == 0);
    assert(context.frame_position() == 0);

    assert(context.audio_input_count() == 0);
    assert(context.audio_output_count() == 0);

    assert(context.event_input_count() == 0);
    assert(context.event_output_count() == 0);
}


void test_block_information()
{
    const grafite::ProcessContext context{
        256,
        4096,
        {},
        {},
        {},
        {}
    };

    assert(context.frames() == 256);
    assert(context.frame_position() == 4096);
}


void test_audio_input()
{
    grafite::AudioBuffer input(2, 256);

    input(0, 42) = 0.5f;
    input(1, 42) = -0.5f;

    const auto& const_input = input;

    std::array<grafite::ConstAudioBufferView, 1> inputs{
        const_input.view()
    };

    const grafite::ProcessContext context{
        256,
        0,
        inputs,
        {},
        {},
        {}
    };

    assert(context.audio_input_count() == 1);

    const auto view = context.audio_input(
        grafite::AudioInputIndex{0}
    );

    assert(view.channels() == 2);
    assert(view.frames() == 256);

    assert(view(0, 42) == 0.5f);
    assert(view(1, 42) == -0.5f);
}


void test_audio_output()
{
    grafite::AudioBuffer output(2, 256);

    std::array<grafite::AudioBufferView, 1> outputs{
        output.view()
    };

    const grafite::ProcessContext context{
        256,
        0,
        {},
        outputs,
        {},
        {}
    };

    assert(context.audio_output_count() == 1);

    auto view = context.audio_output(
        grafite::AudioOutputIndex{0}
    );

    view(0, 42) = 0.75f;
    view(1, 42) = -0.75f;

    assert(output(0, 42) == 0.75f);
    assert(output(1, 42) == -0.75f);
}


void test_multiple_audio_ports()
{
    grafite::AudioBuffer input_a(1, 128);
    grafite::AudioBuffer input_b(2, 128);
    grafite::AudioBuffer output(2, 128);

    const auto& const_input_a = input_a;
    const auto& const_input_b = input_b;

    std::array<grafite::ConstAudioBufferView, 2> inputs{
        const_input_a.view(),
        const_input_b.view()
    };

    std::array<grafite::AudioBufferView, 1> outputs{
        output.view()
    };

    const grafite::ProcessContext context{
        128,
        8192,
        inputs,
        outputs,
        {},
        {}
    };

    assert(context.audio_input_count() == 2);
    assert(context.audio_output_count() == 1);

    assert(
        context.audio_input(
            grafite::AudioInputIndex{0}
        ).channels() == 1
    );

    assert(
        context.audio_input(
            grafite::AudioInputIndex{1}
        ).channels() == 2
    );

    assert(
        context.audio_output(
            grafite::AudioOutputIndex{0}
        ).channels() == 2
    );
}


void test_event_input()
{
    grafite::event::EventBuffer input(8);

    assert(
        input.push(
            grafite::event::MidiEvent::make(
                42,
                0x90,
                60,
                100
            )
        )
    );

    const auto& const_input = input;

    std::array<grafite::event::ConstEventBufferView, 1> inputs{
        const_input.view()
    };

    const grafite::ProcessContext context{
        256,
        4096,
        {},
        {},
        inputs,
        {}
    };

    assert(context.event_input_count() == 1);

    const auto view = context.event_input(
        grafite::EventInputIndex{0}
    );

    assert(view.size() == 1);
    assert(view[0].frame_offset() == 42);

    assert(
        grafite::event::MidiEvent::is_midi(view[0])
    );

    assert(
        grafite::event::MidiEvent::byte(view[0], 0)
        == 0x90
    );

    assert(
        grafite::event::MidiEvent::byte(view[0], 1)
        == 60
    );

    assert(
        grafite::event::MidiEvent::byte(view[0], 2)
        == 100
    );
}


void test_event_output()
{
    grafite::event::EventBuffer output(8);

    std::array<grafite::event::EventBufferView, 1> outputs{
        output.view()
    };

    const grafite::ProcessContext context{
        256,
        4096,
        {},
        {},
        {},
        outputs
    };

    assert(context.event_output_count() == 1);

    auto view = context.event_output(
        grafite::EventOutputIndex{0}
    );

    assert(
        view.push(
            grafite::event::MidiEvent::make(
                128,
                0x90,
                64,
                100
            )
        )
    );

    assert(output.size() == 1);
    assert(output[0].frame_offset() == 128);

    assert(
        grafite::event::MidiEvent::is_midi(output[0])
    );

    assert(
        grafite::event::MidiEvent::byte(output[0], 1)
        == 64
    );
}


void test_multiple_event_ports()
{
    grafite::event::EventBuffer input_a(4);
    grafite::event::EventBuffer input_b(4);
    grafite::event::EventBuffer output(4);

    const auto& const_input_a = input_a;
    const auto& const_input_b = input_b;

    std::array<grafite::event::ConstEventBufferView, 2> inputs{
        const_input_a.view(),
        const_input_b.view()
    };

    std::array<grafite::event::EventBufferView, 1> outputs{
        output.view()
    };

    const grafite::ProcessContext context{
        128,
        0,
        {},
        {},
        inputs,
        outputs
    };

    assert(context.event_input_count() == 2);
    assert(context.event_output_count() == 1);

    assert(
        context.event_input(
            grafite::EventInputIndex{0}
        ).empty()
    );

    assert(
        context.event_input(
            grafite::EventInputIndex{1}
        ).empty()
    );

    assert(
        context.event_output(
            grafite::EventOutputIndex{0}
        ).empty()
    );
}


void test_mixed_resources()
{
    grafite::AudioBuffer audio_input(2, 128);
    grafite::AudioBuffer audio_output(2, 128);

    grafite::event::EventBuffer event_input(8);
    grafite::event::EventBuffer event_output(8);

    audio_input(0, 32) = 0.25f;

    assert(
        event_input.push(
            grafite::event::MidiEvent::make(
                32,
                0x90,
                60,
                100
            )
        )
    );

    const auto& const_audio_input = audio_input;
    const auto& const_event_input = event_input;

    std::array<grafite::ConstAudioBufferView, 1> audio_inputs{
        const_audio_input.view()
    };

    std::array<grafite::AudioBufferView, 1> audio_outputs{
        audio_output.view()
    };

    std::array<grafite::event::ConstEventBufferView, 1> event_inputs{
        const_event_input.view()
    };

    std::array<grafite::event::EventBufferView, 1> event_outputs{
        event_output.view()
    };

    const grafite::ProcessContext context{
        128,
        16384,
        audio_inputs,
        audio_outputs,
        event_inputs,
        event_outputs
    };

    assert(context.frames() == 128);
    assert(context.frame_position() == 16384);

    assert(context.audio_input_count() == 1);
    assert(context.audio_output_count() == 1);
    assert(context.event_input_count() == 1);
    assert(context.event_output_count() == 1);

    const auto input = context.audio_input(
        grafite::AudioInputIndex{0}
    );

    auto output = context.audio_output(
        grafite::AudioOutputIndex{0}
    );

    const auto events = context.event_input(
        grafite::EventInputIndex{0}
    );

    auto event_output_view = context.event_output(
        grafite::EventOutputIndex{0}
    );

    assert(input(0, 32) == 0.25f);
    assert(events.size() == 1);
    assert(events[0].frame_offset() == 32);

    output(0, 32) = input(0, 32);

    assert(audio_output(0, 32) == 0.25f);

    assert(event_output_view.push(events[0]));

    assert(event_output.size() == 1);
    assert(event_output[0].frame_offset() == 32);
}

}  // namespace


int main()
{
    test_empty_context();
    test_block_information();

    test_audio_input();
    test_audio_output();
    test_multiple_audio_ports();

    test_event_input();
    test_event_output();
    test_multiple_event_ports();

    test_mixed_resources();

    return 0;
}