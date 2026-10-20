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

#include "grafite/core/port.hpp"

namespace {

void test_default_port()
{
    constexpr grafite::Port port;

    static_assert(port.id() == grafite::invalid_port_id);
    static_assert(port.is_input());
    static_assert(port.is_audio());
    static_assert(port.channels() == 0);
}


void test_audio_input()
{
    constexpr auto port = grafite::Port::audio(
        1,
        grafite::PortDirection::input,
        2
    );

    static_assert(port.id() == 1);
    static_assert(
        port.direction() == grafite::PortDirection::input
    );
    static_assert(port.type() == grafite::PortType::audio);
    static_assert(port.channels() == 2);

    static_assert(port.is_input());
    static_assert(!port.is_output());

    static_assert(port.is_audio());
    static_assert(!port.is_event());
}


void test_audio_output()
{
    constexpr auto port = grafite::Port::audio(
        2,
        grafite::PortDirection::output,
        6
    );

    static_assert(port.id() == 2);
    static_assert(
        port.direction() == grafite::PortDirection::output
    );
    static_assert(port.type() == grafite::PortType::audio);
    static_assert(port.channels() == 6);

    static_assert(!port.is_input());
    static_assert(port.is_output());

    static_assert(port.is_audio());
    static_assert(!port.is_event());
}


void test_event_input()
{
    constexpr auto port = grafite::Port::event(
        3,
        grafite::PortDirection::input
    );

    static_assert(port.id() == 3);
    static_assert(
        port.direction() == grafite::PortDirection::input
    );
    static_assert(port.type() == grafite::PortType::event);
    static_assert(port.channels() == 0);

    static_assert(port.is_input());
    static_assert(!port.is_output());

    static_assert(!port.is_audio());
    static_assert(port.is_event());
}


void test_event_output()
{
    constexpr auto port = grafite::Port::event(
        4,
        grafite::PortDirection::output
    );

    static_assert(port.id() == 4);
    static_assert(
        port.direction() == grafite::PortDirection::output
    );
    static_assert(port.type() == grafite::PortType::event);
    static_assert(port.channels() == 0);

    static_assert(!port.is_input());
    static_assert(port.is_output());

    static_assert(!port.is_audio());
    static_assert(port.is_event());
}


void test_runtime_values()
{
    const auto port = grafite::Port::audio(
        42,
        grafite::PortDirection::output,
        2
    );

    assert(port.id() == 42);
    assert(port.direction() == grafite::PortDirection::output);
    assert(port.type() == grafite::PortType::audio);
    assert(port.channels() == 2);

    assert(port.is_output());
    assert(port.is_audio());
}

}  // namespace


int main()
{
    test_default_port();

    test_audio_input();
    test_audio_output();

    test_event_input();
    test_event_output();

    test_runtime_values();

    return 0;
}