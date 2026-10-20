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

#ifndef GRAFITE_EVENT_EVENT_HPP
#define GRAFITE_EVENT_EVENT_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "grafite/core/types.hpp"

namespace grafite::event {

/// Identifies the semantic type of a real-time event.
enum class EventType : std::uint8_t {
    invalid,
    midi
};

/// Small, fixed-size event suitable for sample-accurate real-time processing.
///
/// Event payloads are stored inline to avoid dynamic allocation in the
/// real-time processing path. Large or variable-size messages belong to the
/// control plane rather than to real-time event streams.
class Event {
public:
    static constexpr std::size_t max_payload_size = 16;

    constexpr Event() noexcept = default;

    [[nodiscard]]
    constexpr FrameCount frame_offset() const noexcept
    {
        return frame_offset_;
    }

    [[nodiscard]]
    constexpr EventType type() const noexcept
    {
        return type_;
    }

    [[nodiscard]]
    constexpr std::size_t payload_size() const noexcept
    {
        return payload_size_;
    }

    [[nodiscard]]
    constexpr const std::byte* payload() const noexcept
    {
        return payload_.data();
    }

private:
    friend class MidiEvent;

    FrameCount frame_offset_ = 0;
    EventType type_ = EventType::invalid;
    std::array<std::byte, max_payload_size> payload_{};
    std::size_t payload_size_ = 0;
};

}  // namespace grafite::event

#endif  // GRAFITE_EVENT_EVENT_HPP