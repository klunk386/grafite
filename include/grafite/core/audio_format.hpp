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

#ifndef GRAFITE_CORE_AUDIO_FORMAT_HPP
#define GRAFITE_CORE_AUDIO_FORMAT_HPP

#include "grafite/core/types.hpp"

namespace grafite {

struct AudioFormat {
    double sample_rate = 0.0;
    ChannelCount channels = 0;
};

}  // namespace grafite

#endif  // GRAFITE_CORE_AUDIO_FORMAT_HPP