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

#ifndef GRAFITE_CORE_TYPES_HPP
#define GRAFITE_CORE_TYPES_HPP

#include <cstddef>
#include <cstdint>

namespace grafite {

/// Sample type used internally by the DSP engine.
using Sample = float;

/// Number of audio frames in a block or buffer.
using FrameCount = std::size_t;

/// Number of audio channels.
using ChannelCount = std::size_t;

/// Absolute frame position on the processing timeline.
using FrameIndex = std::uint64_t;

/// Unique identifiers used by graph objects.
using NodeId = std::uint64_t;
using PortId = std::uint64_t;

inline constexpr NodeId invalid_node_id = 0;
inline constexpr PortId invalid_port_id = 0;

/// Typed indices used to access resources during block processing.
///
/// These indices are local to their respective input/output collections and
/// are intentionally distinct to prevent mixing incompatible resource types.
struct AudioInputIndex {
    std::size_t value = 0;
};

struct AudioOutputIndex {
    std::size_t value = 0;
};

struct EventInputIndex {
    std::size_t value = 0;
};

struct EventOutputIndex {
    std::size_t value = 0;
};

}  // namespace grafite

#endif  // GRAFITE_CORE_TYPES_HPP