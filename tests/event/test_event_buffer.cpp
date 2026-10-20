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

#include "grafite/event/event_buffer.hpp"
#include "grafite/event/midi_event.hpp"

namespace {

void test_midi_event()
{
    constexpr auto event = grafite::event::MidiEvent::make(
        42,
        0x90,
        60,
        100
    );

    static_assert(event.frame_offset() == 42);
    static_assert(event.type() == grafite::event::EventType::midi);
    static_assert(event.payload_size() == 3);

    static_assert(grafite::event::MidiEvent::is_midi(event));
    static_assert(grafite::event::MidiEvent::size(event) == 3);

    static_assert(grafite::event::MidiEvent::byte(event, 0) == 0x90);
    static_assert(grafite::event::MidiEvent::byte(event, 1) == 60);
    static_assert(grafite::event::MidiEvent::byte(event, 2) == 100);
}

void test_empty_buffer()
{
    grafite::event::EventBuffer buffer(8);

    assert(buffer.empty());
    assert(buffer.size() == 0);
    assert(buffer.capacity() == 8);
}

void test_push()
{
    grafite::event::EventBuffer buffer(4);

    const auto event = grafite::event::MidiEvent::make(
        10,
        0x90,
        60,
        100
    );

    assert(buffer.push(event));
    assert(buffer.size() == 1);
    assert(buffer[0].frame_offset() == 10);
}

void test_capacity()
{
    grafite::event::EventBuffer buffer(2);

    assert(buffer.push(
        grafite::event::MidiEvent::make(0, 0x90, 60, 100)
    ));

    assert(buffer.push(
        grafite::event::MidiEvent::make(10, 0x80, 60, 0)
    ));

    assert(!buffer.push(
        grafite::event::MidiEvent::make(20, 0x90, 64, 100)
    ));

    assert(buffer.size() == 2);
}

void test_clear_preserves_capacity()
{
    grafite::event::EventBuffer buffer(8);

    assert(buffer.push(
        grafite::event::MidiEvent::make(0, 0x90, 60, 100)
    ));

    const auto capacity = buffer.capacity();

    buffer.clear();

    assert(buffer.empty());
    assert(buffer.capacity() == capacity);
}

void test_mutable_view()
{
    grafite::event::EventBuffer buffer(4);
    auto view = buffer.view();

    assert(view.push(
        grafite::event::MidiEvent::make(10, 0x90, 60, 100)
    ));

    assert(buffer.size() == 1);
    assert(view.size() == 1);
    assert(buffer[0].frame_offset() == 10);
}

void test_const_view()
{
    grafite::event::EventBuffer buffer(4);

    assert(buffer.push(
        grafite::event::MidiEvent::make(10, 0x90, 60, 100)
    ));

    const auto& const_buffer = buffer;
    const auto view = const_buffer.view();

    assert(view.size() == 1);
    assert(view[0].frame_offset() == 10);
}

void test_iteration()
{
    grafite::event::EventBuffer buffer(4);

    assert(buffer.push(
        grafite::event::MidiEvent::make(10, 0x90, 60, 100)
    ));

    assert(buffer.push(
        grafite::event::MidiEvent::make(20, 0x80, 60, 0)
    ));

    grafite::FrameCount expected_offset = 10;

    for (const auto& event : buffer.view()) {
        assert(event.frame_offset() == expected_offset);
        expected_offset += 10;
    }
}

void test_resize()
{
    grafite::event::EventBuffer buffer(2);

    assert(buffer.push(
        grafite::event::MidiEvent::make(10, 0x90, 60, 100)
    ));

    buffer.resize(8);

    assert(buffer.empty());
    assert(buffer.size() == 0);
    assert(buffer.capacity() == 8);
}

}  // namespace

int main()
{
    test_midi_event();
    test_empty_buffer();
    test_push();
    test_capacity();
    test_clear_preserves_capacity();
    test_mutable_view();
    test_const_view();
    test_iteration();
    test_resize();

    return 0;
}