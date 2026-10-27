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
#include <thread>

#include "grafite/io/audio_ring_buffer.hpp"

using grafite::AudioBuffer;
using grafite::io::AudioRingBuffer;

int main()
{
    AudioRingBuffer ring(2, 5);
    AudioBuffer source(2, 8);
    AudioBuffer destination(2, 8);
    for (std::size_t i = 0; i < 8; ++i) {
        source(0, i) = static_cast<float>(i + 1);
        source(1, i) = static_cast<float>(i + 101);
    }
    destination.clear();

    assert(ring.available_read() == 0);
    assert(ring.available_write() == 5);
    assert(ring.write(source.view(), 8) == 5);
    assert(ring.available_write() == 0);
    assert(ring.read(destination.view(), 3) == 3);
    for (std::size_t i = 0; i < 3; ++i) {
        assert(destination(0, i) == source(0, i));
        assert(destination(1, i) == source(1, i));
    }

    // A second transfer crosses the circular boundary.
    assert(ring.write(source.view(), 4) == 3);
    assert(ring.read(destination.view(), 5) == 5);
    const std::array<float, 5> expected0{4, 5, 1, 2, 3};
    const std::array<float, 5> expected1{104, 105, 101, 102, 103};
    for (std::size_t i = 0; i < 5; ++i) {
        assert(destination(0, i) == expected0[i]);
        assert(destination(1, i) == expected1[i]);
    }
    assert(ring.read(destination.view(), 1) == 0);

    // Invalid views are rejected without changing FIFO state.
    AudioBuffer wrong(1, 8);
    assert(ring.write(wrong.view(), 2) == 0);
    assert(ring.read(wrong.view(), 2) == 0);
    assert(ring.available_read() == 0);

    // Concurrent producer/consumer: verify ordering and channel alignment.
    AudioRingBuffer concurrent(2, 17);
    constexpr std::size_t total = 100000;
    std::thread producer([&] {
        AudioBuffer block(2, 1);
        for (std::size_t i = 0; i < total; ++i) {
            block(0, 0) = static_cast<float>(i);
            block(1, 0) = static_cast<float>(i + 100000);
            while (concurrent.write(block.view(), 1) == 0) {
                std::this_thread::yield();
            }
        }
    });
    AudioBuffer block(2, 1);
    for (std::size_t i = 0; i < total; ++i) {
        while (concurrent.read(block.view(), 1) == 0) {
            std::this_thread::yield();
        }
        assert(block(0, 0) == static_cast<float>(i));
        assert(block(1, 0) == static_cast<float>(i + 100000));
    }
    producer.join();
    assert(concurrent.available_read() == 0);
}
