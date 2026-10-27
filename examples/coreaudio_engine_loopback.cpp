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
#include <cstdint>
#include <exception>
#include <iomanip>
#include <string>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>

#include "grafite/core/engine.hpp"
#include "grafite/core/graph.hpp"
#include "grafite/io/audio_ring_buffer.hpp"
#include "grafite/io/coreaudio_input_backend.hpp"
#include "grafite/io/coreaudio_output_backend.hpp"
#include "grafite/io/engine_audio_processor.hpp"
#include "grafite/io/ring_buffer_engine_processor.hpp"
#include "grafite/io/ring_buffer_playback_processor.hpp"
#include "wav_gain_processor.hpp"

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
        if (written != frames) {
            dropped_.fetch_add(
                frames - written, std::memory_order_relaxed
            );
            overflow_events_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    [[nodiscard]] std::uint64_t overflow_events() const noexcept
    {
        return overflow_events_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::uint64_t dropped() const noexcept
    {
        return dropped_.load(std::memory_order_relaxed);
    }

private:
    grafite::io::AudioRingBuffer& ring_;
    std::atomic<std::uint64_t> dropped_{0};
    std::atomic<std::uint64_t> overflow_events_{0};
};

}  // namespace

int main(int argc, char* argv[])
{
    try {
        if (argc > 2 || (argc == 2 &&
            std::string(argv[1]) != "--monitor")) {
            std::cerr << "Usage: " << argv[0] << " [--monitor]\n";
            return 2;
        }
        const bool monitor = argc == 2;
        const int duration_seconds = monitor ? 60 : 5;
        constexpr double input_rate = 48000.0;
        constexpr double output_rate = 44100.0;
        constexpr grafite::FrameCount block_size = 512;
        constexpr grafite::FrameCount capacity = 48000;
        constexpr grafite::FrameCount startup = 4800;

        grafite::io::AudioRingBuffer ring(1, capacity);
        CaptureSink sink(ring);
        grafite::io::RingBufferPlaybackProcessor source(
            ring, input_rate, output_rate, startup, 1.0F
        );

        grafite::Graph graph;
        const auto input_id = graph.add_audio_input("Mic stereo", 2);
        const auto output_id = graph.add_audio_output("Speakers", 2);
        auto& gain = graph.add_node<grafite::examples::GainNode>(
            2, 0.02F
        );
        graph.connect(input_id, gain, grafite::AudioInputIndex{0});
        graph.connect(gain, grafite::AudioOutputIndex{0}, output_id);

        grafite::ProcessSpec spec;
        spec.sample_rate = output_rate;
        spec.max_block_size = block_size;

        grafite::Engine engine(std::move(graph).compile(spec));
        grafite::io::EngineAudioProcessor engine_processor(
            engine,
            std::vector<grafite::GraphAudioInputId>{input_id},
            std::vector<grafite::GraphAudioOutputId>{output_id}
        );
        grafite::io::RingBufferEngineProcessor processor(
            source, engine_processor, block_size
        );

        grafite::io::CoreAudioInputBackend input(
            sink, input_rate, block_size
        );
        grafite::io::CoreAudioOutputBackend output(
            processor, output_rate, block_size
        );

        std::cout << "Mic -> FIFO -> resampler -> Engine/GainNode -> "
                  << "speakers (" << duration_seconds << " seconds).\n"
                  << "CAUTION: reduce speaker volume to prevent feedback.\n";
        input.start();
        output.start();
        const auto started = std::chrono::steady_clock::now();
        if (monitor) {
            std::cout << "sec,occupancy_frames,occupancy_ms,"
                      << "dropped_frames,overflow_callbacks,"
                      << "underflow_events,underflow_frames,"
                      << "recovery_events\n";
        }
        for (int second = 1; second <= duration_seconds; ++second) {
            std::this_thread::sleep_until(
                started + std::chrono::seconds(second)
            );
            if (monitor) {
                const auto occupancy = ring.available_read();
                std::cout << second << ',' << occupancy << ','
                          << std::fixed << std::setprecision(3)
                          << (1000.0 * static_cast<double>(occupancy) /
                              input_rate) << ','
                          << sink.dropped() << ','
                          << sink.overflow_events() << ','
                          << source.underflow_events() << ','
                          << source.underflow_frames() << ','
                          << source.recovery_events() << '\n';
            }
        }
        output.stop();
        input.stop();

        std::cout << "Input frames dropped: " << sink.dropped()
                  << "\nInput overflow callbacks: "
                  << sink.overflow_events()
                  << "\nPlayback underflow events: "
                  << source.underflow_events()
                  << "\nPlayback underflow frames: "
                  << source.underflow_frames()
                  << "\nPlayback recovery events: "
                  << source.recovery_events()
                  << "\nPeak observed FIFO frames: "
                  << source.peak_occupancy()
                  << "\nRemaining FIFO frames: "
                  << ring.available_read() << '\n';
    } catch (const std::exception& error) {
        std::cerr << "CoreAudio Engine example failed: "
                  << error.what() << '\n';
        return 1;
    }
    return 0;
}
