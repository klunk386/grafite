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

#ifndef GRAFITE_CORE_AUDIO_BUFFER_HPP
#define GRAFITE_CORE_AUDIO_BUFFER_HPP

#include <algorithm>
#include <vector>

#include "grafite/core/types.hpp"

namespace grafite {

/// Non-owning mutable view over planar audio data.
///
/// Audio samples are stored in contiguous blocks for each channel. The
/// channel stride specifies the distance, in samples, between the beginning
/// of consecutive channels.
///
/// AudioBufferView does not allocate or own memory. The referenced storage
/// must remain valid for the lifetime of the view.
class AudioBufferView {
public:
    AudioBufferView() = default;

    AudioBufferView(
        Sample* data,
        ChannelCount channels,
        FrameCount frames,
        FrameCount channel_stride
    ) noexcept
        : data_(data),
          channels_(channels),
          frames_(frames),
          channel_stride_(channel_stride)
    {
    }

    /// Returns a pointer to the first sample of a channel.
    [[nodiscard]]
    Sample* channel(ChannelCount index) const noexcept
    {
        return data_ + index * channel_stride_;
    }

    /// Returns a reference to an individual sample.
    [[nodiscard]]
    Sample& operator()(
        ChannelCount channel_index,
        FrameCount frame_index
    ) const noexcept
    {
        return channel(channel_index)[frame_index];
    }

    [[nodiscard]]
    ChannelCount channels() const noexcept
    {
        return channels_;
    }

    [[nodiscard]]
    FrameCount frames() const noexcept
    {
        return frames_;
    }

    [[nodiscard]]
    FrameCount channel_stride() const noexcept
    {
        return channel_stride_;
    }

    [[nodiscard]]
    bool empty() const noexcept
    {
        return channels_ == 0 || frames_ == 0;
    }

    /// Sets all samples referenced by the view to zero.
    void clear() noexcept
    {
        for (ChannelCount ch = 0; ch < channels_; ++ch) {
            std::fill_n(channel(ch), frames_, Sample{0});
        }
    }

private:
    Sample* data_ = nullptr;
    ChannelCount channels_ = 0;
    FrameCount frames_ = 0;
    FrameCount channel_stride_ = 0;
};


/// Non-owning read-only view over planar audio data.
///
/// This is the immutable counterpart of AudioBufferView and can be used for
/// inputs that must not be modified by a processing component.
class ConstAudioBufferView {
public:
    ConstAudioBufferView() = default;

    ConstAudioBufferView(
        const Sample* data,
        ChannelCount channels,
        FrameCount frames,
        FrameCount channel_stride
    ) noexcept
        : data_(data),
          channels_(channels),
          frames_(frames),
          channel_stride_(channel_stride)
    {
    }

    /// Creates a read-only view from a mutable view.
    ConstAudioBufferView(const AudioBufferView& view) noexcept
        : data_(view.channels() == 0 ? nullptr : view.channel(0)),
          channels_(view.channels()),
          frames_(view.frames()),
          channel_stride_(view.channel_stride())
    {
    }

    /// Returns a pointer to the first sample of a channel.
    [[nodiscard]]
    const Sample* channel(ChannelCount index) const noexcept
    {
        return data_ + index * channel_stride_;
    }

    /// Returns a reference to an individual sample.
    [[nodiscard]]
    const Sample& operator()(
        ChannelCount channel_index,
        FrameCount frame_index
    ) const noexcept
    {
        return channel(channel_index)[frame_index];
    }

    [[nodiscard]]
    ChannelCount channels() const noexcept
    {
        return channels_;
    }

    [[nodiscard]]
    FrameCount frames() const noexcept
    {
        return frames_;
    }

    [[nodiscard]]
    FrameCount channel_stride() const noexcept
    {
        return channel_stride_;
    }

    [[nodiscard]]
    bool empty() const noexcept
    {
        return channels_ == 0 || frames_ == 0;
    }

private:
    const Sample* data_ = nullptr;
    ChannelCount channels_ = 0;
    FrameCount frames_ = 0;
    FrameCount channel_stride_ = 0;
};


/// Owning planar audio buffer.
///
/// AudioBuffer owns one contiguous memory region containing all channels.
/// Memory allocation and resizing are control-side operations and must not be
/// performed from the real-time processing thread.
class AudioBuffer {
public:
    AudioBuffer() = default;

    AudioBuffer(ChannelCount channels, FrameCount frames)
        : data_(channels * frames, Sample{0}),
          channels_(channels),
          frames_(frames)
    {
    }

    /// Resizes the buffer and clears its contents.
    ///
    /// This operation may allocate memory and is therefore not real-time
    /// safe.
    void resize(ChannelCount channels, FrameCount frames)
    {
        data_.assign(channels * frames, Sample{0});
        channels_ = channels;
        frames_ = frames;
    }

    /// Sets all samples in the buffer to zero.
    void clear() noexcept
    {
        std::fill(data_.begin(), data_.end(), Sample{0});
    }

    /// Returns a pointer to the first sample of a channel.
    [[nodiscard]]
    Sample* channel(ChannelCount index) noexcept
    {
        return data_.data() + index * frames_;
    }

    /// Returns a pointer to the first sample of a channel.
    [[nodiscard]]
    const Sample* channel(ChannelCount index) const noexcept
    {
        return data_.data() + index * frames_;
    }

    /// Returns a reference to an individual sample.
    [[nodiscard]]
    Sample& operator()(
        ChannelCount channel_index,
        FrameCount frame_index
    ) noexcept
    {
        return channel(channel_index)[frame_index];
    }

    /// Returns a reference to an individual sample.
    [[nodiscard]]
    const Sample& operator()(
        ChannelCount channel_index,
        FrameCount frame_index
    ) const noexcept
    {
        return channel(channel_index)[frame_index];
    }

    [[nodiscard]]
    ChannelCount channels() const noexcept
    {
        return channels_;
    }

    [[nodiscard]]
    FrameCount frames() const noexcept
    {
        return frames_;
    }

    [[nodiscard]]
    bool empty() const noexcept
    {
        return channels_ == 0 || frames_ == 0;
    }

    /// Returns a mutable view over the complete buffer.
    [[nodiscard]]
    AudioBufferView view() noexcept
    {
        return {
            data_.data(),
            channels_,
            frames_,
            frames_
        };
    }

    /// Returns a read-only view over the complete buffer.
    [[nodiscard]]
    ConstAudioBufferView view() const noexcept
    {
        return {
            data_.data(),
            channels_,
            frames_,
            frames_
        };
    }

private:
    std::vector<Sample> data_;
    ChannelCount channels_ = 0;
    FrameCount frames_ = 0;
};

}  // namespace grafite

#endif  // GRAFITE_CORE_AUDIO_BUFFER_HPP