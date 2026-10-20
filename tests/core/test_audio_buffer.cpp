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

#include "grafite/core/audio_buffer.hpp"

namespace {

void test_construction()
{
    grafite::AudioBuffer buffer(2, 256);

    assert(buffer.channels() == 2);
    assert(buffer.frames() == 256);
    assert(!buffer.empty());
}

void test_sample_access()
{
    grafite::AudioBuffer buffer(2, 256);

    buffer(0, 42) = 0.5f;
    buffer(1, 100) = -0.25f;

    assert(buffer(0, 42) == 0.5f);
    assert(buffer(1, 100) == -0.25f);
}

void test_channel_access()
{
    grafite::AudioBuffer buffer(2, 256);

    auto* left = buffer.channel(0);
    auto* right = buffer.channel(1);

    left[10] = 0.25f;
    right[10] = -0.25f;

    assert(buffer(0, 10) == 0.25f);
    assert(buffer(1, 10) == -0.25f);
}

void test_mutable_view()
{
    grafite::AudioBuffer buffer(2, 256);
    auto view = buffer.view();

    assert(view.channels() == 2);
    assert(view.frames() == 256);

    view(0, 42) = 0.75f;

    assert(buffer(0, 42) == 0.75f);
}

void test_const_view()
{
    grafite::AudioBuffer buffer(2, 256);
    buffer(1, 42) = 0.75f;

    const auto& const_buffer = buffer;
    auto view = const_buffer.view();

    assert(view.channels() == 2);
    assert(view.frames() == 256);
    assert(view(1, 42) == 0.75f);
}

void test_clear()
{
    grafite::AudioBuffer buffer(2, 256);

    buffer(0, 10) = 1.0f;
    buffer(1, 20) = -1.0f;

    buffer.clear();

    assert(buffer(0, 10) == 0.0f);
    assert(buffer(1, 20) == 0.0f);
}

void test_view_clear()
{
    grafite::AudioBuffer buffer(2, 256);

    buffer(0, 10) = 1.0f;
    buffer(1, 20) = -1.0f;

    auto view = buffer.view();
    view.clear();

    assert(buffer(0, 10) == 0.0f);
    assert(buffer(1, 20) == 0.0f);
}

void test_resize()
{
    grafite::AudioBuffer buffer(2, 256);

    buffer(0, 10) = 1.0f;

    buffer.resize(4, 512);

    assert(buffer.channels() == 4);
    assert(buffer.frames() == 512);
    assert(buffer(0, 10) == 0.0f);
}

void test_empty_buffer()
{
    grafite::AudioBuffer buffer;

    assert(buffer.channels() == 0);
    assert(buffer.frames() == 0);
    assert(buffer.empty());
}

}  // namespace

int main()
{
    test_construction();
    test_sample_access();
    test_channel_access();
    test_mutable_view();
    test_const_view();
    test_clear();
    test_view_clear();
    test_resize();
    test_empty_buffer();

    return 0;
}