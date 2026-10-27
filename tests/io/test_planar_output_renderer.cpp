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

#include <array>
#include <cassert>
#include <cstddef>

#include "grafite/core/audio_buffer.hpp"
#include "grafite/io/planar_output_renderer.hpp"

namespace {

class MultichannelProcessor final : public grafite::io::AudioProcessor {
public:
    void process(
        std::span<const grafite::ConstAudioBufferView>,
        std::span<grafite::AudioBufferView> outputs,
        grafite::FrameCount frames
    ) noexcept override
    {
        assert(outputs.size() == 1);
        assert(outputs[0].channels() == 3);
        for (grafite::ChannelCount ch = 0; ch < 3; ++ch) {
            for (grafite::FrameCount i = 0; i < frames; ++i) {
                outputs[0](ch, i) = static_cast<float>(
                    100 * ch + position + i
                );
            }
        }
        position += frames;
    }
    grafite::FrameCount position = 0;
};

class TestProcessor final : public grafite::io::AudioProcessor {
public:
    void process(
        std::span<const grafite::ConstAudioBufferView> inputs,
        std::span<grafite::AudioBufferView> outputs,
        grafite::FrameCount frames
    ) noexcept override
    {
        assert(inputs.empty());
        assert(outputs.size() == 1);
        assert(outputs[0].channels() == 2);
        assert(frames <= 3);
        ++calls;
        for (grafite::FrameCount i = 0; i < frames; ++i) {
            outputs[0](0, i) = static_cast<float>(position + i);
            outputs[0](1, i) = -static_cast<float>(position + i);
        }
        position += frames;
    }

    std::size_t calls = 0;
    std::size_t position = 0;
};

}  // namespace

int main()
{
    TestProcessor processor;
    grafite::AudioBuffer scratch(2, 3);
    grafite::io::PlanarOutputRenderer renderer(processor, scratch);

    std::array<float, 9> left{};
    std::array<float, 9> right{};
    assert(renderer.render(left.data(), right.data(), 8));
    assert(processor.calls == 3);
    assert(processor.position == 8);
    for (std::size_t i = 0; i < 8; ++i) {
        assert(left[i] == static_cast<float>(i));
        assert(right[i] == -static_cast<float>(i));
    }
    assert(left[8] == 0.0F);
    assert(right[8] == 0.0F);

    assert(renderer.render(left.data(), right.data(), 0));
    assert(processor.calls == 3);
    assert(!renderer.render(nullptr, right.data(), 3));
    assert(!renderer.render(left.data(), left.data(), 3));
    assert(processor.calls == 3);

    grafite::AudioBuffer invalid_scratch(1, 3);
    grafite::io::PlanarOutputRenderer invalid(
        processor, invalid_scratch
    );
    assert(!invalid.render(left.data(), right.data(), 3));
    assert(processor.calls == 3);

    MultichannelProcessor multichannel;
    grafite::AudioBuffer multi_scratch(3, 2);
    grafite::io::PlanarOutputRenderer multi_renderer(
        multichannel, multi_scratch
    );
    std::array<float, 5> a{};
    std::array<float, 5> b{};
    std::array<float, 5> c{};
    std::array<float*, 3> channels{a.data(), b.data(), c.data()};
    assert(multi_renderer.render(channels, 5));
    assert(multichannel.position == 5);
    for (std::size_t i = 0; i < 5; ++i) {
        assert(a[i] == static_cast<float>(i));
        assert(b[i] == static_cast<float>(100 + i));
        assert(c[i] == static_cast<float>(200 + i));
    }
    std::array<float*, 2> wrong_count{a.data(), b.data()};
    assert(!multi_renderer.render(wrong_count, 5));
    std::array<float*, 3> duplicate{a.data(), b.data(), a.data()};
    assert(!multi_renderer.render(duplicate, 5));
    assert(multichannel.position == 5);
}
