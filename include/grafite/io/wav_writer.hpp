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

#ifndef GRAFITE_IO_WAV_WRITER_HPP
#define GRAFITE_IO_WAV_WRITER_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

#include "grafite/core/audio_buffer.hpp"

namespace grafite::io {

/// Writes planar floating-point audio as interleaved PCM16 RIFF/WAVE.
///
/// This is an offline file writer, not a real-time callback component.
/// Call close() to finalize the header and report any I/O errors.
/// Destruction attempts best-effort finalization without throwing.
class WavWriter {
public:
    WavWriter(
        const std::string& path,
        ChannelCount channels,
        std::uint32_t sample_rate
    )
        : stream_(path, std::ios::binary | std::ios::trunc),
          channels_(channels),
          sample_rate_(sample_rate)
    {
        if (channels == 0 ||
            channels > std::numeric_limits<std::uint16_t>::max() / 2 ||
            sample_rate == 0 ||
            static_cast<std::uint64_t>(sample_rate) * channels * 2 >
                std::numeric_limits<std::uint32_t>::max()) {
            throw std::invalid_argument("Invalid WAV output format");
        }

        if (!stream_) {
            throw std::runtime_error("Cannot open WAV output file");
        }

        write_header(0);
    }

    WavWriter(const WavWriter&) = delete;
    WavWriter& operator=(const WavWriter&) = delete;
    WavWriter(WavWriter&&) = delete;
    WavWriter& operator=(WavWriter&&) = delete;

    ~WavWriter() noexcept
    {
        if (!closed_) {
            try {
                close();
            } catch (...) {
                // Destructors must not propagate I/O errors.
            }
        }
    }

    [[nodiscard]]
    ChannelCount channels() const noexcept
    {
        return channels_;
    }

    [[nodiscard]]
    std::uint32_t sample_rate() const noexcept
    {
        return sample_rate_;
    }

    [[nodiscard]]
    std::uint64_t frames_written() const noexcept
    {
        return frames_written_;
    }

    /// Appends frames from a caller-owned planar audio view.
    ///
    /// The view must have the configured channel count and at least frames
    /// samples per channel. Samples are clipped to [-1, 1]; non-finite
    /// samples are encoded as silence. This operation performs file I/O.
    void write_frames(ConstAudioBufferView source, FrameCount frames)
    {
        if (closed_) {
            throw std::logic_error("WAV writer is closed");
        }

        if (source.channels() != channels_ || frames > source.frames()) {
            throw std::invalid_argument("Invalid WAV source view");
        }

        constexpr std::uint64_t max_data_bytes =
            std::numeric_limits<std::uint32_t>::max() - 36;
        const auto bytes_per_frame = static_cast<std::uint64_t>(
            channels_
        ) * 2;

        if (frames > (max_data_bytes - data_bytes_) / bytes_per_frame) {
            throw std::length_error("WAV data exceeds RIFF size limit");
        }

        for (FrameCount frame = 0; frame < frames; ++frame) {
            for (ChannelCount channel = 0; channel < channels_; ++channel) {
                const auto value = source(channel, frame);
                const auto finite = std::isfinite(value) ? value : 0.0F;
                const auto clipped = std::clamp(finite, -1.0F, 1.0F);
                const auto scaled = std::lround(clipped * 32768.0F);
                const auto sample = static_cast<std::int16_t>(
                    std::clamp(scaled, -32768L, 32767L)
                );
                write_u16(static_cast<std::uint16_t>(sample));
            }
        }

        if (!stream_) {
            throw std::runtime_error("Cannot write WAV audio data");
        }

        data_bytes_ += static_cast<std::uint64_t>(frames) *
                       bytes_per_frame;
        frames_written_ += frames;
    }

    /// Finalizes the RIFF header and closes the output file.
    ///
    /// May throw on I/O failure. Calling close() again has no effect.
    void close()
    {
        if (closed_) {
            return;
        }

        stream_.seekp(0, std::ios::beg);
        if (!stream_) {
            throw std::runtime_error("Cannot seek to WAV header");
        }

        write_header(static_cast<std::uint32_t>(data_bytes_));
        stream_.flush();

        if (!stream_) {
            throw std::runtime_error("Cannot finalize WAV header");
        }

        stream_.close();
        if (stream_.fail()) {
            throw std::runtime_error("Cannot close WAV output file");
        }

        closed_ = true;
    }

private:
    void write_tag(const char* tag)
    {
        stream_.write(tag, 4);
    }

    void write_u16(std::uint16_t value)
    {
        const char bytes[2] = {
            static_cast<char>(value & 0xFF),
            static_cast<char>((value >> 8) & 0xFF)
        };
        stream_.write(bytes, 2);
    }

    void write_u32(std::uint32_t value)
    {
        write_u16(static_cast<std::uint16_t>(value));
        write_u16(static_cast<std::uint16_t>(value >> 16));
    }

    void write_header(std::uint32_t data_bytes)
    {
        const auto block_align = static_cast<std::uint16_t>(channels_ * 2);
        const auto byte_rate = static_cast<std::uint32_t>(
            sample_rate_ * static_cast<std::uint64_t>(block_align)
        );

        write_tag("RIFF");
        write_u32(36 + data_bytes);
        write_tag("WAVE");
        write_tag("fmt ");
        write_u32(16);
        write_u16(1);
        write_u16(static_cast<std::uint16_t>(channels_));
        write_u32(sample_rate_);
        write_u32(byte_rate);
        write_u16(block_align);
        write_u16(16);
        write_tag("data");
        write_u32(data_bytes);

        if (!stream_) {
            throw std::runtime_error("Cannot write WAV header");
        }
    }

    std::ofstream stream_;
    ChannelCount channels_;
    std::uint32_t sample_rate_;
    std::uint64_t frames_written_ = 0;
    std::uint64_t data_bytes_ = 0;
    bool closed_ = false;
};

}  // namespace grafite::io

#endif  // GRAFITE_IO_WAV_WRITER_HPP
