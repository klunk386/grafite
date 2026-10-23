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

#ifndef GRAFITE_CORE_ENGINE_HPP
#define GRAFITE_CORE_ENGINE_HPP

#include <cassert>
#include <utility>

#include "grafite/core/compiled_graph.hpp"
#include "grafite/core/types.hpp"

namespace grafite {

/// Drives execution of a compiled processing graph.
///
/// Engine owns one CompiledGraph and maintains the absolute processing
/// position. It is independent of audio I/O backends: external components
/// such as JACK, ALSA, or file backends are responsible for supplying the
/// block size and invoking process().
///
/// Graph compilation and replacement are intentionally outside the initial
/// Engine contract. This keeps the real-time execution layer independent
/// from graph construction and backend-specific lifecycle management.
class Engine {
public:
    explicit Engine(CompiledGraph graph) noexcept
        : graph_(std::move(graph))
    {
    }

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    Engine(Engine&&) noexcept = default;
    Engine& operator=(Engine&&) noexcept = default;

    /// Returns the processing specification of the active graph.
    [[nodiscard]]
    const ProcessSpec& spec() const noexcept
    {
        return graph_.spec();
    }

    /// Returns the absolute frame position of the next processing block.
    [[nodiscard]]
    FrameIndex frame_position() const noexcept
    {
        return frame_position_;
    }

    /// Returns the active compiled graph for read-only inspection.
    [[nodiscard]]
    const CompiledGraph& graph() const noexcept
    {
        return graph_;
    }

    /// Returns a writable view of a preallocated external audio input.
    ///
    /// Resolve the identifier during setup, outside the audio callback.
    /// The view refers to graph-owned storage and remains valid across
    /// process() calls while the Engine owns that storage. Clear inputs
    /// before filling each block; only the first frames samples of each
    /// channel are consumed by process(frames).
    /// Throws std::invalid_argument for an unknown input identifier.
    [[nodiscard]]
    AudioBufferView audio_input_buffer(GraphAudioInputId id)
    {
        return graph_.audio_input_buffer(id);
    }

    /// Returns a read-only view of a graph audio output.
    ///
    /// Resolve the identifier during setup, outside the audio callback.
    /// The view refers to graph-owned storage and remains valid across
    /// process() calls while the Engine owns that storage. Only the first
    /// frames samples of each channel contain the current block's output
    /// after process(frames).
    /// Throws std::invalid_argument for an unknown output identifier.
    [[nodiscard]]
    ConstAudioBufferView audio_output_buffer(
        GraphAudioOutputId id
    ) const
    {
        return graph_.audio_output_buffer(id);
    }

    /// Clears external input samples for the next processing block.
    ///
    /// Call before filling external inputs for each block. Unwritten
    /// samples remain silent. frames must not exceed spec().max_block_size.
    /// This method does not allocate memory.
    void clear_audio_inputs(FrameCount frames) noexcept
    {
        graph_.clear_audio_inputs(frames);
    }

    /// Processes one block and advances the absolute frame position.
    ///
    /// frames must not exceed the maximum block size of the compiled graph.
    /// A zero-frame block is ignored and does not advance the timeline.
    ///
    /// This method performs no dynamic memory allocation.
    void process(FrameCount frames) noexcept
    {
        assert(frames <= graph_.spec().max_block_size);

        if (frames == 0) {
            return;
        }

        graph_.process(
            frames,
            frame_position_
        );

        frame_position_ += frames;
    }

    /// Resets node processing state and restarts the timeline from zero.
    ///
    /// This method is not assumed to be real-time safe.
    void reset() noexcept
    {
        graph_.reset();
        frame_position_ = 0;
    }

private:
    CompiledGraph graph_;
    FrameIndex frame_position_ = 0;
};

}  // namespace grafite

#endif  // GRAFITE_CORE_ENGINE_HPP