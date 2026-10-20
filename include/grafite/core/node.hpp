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

#ifndef GRAFITE_CORE_NODE_HPP
#define GRAFITE_CORE_NODE_HPP

#include <vector>

#include "grafite/core/port.hpp"
#include "grafite/core/process_context.hpp"
#include "grafite/core/types.hpp"

namespace grafite {

class Graph;

/// Base class for processing nodes.
///
/// A Node declares its structural input and output ports and implements DSP
/// processing for one block at a time. Nodes do not own graph connections,
/// routing state, processing buffers, or backend resources.
///
/// Structural ports use PortId values, while real-time processing accesses
/// resolved resources through strongly typed processing indices. These two
/// forms of identification are intentionally independent.
///
/// Ports must be declared during node construction and must remain unchanged
/// once the node is part of a compiled graph.
///
/// prepare() configures processing resources and may be called again whenever
/// the processing specification changes. It is executed outside the real-time
/// processing path and may allocate memory.
///
/// process() executes one processing block and must be real-time safe.
///
/// reset() restores internal DSP state without changing node configuration.
/// It is not assumed to be real-time safe.
///
/// Nodes must not retain ProcessContext views or references after process()
/// returns.
class Node {
public:
    Node() = default;

    virtual ~Node() = default;

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    Node(Node&&) = delete;
    Node& operator=(Node&&) = delete;

    /// Returns the structural identifier assigned by the Graph.
    [[nodiscard]]
    NodeId id() const noexcept
    {
        return id_;
    }

    /// Returns all ports declared by this node.
    [[nodiscard]]
    const std::vector<Port>& ports() const noexcept
    {
        return ports_;
    }

    /// Returns the structural PortId associated with an audio input index.
    ///
    /// The index must refer to an existing audio input port.
    [[nodiscard]]
    PortId port_id(AudioInputIndex index) const noexcept
    {
        return audio_input_ports_[index.value];
    }

    /// Returns the structural PortId associated with an audio output index.
    ///
    /// The index must refer to an existing audio output port.
    [[nodiscard]]
    PortId port_id(AudioOutputIndex index) const noexcept
    {
        return audio_output_ports_[index.value];
    }

    /// Returns the structural PortId associated with an event input index.
    ///
    /// The index must refer to an existing event input port.
    [[nodiscard]]
    PortId port_id(EventInputIndex index) const noexcept
    {
        return event_input_ports_[index.value];
    }

    /// Returns the structural PortId associated with an event output index.
    ///
    /// The index must refer to an existing event output port.
    [[nodiscard]]
    PortId port_id(EventOutputIndex index) const noexcept
    {
        return event_output_ports_[index.value];
    }

    /// Returns whether an audio input index refers to an existing port.
    [[nodiscard]]
    bool has_port(AudioInputIndex index) const noexcept
    {
        return index.value < audio_input_ports_.size();
    }

    /// Returns whether an audio output index refers to an existing port.
    [[nodiscard]]
    bool has_port(AudioOutputIndex index) const noexcept
    {
        return index.value < audio_output_ports_.size();
    }

    /// Returns whether an event input index refers to an existing port.
    [[nodiscard]]
    bool has_port(EventInputIndex index) const noexcept
    {
        return index.value < event_input_ports_.size();
    }

    /// Returns whether an event output index refers to an existing port.
    [[nodiscard]]
    bool has_port(EventOutputIndex index) const noexcept
    {
        return index.value < event_output_ports_.size();
    }

    /// Prepares the node for processing.
    ///
    /// This method is called outside the real-time processing path and may
    /// allocate resources required by process(). It may be called again when
    /// the processing specification changes.
    virtual void prepare(const ProcessSpec& spec)
    {
        (void)spec;
    }

    /// Processes one block of audio and event data.
    ///
    /// Implementations must be real-time safe and must not retain references
    /// or views obtained from the context after this method returns.
    virtual void process(
        const ProcessContext& context
    ) noexcept = 0;

    /// Resets internal processing state without changing node configuration.
    ///
    /// The default implementation does nothing. This method is not assumed
    /// to be real-time safe.
    virtual void reset() noexcept
    {
    }

protected:
    /// Declares an audio input bus and returns its processing index.
    [[nodiscard]]
    AudioInputIndex add_audio_input(ChannelCount channels)
    {
        const AudioInputIndex index{
            audio_input_ports_.size()
        };

        const PortId port_id = next_port_id_++;

        ports_.push_back(
            Port::audio(
                port_id,
                PortDirection::input,
                channels
            )
        );

        audio_input_ports_.push_back(port_id);

        return index;
    }

    /// Declares an audio output bus and returns its processing index.
    [[nodiscard]]
    AudioOutputIndex add_audio_output(ChannelCount channels)
    {
        const AudioOutputIndex index{
            audio_output_ports_.size()
        };

        const PortId port_id = next_port_id_++;

        ports_.push_back(
            Port::audio(
                port_id,
                PortDirection::output,
                channels
            )
        );

        audio_output_ports_.push_back(port_id);

        return index;
    }

    /// Declares an event input and returns its processing index.
    [[nodiscard]]
    EventInputIndex add_event_input()
    {
        const EventInputIndex index{
            event_input_ports_.size()
        };

        const PortId port_id = next_port_id_++;

        ports_.push_back(
            Port::event(
                port_id,
                PortDirection::input
            )
        );

        event_input_ports_.push_back(port_id);

        return index;
    }

    /// Declares an event output and returns its processing index.
    [[nodiscard]]
    EventOutputIndex add_event_output()
    {
        const EventOutputIndex index{
            event_output_ports_.size()
        };

        const PortId port_id = next_port_id_++;

        ports_.push_back(
            Port::event(
                port_id,
                PortDirection::output
            )
        );

        event_output_ports_.push_back(port_id);

        return index;
    }

private:
    friend class Graph;

    /// Assigns the structural identifier managed by the Graph.
    void set_id(NodeId id) noexcept
    {
        id_ = id;
    }

    NodeId id_ = invalid_node_id;

    std::vector<Port> ports_;

    std::vector<PortId> audio_input_ports_;
    std::vector<PortId> audio_output_ports_;
    std::vector<PortId> event_input_ports_;
    std::vector<PortId> event_output_ports_;

    PortId next_port_id_ = 1;
};

}  // namespace grafite

#endif  // GRAFITE_CORE_NODE_HPP