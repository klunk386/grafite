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

#ifndef GRAFITE_CORE_PROCESS_CONTEXT_HPP
#define GRAFITE_CORE_PROCESS_CONTEXT_HPP

#include <cstddef>
#include <span>

#include "grafite/core/audio_buffer.hpp"
#include "grafite/core/types.hpp"
#include "grafite/event/event_buffer.hpp"

namespace grafite {

/// Describes the processing configuration shared by graph nodes.
///
/// ProcessSpec is provided during node preparation, outside the real-time
/// processing path.
struct ProcessSpec {
    double sample_rate = 0.0;
    FrameCount max_block_size = 0;
    std::size_t max_events_per_port = 256;
};

/// Non-owning view of the resources available to a node for one processing
/// block.
///
/// ProcessContext is constructed by the processing runtime from resources
/// prepared in advance. It performs no allocation and owns none of the
/// referenced audio or event buffers.
///
/// frames() specifies the number of valid frames in the current block. Audio
/// buffers may be larger than the current block, but must contain at least
/// frames() frames.
///
/// Event frame offsets are relative to the beginning of the current block and
/// must be smaller than frames().
///
/// All referenced binding arrays and buffer storage must remain valid for the
/// entire process() call. Nodes must not retain views or references obtained
/// from a ProcessContext after process() returns.
///
/// A zero-frame context is representable, although the processing runtime
/// should normally avoid invoking nodes for zero-frame blocks.
class ProcessContext {
public:
    ProcessContext() = default;

    ProcessContext(
        FrameCount frames,
        FrameIndex frame_position,
        std::span<const ConstAudioBufferView> audio_inputs,
        std::span<AudioBufferView> audio_outputs,
        std::span<const event::ConstEventBufferView> event_inputs,
        std::span<event::EventBufferView> event_outputs
    ) noexcept
        : frames_(frames),
          frame_position_(frame_position),
          audio_inputs_(audio_inputs),
          audio_outputs_(audio_outputs),
          event_inputs_(event_inputs),
          event_outputs_(event_outputs)
    {
    }

    /// Returns the number of frames in the current processing block.
    [[nodiscard]]
    FrameCount frames() const noexcept
    {
        return frames_;
    }

    /// Returns the absolute frame position of the beginning of the block.
    [[nodiscard]]
    FrameIndex frame_position() const noexcept
    {
        return frame_position_;
    }

    /// Returns a read-only audio input view.
    [[nodiscard]]
    ConstAudioBufferView audio_input(
        AudioInputIndex index
    ) const noexcept
    {
        return audio_inputs_[index.value];
    }

    /// Returns a mutable audio output view.
    [[nodiscard]]
    AudioBufferView audio_output(
        AudioOutputIndex index
    ) const noexcept
    {
        return audio_outputs_[index.value];
    }

    /// Returns a read-only event input view.
    [[nodiscard]]
    event::ConstEventBufferView event_input(
        EventInputIndex index
    ) const noexcept
    {
        return event_inputs_[index.value];
    }

    /// Returns a mutable event output view.
    [[nodiscard]]
    event::EventBufferView event_output(
        EventOutputIndex index
    ) const noexcept
    {
        return event_outputs_[index.value];
    }

    /// Returns the number of audio inputs available to the node.
    [[nodiscard]]
    std::size_t audio_input_count() const noexcept
    {
        return audio_inputs_.size();
    }

    /// Returns the number of audio outputs available to the node.
    [[nodiscard]]
    std::size_t audio_output_count() const noexcept
    {
        return audio_outputs_.size();
    }

    /// Returns the number of event inputs available to the node.
    [[nodiscard]]
    std::size_t event_input_count() const noexcept
    {
        return event_inputs_.size();
    }

    /// Returns the number of event outputs available to the node.
    [[nodiscard]]
    std::size_t event_output_count() const noexcept
    {
        return event_outputs_.size();
    }

private:
    FrameCount frames_ = 0;
    FrameIndex frame_position_ = 0;

    std::span<const ConstAudioBufferView> audio_inputs_;
    std::span<AudioBufferView> audio_outputs_;

    std::span<const event::ConstEventBufferView> event_inputs_;
    std::span<event::EventBufferView> event_outputs_;
};

}  // namespace grafite

#endif  // GRAFITE_CORE_PROCESS_CONTEXT_HPP