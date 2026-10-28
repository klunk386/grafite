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

#include <algorithm>
#include <array>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <optional>

#include "grafite/io/audio_ring_buffer.hpp"
#include "grafite/io/ring_buffer_playback_processor.hpp"

namespace {

struct Result {
    double final_error;
    double correction_ppm;
    std::uint64_t underflows;
};

Result simulate(double clock_ppm, bool adaptive)
{
    using namespace grafite;
    using namespace grafite::io;
    constexpr FrameCount target = 4800;
    constexpr FrameCount capacity = 16384;
    constexpr double rate = 48000.0;
    constexpr int callbacks = 12000;

    AudioRingBuffer ring(2, capacity);
    AudioBuffer source(2, 512);
    for (FrameCount i = 0; i < 512; ++i) {
        source(0, i) = 0.25F;
        source(1, i) = 0.5F;
    }
    // Prefill the FIFO before starting the simulated consumer.
    FrameCount remaining = target;
    while (remaining != 0) {
        const auto n = std::min<FrameCount>(remaining, 512);
        assert(ring.write(source.view(), n) == n);
        remaining -= n;
    }

    std::optional<FifoDriftController> controller;
    if (adaptive) {
        controller.emplace(1.0, target, 300.0, 2.0, 1.0, 0.1);
    }
    RingBufferPlaybackProcessor processor(
        ring, rate, rate, 1024, 1.0F, ChannelRouting({1, 0}),
        controller
    );
    AudioBuffer output(2, 384);
    std::array<AudioBufferView, 1> views{output.view()};
    double producer_fraction = 0.0;
    for (int i = 0; i < callbacks; ++i) {
        // Different callback sizes with equal long-term output rate.
        const FrameCount frames = (i % 3 == 0) ? 128 :
                                  (i % 3 == 1) ? 256 : 384;
        producer_fraction += static_cast<double>(frames) *
                             (1.0 + clock_ppm * 1e-6);
        auto produced = static_cast<FrameCount>(
            std::floor(producer_fraction)
        );
        producer_fraction -= static_cast<double>(produced);
        while (produced != 0) {
            const auto n = std::min<FrameCount>(produced, 512);
            assert(ring.write(source.view(), n) == n);
            produced -= n;
        }
        processor.process({}, views, frames);
        for (FrameCount j = 0; j < frames; ++j) {
            assert(std::abs(output(0, j) - 0.5F) < 1e-6F);
            assert(std::abs(output(1, j) - 0.25F) < 1e-6F);
        }
    }
    return {
        static_cast<double>(ring.available_read()) -
            static_cast<double>(target),
        processor.drift_correction_ppm(),
        processor.underflow_events()
    };
}

void verify(double clock_ppm)
{
    const auto fixed = simulate(clock_ppm, false);
    const auto adaptive = simulate(clock_ppm, true);
    std::fprintf(
        stderr,
        "clock=%+.0f ppm | fixed error=%+.1f frames | "
        "adaptive error=%+.1f frames | correction=%+.2f ppm | "
        "underflows=%llu/%llu\\n",
        clock_ppm, fixed.final_error, adaptive.final_error,
        adaptive.correction_ppm,
        static_cast<unsigned long long>(fixed.underflows),
        static_cast<unsigned long long>(adaptive.underflows)
    );
    assert(fixed.underflows == 0);
    assert(adaptive.underflows == 0);
    assert(std::abs(fixed.final_error) > 250.0);
    assert(std::abs(adaptive.final_error) < 90.0);
    assert(std::abs(adaptive.final_error) <
           std::abs(fixed.final_error) * 0.35);
    assert(std::abs(adaptive.correction_ppm - clock_ppm) < 30.0);
}

}  // namespace

int main()
{
    verify(100.0);
    verify(-100.0);
}
