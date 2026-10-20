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

#ifndef GRAFITE_CORE_PORT_HPP
#define GRAFITE_CORE_PORT_HPP

#include "grafite/core/types.hpp"

namespace grafite {

/// Direction of a node port.
enum class PortDirection {
    input,
    output
};

/// Type of data carried by a node port.
enum class PortType {
    audio,
    event
};

/// Describes an input or output endpoint of a processing node.
///
/// Port contains structural information only. It does not own processing
/// buffers, store graph connections, or perform data transfer.
///
/// Audio ports represent buses containing one or more channels. Event ports
/// do not carry audio channels and therefore report zero channels.
class Port {
public:
    constexpr Port() noexcept = default;

    /// Creates an audio port representing an N-channel bus.
    [[nodiscard]]
    static constexpr Port audio(
        PortId id,
        PortDirection direction,
        ChannelCount channels
    ) noexcept
    {
        return Port{
            id,
            direction,
            PortType::audio,
            channels
        };
    }

    /// Creates an event port.
    [[nodiscard]]
    static constexpr Port event(
        PortId id,
        PortDirection direction
    ) noexcept
    {
        return Port{
            id,
            direction,
            PortType::event,
            0
        };
    }

    [[nodiscard]]
    constexpr PortId id() const noexcept
    {
        return id_;
    }

    [[nodiscard]]
    constexpr PortDirection direction() const noexcept
    {
        return direction_;
    }

    [[nodiscard]]
    constexpr PortType type() const noexcept
    {
        return type_;
    }

    /// Returns the number of channels carried by an audio port.
    ///
    /// Event ports return zero.
    [[nodiscard]]
    constexpr ChannelCount channels() const noexcept
    {
        return channels_;
    }

    [[nodiscard]]
    constexpr bool is_input() const noexcept
    {
        return direction_ == PortDirection::input;
    }

    [[nodiscard]]
    constexpr bool is_output() const noexcept
    {
        return direction_ == PortDirection::output;
    }

    [[nodiscard]]
    constexpr bool is_audio() const noexcept
    {
        return type_ == PortType::audio;
    }

    [[nodiscard]]
    constexpr bool is_event() const noexcept
    {
        return type_ == PortType::event;
    }

private:
    constexpr Port(
        PortId id,
        PortDirection direction,
        PortType type,
        ChannelCount channels
    ) noexcept
        : id_(id),
          direction_(direction),
          type_(type),
          channels_(channels)
    {
    }

    PortId id_ = invalid_port_id;
    PortDirection direction_ = PortDirection::input;
    PortType type_ = PortType::audio;
    ChannelCount channels_ = 0;
};

}  // namespace grafite

#endif  // GRAFITE_CORE_PORT_HPP