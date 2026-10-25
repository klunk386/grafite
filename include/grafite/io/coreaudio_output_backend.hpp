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

#ifndef GRAFITE_IO_COREAUDIO_OUTPUT_BACKEND_HPP
#define GRAFITE_IO_COREAUDIO_OUTPUT_BACKEND_HPP

#ifdef __APPLE__

#include <AudioToolbox/AudioToolbox.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

#include "grafite/core/audio_buffer.hpp"
#include "grafite/io/audio_processor.hpp"

namespace grafite::io {

/// Native macOS stereo output driver using a non-interleaved Float32 AudioUnit.
///
/// Configuration, initialization, start and stop are control-side operations.
/// AudioProcessor and all referenced buffers must outlive the active unit.
/// A processor is not called concurrently by multiple instances unless it
/// explicitly supports that usage.
class CoreAudioOutputBackend {
public:
    CoreAudioOutputBackend(
        AudioProcessor& processor,
        double sample_rate,
        FrameCount max_block_size
    )
        : processor_(processor),
          max_block_size_(max_block_size),
          output_(2, max_block_size)
    {
        if (sample_rate <= 0.0 || max_block_size == 0) {
            throw std::invalid_argument("Invalid CoreAudio configuration");
        }

        AudioComponentDescription description{};
        description.componentType = kAudioUnitType_Output;
        description.componentSubType = kAudioUnitSubType_DefaultOutput;
        description.componentManufacturer = kAudioUnitManufacturer_Apple;

        const auto component = AudioComponentFindNext(
            nullptr, &description
        );
        if (component == nullptr ||
            AudioComponentInstanceNew(component, &unit_) != noErr) {
            throw std::runtime_error("Cannot create CoreAudio output unit");
        }

        try {
            AudioStreamBasicDescription format{};
            format.mSampleRate = sample_rate;
            format.mFormatID = kAudioFormatLinearPCM;
            format.mFormatFlags =
                kAudioFormatFlagIsFloat |
                kAudioFormatFlagIsPacked |
                kAudioFormatFlagIsNonInterleaved;
            format.mBitsPerChannel = 32;
            format.mChannelsPerFrame = 2;
            format.mFramesPerPacket = 1;
            format.mBytesPerFrame = sizeof(float);
            format.mBytesPerPacket = sizeof(float);

            check(AudioUnitSetProperty(
                unit_,
                kAudioUnitProperty_StreamFormat,
                kAudioUnitScope_Input,
                0,
                &format,
                sizeof(format)
            ), "Cannot configure CoreAudio output format");

            AURenderCallbackStruct callback{};
            callback.inputProc = &CoreAudioOutputBackend::render;
            callback.inputProcRefCon = this;

            check(AudioUnitSetProperty(
                unit_,
                kAudioUnitProperty_SetRenderCallback,
                kAudioUnitScope_Input,
                0,
                &callback,
                sizeof(callback)
            ), "Cannot register CoreAudio callback");

            check(AudioUnitInitialize(unit_), "Cannot initialize AudioUnit");
            initialized_ = true;
        } catch (...) {
            AudioComponentInstanceDispose(unit_);
            unit_ = nullptr;
            throw;
        }
    }

    CoreAudioOutputBackend(const CoreAudioOutputBackend&) = delete;
    CoreAudioOutputBackend& operator=(const CoreAudioOutputBackend&) = delete;

    ~CoreAudioOutputBackend()
    {
        stop();
        if (initialized_) {
            AudioUnitUninitialize(unit_);
        }
        if (unit_ != nullptr) {
            AudioComponentInstanceDispose(unit_);
        }
    }

    void start()
    {
        if (!running_) {
            check(AudioOutputUnitStart(unit_), "Cannot start CoreAudio");
            running_ = true;
        }
    }

    void stop() noexcept
    {
        if (running_) {
            AudioOutputUnitStop(unit_);
            running_ = false;
        }
    }

private:
    static void check(OSStatus status, const char* message)
    {
        if (status != noErr) {
            throw std::runtime_error(message);
        }
    }

    static OSStatus render(
        void* context,
        AudioUnitRenderActionFlags*,
        const AudioTimeStamp*,
        UInt32,
        UInt32 frames,
        AudioBufferList* data
    )
    {
        auto& self = *static_cast<CoreAudioOutputBackend*>(context);
        if (data == nullptr) {
            return noErr;
        }

        // Clear all hardware buffers before any processing or early return.
        for (UInt32 bus = 0; bus < data->mNumberBuffers; ++bus) {
            auto& buffer = data->mBuffers[bus];
            if (buffer.mData != nullptr) {
                std::fill_n(
                    static_cast<std::uint8_t*>(buffer.mData),
                    buffer.mDataByteSize,
                    std::uint8_t{0}
                );
            }
        }

        // The requested planar stereo layout is mandatory for this driver.
        if (data->mNumberBuffers != 2 ||
            data->mBuffers[0].mNumberChannels != 1 ||
            data->mBuffers[1].mNumberChannels != 1) {
            return noErr;
        }

        const auto* const left = data->mBuffers[0].mData;
        const auto* const right = data->mBuffers[1].mData;
        if (left == nullptr || right == nullptr) {
            return noErr;
        }

        if (data->mBuffers[0].mDataByteSize <
                frames * sizeof(float) ||
            data->mBuffers[1].mDataByteSize <
                frames * sizeof(float)) {
            return noErr;
        }

        auto* destination_left = static_cast<float*>(
            data->mBuffers[0].mData
        );
        auto* destination_right = static_cast<float*>(
            data->mBuffers[1].mData
        );

        FrameCount position = 0;
        while (position < frames) {
            const auto count = std::min(
                self.max_block_size_,
                static_cast<FrameCount>(frames) - position
            );

            std::array<AudioBufferView, 1> outputs{
                self.output_.view()
            };
            self.processor_.process({}, outputs, count);

            std::copy_n(
                self.output_.channel(0),
                count,
                destination_left + position
            );
            std::copy_n(
                self.output_.channel(1),
                count,
                destination_right + position
            );
            position += count;
        }

        return noErr;
    }

    AudioProcessor& processor_;
    FrameCount max_block_size_;
    AudioBuffer output_;
    AudioComponentInstance unit_ = nullptr;
    bool initialized_ = false;
    bool running_ = false;
};

}  // namespace grafite::io

#endif  // __APPLE__
#endif  // GRAFITE_IO_COREAUDIO_OUTPUT_BACKEND_HPP
