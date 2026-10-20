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

#ifndef GRAFITE_CORE_COMPILED_GRAPH_HPP
#define GRAFITE_CORE_COMPILED_GRAPH_HPP

#include <cassert>
#include <cmath>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "grafite/core/audio_buffer.hpp"
#include "grafite/core/connection.hpp"
#include "grafite/core/graph_io.hpp"
#include "grafite/core/node.hpp"
#include "grafite/core/process_context.hpp"
#include "grafite/core/types.hpp"
#include "grafite/event/event_buffer.hpp"

namespace grafite {

class Graph;

/// Represents a validated graph prepared for real-time execution.
///
/// CompiledGraph owns all nodes and runtime buffers required to execute a
/// declarative Graph. Nodes are stored in topological execution order and all
/// structural connections are resolved to preallocated runtime resources.
///
/// Compilation and preparation may allocate memory. process() performs no
/// dynamic allocation and requires no structural graph lookup.
class CompiledGraph {
public:
    CompiledGraph(const CompiledGraph&) = delete;
    CompiledGraph& operator=(const CompiledGraph&) = delete;

    CompiledGraph(CompiledGraph&&) noexcept = default;
    CompiledGraph& operator=(CompiledGraph&&) noexcept = default;

    /// Returns the processing specification used during compilation.
    [[nodiscard]]
    const ProcessSpec& spec() const noexcept
    {
        return spec_;
    }

    /// Returns the number of processing nodes.
    [[nodiscard]]
    std::size_t node_count() const noexcept
    {
        return nodes_.size();
    }

    /// Returns the node at the specified execution position.
    [[nodiscard]]
    Node& node(std::size_t index) noexcept
    {
        return *nodes_[index];
    }

    /// Returns the node at the specified execution position.
    [[nodiscard]]
    const Node& node(std::size_t index) const noexcept
    {
        return *nodes_[index];
    }

    /// Processes one block through all nodes in topological order.
    ///
    /// frames must not exceed the maximum block size supplied during
    /// compilation. A zero-frame block is ignored.
    ///
    /// This method performs no dynamic memory allocation.
    void process(
        FrameCount frames,
        FrameIndex frame_position
    ) noexcept
    {
        assert(frames <= spec_.max_block_size);

        if (frames == 0) {
            return;
        }

        for (auto& runtime_node : runtime_nodes_) {
            for (auto& output : runtime_node.audio_outputs) {
                output.clear();
            }

            for (auto& output : runtime_node.event_outputs) {
                output.clear();
            }

            for (std::size_t index = 0;
                 index < runtime_node.event_inputs.size();
                 ++index) {
                const auto& source =
                    *runtime_node.event_input_sources[index];

                runtime_node.event_inputs[index] =
                    source.view();
            }

            ProcessContext context(
                frames,
                frame_position,
                runtime_node.audio_inputs,
                runtime_node.audio_outputs,
                runtime_node.event_inputs,
                runtime_node.event_outputs
            );

            runtime_node.node->process(context);
        }
    }

    /// Resets the internal processing state of all nodes.
    ///
    /// This method does not alter graph structure, runtime bindings, or the
    /// processing specification. It is not assumed to be real-time safe.
    void reset() noexcept
    {
        for (auto& node : nodes_) {
            node->reset();
        }
    }

private:
    friend class Graph;

    struct AudioResource {
        PortAddress address;
        std::unique_ptr<AudioBuffer> buffer;
    };

    struct ExternalAudioInputResource {
        GraphAudioInputId id;
        std::unique_ptr<AudioBuffer> buffer;
    };

    struct EventResource {
        PortAddress address;
        std::unique_ptr<event::EventBuffer> buffer;
    };

    struct SilentAudioResource {
        ChannelCount channels = 0;
        std::unique_ptr<AudioBuffer> buffer;
    };

    struct RuntimeNode {
        Node* node = nullptr;

        std::vector<ConstAudioBufferView> audio_inputs;
        std::vector<AudioBufferView> audio_outputs;

        std::vector<event::EventBuffer*> event_input_sources;
        std::vector<event::ConstEventBufferView> event_inputs;
        std::vector<event::EventBufferView> event_outputs;
    };

    CompiledGraph(
        ProcessSpec spec,
        std::vector<std::unique_ptr<Node>> nodes,
        std::vector<Connection> connections,
        std::vector<GraphAudioPort> audio_ports,
        std::vector<GraphAudioInputConnection>
            audio_input_connections,
        std::vector<GraphAudioOutputConnection>
            audio_output_connections
    )
        : spec_(spec),
          nodes_(std::move(nodes)),
          empty_event_buffer_(
              std::make_unique<event::EventBuffer>()
          ),
          audio_ports_(std::move(audio_ports)),
          audio_input_connections_(
              std::move(audio_input_connections)
          ),
          audio_output_connections_(
              std::move(audio_output_connections)
          )
    {
        validate_spec();
        allocate_output_resources();
        allocate_external_audio_inputs();
        build_runtime_nodes(connections);

        for (auto& node : nodes_) {
            node->prepare(spec_);
        }
    }

    void validate_spec() const
    {
        if (!std::isfinite(spec_.sample_rate) ||
            spec_.sample_rate <= 0.0) {
            throw std::invalid_argument(
                "Process sample rate must be positive"
            );
        }

        if (spec_.max_block_size == 0) {
            throw std::invalid_argument(
                "Maximum block size must be greater than zero"
            );
        }
    }

    [[nodiscard]]
    static bool same_address(
        PortAddress first,
        PortAddress second
    ) noexcept
    {
        return
            first.node_id == second.node_id &&
            first.port_id == second.port_id;
    }

    [[nodiscard]]
    const Connection* find_connection(
        const std::vector<Connection>& connections,
        PortAddress destination
    ) const noexcept
    {
        for (const auto& connection : connections) {
            if (same_address(
                    connection.destination,
                    destination
                )) {
                return &connection;
            }
        }

        return nullptr;
    }

    [[nodiscard]]
    const GraphAudioInputConnection*
    find_external_audio_input_connection(
        PortAddress destination
    ) const noexcept
    {
        for (const auto& connection :
             audio_input_connections_) {
            if (same_address(
                    connection.destination,
                    destination
                )) {
                return &connection;
            }
        }

        return nullptr;
    }

    [[nodiscard]]
    AudioBuffer& audio_resource(
        PortAddress address
    )
    {
        for (auto& resource : audio_resources_) {
            if (same_address(resource.address, address)) {
                return *resource.buffer;
            }
        }

        throw std::invalid_argument(
            "Audio output resource was not found"
        );
    }

    [[nodiscard]]
    AudioBuffer& external_audio_input_resource(
        GraphAudioInputId id
    )
    {
        for (auto& resource : external_audio_inputs_) {
            if (resource.id.value == id.value) {
                return *resource.buffer;
            }
        }

        throw std::invalid_argument(
            "External audio input resource was not found"
        );
    }

    [[nodiscard]]
    event::EventBuffer& event_resource(
        PortAddress address
    )
    {
        for (auto& resource : event_resources_) {
            if (same_address(resource.address, address)) {
                return *resource.buffer;
            }
        }

        throw std::invalid_argument(
            "Event output resource was not found"
        );
    }

    [[nodiscard]]
    AudioBuffer& silent_audio_resource(
        ChannelCount channels
    )
    {
        for (auto& resource : silent_audio_resources_) {
            if (resource.channels == channels) {
                return *resource.buffer;
            }
        }

        SilentAudioResource resource;

        resource.channels = channels;
        resource.buffer = std::make_unique<AudioBuffer>(
            channels,
            spec_.max_block_size
        );

        silent_audio_resources_.push_back(
            std::move(resource)
        );

        return *silent_audio_resources_.back().buffer;
    }

    void allocate_output_resources()
    {
        for (const auto& node : nodes_) {
            for (const auto& port : node->ports()) {
                const PortAddress address{
                    node->id(),
                    port.id()
                };

                if (port.is_audio() &&
                    port.is_output()) {
                    AudioResource resource;

                    resource.address = address;
                    resource.buffer =
                        std::make_unique<AudioBuffer>(
                            port.channels(),
                            spec_.max_block_size
                        );

                    audio_resources_.push_back(
                        std::move(resource)
                    );
                }

                if (port.is_event() &&
                    port.is_output()) {
                    EventResource resource;

                    resource.address = address;
                    resource.buffer =
                        std::make_unique<event::EventBuffer>(
                            spec_.max_events_per_port
                        );

                    event_resources_.push_back(
                        std::move(resource)
                    );
                }
            }
        }
    }

    void allocate_external_audio_inputs()
    {
        for (const auto& port : audio_ports_) {
            if (port.direction != GraphPortDirection::input) {
                continue;
            }

            ExternalAudioInputResource resource;

            resource.id = GraphAudioInputId{port.id};
            resource.buffer = std::make_unique<AudioBuffer>(
                port.channels,
                spec_.max_block_size
            );

            external_audio_inputs_.push_back(
                std::move(resource)
            );
        }
    }

    void build_runtime_nodes(
        const std::vector<Connection>& connections
    )
    {
        runtime_nodes_.reserve(nodes_.size());

        for (const auto& node : nodes_) {
            RuntimeNode runtime_node;

            runtime_node.node = node.get();

            for (const auto& port : node->ports()) {
                const PortAddress address{
                    node->id(),
                    port.id()
                };

                if (port.is_audio() &&
                    port.is_input()) {
                    const Connection* connection =
                        find_connection(
                            connections,
                            address
                        );

                    if (connection != nullptr) {
                        const auto& source =
                            audio_resource(
                                connection->source
                            );

                        runtime_node.audio_inputs.push_back(
                            source.view()
                        );
                    } else {
                        const auto* external_connection =
                            find_external_audio_input_connection(
                                address
                            );

                        if (external_connection != nullptr) {
                            const auto& source =
                                external_audio_input_resource(
                                    external_connection->source
                                );

                            runtime_node.audio_inputs.push_back(
                                source.view()
                            );
                        } else {
                            const auto& silent =
                                silent_audio_resource(
                                    port.channels()
                                );

                            runtime_node.audio_inputs.push_back(
                                silent.view()
                            );
                        }
                    }
                }

                if (port.is_audio() &&
                    port.is_output()) {
                    auto& output =
                        audio_resource(address);

                    runtime_node.audio_outputs.push_back(
                        output.view()
                    );
                }

                if (port.is_event() &&
                    port.is_input()) {
                    const Connection* connection =
                        find_connection(
                            connections,
                            address
                        );

                    event::EventBuffer* source = nullptr;

                    if (connection == nullptr) {
                        source =
                            empty_event_buffer_.get();
                    } else {
                        source =
                            &event_resource(
                                connection->source
                            );
                    }

                    runtime_node.event_input_sources.push_back(
                        source
                    );

                    const auto& const_source = *source;

                    runtime_node.event_inputs.push_back(
                        const_source.view()
                    );
                }

                if (port.is_event() &&
                    port.is_output()) {
                    auto& output =
                        event_resource(address);

                    runtime_node.event_outputs.push_back(
                        output.view()
                    );
                }
            }

            runtime_nodes_.push_back(
                std::move(runtime_node)
            );
        }
    }

    ProcessSpec spec_;

    std::vector<std::unique_ptr<Node>> nodes_;

    std::vector<AudioResource> audio_resources_;
    std::vector<EventResource> event_resources_;

    std::vector<ExternalAudioInputResource>
        external_audio_inputs_;

    std::vector<SilentAudioResource>
        silent_audio_resources_;

    std::unique_ptr<event::EventBuffer>
        empty_event_buffer_;

    std::vector<RuntimeNode> runtime_nodes_;

    std::vector<GraphAudioPort> audio_ports_;

    std::vector<GraphAudioInputConnection>
        audio_input_connections_;

    std::vector<GraphAudioOutputConnection>
        audio_output_connections_;
};

}  // namespace grafite

#endif  // GRAFITE_CORE_COMPILED_GRAPH_HPP