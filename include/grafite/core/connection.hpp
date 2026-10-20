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

#ifndef GRAFITE_CORE_CONNECTION_HPP
#define GRAFITE_CORE_CONNECTION_HPP

#include "grafite/core/types.hpp"

namespace grafite {

/// Identifies a port within a graph.
///
/// PortId values are local to a Node, so a complete structural port address
/// requires both the owning NodeId and the local PortId.
struct PortAddress {
    NodeId node_id = invalid_node_id;
    PortId port_id = invalid_port_id;
};

/// Describes a directed structural connection between two node ports.
///
/// Connections contain structural identifiers only. Runtime buffer bindings
/// are resolved when the graph is compiled.
struct Connection {
    PortAddress source;
    PortAddress destination;
};

}  // namespace grafite

#endif  // GRAFITE_CORE_CONNECTION_HPP