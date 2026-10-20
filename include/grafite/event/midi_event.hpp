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

#ifndef GRAFITE_EVENT_MIDI_EVENT_HPP
#define GRAFITE_EVENT_MIDI_EVENT_HPP

#include <cstddef>
#include <cstdint>

#include "grafite/event/event.hpp"

namespace grafite::event {

/// Utility for creating and inspecting short MIDI 1.0 real-time events.
///
/// Short MIDI messages contain at most three bytes. Variable-length messages
/// such as System Exclusive are intentionally outside the real-time event
/// representation provided here.
class MidiEvent {
public:
    static constexpr std::size_t max_size = 3;

    [[nodiscard]]
    static constexpr Event make(
        FrameCount frame_offset,
        std::uint8_t byte0
    ) noexcept
    {
        return make_impl(frame_offset, byte0, 0, 0, 1);
    }

    [[nodiscard]]
    static constexpr Event make(
        FrameCount frame_offset,
        std::uint8_t byte0,
        std::uint8_t byte1
    ) noexcept
    {
        return make_impl(frame_offset, byte0, byte1, 0, 2);
    }

    [[nodiscard]]
    static constexpr Event make(
        FrameCount frame_offset,
        std::uint8_t byte0,
        std::uint8_t byte1,
        std::uint8_t byte2
    ) noexcept
    {
        return make_impl(frame_offset, byte0, byte1, byte2, 3);
    }

    [[nodiscard]]
    static constexpr bool is_midi(const Event& event) noexcept
    {
        return event.type_ == EventType::midi;
    }

    [[nodiscard]]
    static constexpr std::size_t size(const Event& event) noexcept
    {
        return is_midi(event) ? event.payload_size_ : 0;
    }

    [[nodiscard]]
    static constexpr std::uint8_t byte(
        const Event& event,
        std::size_t index
    ) noexcept
    {
        return std::to_integer<std::uint8_t>(event.payload_[index]);
    }

private:
    [[nodiscard]]
    static constexpr Event make_impl(
        FrameCount frame_offset,
        std::uint8_t byte0,
        std::uint8_t byte1,
        std::uint8_t byte2,
        std::size_t size
    ) noexcept
    {
        Event event;

        event.frame_offset_ = frame_offset;
        event.type_ = EventType::midi;
        event.payload_size_ = size;

        event.payload_[0] = static_cast<std::byte>(byte0);
        event.payload_[1] = static_cast<std::byte>(byte1);
        event.payload_[2] = static_cast<std::byte>(byte2);

        return event;
    }
};

}  // namespace grafite::event

#endif  // GRAFITE_EVENT_MIDI_EVENT_HPP