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
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "grafite/io/wav_reader.hpp"

namespace {

void append_u16(std::vector<unsigned char>& bytes, std::uint16_t value)
{
    bytes.push_back(static_cast<unsigned char>(value & 0xFF));
    bytes.push_back(static_cast<unsigned char>(value >> 8));
}

void append_u32(std::vector<unsigned char>& bytes, std::uint32_t value)
{
    append_u16(bytes, static_cast<std::uint16_t>(value));
    append_u16(bytes, static_cast<std::uint16_t>(value >> 16));
}

void append_tag(std::vector<unsigned char>& bytes, const char* tag)
{
    for (int i = 0; i < 4; ++i) {
        bytes.push_back(static_cast<unsigned char>(tag[i]));
    }
}

std::vector<unsigned char> make_wav(
    const std::vector<std::int16_t>& samples,
    std::uint16_t channels = 2
)
{
    std::vector<unsigned char> bytes;
    append_tag(bytes, "RIFF");
    append_u32(bytes, static_cast<std::uint32_t>(
        36 + samples.size() * 2
    ));
    append_tag(bytes, "WAVE");
    append_tag(bytes, "fmt ");
    append_u32(bytes, 16);
    append_u16(bytes, 1);
    append_u16(bytes, channels);
    append_u32(bytes, 48000);
    append_u32(bytes, 48000 * channels * 2);
    append_u16(bytes, channels * 2);
    append_u16(bytes, 16);
    append_tag(bytes, "data");
    append_u32(bytes, static_cast<std::uint32_t>(samples.size() * 2));

    for (const auto sample : samples) {
        append_u16(bytes, static_cast<std::uint16_t>(sample));
    }

    return bytes;
}

class TemporaryFile {
public:
    explicit TemporaryFile(const std::vector<unsigned char>& bytes)
    {
        static unsigned counter = 0;
        path_ = std::filesystem::temp_directory_path() /
                ("grafite_wav_reader_" + std::to_string(++counter) + ".wav");

        std::ofstream stream(path_, std::ios::binary | std::ios::trunc);
        if (!stream.write(
                reinterpret_cast<const char*>(bytes.data()), bytes.size()
            )) {
            throw std::runtime_error("Cannot create WAV test fixture");
        }
    }

    ~TemporaryFile()
    {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    [[nodiscard]]
    std::string path() const
    {
        return path_.string();
    }

private:
    std::filesystem::path path_;
};

template<typename Function>
void expect_failure(Function&& function)
{
    bool failed = false;
    try {
        function();
    } catch (const std::runtime_error&) {
        failed = true;
    }
    assert(failed);
}

void test_stereo_blocks_and_end_of_file()
{
    TemporaryFile file(make_wav({
        0, 32767, -32768, 16384, 8192, -8192
    }));
    grafite::io::WavReader reader(file.path());

    assert(reader.channels() == 2);
    assert(reader.sample_rate() == 48000);
    assert(reader.total_frames() == 3);
    assert(reader.frame_position() == 0);

    grafite::AudioBuffer buffer(2, 4);
    buffer(0, 3) = 0.75F;
    auto view = buffer.view();

    assert(reader.read_frames(view, 2) == 2);
    assert(view(0, 0) == 0.0F);
    assert(view(1, 0) == 32767.0F / 32768.0F);
    assert(view(0, 1) == -1.0F);
    assert(view(1, 1) == 0.5F);
    assert(reader.frame_position() == 2);

    assert(reader.read_frames(view, 4) == 1);
    assert(view(0, 0) == 0.25F);
    assert(view(1, 0) == -0.25F);
    assert(view(0, 3) == 0.75F);
    assert(reader.read_frames(view, 4) == 0);
    assert(reader.frame_position() == 3);
}

void test_zero_frames_and_invalid_view()
{
    TemporaryFile file(make_wav({1000, -1000}));
    grafite::io::WavReader reader(file.path());
    grafite::AudioBuffer buffer(2, 1);

    assert(reader.read_frames(buffer.view(), 0) == 0);
    assert(reader.frame_position() == 0);

    bool failed = false;
    try {
        grafite::AudioBuffer mono(1, 1);
        reader.read_frames(mono.view(), 1);
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    assert(failed);
    assert(reader.frame_position() == 0);
}

void test_invalid_files()
{
    auto invalid = make_wav({0, 0});

    invalid[0] = 'X';
    TemporaryFile wrong_riff(invalid);
    expect_failure([&] {
        grafite::io::WavReader reader(wrong_riff.path());
    });

    invalid = make_wav({0, 0});
    invalid[20] = 3;
    TemporaryFile unsupported(invalid);
    expect_failure([&] {
        grafite::io::WavReader reader(unsupported.path());
    });

    invalid = make_wav({0, 0});
    invalid.pop_back();
    TemporaryFile truncated(invalid);
    expect_failure([&] {
        grafite::io::WavReader reader(truncated.path());
    });

    invalid = make_wav({0, 0});
    invalid[40] = 3;
    TemporaryFile misaligned(invalid);
    expect_failure([&] {
        grafite::io::WavReader reader(misaligned.path());
    });
}

}  // namespace


int main()
{
    test_stereo_blocks_and_end_of_file();
    test_zero_frames_and_invalid_view();
    test_invalid_files();

    return 0;
}
