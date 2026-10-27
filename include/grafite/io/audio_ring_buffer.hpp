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

#ifndef GRAFITE_IO_AUDIO_RING_BUFFER_HPP
#define GRAFITE_IO_AUDIO_RING_BUFFER_HPP

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include "grafite/core/audio_buffer.hpp"

namespace grafite::io {

/// Fixed-capacity, planar, single-producer/single-consumer audio FIFO.
///
/// Exactly one thread may call write(), and exactly one other thread may
/// call read(). The buffer must not be resized or destroyed while active.
/// Partial transfers are permitted; callers handle overflow and underflow.
/// A successful transfer publishes all channels of each frame together.
/// The caller must not modify a source region until write() returns, nor
/// inspect a destination region until read() returns.
/// Query methods are advisory during concurrent operation; they must not
/// be used to reserve capacity or replace the returned transfer counts.
/// The frame counters assume fewer than 2^63 outstanding increments across
/// the lifetime of the object (no counter wraparound).
/// Indices are monotonically increasing frame counters (not sample counters).
///
/// The class is not a clock synchronizer or a sample-rate converter.
class AudioRingBuffer {
public:
    AudioRingBuffer(ChannelCount channels, FrameCount capacity_frames)
        : channels_(channels),
          capacity_(capacity_frames),
          storage_(channels, capacity_frames)
    {
        if (channels == 0 || capacity_frames == 0 ||
            capacity_frames > std::numeric_limits<std::uint64_t>::max() / 2 ||
            !read_pos_.is_lock_free() || !write_pos_.is_lock_free()) {
            throw std::invalid_argument("Invalid audio ring configuration");
        }
    }

    AudioRingBuffer(const AudioRingBuffer&) = delete;
    AudioRingBuffer& operator=(const AudioRingBuffer&) = delete;

    [[nodiscard]] ChannelCount channels() const noexcept
    {
        return channels_;
    }

    [[nodiscard]] FrameCount capacity() const noexcept
    {
        return capacity_;
    }

    /// Snapshot: exact only when the opposite thread is not advancing.
    [[nodiscard]] FrameCount available_read() const noexcept
    {
        const auto written = write_pos_.load(std::memory_order_acquire);
        const auto read = read_pos_.load(std::memory_order_acquire);
        return static_cast<FrameCount>(written - read);
    }

    [[nodiscard]] FrameCount available_write() const noexcept
    {
        return capacity_ - available_read();
    }

    /// Writes as many complete frames as fit, without overwriting unread data.
    /// The producer must provide at least 'frames' samples per channel.
    [[nodiscard]] FrameCount write(
        ConstAudioBufferView source,
        FrameCount frames
    ) noexcept
    {
        if (source.channels() != channels_ || frames > source.frames()) {
            return 0;
        }
        const auto write = write_pos_.load(std::memory_order_relaxed);
        const auto read = read_pos_.load(std::memory_order_acquire);
        const auto count = std::min(
            frames, capacity_ - static_cast<FrameCount>(write - read)
        );
        const auto start = static_cast<FrameCount>(write % capacity_);
        const auto first = std::min(count, capacity_ - start);
        for (ChannelCount ch = 0; ch < channels_; ++ch) {
            std::copy_n(source.channel(ch), first,
                        storage_.channel(ch) + start);
            std::copy_n(source.channel(ch) + first, count - first,
                        storage_.channel(ch));
        }
        write_pos_.store(write + count, std::memory_order_release);
        return count;
    }

    /// Reads up to 'frames' complete frames, leaving any remainder untouched.
    [[nodiscard]] FrameCount read(
        AudioBufferView destination,
        FrameCount frames
    ) noexcept
    {
        if (destination.channels() != channels_ ||
            frames > destination.frames()) {
            return 0;
        }
        const auto read = read_pos_.load(std::memory_order_relaxed);
        const auto written = write_pos_.load(std::memory_order_acquire);
        const auto count = std::min(
            frames, static_cast<FrameCount>(written - read)
        );
        const auto start = static_cast<FrameCount>(read % capacity_);
        const auto first = std::min(count, capacity_ - start);
        for (ChannelCount ch = 0; ch < channels_; ++ch) {
            std::copy_n(storage_.channel(ch) + start, first,
                        destination.channel(ch));
            std::copy_n(storage_.channel(ch), count - first,
                        destination.channel(ch) + first);
        }
        read_pos_.store(read + count, std::memory_order_release);
        return count;
    }

private:
    ChannelCount channels_;
    FrameCount capacity_;
    AudioBuffer storage_;
    alignas(64) std::atomic<std::uint64_t> read_pos_{0};
    alignas(64) std::atomic<std::uint64_t> write_pos_{0};
};

}  // namespace grafite::io

#endif  // GRAFITE_IO_AUDIO_RING_BUFFER_HPP
