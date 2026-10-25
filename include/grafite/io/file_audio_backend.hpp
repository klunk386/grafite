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

#ifndef GRAFITE_IO_FILE_AUDIO_BACKEND_HPP
#define GRAFITE_IO_FILE_AUDIO_BACKEND_HPP

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

#include "grafite/core/audio_buffer.hpp"
#include "grafite/io/audio_processor.hpp"
#include "grafite/io/wav_reader.hpp"
#include "grafite/io/wav_writer.hpp"

namespace grafite::io {

/// Offline WAV-to-WAV processing driver, independent of Engine and Graph.
///
/// All file operations happen outside AudioProcessor::process(). The
/// processor is responsible for its own configuration and bus compatibility.
/// This initial driver supports one input bus and one output bus, each with
/// the channel count of the input WAV.
class FileAudioBackend {
public:
    explicit FileAudioBackend(
        const std::string& input_path,
        FrameCount block_size = 256
    )
        : reader_(input_path),
          block_size_(block_size),
          input_(reader_.channels(), block_size),
          output_(reader_.channels(), block_size)
    {
        if (block_size == 0) {
            throw std::invalid_argument("WAV block size must be positive");
        }
    }

    [[nodiscard]]
    ChannelCount channels() const noexcept
    {
        return reader_.channels();
    }

    [[nodiscard]]
    std::uint32_t sample_rate() const noexcept
    {
        return reader_.sample_rate();
    }

    [[nodiscard]]
    FrameCount block_size() const noexcept
    {
        return block_size_;
    }

    /// Runs until EOF and finalizes the output WAV.
    ///
    /// The processor must be configured for one input and output bus with
    /// matching channel counts and a maximum block size >= block_size().
    /// The caller must ensure that input and output paths are distinct.
    void run(AudioProcessor& processor, const std::string& output_path)
    {
        WavWriter writer(output_path, channels(), sample_rate());

        const std::array<ConstAudioBufferView, 1> inputs{
            input_.view()
        };
        std::array<AudioBufferView, 1> outputs{
            output_.view()
        };

        while (true) {
            const auto frames = reader_.read_frames(
                input_.view(), block_size_
            );

            if (frames == 0) {
                break;
            }

            processor.process(inputs, outputs, frames);
            writer.write_frames(output_.view(), frames);
        }

        writer.close();
    }

private:
    WavReader reader_;
    FrameCount block_size_;
    AudioBuffer input_;
    AudioBuffer output_;
};

}  // namespace grafite::io

#endif  // GRAFITE_IO_FILE_AUDIO_BACKEND_HPP
