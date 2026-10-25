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

#ifndef GRAFITE_IO_AUDIO_PROCESSOR_HPP
#define GRAFITE_IO_AUDIO_PROCESSOR_HPP

#include <span>

#include "grafite/core/audio_buffer.hpp"
#include "grafite/core/types.hpp"

namespace grafite::io {

/// Backend-independent callback target for block-based audio processing.
///
/// Each span element represents one planar audio bus. The caller supplies
/// buffers and controls the processing schedule; the processor does not own
/// backend devices or their buffers. Bus order and channel counts are
/// configured before processing and remain fixed.
///
/// process() must not allocate, block, or throw. The caller guarantees that
/// frames does not exceed the configured maximum block size and that all
/// views contain at least frames samples per channel. A zero-frame call
/// must leave buffers and the processing timeline unchanged.
///
/// The processor and its buffers must not be modified or destroyed while
/// process() is running. Configuration and lifecycle are control-side tasks.
class AudioProcessor {
public:
    virtual ~AudioProcessor() = default;

    AudioProcessor(const AudioProcessor&) = delete;
    AudioProcessor& operator=(const AudioProcessor&) = delete;

    virtual void process(
        std::span<const ConstAudioBufferView> inputs,
        std::span<AudioBufferView> outputs,
        FrameCount frames
    ) noexcept = 0;

protected:
    AudioProcessor() = default;
};

}  // namespace grafite::io

#endif  // GRAFITE_IO_AUDIO_PROCESSOR_HPP
