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

#ifndef GRAFITE_IO_PLANAR_OUTPUT_RENDERER_HPP
#define GRAFITE_IO_PLANAR_OUTPUT_RENDERER_HPP

#include <algorithm>
#include <array>

#include "grafite/core/audio_buffer.hpp"
#include "grafite/io/audio_processor.hpp"

namespace grafite::io {

/// Hardware-independent stereo output transfer.
///
/// Caller owns and preallocates scratch. The output pointers must each
/// reference at least frames writable samples. No allocations or locks.
class PlanarOutputRenderer {
public:
    PlanarOutputRenderer(
        AudioProcessor& processor,
        AudioBuffer& scratch
    ) noexcept
        : processor_(processor), scratch_(scratch)
    {
    }

    /// Returns false for invalid buffers; valid output is zeroed first.
    /// The processor must satisfy AudioProcessor's noexcept contract.
    bool render(
        Sample* left,
        Sample* right,
        FrameCount frames
    ) noexcept
    {
        if (left == nullptr || right == nullptr || left == right ||
            scratch_.channels() != 2 || scratch_.frames() == 0) {
            return false;
        }

        std::fill_n(left, frames, Sample{0});
        std::fill_n(right, frames, Sample{0});

        FrameCount offset = 0;
        while (offset < frames) {
            const FrameCount count = std::min(
                scratch_.frames(), frames - offset
            );

            std::array<AudioBufferView, 1> outputs{scratch_.view()};
            processor_.process({}, outputs, count);

            std::copy_n(scratch_.channel(0), count, left + offset);
            std::copy_n(scratch_.channel(1), count, right + offset);
            offset += count;
        }
        return true;
    }

private:
    AudioProcessor& processor_;
    AudioBuffer& scratch_;
};

}  // namespace grafite::io

#endif  // GRAFITE_IO_PLANAR_OUTPUT_RENDERER_HPP
