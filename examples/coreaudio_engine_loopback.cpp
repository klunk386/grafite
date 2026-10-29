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
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdlib>
#include <cerrno>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <optional>
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

// Monitoring is performed exclusively on the control thread.
struct RunningStats {
    std::uint64_t count = 0;
    double mean = 0.0;
    double m2 = 0.0;
    double minimum = std::numeric_limits<double>::infinity();
    double maximum = -std::numeric_limits<double>::infinity();

    void add(double value) noexcept
    {
        ++count;
        const double delta = value - mean;
        mean += delta / static_cast<double>(count);
        m2 += delta * (value - mean);
        minimum = std::min(minimum, value);
        maximum = std::max(maximum, value);
    }

    [[nodiscard]] double standard_deviation() const noexcept
    {
        return count > 1 ? std::sqrt(m2 / static_cast<double>(count - 1))
                         : 0.0;
    }
};

// Ordinary least-squares trend of window means, control thread only.
struct LinearTrend {
    std::uint64_t count = 0;
    double mean_time = 0.0;
    double mean_value = 0.0;
    double covariance = 0.0;
    double time_variance = 0.0;

    void add(double time, double value) noexcept
    {
        ++count;
        const double dt = time - mean_time;
        mean_time += dt / static_cast<double>(count);
        const double dv = value - mean_value;
        mean_value += dv / static_cast<double>(count);
        covariance += dt * (value - mean_value);
        time_variance += dt * (time - mean_time);
    }

    [[nodiscard]] double slope() const noexcept
    {
        return time_variance > 0.0 ? covariance / time_variance : 0.0;
    }
};

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
        bool monitor = false;
        bool compensate_drift = false;
        int duration_seconds = 60;
        int window_seconds = 10;
        bool duration_given = false;
        for (int i = 1; i < argc; ++i) {
            const std::string arg(argv[i]);
            if (arg == "--monitor") {
                monitor = true;
            } else if (arg == "--drift") {
                compensate_drift = true;
            } else if ((arg == "--duration" || arg == "--window") &&
                       i + 1 < argc) {
                const std::string value(argv[++i]);
                char* end = nullptr;
                errno = 0;
                const long parsed = std::strtol(value.c_str(), &end, 10);
                if (errno != 0 || end == value.c_str() || *end != '\0' ||
                    parsed < 1 || parsed > 3600) {
                    std::cerr << "Invalid value for " << arg << '\n';
                    return 2;
                }
                if (arg == "--duration") {
                    duration_seconds = static_cast<int>(parsed);
                    duration_given = true;
                } else {
                    window_seconds = static_cast<int>(parsed);
                }
            } else {
                std::cerr << "Usage: " << argv[0]
                          << " [--monitor] [--drift]"
                          << " [--duration 1..3600]"
                          << " [--window 1..3600]\n";
                return 2;
            }
        }
        if (!duration_given && !monitor) {
            duration_seconds = 5;
        }
        if (duration_seconds <= 5) {
            std::cerr << "Duration must exceed the 5-second warmup.\n";
            return 2;
        }
        if (window_seconds > duration_seconds) {
            window_seconds = duration_seconds;
        }
        constexpr double input_rate = 48000.0;
        constexpr double output_rate = 44100.0;
        constexpr grafite::FrameCount block_size = 512;
        constexpr grafite::FrameCount capacity = 48000;
        constexpr grafite::FrameCount startup = 4800;

        grafite::io::AudioRingBuffer ring(1, capacity);
        CaptureSink sink(ring);
        std::optional<grafite::io::FifoDriftController> drift;
        if (compensate_drift) {
            drift.emplace(
                input_rate / output_rate, startup,
                300.0, 0.1, 1.0, 0.25
            );
        }
        grafite::io::RingBufferPlaybackProcessor source(
            ring, input_rate, output_rate, startup, 1.0F,
            grafite::io::ChannelRouting({0, 0}), drift
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
                  << "speakers (" << duration_seconds << " seconds, "
                  << (compensate_drift ? "drift enabled" : "fixed SRC")
                  << ").\n"
                  << "CAUTION: reduce speaker volume to prevent feedback.\n";
        input.start();
        output.start();
        const auto started = std::chrono::steady_clock::now();
        if (monitor) {
            std::cout << "sec,occupancy_frames,occupancy_ms,"
                      << "dropped_frames,overflow_callbacks,"
                      << "underflow_events,underflow_frames,"
                      << "recovery_events,drift_ppm\n";
        }
        // Sample both modes at the same cadence. Ignore the first five
        // seconds in aggregate statistics to exclude startup transients.
        RunningStats occupancy_stats;
        RunningStats ppm_stats;
        RunningStats window_occupancy;
        RunningStats window_ppm;
        LinearTrend occupancy_trend;
        if (monitor) {
            std::cout << "window,window_start_s,window_end_s,"
                      << "fifo_mean_frames,fifo_stddev_frames,"
                      << "fifo_min_frames,fifo_max_frames,"
                      << "drift_mean_ppm,drift_stddev_ppm,"
                      << "drift_min_ppm,drift_max_ppm\n";
        }
        constexpr int samples_per_second = 50;
        const int total_samples = duration_seconds * samples_per_second;
        for (int sample = 1; sample <= total_samples; ++sample) {
            std::this_thread::sleep_until(
                started + std::chrono::milliseconds(sample * 20)
            );
            const auto occupancy = ring.available_read();
            const double ppm = source.drift_correction_ppm();
            if (sample > 5 * samples_per_second) {
                occupancy_stats.add(static_cast<double>(occupancy));
                ppm_stats.add(ppm);
                window_occupancy.add(static_cast<double>(occupancy));
                window_ppm.add(ppm);
            }
            const bool window_end =
                sample % (window_seconds * samples_per_second) == 0 ||
                sample == total_samples;
            if (window_end && window_occupancy.count > 0) {
                const double end_seconds =
                    static_cast<double>(sample) / samples_per_second;
                const double start_seconds = std::max(
                    5.0, end_seconds -
                    static_cast<double>(window_occupancy.count) /
                        samples_per_second
                );
                occupancy_trend.add(
                    0.5 * (start_seconds + end_seconds),
                    window_occupancy.mean
                );
                if (monitor) {
                    std::cout << "window," << start_seconds << ','
                              << end_seconds << ','
                              << window_occupancy.mean << ','
                              << window_occupancy.standard_deviation()
                              << ',' << window_occupancy.minimum
                              << ',' << window_occupancy.maximum
                              << ',' << window_ppm.mean
                              << ',' << window_ppm.standard_deviation()
                              << ',' << window_ppm.minimum
                              << ',' << window_ppm.maximum << '\n';
                }
                window_occupancy = {};
                window_ppm = {};
            }
            if (monitor && sample % samples_per_second == 0) {
                const int second = sample / samples_per_second;
                std::cout << second << ',' << occupancy << ','
                          << std::fixed << std::setprecision(3)
                          << (1000.0 * static_cast<double>(occupancy) /
                              input_rate) << ','
                          << sink.dropped() << ','
                          << sink.overflow_events() << ','
                          << source.underflow_events() << ','
                          << source.underflow_frames() << ','
                          << source.recovery_events() << ','
                          << ppm << '\n';
            }
        }
        // Capture the operational snapshot before either backend stops.
        const auto running_occupancy = ring.available_read();
        const auto running_ppm = source.drift_correction_ppm();
        const auto input_diagnostics = input.diagnostics();
        const auto output_diagnostics = output.diagnostics();
        input.stop();
        output.stop();

        std::cout << std::fixed << std::setprecision(3)
                  << "Mode: "
                  << (compensate_drift ? "adaptive" : "fixed")
                  << "\nMonitoring samples (after 5 s): "
                  << occupancy_stats.count
                  << "\nFIFO mean (frames): " << occupancy_stats.mean
                  << "\nFIFO stddev (frames): "
                  << occupancy_stats.standard_deviation()
                  << "\nFIFO min/max (frames): "
                  << occupancy_stats.minimum << " / "
                  << occupancy_stats.maximum
                  << "\nDrift mean (ppm): " << ppm_stats.mean
                  << "\nDrift stddev (ppm): "
                  << ppm_stats.standard_deviation()
                  << "\nDrift min/max (ppm): "
                  << ppm_stats.minimum << " / " << ppm_stats.maximum
                  << "\nFIFO trend (frames/s): "
                  << occupancy_trend.slope()
                  << "\nFIFO trend (ppm at input rate): "
                  << (occupancy_trend.slope() * 1e6 / input_rate)
                  << "\nTrend windows: " << occupancy_trend.count
                  << "\nRunning FIFO snapshot (frames): "
                  << running_occupancy
                  << "\nRunning drift snapshot (ppm): "
                  << running_ppm << '\n';

        std::cout << "Capture callbacks: "
                  << input_diagnostics.callbacks
                  << "\nCaptured frames: "
                  << input_diagnostics.frames
                  << "\nCapture render errors: "
                  << input_diagnostics.render_errors
                  << "\nMaximum capture callback gap (ms): "
                  << (static_cast<double>(
                          input_diagnostics.maximum_callback_gap_ns
                      ) / 1e6)
                  << "\nOutput callbacks: "
                  << output_diagnostics.callbacks
                  << "\nOutput requested frames: "
                  << output_diagnostics.requested_frames
                  << "\nInvalid output callbacks: "
                  << output_diagnostics.invalid_output_callbacks
                  << "\nMaximum output callback gap (ms): "
                  << (static_cast<double>(
                          output_diagnostics.maximum_callback_gap_ns
                      ) / 1e6)
                  << "\nInput frames dropped: " << sink.dropped()
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
                  << "\nFinal drift correction (ppm): "
                  << source.drift_correction_ppm()
                  << "\nRemaining FIFO frames: "
                  << ring.available_read() << '\n';
    } catch (const std::exception& error) {
        std::cerr << "CoreAudio Engine example failed: "
                  << error.what() << '\n';
        return 1;
    }
    return 0;
}
