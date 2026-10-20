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

#ifndef GRAFITE_CORE_GRAPH_HPP
#define GRAFITE_CORE_GRAPH_HPP

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "grafite/core/compiled_graph.hpp"
#include "grafite/core/connection.hpp"
#include "grafite/core/graph_io.hpp"
#include "grafite/core/node.hpp"
#include "grafite/core/port.hpp"
#include "grafite/core/process_context.hpp"
#include "grafite/core/types.hpp"

namespace grafite {

/// Owns processing nodes and describes their structural connections.
///
/// Graph represents the declarative topology of a processing network. It does
/// not execute DSP directly and does not own runtime processing buffers.
///
/// Nodes are owned by the Graph and receive stable structural NodeId values.
/// Connections are expressed internally using NodeId and PortId pairs, while
/// the public typed connect() interface uses the strongly typed processing
/// indices exposed by Node subclasses.
///
/// Graph construction and connection validation occur outside the real-time
/// processing path.
///
/// Compilation consumes the Graph and produces a CompiledGraph containing the
/// nodes in topological execution order.
class Graph {
public:
    Graph() = default;

    Graph(const Graph&) = delete;
    Graph& operator=(const Graph&) = delete;

    Graph(Graph&&) = delete;
    Graph& operator=(Graph&&) = delete;

    /// Constructs a node, transfers its ownership to the graph, and returns
    /// a reference to the stored object.
    template <typename NodeType, typename... Args>
    NodeType& add_node(Args&&... args)
    {
        static_assert(
            std::is_base_of_v<Node, NodeType>,
            "NodeType must derive from grafite::Node"
        );

        auto node = std::make_unique<NodeType>(
            std::forward<Args>(args)...
        );

        node->set_id(next_node_id_);

        NodeType& reference = *node;

        nodes_.push_back(std::move(node));

        ++next_node_id_;

        return reference;
    }

    /// Returns a node by structural identifier, or nullptr if not found.
    [[nodiscard]]
    Node* node(NodeId id) noexcept
    {
        for (auto& candidate : nodes_) {
            if (candidate->id() == id) {
                return candidate.get();
            }
        }

        return nullptr;
    }

    /// Returns a node by structural identifier, or nullptr if not found.
    [[nodiscard]]
    const Node* node(NodeId id) const noexcept
    {
        for (const auto& candidate : nodes_) {
            if (candidate->id() == id) {
                return candidate.get();
            }
        }

        return nullptr;
    }

    /// Returns all nodes owned by the graph.
    [[nodiscard]]
    const std::vector<std::unique_ptr<Node>>& nodes() const noexcept
    {
        return nodes_;
    }

    /// Declares an external audio input bus.
    GraphAudioInputId add_audio_input(
        std::string name,
        ChannelCount channels
    )
    {
        return GraphAudioInputId{
            add_audio_port(
                std::move(name),
                GraphPortDirection::input,
                channels
            )
        };
    }

    /// Declares an external audio output bus.
    GraphAudioOutputId add_audio_output(
        std::string name,
        ChannelCount channels
    )
    {
        return GraphAudioOutputId{
            add_audio_port(
                std::move(name),
                GraphPortDirection::output,
                channels
            )
        };
    }

    /// Returns the external audio port declarations.
    [[nodiscard]]
    const std::vector<GraphAudioPort>& audio_ports() const noexcept
    {
        return audio_ports_;
    }

    /// Returns all structural connections in the graph.
    [[nodiscard]]
    const std::vector<Connection>& connections() const noexcept
    {
        return connections_;
    }

    /// Connects an external audio input to a node audio input.
    void connect(
        GraphAudioInputId source,
        Node& destination,
        AudioInputIndex destination_index
    )
    {
        const GraphAudioPort& source_port =
            require_audio_port(
                source.value,
                GraphPortDirection::input
            );

        validate_node_membership(destination);

        if (!destination.has_port(destination_index)) {
            throw std::out_of_range(
                "Audio input index is out of range"
            );
        }

        const PortId destination_port_id =
            destination.port_id(destination_index);

        const Port& destination_port =
            require_port(destination, destination_port_id);

        if (!destination_port.is_audio() ||
            !destination_port.is_input()) {
            throw std::invalid_argument(
                "Destination must be an audio input port"
            );
        }

        if (source_port.channels != destination_port.channels()) {
            throw std::invalid_argument(
                "Audio port channel counts do not match"
            );
        }

        const PortAddress address{
            destination.id(),
            destination_port_id
        };

        validate_audio_input_available(address);

        audio_input_connections_.push_back(
            GraphAudioInputConnection{source, address}
        );
    }

    /// Connects a node audio output to an external audio output.
    void connect(
        Node& source,
        AudioOutputIndex source_index,
        GraphAudioOutputId destination
    )
    {
        const GraphAudioPort& destination_port =
            require_audio_port(
                destination.value,
                GraphPortDirection::output
            );

        validate_node_membership(source);

        if (!source.has_port(source_index)) {
            throw std::out_of_range(
                "Audio output index is out of range"
            );
        }

        const PortId source_port_id =
            source.port_id(source_index);

        const Port& source_port =
            require_port(source, source_port_id);

        if (!source_port.is_audio() ||
            !source_port.is_output()) {
            throw std::invalid_argument(
                "Source must be an audio output port"
            );
        }

        if (source_port.channels() != destination_port.channels) {
            throw std::invalid_argument(
                "Audio port channel counts do not match"
            );
        }

        for (const auto& connection : audio_output_connections_) {
            if (connection.destination.value == destination.value) {
                throw std::invalid_argument(
                    "Graph audio output is already connected"
                );
            }
        }

        audio_output_connections_.push_back(
            GraphAudioOutputConnection{
                PortAddress{source.id(), source_port_id},
                destination
            }
        );
    }

    /// Returns external audio input connections.
    [[nodiscard]]
    const std::vector<GraphAudioInputConnection>&
    audio_input_connections() const noexcept
    {
        return audio_input_connections_;
    }

    /// Returns external audio output connections.
    [[nodiscard]]
    const std::vector<GraphAudioOutputConnection>&
    audio_output_connections() const noexcept
    {
        return audio_output_connections_;
    }

    /// Connects an audio output to an audio input.
    void connect(
        Node& source,
        AudioOutputIndex source_index,
        Node& destination,
        AudioInputIndex destination_index
    )
    {
        validate_node_membership(source);
        validate_node_membership(destination);

        if (&source == &destination) {
            throw std::invalid_argument(
                "Self-connections are not supported"
            );
        }

        if (!source.has_port(source_index)) {
            throw std::out_of_range(
                "Audio output index is out of range"
            );
        }

        if (!destination.has_port(destination_index)) {
            throw std::out_of_range(
                "Audio input index is out of range"
            );
        }

        const PortId source_port_id =
            source.port_id(source_index);

        const PortId destination_port_id =
            destination.port_id(destination_index);

        const Port& source_port =
            require_port(source, source_port_id);

        const Port& destination_port =
            require_port(destination, destination_port_id);

        if (!source_port.is_audio() ||
            !source_port.is_output()) {
            throw std::invalid_argument(
                "Source must be an audio output port"
            );
        }

        if (!destination_port.is_audio() ||
            !destination_port.is_input()) {
            throw std::invalid_argument(
                "Destination must be an audio input port"
            );
        }

        if (source_port.channels() !=
            destination_port.channels()) {
            throw std::invalid_argument(
                "Audio port channel counts do not match"
            );
        }

        add_connection(
            PortAddress{
                source.id(),
                source_port_id
            },
            PortAddress{
                destination.id(),
                destination_port_id
            }
        );
    }

    /// Connects an event output to an event input.
    void connect(
        Node& source,
        EventOutputIndex source_index,
        Node& destination,
        EventInputIndex destination_index
    )
    {
        validate_node_membership(source);
        validate_node_membership(destination);

        if (&source == &destination) {
            throw std::invalid_argument(
                "Self-connections are not supported"
            );
        }

        if (!source.has_port(source_index)) {
            throw std::out_of_range(
                "Event output index is out of range"
            );
        }

        if (!destination.has_port(destination_index)) {
            throw std::out_of_range(
                "Event input index is out of range"
            );
        }

        const PortId source_port_id =
            source.port_id(source_index);

        const PortId destination_port_id =
            destination.port_id(destination_index);

        const Port& source_port =
            require_port(source, source_port_id);

        const Port& destination_port =
            require_port(destination, destination_port_id);

        if (!source_port.is_event() ||
            !source_port.is_output()) {
            throw std::invalid_argument(
                "Source must be an event output port"
            );
        }

        if (!destination_port.is_event() ||
            !destination_port.is_input()) {
            throw std::invalid_argument(
                "Destination must be an event input port"
            );
        }

        add_connection(
            PortAddress{
                source.id(),
                source_port_id
            },
            PortAddress{
                destination.id(),
                destination_port_id
            }
        );
    }

    /// Compiles and consumes the graph.
    ///
    /// The graph must be passed as an rvalue. Compilation validates that the
    /// node dependency graph is acyclic and transfers node ownership into the
    /// resulting CompiledGraph in topological execution order.
    ///
    /// Runtime buffer allocation and binding resolution are performed by
    /// later compilation stages.
    [[nodiscard]]
    CompiledGraph compile(
        const ProcessSpec& spec
    ) &&
    {
        if (!audio_ports_.empty()) {
            throw std::logic_error(
                "External graph audio ports are not yet compiled"
            );
        }

        const std::size_t count = nodes_.size();

        std::vector<std::size_t> indegree(count, 0);
        std::vector<std::vector<std::size_t>> adjacency(count);

        for (const auto& connection : connections_) {
            const std::size_t source_index =
                node_storage_index(connection.source.node_id);

            const std::size_t destination_index =
                node_storage_index(
                    connection.destination.node_id
                );

            adjacency[source_index].push_back(
                destination_index
            );

            ++indegree[destination_index];
        }

        std::vector<std::size_t> ready;
        ready.reserve(count);

        for (std::size_t index = 0; index < count; ++index) {
            if (indegree[index] == 0) {
                ready.push_back(index);
            }
        }

        std::vector<std::size_t> order;
        order.reserve(count);

        std::size_t ready_position = 0;

        while (ready_position < ready.size()) {
            const std::size_t current =
                ready[ready_position++];

            order.push_back(current);

            for (const std::size_t destination :
                 adjacency[current]) {
                --indegree[destination];

                if (indegree[destination] == 0) {
                    ready.push_back(destination);
                }
            }
        }

        if (order.size() != count) {
            throw std::invalid_argument(
                "Graph contains a processing cycle"
            );
        }

        std::vector<std::unique_ptr<Node>> ordered_nodes;
        ordered_nodes.reserve(count);

        for (const std::size_t index : order) {
            ordered_nodes.push_back(
                std::move(nodes_[index])
            );
        }

        nodes_.clear();

        return CompiledGraph{
            spec,
            std::move(ordered_nodes),
            std::move(connections_),
            std::move(audio_ports_),
            std::move(audio_input_connections_),
            std::move(audio_output_connections_)
        };
    }

private:
    /// Resolves and validates an external audio port.
    [[nodiscard]]
    const GraphAudioPort& require_audio_port(
        GraphPortId id,
        GraphPortDirection direction
    ) const
    {
        for (const auto& port : audio_ports_) {
            if (port.id == id && port.direction == direction) {
                return port;
            }
        }

        throw std::invalid_argument(
            "Unknown or incompatible graph audio port"
        );
    }

    /// Ensures that a node audio input has no existing connection.
    void validate_audio_input_available(
        PortAddress destination
    ) const
    {
        for (const auto& connection : connections_) {
            if (connection.destination.node_id ==
                    destination.node_id &&
                connection.destination.port_id ==
                    destination.port_id) {
                throw std::invalid_argument(
                    "Destination input is already connected"
                );
            }
        }

        for (const auto& connection : audio_input_connections_) {
            if (connection.destination.node_id ==
                    destination.node_id &&
                connection.destination.port_id ==
                    destination.port_id) {
                throw std::invalid_argument(
                    "Destination input is already connected"
                );
            }
        }
    }

    /// Returns the requested structural port or throws if it does not exist.
    [[nodiscard]]
    static const Port& require_port(
        const Node& node,
        PortId port_id
    )
    {
        for (const auto& port : node.ports()) {
            if (port.id() == port_id) {
                return port;
            }
        }

        throw std::invalid_argument(
            "Port does not belong to the specified node"
        );
    }

    /// Verifies that a node is owned by this graph.
    void validate_node_membership(
        const Node& node
    ) const
    {
        for (const auto& candidate : nodes_) {
            if (candidate.get() == &node) {
                return;
            }
        }

        throw std::invalid_argument(
            "Node does not belong to this graph"
        );
    }

    /// Returns the storage position of a node with the requested identifier.
    [[nodiscard]]
    std::size_t node_storage_index(
        NodeId id
    ) const
    {
        for (std::size_t index = 0;
             index < nodes_.size();
             ++index) {
            if (nodes_[index]->id() == id) {
                return index;
            }
        }

        throw std::invalid_argument(
            "Connection references an unknown node"
        );
    }

    /// Adds a validated structural connection.
    void add_connection(
        PortAddress source,
        PortAddress destination
    )
    {
        validate_audio_input_available(destination);

        connections_.push_back(
            Connection{
                source,
                destination
            }
        );
    }

    /// Registers and validates an external audio port.
    GraphPortId add_audio_port(
        std::string name,
        GraphPortDirection direction,
        ChannelCount channels
    )
    {
        if (name.empty()) {
            throw std::invalid_argument(
                "Graph audio port name must not be empty"
            );
        }

        if (channels == 0) {
            throw std::invalid_argument(
                "Graph audio port must have at least one channel"
            );
        }

        for (const auto& port : audio_ports_) {
            if (port.name == name &&
                port.direction == direction) {
                throw std::invalid_argument(
                    "Duplicate graph audio port name"
                );
            }
        }

        const GraphPortId id = next_graph_port_id_;

        audio_ports_.push_back(
            GraphAudioPort{
                id,
                std::move(name),
                direction,
                channels
            }
        );

        ++next_graph_port_id_;

        return id;
    }

    NodeId next_node_id_ = 1;

    std::vector<std::unique_ptr<Node>> nodes_;
    std::vector<Connection> connections_;

    GraphPortId next_graph_port_id_ = 1;
    std::vector<GraphAudioPort> audio_ports_;
    std::vector<GraphAudioInputConnection> audio_input_connections_;
    std::vector<GraphAudioOutputConnection> audio_output_connections_;
};

}  // namespace grafite

#endif  // GRAFITE_CORE_GRAPH_HPP