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
#include <CoreAudio/HostTime.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <cstdint>
#include <cmath>
#include <stdexcept>
#include <string>

#include "grafite/core/audio_buffer.hpp"
#include "grafite/io/audio_processor.hpp"
#include "grafite/io/planar_output_renderer.hpp"

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
          output_(2, max_block_size),
          renderer_(processor_, output_)
    {
        if (!std::isfinite(sample_rate) || sample_rate <= 0.0 ||
            max_block_size == 0) {
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

            // Verify the format actually accepted by the AudioUnit.
            // In particular, the DSP rate must match the render rate.
            AudioStreamBasicDescription actual{};
            UInt32 actual_size = sizeof(actual);
            check(AudioUnitGetProperty(
                unit_,
                kAudioUnitProperty_StreamFormat,
                kAudioUnitScope_Input,
                0,
                &actual,
                &actual_size
            ), "Cannot query CoreAudio output format");

            if (actual_size != sizeof(actual) ||
                actual.mSampleRate != sample_rate ||
                actual.mFormatID != kAudioFormatLinearPCM ||
                (actual.mFormatFlags & kAudioFormatFlagIsFloat) == 0 ||
                (actual.mFormatFlags &
                    kAudioFormatFlagIsNonInterleaved) == 0 ||
                actual.mChannelsPerFrame != 2 ||
                actual.mBitsPerChannel != 32 ||
                actual.mBytesPerFrame != sizeof(float) ||
                actual.mFramesPerPacket != 1 ||
                actual.mBytesPerPacket != sizeof(float)) {
                throw std::runtime_error(
                    "CoreAudio did not accept planar stereo Float32 "
                    "at the requested sample rate"
                );
            }

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

    ~CoreAudioOutputBackend() noexcept
    {
        if (running_) {
            // Best-effort stop: destruction cannot propagate errors.
            AudioOutputUnitStop(unit_);
            running_ = false;
        }
        if (initialized_) {
            AudioUnitUninitialize(unit_);
        }
        if (unit_ != nullptr) {
            AudioComponentInstanceDispose(unit_);
        }
    }

    struct Diagnostics {
        std::uint64_t callbacks;
        std::uint64_t requested_frames;
        std::uint64_t maximum_callback_gap_ns;
        std::uint64_t invalid_output_callbacks;
    };

    [[nodiscard]] Diagnostics diagnostics() const noexcept
    {
        return {
            callback_count_.load(std::memory_order_relaxed),
            requested_frames_.load(std::memory_order_relaxed),
            maximum_gap_ns_.load(std::memory_order_relaxed),
            invalid_output_callbacks_.load(std::memory_order_relaxed)
        };
    }

    void start()
    {
        if (!running_) {
            previous_host_time_ = 0;
            check(AudioOutputUnitStart(unit_), "Cannot start CoreAudio");
            running_ = true;
        }
    }

    /// Stops rendering on the control thread and reports failures.
    void stop()
    {
        if (running_) {
            check(AudioOutputUnitStop(unit_), "Cannot stop CoreAudio");
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
        const AudioTimeStamp* timestamp,
        UInt32,
        UInt32 frames,
        AudioBufferList* data
    )
    {
        auto& self = *static_cast<CoreAudioOutputBackend*>(context);
        self.callback_count_.fetch_add(1, std::memory_order_relaxed);
        self.requested_frames_.fetch_add(frames, std::memory_order_relaxed);
        if (timestamp != nullptr &&
            (timestamp->mFlags & kAudioTimeStampHostTimeValid) != 0) {
            const auto host_time = timestamp->mHostTime;
            if (self.previous_host_time_ != 0 &&
                host_time > self.previous_host_time_) {
                const auto gap = AudioConvertHostTimeToNanos(
                    host_time - self.previous_host_time_
                );
                auto maximum = self.maximum_gap_ns_.load(
                    std::memory_order_relaxed
                );
                while (gap > maximum &&
                    !self.maximum_gap_ns_.compare_exchange_weak(
                        maximum, gap, std::memory_order_relaxed
                    )) {
                }
            }
            self.previous_host_time_ = host_time;
        }
        if (data == nullptr) {
            self.invalid_output_callbacks_.fetch_add(
                1, std::memory_order_relaxed
            );
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
            self.invalid_output_callbacks_.fetch_add(
                1, std::memory_order_relaxed
            );
            return noErr;
        }

        const auto* const left = data->mBuffers[0].mData;
        const auto* const right = data->mBuffers[1].mData;
        if (left == nullptr || right == nullptr) {
            self.invalid_output_callbacks_.fetch_add(
                1, std::memory_order_relaxed
            );
            return noErr;
        }

        if (static_cast<std::uint64_t>(frames) * sizeof(float) >
                data->mBuffers[0].mDataByteSize ||
            static_cast<std::uint64_t>(frames) * sizeof(float) >
                data->mBuffers[1].mDataByteSize) {
            self.invalid_output_callbacks_.fetch_add(
                1, std::memory_order_relaxed
            );
            return noErr;
        }

        auto* destination_left = static_cast<float*>(
            data->mBuffers[0].mData
        );
        auto* destination_right = static_cast<float*>(
            data->mBuffers[1].mData
        );

        self.renderer_.render(
            destination_left, destination_right, frames
        );

        return noErr;
    }

    std::atomic<std::uint64_t> callback_count_{0};
    std::atomic<std::uint64_t> requested_frames_{0};
    std::atomic<std::uint64_t> maximum_gap_ns_{0};
    std::atomic<std::uint64_t> invalid_output_callbacks_{0};
    // Accessed only by the serial output callback, except during start.
    std::uint64_t previous_host_time_ = 0;

    AudioProcessor& processor_;
    FrameCount max_block_size_;
    AudioBuffer output_;
    PlanarOutputRenderer renderer_;
    AudioComponentInstance unit_ = nullptr;
    bool initialized_ = false;
    bool running_ = false;
};

}  // namespace grafite::io

#endif  // __APPLE__
#endif  // GRAFITE_IO_COREAUDIO_OUTPUT_BACKEND_HPP
