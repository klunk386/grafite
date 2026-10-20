
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

#ifndef GRAFITE_CORE_GRAPH_IO_HPP
#define GRAFITE_CORE_GRAPH_IO_HPP

#include <cstdint>
#include <string>

#include "grafite/core/connection.hpp"
#include "grafite/core/types.hpp"

namespace grafite {

/// Structural identifier of an external graph port.
using GraphPortId = std::uint64_t;

inline constexpr GraphPortId invalid_graph_port_id = 0;

/// Typed identifiers for external audio input and output buses.
///
/// These identifiers are intentionally distinct to prevent connections
/// between incompatible graph boundaries.
struct GraphAudioInputId {
    GraphPortId value = invalid_graph_port_id;
};

struct GraphAudioOutputId {
    GraphPortId value = invalid_graph_port_id;
};

/// Direction of an external graph port.
enum class GraphPortDirection {
    input,
    output
};

/// Declarative description of an external audio bus.
struct GraphAudioPort {
    GraphPortId id = invalid_graph_port_id;
    std::string name;
    GraphPortDirection direction = GraphPortDirection::input;
    ChannelCount channels = 0;
};

/// Connects an external audio input to a node audio input.
struct GraphAudioInputConnection {
    GraphAudioInputId source;
    PortAddress destination;
};

/// Connects a node audio output to an external audio output.
struct GraphAudioOutputConnection {
    PortAddress source;
    GraphAudioOutputId destination;
};

}  // namespace grafite

#endif  // GRAFITE_CORE_GRAPH_IO_HPP
