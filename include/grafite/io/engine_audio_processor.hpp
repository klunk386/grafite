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

#ifndef GRAFITE_IO_ENGINE_AUDIO_PROCESSOR_HPP
#define GRAFITE_IO_ENGINE_AUDIO_PROCESSOR_HPP

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <utility>
#include <vector>

#include "grafite/core/engine.hpp"
#include "grafite/io/audio_processor.hpp"

namespace grafite::io {

/// Binds external audio buses to an Engine without exposing it to backends.
///
/// All graph lookups and allocations occur during construction. The caller
/// must keep the Engine alive and stationary for the adapter's lifetime.
/// Input and output IDs are ordered to match the backend bus arrays.
/// A future execution domain can implement AudioProcessor instead.
class EngineAudioProcessor final : public AudioProcessor {
public:
    EngineAudioProcessor(
        Engine& engine,
        const std::vector<GraphAudioInputId>& input_ids,
        const std::vector<GraphAudioOutputId>& output_ids
    )
        : engine_(engine)
    {
        inputs_.reserve(input_ids.size());
        outputs_.reserve(output_ids.size());

        for (const auto id : input_ids) {
            inputs_.push_back(engine.audio_input_buffer(id));
        }

        for (const auto id : output_ids) {
            outputs_.push_back(engine.audio_output_buffer(id));
        }
    }

    /// Transfers one block between backend buffers and graph-owned buffers.
    ///
    /// Buffer counts and channel counts must match the configured buses.
    /// These conditions are assertions rather than callback-time exceptions.
    /// Inputs are cleared before copying; outputs are overwritten after DSP.
    void process(
        std::span<const ConstAudioBufferView> inputs,
        std::span<AudioBufferView> outputs,
        FrameCount frames
    ) noexcept override
    {
        assert(inputs.size() == inputs_.size());
        assert(outputs.size() == outputs_.size());
        assert(frames <= engine_.spec().max_block_size);

        if (frames == 0) {
            return;
        }

        engine_.clear_audio_inputs(frames);

        for (std::size_t bus = 0; bus < inputs_.size(); ++bus) {
            const auto source = inputs[bus];
            const auto destination = inputs_[bus];

            assert(source.channels() == destination.channels());
            assert(source.frames() >= frames);

            for (ChannelCount channel = 0;
                 channel < destination.channels(); ++channel) {
                std::copy_n(
                    source.channel(channel),
                    frames,
                    destination.channel(channel)
                );
            }
        }

        engine_.process(frames);

        for (std::size_t bus = 0; bus < outputs_.size(); ++bus) {
            const auto source = outputs_[bus];
            const auto destination = outputs[bus];

            assert(source.channels() == destination.channels());
            assert(destination.frames() >= frames);

            for (ChannelCount channel = 0;
                 channel < source.channels(); ++channel) {
                std::copy_n(
                    source.channel(channel),
                    frames,
                    destination.channel(channel)
                );
            }
        }
    }

private:
    Engine& engine_;
    std::vector<AudioBufferView> inputs_;
    std::vector<ConstAudioBufferView> outputs_;
};

}  // namespace grafite::io

#endif  // GRAFITE_IO_ENGINE_AUDIO_PROCESSOR_HPP
