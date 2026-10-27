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

#ifndef GRAFITE_IO_RING_BUFFER_ENGINE_PROCESSOR_HPP
#define GRAFITE_IO_RING_BUFFER_ENGINE_PROCESSOR_HPP

#include <array>
#include <cassert>
#include <stdexcept>

#include "grafite/io/audio_processor.hpp"

namespace grafite::io {

/// Connects a source processor to a DSP processor with matching bus width.
///
/// The source and destination are AudioProcessor implementations. The
/// preallocated scratch buffer belongs to this adapter. This component
/// knows nothing about CoreAudio or the concrete Engine implementation.
/// Both processors must accept one audio output/input bus respectively.
/// Their lifetime must exceed that of the adapter.
class RingBufferEngineProcessor final : public AudioProcessor {
public:
    RingBufferEngineProcessor(
        AudioProcessor& source,
        AudioProcessor& engine,
        FrameCount max_block_size,
        ChannelCount channels = 2
    )
        : source_(source),
          engine_(engine),
          scratch_(channels, max_block_size)
    {
        if (max_block_size == 0 || channels == 0) {
            throw std::invalid_argument("Invalid processing block size");
        }
    }

    void process(
        std::span<const ConstAudioBufferView> inputs,
        std::span<AudioBufferView> outputs,
        FrameCount frames
    ) noexcept override
    {
        assert(inputs.empty());
        assert(outputs.size() == 1);
        assert(outputs[0].channels() > 0);
        assert(outputs[0].frames() >= frames);
        assert(frames <= scratch_.frames());

        if (frames == 0) {
            return;
        }

        std::array<AudioBufferView, 1> source_outputs{
            scratch_.view()
        };
        source_.process({}, source_outputs, frames);

        std::array<ConstAudioBufferView, 1> engine_inputs{
            ConstAudioBufferView(scratch_.view())
        };
        engine_.process(engine_inputs, outputs, frames);
    }

private:
    AudioProcessor& source_;
    AudioProcessor& engine_;
    AudioBuffer scratch_;
};

}  // namespace grafite::io

#endif  // GRAFITE_IO_RING_BUFFER_ENGINE_PROCESSOR_HPP
