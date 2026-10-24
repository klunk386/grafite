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

#ifndef GRAFITE_IO_WAV_READER_HPP
#define GRAFITE_IO_WAV_READER_HPP

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

#include "grafite/core/audio_buffer.hpp"

namespace grafite::io {

/// Reads uncompressed RIFF/WAVE PCM16 audio into planar floating-point views.
///
/// File I/O is an offline operation and must not run in a real-time callback.
/// The reader is not thread-safe. Output views are caller-owned; the reader
/// does not allocate during read_frames().
class WavReader {
public:
    explicit WavReader(const std::string& path)
        : stream_(path, std::ios::binary)
    {
        if (!stream_) {
            throw std::runtime_error("Cannot open WAV file");
        }

        parse_header();
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
    std::uint64_t total_frames() const noexcept
    {
        return total_frames_;
    }

    [[nodiscard]]
    std::uint64_t frame_position() const noexcept
    {
        return frame_position_;
    }

    /// Reads up to frames into the first frames positions of destination.
    ///
    /// Returns the number of frames read; zero indicates end of file.
    /// Destination must have the file's channel count and sufficient capacity.
    /// Samples beyond the returned frame count are left unchanged.
    FrameCount read_frames(AudioBufferView destination, FrameCount frames)
    {
        if (destination.channels() != channels_ ||
            frames > destination.frames()) {
            throw std::invalid_argument("Invalid WAV destination view");
        }

        const auto available = total_frames_ - frame_position_;
        const auto count = static_cast<FrameCount>(
            std::min<std::uint64_t>(frames, available)
        );

        for (FrameCount frame = 0; frame < count; ++frame) {
            for (ChannelCount channel = 0; channel < channels_; ++channel) {
                const auto sample = read_u16(stream_);
                destination(channel, frame) = static_cast<Sample>(
                    static_cast<std::int16_t>(sample)
                ) / 32768.0F;
            }
        }

        frame_position_ += count;
        return count;
    }

private:
    static std::uint16_t read_u16(std::istream& stream)
    {
        std::array<unsigned char, 2> bytes{};
        if (!stream.read(
                reinterpret_cast<char*>(bytes.data()), bytes.size()
            )) {
            throw std::runtime_error("Truncated WAV file");
        }

        return static_cast<std::uint16_t>(
            bytes[0] | (static_cast<std::uint16_t>(bytes[1]) << 8)
        );
    }

    static std::uint32_t read_u32(std::istream& stream)
    {
        const auto low = read_u16(stream);
        const auto high = read_u16(stream);
        return static_cast<std::uint32_t>(low) |
               (static_cast<std::uint32_t>(high) << 16);
    }

    static std::array<char, 4> read_tag(std::istream& stream)
    {
        std::array<char, 4> tag{};
        if (!stream.read(tag.data(), tag.size())) {
            throw std::runtime_error("Truncated WAV chunk");
        }
        return tag;
    }

    void parse_header()
    {
        if (read_tag(stream_) != std::array<char, 4>{'R', 'I', 'F', 'F'}) {
            throw std::runtime_error("Not a RIFF file");
        }

        const auto riff_size = read_u32(stream_);
        if (riff_size < 4 ||
            read_tag(stream_) != std::array<char, 4>{'W', 'A', 'V', 'E'}) {
            throw std::runtime_error("Not a RIFF/WAVE file");
        }

        const auto riff_end = std::uint64_t{8} + riff_size;
        bool found_format = false;
        bool found_data = false;
        std::uint64_t offset = 12;
        std::uint32_t data_size = 0;
        std::streampos data_position{};

        while (offset + 8 <= riff_end) {
            const auto tag = read_tag(stream_);
            const auto size = read_u32(stream_);
            offset += 8;
            const auto padded_size = std::uint64_t{size} + (size & 1U);

            if (padded_size > riff_end - offset) {
                throw std::runtime_error("Invalid WAV chunk size");
            }

            if (tag == std::array<char, 4>{'f', 'm', 't', ' '}) {
                if (found_format || size < 16) {
                    throw std::runtime_error("Invalid WAV format chunk");
                }

                const auto format = read_u16(stream_);
                channels_ = read_u16(stream_);
                sample_rate_ = read_u32(stream_);
                const auto byte_rate = read_u32(stream_);
                const auto block_align = read_u16(stream_);
                const auto bits = read_u16(stream_);

                if (format != 1 || bits != 16 || channels_ == 0 ||
                    sample_rate_ == 0 ||
                    block_align != static_cast<std::uint32_t>(
                        channels_) * 2 ||
                    byte_rate != static_cast<std::uint64_t>(
                        sample_rate_) * block_align) {
                    throw std::runtime_error("Unsupported WAV format");
                }
                found_format = true;
            } else if (tag == std::array<char, 4>{'d', 'a', 't', 'a'}) {
                if (found_data) {
                    throw std::runtime_error("Duplicate WAV data chunk");
                }
                data_position = stream_.tellg();
                data_size = size;
                found_data = true;
            }

            stream_.seekg(
                static_cast<std::streamoff>(offset + padded_size),
                std::ios::beg
            );
            if (!stream_) {
                throw std::runtime_error("Invalid WAV chunk offset");
            }
            offset += padded_size;
        }

        if (!found_format || !found_data ||
            data_size % (static_cast<std::uint32_t>(channels_) * 2) != 0) {
            throw std::runtime_error("Incomplete WAV file");
        }

        stream_.seekg(0, std::ios::end);
        if (!stream_ ||
            stream_.tellg() < data_position +
                static_cast<std::streamoff>(data_size)) {
            throw std::runtime_error("Truncated WAV audio data");
        }

        total_frames_ = data_size / (channels_ * 2);
        stream_.seekg(data_position);
        if (!stream_) {
            throw std::runtime_error("Cannot seek to WAV audio data");
        }
    }

    std::ifstream stream_;
    ChannelCount channels_ = 0;
    std::uint32_t sample_rate_ = 0;
    std::uint64_t total_frames_ = 0;
    std::uint64_t frame_position_ = 0;
};

}  // namespace grafite::io

#endif  // GRAFITE_IO_WAV_READER_HPP
