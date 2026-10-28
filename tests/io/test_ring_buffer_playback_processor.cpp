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
#include <stdexcept>

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

    assert(processor.peak_occupancy() == 8);
    assert(processor.underflow_events() == 0);
    assert(processor.underflow_frames() == 0);

    // Exhausted FIFO eventually produces silence without blocking.
    processor.process({}, outputs, 4);
    assert(output(0, 3) == 0.0F);
    assert(processor.underflow_events() == 1);
    assert(processor.underflow_frames() > 0);
    assert(processor.recovery_events() == 0);

    // Refill enough data to recover after a real underflow.
    assert(ring.write(source.view(), 8) == 8);
    processor.process({}, outputs, 4);
    assert(processor.recovery_events() == 1);

    // Explicit routing can produce mono or multiple duplicate channels.
    grafite::io::AudioRingBuffer mono_ring(1, 16);
    assert(mono_ring.write(source.view(), 8) == 8);
    grafite::io::RingBufferPlaybackProcessor mono_processor(
        mono_ring, 48000.0, 48000.0, 2, 1.0F,
        grafite::io::ChannelRouting({0})
    );
    grafite::AudioBuffer mono_output(1, 4);
    std::array<grafite::AudioBufferView, 1> mono_views{
        mono_output.view()
    };
    mono_processor.process({}, mono_views, 4);
    for (std::size_t i = 0; i < 4; ++i) {
        assert(mono_output(0, i) == static_cast<float>(i));
    }

    grafite::io::AudioRingBuffer multi_ring(1, 16);
    assert(multi_ring.write(source.view(), 8) == 8);
    grafite::io::RingBufferPlaybackProcessor multi_processor(
        multi_ring, 48000.0, 48000.0, 2, 1.0F,
        grafite::io::ChannelRouting({0, 0, 0})
    );
    grafite::AudioBuffer multi_output(3, 4);
    std::array<grafite::AudioBufferView, 1> multi_views{
        multi_output.view()
    };
    multi_processor.process({}, multi_views, 4);
    for (std::size_t i = 0; i < 4; ++i) {
        for (std::size_t ch = 0; ch < 3; ++ch) {
            assert(multi_output(ch, i) == static_cast<float>(i));
        }
    }

    // Two independent source channels retain alignment through SRC.
    grafite::io::AudioRingBuffer stereo_ring(2, 16);
    grafite::AudioBuffer stereo_source(2, 8);
    for (std::size_t i = 0; i < 8; ++i) {
        stereo_source(0, i) = static_cast<float>(i);
        stereo_source(1, i) = 100.0F + static_cast<float>(i);
    }
    assert(stereo_ring.write(stereo_source.view(), 8) == 8);
    grafite::io::RingBufferPlaybackProcessor stereo_processor(
        stereo_ring, 48000.0, 24000.0, 4, 1.0F,
        grafite::io::ChannelRouting({1, 0})
    );
    grafite::AudioBuffer stereo_output(2, 2);
    std::array<grafite::AudioBufferView, 1> stereo_views{
        stereo_output.view()
    };
    stereo_processor.process({}, stereo_views, 2);
    for (std::size_t i = 0; i < 2; ++i) {
        assert(stereo_output(0, i) == 100.0F + 2.0F * i);
        assert(stereo_output(1, i) == 2.0F * i);
    }
    // The shared SRC phase continues across variable-sized callbacks.
    stereo_processor.process({}, stereo_views, 1);
    assert(stereo_output(0, 0) == 104.0F);
    assert(stereo_output(1, 0) == 4.0F);
    assert(stereo_processor.underflow_events() == 0);

    // A zero-frame callback must not alter playback diagnostics.
    const auto peak_before = stereo_processor.peak_occupancy();
    const auto missing_before = stereo_processor.underflow_frames();
    stereo_processor.process({}, stereo_views, 0);
    assert(stereo_processor.peak_occupancy() == peak_before);
    assert(stereo_processor.underflow_frames() == missing_before);

    // Stereo underflow and recovery must affect both channels together.
    stereo_processor.process({}, stereo_views, 2);
    assert(stereo_processor.underflow_events() == 1);
    assert(stereo_processor.underflow_frames() > 0);
    assert(stereo_ring.write(stereo_source.view(), 8) == 8);
    stereo_processor.process({}, stereo_views, 2);
    assert(stereo_processor.recovery_events() == 1);
    assert(stereo_output(0, 0) == 100.0F);
    assert(stereo_output(1, 0) == 0.0F);
    assert(stereo_output(0, 1) == 102.0F);
    assert(stereo_output(1, 1) == 2.0F);

    // Optional feedback is inactive by default and updates per callback.
    grafite::io::AudioRingBuffer drift_ring(1, 64);
    grafite::AudioBuffer drift_source(1, 32);
    for (std::size_t i = 0; i < 32; ++i) {
        drift_source(0, i) = static_cast<float>(i);
    }
    assert(drift_ring.write(drift_source.view(), 32) == 32);
    grafite::io::RingBufferPlaybackProcessor drift_processor(
        drift_ring, 48000.0, 48000.0, 4, 1.0F,
        grafite::io::ChannelRouting({0}),
        grafite::io::FifoDriftController(1.0, 8, 200.0, 10.0)
    );
    assert(drift_processor.drift_correction_ppm() == 0.0);
    drift_processor.process({}, mono_views, 1);
    assert(drift_processor.drift_correction_ppm() == 0.0);
    assert(mono_output(0, 0) == 0.0F);
    drift_processor.process({}, mono_views, 1);
    assert(drift_processor.drift_correction_ppm() == 200.0);
    assert(mono_output(0, 0) == 1.0F);
    assert(drift_processor.underflow_events() == 0);
    assert(processor.drift_correction_ppm() == 0.0);

    bool mismatch_rejected = false;
    try {
        grafite::io::RingBufferPlaybackProcessor invalid_drift(
            drift_ring, 48000.0, 48000.0, 4, 1.0F,
            grafite::io::ChannelRouting({0}),
            grafite::io::FifoDriftController(2.0, 8)
        );
    } catch (const std::invalid_argument&) {
        mismatch_rejected = true;
    }
    assert(mismatch_rejected);

    // Below startup threshold, output remains silent.
    grafite::io::AudioRingBuffer empty(1, 16);
    grafite::io::RingBufferPlaybackProcessor waiting(
        empty, 48000.0, 44100.0, 4
    );
    output.clear();
    waiting.process({}, outputs, 4);
    assert(waiting.underflow_events() == 0);
    assert(waiting.underflow_frames() == 0);
    assert(waiting.recovery_events() == 0);
    for (std::size_t i = 0; i < 4; ++i) {
        assert(output(0, i) == 0.0F);
        assert(output(1, i) == 0.0F);
    }
}
