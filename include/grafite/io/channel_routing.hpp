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

#ifndef GRAFITE_IO_CHANNEL_ROUTING_HPP
#define GRAFITE_IO_CHANNEL_ROUTING_HPP

#include <stdexcept>
#include <vector>

#include "grafite/core/audio_buffer.hpp"

namespace grafite::io {

/// Fixed channel mapping, configured before real-time processing.
///
/// Each output channel selects one source channel. Repetition implements
/// duplication; no mixing or gain is performed. The mapping is immutable
/// while an audio callback is active.
class ChannelRouting {
public:
    explicit ChannelRouting(std::vector<ChannelCount> sources)
        : sources_(std::move(sources))
    {
        if (sources_.empty()) {
            throw std::invalid_argument("Empty channel routing");
        }
    }

    /// Validates source indices against the configured source bus width.
    void validate(ChannelCount source_channels) const
    {
        if (source_channels == 0) {
            throw std::invalid_argument("Empty source bus");
        }
        for (const auto source : sources_) {
            if (source >= source_channels) {
                throw std::invalid_argument("Invalid channel routing");
            }
        }
    }

    [[nodiscard]] ChannelCount output_channels() const noexcept
    {
        return sources_.size();
    }

    /// Routes a single frame, without allocation.
    void route(
        ConstAudioBufferView source,
        AudioBufferView output,
        FrameCount source_frame,
        FrameCount output_frame
    ) const noexcept
    {
        for (ChannelCount ch = 0; ch < sources_.size(); ++ch) {
            output(ch, output_frame) = source(sources_[ch], source_frame);
        }
    }

private:
    std::vector<ChannelCount> sources_;
};

}  // namespace grafite::io

#endif  // GRAFITE_IO_CHANNEL_ROUTING_HPP
