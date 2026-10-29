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

#ifndef GRAFITE_IO_COREAUDIO_INPUT_BACKEND_HPP
#define GRAFITE_IO_COREAUDIO_INPUT_BACKEND_HPP

#ifdef __APPLE__

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <stdexcept>

#include "grafite/core/audio_buffer.hpp"

namespace grafite::io {

/// Receives planar microphone blocks on the CoreAudio real-time thread.
/// Implementations must not allocate, block, or throw.
class AudioInputSink {
public:
    virtual ~AudioInputSink() = default;
    virtual void process_input(
        ConstAudioBufferView input,
        FrameCount frames
    ) noexcept = 0;

protected:
    AudioInputSink() = default;
};

/// Minimal independent CoreAudio capture driver.
///
/// Uses the default input device at construction time. The sink must outlive
/// the backend. Device changes and permission management are future work.
class CoreAudioInputBackend {
public:
    CoreAudioInputBackend(
        AudioInputSink& sink,
        double sample_rate,
        FrameCount max_block_size
    )
        : sink_(sink),
          buffer_(1, max_block_size)
    {
        if (!std::isfinite(sample_rate) || sample_rate <= 0.0 ||
            max_block_size == 0) {
            throw std::invalid_argument("Invalid input configuration");
        }

        AudioObjectPropertyAddress address{
            kAudioHardwarePropertyDefaultInputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        UInt32 size = sizeof(device_);
        check(AudioObjectGetPropertyData(
            kAudioObjectSystemObject, &address, 0, nullptr,
            &size, &device_
        ), "Cannot query default input device");
        if (size != sizeof(device_) ||
            device_ == kAudioObjectUnknown) {
            throw std::runtime_error("No default input device");
        }

        AudioComponentDescription description{};
        description.componentType = kAudioUnitType_Output;
        description.componentSubType = kAudioUnitSubType_HALOutput;
        description.componentManufacturer = kAudioUnitManufacturer_Apple;
        const auto component = AudioComponentFindNext(
            nullptr, &description
        );
        if (component == nullptr ||
            AudioComponentInstanceNew(component, &unit_) != noErr) {
            throw std::runtime_error("Cannot create HAL input unit");
        }

        try {
            UInt32 enabled = 1;
            UInt32 disabled = 0;
            check(AudioUnitSetProperty(
                unit_, kAudioOutputUnitProperty_EnableIO,
                kAudioUnitScope_Input, 1, &enabled, sizeof(enabled)
            ), "Cannot enable HAL input");
            check(AudioUnitSetProperty(
                unit_, kAudioOutputUnitProperty_EnableIO,
                kAudioUnitScope_Output, 0, &disabled, sizeof(disabled)
            ), "Cannot disable HAL output");
            check(AudioUnitSetProperty(
                unit_, kAudioOutputUnitProperty_CurrentDevice,
                kAudioUnitScope_Global, 0, &device_, sizeof(device_)
            ), "Cannot select input device");

            AudioStreamBasicDescription format{};
            format.mSampleRate = sample_rate;
            format.mFormatID = kAudioFormatLinearPCM;
            format.mFormatFlags = kAudioFormatFlagIsFloat |
                kAudioFormatFlagIsPacked |
                kAudioFormatFlagIsNonInterleaved;
            format.mBitsPerChannel = 32;
            format.mChannelsPerFrame = 1;
            format.mFramesPerPacket = 1;
            format.mBytesPerFrame = sizeof(float);
            format.mBytesPerPacket = sizeof(float);

            check(AudioUnitSetProperty(
                unit_, kAudioUnitProperty_StreamFormat,
                kAudioUnitScope_Output, 1, &format, sizeof(format)
            ), "Cannot set HAL input format");

            AudioStreamBasicDescription actual{};
            size = sizeof(actual);
            check(AudioUnitGetProperty(
                unit_, kAudioUnitProperty_StreamFormat,
                kAudioUnitScope_Output, 1, &actual, &size
            ), "Cannot query HAL input format");
            if (size != sizeof(actual) ||
                actual.mSampleRate != sample_rate ||
                actual.mChannelsPerFrame != 1 ||
                actual.mFormatID != kAudioFormatLinearPCM ||
                (actual.mFormatFlags & kAudioFormatFlagIsFloat) == 0 ||
                (actual.mFormatFlags &
                    kAudioFormatFlagIsNonInterleaved) == 0 ||
                actual.mBitsPerChannel != 32 ||
                actual.mBytesPerFrame != sizeof(float)) {
                throw std::runtime_error("Unexpected HAL input format");
            }

            AURenderCallbackStruct callback{};
            callback.inputProc = &CoreAudioInputBackend::capture;
            callback.inputProcRefCon = this;
            check(AudioUnitSetProperty(
                unit_, kAudioOutputUnitProperty_SetInputCallback,
                kAudioUnitScope_Global, 0, &callback, sizeof(callback)
            ), "Cannot register HAL input callback");

            check(AudioUnitInitialize(unit_), "Cannot initialize HAL input");
            initialized_ = true;
        } catch (...) {
            AudioComponentInstanceDispose(unit_);
            unit_ = nullptr;
            throw;
        }
    }

    CoreAudioInputBackend(const CoreAudioInputBackend&) = delete;
    CoreAudioInputBackend& operator=(const CoreAudioInputBackend&) = delete;

    ~CoreAudioInputBackend() noexcept
    {
        if (running_) {
            AudioOutputUnitStop(unit_);
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
        std::uint64_t frames;
        std::uint64_t render_errors;
        std::uint64_t maximum_callback_gap_ns;
    };

    [[nodiscard]] Diagnostics diagnostics() const noexcept
    {
        return {
            callback_count_.load(std::memory_order_relaxed),
            captured_frames_.load(std::memory_order_relaxed),
            render_errors_.load(std::memory_order_relaxed),
            maximum_gap_ns_.load(std::memory_order_relaxed)
        };
    }

    void start()
    {
        if (!running_) {
            previous_host_time_ = 0;
            check(AudioOutputUnitStart(unit_), "Cannot start HAL input");
            running_ = true;
        }
    }

    void stop()
    {
        if (running_) {
            check(AudioOutputUnitStop(unit_), "Cannot stop HAL input");
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

    static OSStatus capture(
        void* context,
        AudioUnitRenderActionFlags* flags,
        const AudioTimeStamp* timestamp,
        UInt32,
        UInt32 frames,
        AudioBufferList*
    )
    {
        auto& self = *static_cast<CoreAudioInputBackend*>(context);
        self.callback_count_.fetch_add(1, std::memory_order_relaxed);
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
        if (frames == 0) {
            return noErr;
        }

        // Split unusually large callbacks without allocating. All calls
        // share the same AudioUnit input clock and capture timeline.
        UInt32 offset = 0;
        while (offset < frames) {
            const auto count = static_cast<UInt32>(
                std::min<FrameCount>(
                    frames - offset, self.buffer_.frames()
                )
            );
            AudioBufferList list{};
            list.mNumberBuffers = 1;
            list.mBuffers[0].mNumberChannels = 1;
            list.mBuffers[0].mDataByteSize = count * sizeof(Sample);
            list.mBuffers[0].mData = self.buffer_.channel(0);

            AudioTimeStamp at = *timestamp;
            if ((at.mFlags & kAudioTimeStampSampleTimeValid) != 0) {
                at.mSampleTime += offset;
            }
            const OSStatus status = AudioUnitRender(
                self.unit_, flags, &at, 1, count, &list
            );
            if (status != noErr) {
                self.render_errors_.fetch_add(
                    1, std::memory_order_relaxed
                );
                return status;
            }
            self.captured_frames_.fetch_add(
                count, std::memory_order_relaxed
            );
            self.sink_.process_input(
                ConstAudioBufferView(self.buffer_.view()), count
            );
            offset += count;
        }
        return noErr;
    }

    std::atomic<std::uint64_t> callback_count_{0};
    std::atomic<std::uint64_t> captured_frames_{0};
    std::atomic<std::uint64_t> render_errors_{0};
    std::atomic<std::uint64_t> maximum_gap_ns_{0};
    // Accessed only by the serial capture callback, except during start.
    std::uint64_t previous_host_time_ = 0;

    AudioInputSink& sink_;
    AudioBuffer buffer_;
    AudioDeviceID device_ = kAudioObjectUnknown;
    AudioComponentInstance unit_ = nullptr;
    bool initialized_ = false;
    bool running_ = false;
};

}  // namespace grafite::io

#endif  // __APPLE__
#endif  // GRAFITE_IO_COREAUDIO_INPUT_BACKEND_HPP
