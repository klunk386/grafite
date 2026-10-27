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
#include <span>

#include "grafite/core/audio_buffer.hpp"
#include "grafite/io/audio_processor.hpp"

namespace grafite::io {

/// Hardware-independent multichannel planar output transfer.
///
/// Caller owns and preallocates scratch. Each output pointer must
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

    /// General planar transfer for the configured number of channels.
    /// Invalid buffers return false without writing any samples.
    bool render(
        std::span<Sample* const> channels,
        FrameCount frames
    ) noexcept
    {
        if (channels.size() != scratch_.channels() ||
            scratch_.channels() == 0 || scratch_.frames() == 0) {
            return false;
        }
        for (ChannelCount ch = 0; ch < channels.size(); ++ch) {
            if (channels[ch] == nullptr) {
                return false;
            }
            for (ChannelCount other = 0; other < ch; ++other) {
                if (channels[ch] == channels[other]) {
                    return false;
                }
            }
        }

        for (auto* channel : channels) {
            std::fill_n(channel, frames, Sample{0});
        }
        FrameCount offset = 0;
        while (offset < frames) {
            const FrameCount count = std::min(
                scratch_.frames(), frames - offset
            );
            std::array<AudioBufferView, 1> outputs{scratch_.view()};
            processor_.process({}, outputs, count);
            for (ChannelCount ch = 0; ch < channels.size(); ++ch) {
                std::copy_n(
                    scratch_.channel(ch), count, channels[ch] + offset
                );
            }
            offset += count;
        }
        return true;
    }

    /// Backward-compatible stereo convenience overload.
    bool render(
        Sample* left,
        Sample* right,
        FrameCount frames
    ) noexcept
    {
        std::array<Sample*, 2> channels{left, right};
        return render(channels, frames);
    }

private:
    AudioProcessor& processor_;
    AudioBuffer& scratch_;
};

}  // namespace grafite::io

#endif  // GRAFITE_IO_PLANAR_OUTPUT_RENDERER_HPP
