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

#include <array>
#include <cassert>
#include <cmath>

#include "grafite/io/audio_ring_buffer.hpp"
#include "grafite/io/ring_buffer_playback_processor.hpp"

int main()
{
    grafite::io::AudioRingBuffer ring(1, 16);
    grafite::AudioBuffer source(1, 8);
    for (std::size_t i = 0; i < 8; ++i) {
        source(0, i) = static_cast<float>(i);
    }
    assert(ring.write(source.view(), 8) == 8);
    grafite::io::RingBufferPlaybackProcessor processor(
        ring, 48000.0, 24000.0, 4, 0.5F
    );
    grafite::AudioBuffer output(2, 4);
    std::array<grafite::AudioBufferView, 1> outputs{output.view()};
    processor.process({}, outputs, 4);
    // 2:1 decimation: source samples 0, 2, 4, 6.
    for (std::size_t i = 0; i < 4; ++i) {
        assert(std::abs(output(0, i) - static_cast<float>(i)) < 1e-6F);
        assert(output(0, i) == output(1, i));
    }

    // Exhausted FIFO eventually produces silence without blocking.
    processor.process({}, outputs, 4);
    assert(output(0, 3) == 0.0F);

    // Below startup threshold, output remains silent.
    grafite::io::AudioRingBuffer empty(1, 16);
    grafite::io::RingBufferPlaybackProcessor waiting(
        empty, 48000.0, 44100.0, 4
    );
    output.clear();
    waiting.process({}, outputs, 4);
    for (std::size_t i = 0; i < 4; ++i) {
        assert(output(0, i) == 0.0F);
        assert(output(1, i) == 0.0F);
    }
}
