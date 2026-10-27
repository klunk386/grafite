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

#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <thread>

#include "grafite/io/audio_ring_buffer.hpp"
#include "grafite/io/coreaudio_input_backend.hpp"
#include "grafite/io/coreaudio_output_backend.hpp"
#include "grafite/io/ring_buffer_playback_processor.hpp"

namespace {

class CaptureSink final : public grafite::io::AudioInputSink {
public:
    explicit CaptureSink(grafite::io::AudioRingBuffer& ring)
        : ring_(ring)
    {
    }

    void process_input(
        grafite::ConstAudioBufferView input,
        grafite::FrameCount frames
    ) noexcept override
    {
        const auto written = ring_.write(input, frames);
        dropped_.fetch_add(frames - written, std::memory_order_relaxed);
    }

    [[nodiscard]] std::uint64_t dropped() const noexcept
    {
        return dropped_.load(std::memory_order_relaxed);
    }

private:
    grafite::io::AudioRingBuffer& ring_;
    std::atomic<std::uint64_t> dropped_{0};
};

}  // namespace

int main()
{
    try {
        constexpr double input_rate = 48000.0;
        constexpr double output_rate = 44100.0;
        constexpr grafite::FrameCount block_size = 512;
        constexpr grafite::FrameCount capacity = 48000;
        constexpr grafite::FrameCount startup = 4800;

        grafite::io::AudioRingBuffer ring(1, capacity);
        CaptureSink sink(ring);
        grafite::io::RingBufferPlaybackProcessor processor(
            ring, input_rate, output_rate, startup, 0.02F
        );
        grafite::io::CoreAudioInputBackend input(
            sink, input_rate, block_size
        );
        grafite::io::CoreAudioOutputBackend output(
            processor, output_rate, block_size
        );

        std::cout << "Experimental mic loopback for 5 seconds.\n"
                  << "CAUTION: reduce speaker volume to prevent feedback.\n";
        input.start();
        output.start();
        std::this_thread::sleep_for(std::chrono::seconds(5));
        output.stop();
        input.stop();

        std::cout << "Input frames dropped: " << sink.dropped()
                  << "\nRemaining FIFO frames: "
                  << ring.available_read() << '\n';
    } catch (const std::exception& error) {
        std::cerr << "CoreAudio loopback failed: "
                  << error.what() << '\n';
        return 1;
    }
    return 0;
}
