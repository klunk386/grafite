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

#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>

#include "grafite/io/wav_reader.hpp"
#include "grafite/io/wav_writer.hpp"

namespace {

class TemporaryPath {
public:
    TemporaryPath()
    {
        static unsigned counter = 0;
        path_ = std::filesystem::temp_directory_path() /
                ("grafite_wav_writer_" + std::to_string(++counter) + ".wav");
    }

    ~TemporaryPath()
    {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    [[nodiscard]]
    std::string string() const
    {
        return path_.string();
    }

private:
    std::filesystem::path path_;
};


void test_stereo_round_trip()
{
    TemporaryPath path;
    {
        grafite::io::WavWriter writer(path.string(), 2, 48000);
        assert(writer.channels() == 2);
        assert(writer.sample_rate() == 48000);

        grafite::AudioBuffer buffer(2, 4);
        buffer(0, 0) = -1.0F;
        buffer(1, 0) = 1.0F;
        buffer(0, 1) = 0.5F;
        buffer(1, 1) = -0.5F;
        buffer(0, 2) = 0.25F;
        buffer(1, 2) = -0.25F;
        writer.write_frames(buffer.view(), 2);
        writer.write_frames(buffer.view(), 1);

        assert(writer.frames_written() == 3);
        writer.close();
        writer.close();

        bool failed = false;
        try {
            writer.write_frames(buffer.view(), 1);
        } catch (const std::logic_error&) {
            failed = true;
        }
        assert(failed);
    }

    grafite::io::WavReader reader(path.string());
    assert(reader.channels() == 2);
    assert(reader.sample_rate() == 48000);
    assert(reader.total_frames() == 3);

    grafite::AudioBuffer result(2, 3);
    assert(reader.read_frames(result.view(), 3) == 3);
    assert(result(0, 0) == -1.0F);
    assert(result(1, 0) == 32767.0F / 32768.0F);
    assert(result(0, 1) == 0.5F);
    assert(result(1, 1) == -0.5F);
    assert(result(0, 2) == 0.25F);
    assert(result(1, 2) == -0.25F);
}


void test_clipping_and_non_finite_samples()
{
    TemporaryPath path;
    {
        grafite::io::WavWriter writer(path.string(), 1, 44100);
        grafite::AudioBuffer buffer(1, 5);
        buffer(0, 0) = 2.0F;
        buffer(0, 1) = -2.0F;
        buffer(0, 2) = std::numeric_limits<float>::quiet_NaN();
        buffer(0, 3) = std::numeric_limits<float>::infinity();
        buffer(0, 4) = -std::numeric_limits<float>::infinity();
        writer.write_frames(buffer.view(), 5);
        writer.close();
    }

    grafite::io::WavReader reader(path.string());
    grafite::AudioBuffer result(1, 5);
    assert(reader.read_frames(result.view(), 5) == 5);
    assert(result(0, 0) == 32767.0F / 32768.0F);
    assert(result(0, 1) == -1.0F);
    assert(result(0, 2) == 0.0F);
    assert(result(0, 3) == 0.0F);
    assert(result(0, 4) == 0.0F);
}


void test_empty_file_and_destructor_finalization()
{
    TemporaryPath path;
    {
        grafite::io::WavWriter writer(path.string(), 1, 48000);
        assert(writer.frames_written() == 0);
    }

    grafite::io::WavReader reader(path.string());
    assert(reader.total_frames() == 0);
    grafite::AudioBuffer buffer(1, 1);
    assert(reader.read_frames(buffer.view(), 1) == 0);
}


void test_invalid_configuration_and_source()
{
    TemporaryPath path;
    bool failed = false;
    try {
        grafite::io::WavWriter writer(path.string(), 0, 48000);
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    assert(failed);

    failed = false;
    try {
        grafite::io::WavWriter writer(path.string(), 2, 0);
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    assert(failed);

    failed = false;
    try {
        grafite::io::WavWriter writer(path.string(), 32768, 48000);
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    assert(failed);

    grafite::io::WavWriter writer(path.string(), 2, 48000);
    grafite::AudioBuffer mono(1, 4);

    failed = false;
    try {
        writer.write_frames(mono.view(), 4);
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    assert(failed);
    assert(writer.frames_written() == 0);
    writer.close();
}

}  // namespace


int main()
{
    test_stereo_round_trip();
    test_clipping_and_non_finite_samples();
    test_empty_file_and_destructor_finalization();
    test_invalid_configuration_and_source();

    return 0;
}
