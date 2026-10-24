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

#include "wav_gain_processor.hpp"

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

class TemporaryPath {
public:
    explicit TemporaryPath(const std::string& name)
    {
        path_ = std::filesystem::temp_directory_path() /
                ("grafite_wav_gain_" + name + ".wav");
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

void write_fixture(
    const std::string& path,
    std::uint32_t frames,
    std::uint16_t channels
)
{
    const auto data_bytes = frames * channels * 2;
    std::vector<unsigned char> bytes;
    append_tag(bytes, "RIFF");
    append_u32(bytes, 36 + data_bytes);
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
    append_u32(bytes, data_bytes);

    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        for (std::uint16_t channel = 0; channel < channels; ++channel) {
            const std::int16_t sample =
                (frame % 2 == 0) ? 16384 : -16384;
            append_u16(bytes, static_cast<std::uint16_t>(sample));
        }
    }

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(
        reinterpret_cast<const char*>(bytes.data()), bytes.size()
    );
    assert(stream.good());
}

void test_variable_blocks_and_gain()
{
    TemporaryPath input("input");
    TemporaryPath output("output");
    write_fixture(input.string(), 515, 2);

    grafite::examples::process_wav_gain(
        input.string(), output.string(), 0.5F
    );

    grafite::io::WavReader reader(output.string());
    assert(reader.channels() == 2);
    assert(reader.sample_rate() == 48000);
    assert(reader.total_frames() == 515);

    grafite::AudioBuffer buffer(2, 256);
    std::uint64_t position = 0;

    while (true) {
        const auto count = reader.read_frames(buffer.view(), 256);
        if (count == 0) {
            break;
        }

        for (grafite::FrameCount frame = 0; frame < count; ++frame) {
            const auto expected =
                ((position + frame) % 2 == 0) ? 0.25F : -0.25F;
            for (grafite::ChannelCount ch = 0; ch < 2; ++ch) {
                assert(buffer(ch, frame) == expected);
            }
        }
        position += count;
    }

    assert(position == 515);
}

void test_empty_file()
{
    TemporaryPath input("empty_input");
    TemporaryPath output("empty_output");
    write_fixture(input.string(), 0, 1);

    grafite::examples::process_wav_gain(
        input.string(), output.string(), 0.5F
    );

    grafite::io::WavReader reader(output.string());
    assert(reader.channels() == 1);
    assert(reader.total_frames() == 0);
}

void test_reject_same_file()
{
    TemporaryPath input("same");
    write_fixture(input.string(), 1, 1);

    bool failed = false;
    try {
        grafite::examples::process_wav_gain(
            input.string(), input.string(), 0.5F
        );
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    assert(failed);

    grafite::io::WavReader reader(input.string());
    assert(reader.total_frames() == 1);
}

}  // namespace

int main()
{
    test_variable_blocks_and_gain();
    test_empty_file();
    test_reject_same_file();
    return 0;
}
