<p align="center">
  <img src="logo/grafite-logo.png"
       alt="GRAFITE logo"
       width="220">
</p>

<h1 align="center">
  G R A F I T E
</h1>

<p align="center">
  <strong>General-purpose Real-time Audio Framework</strong>
</p>

<p align="center">
  A modular and extensible C++ engine for real-time and offline audio/DSP
  processing.
</p>

<p align="center">
  <a href="https://github.com/klunk386/grafite/actions/workflows/ci.yml">
    <img src="https://github.com/klunk386/grafite/actions/workflows/ci.yml/badge.svg?branch=develop"
         alt="C++ CI (develop)">
  </a>
  <a href="LICENSE">
    <img src="https://img.shields.io/badge/License-LGPL--3.0--or--later-blue.svg"
         alt="License: LGPL-3.0-or-later">
  </a>
</p>

---

## Overview

**GRAFITE** is a general-purpose audio processing engine designed as a
foundation for building modular, reusable, and interoperable audio
applications and DSP components.

Rather than targeting a single application type, GRAFITE provides a common
processing architecture that can support effects processors, synthesizers,
mixers, loopers, sequencers, analysis tools, and other audio systems. Simple
DSP units and complete processing subsystems can be composed using the same
graph model.

The engine is designed around a platform-independent C++ processing core,
with external audio I/O isolated behind dedicated backends. The portable core is tested on Linux and macOS; the first native real-time
backend is CoreAudio on macOS, with ALSA planned for Linux.

Real-time and offline processing share the same nodes, graphs, buffers, and
event model. Audio and discrete events are represented as separate streams
and synchronized on a common sample timeline.

> [!IMPORTANT]
> GRAFITE is an early-stage project. Graph execution, offline WAV processing,
> and experimental CoreAudio output are functional, but APIs are not stable.
> Some features described below remain design goals.

## Key features and design goals

- Modular graph-based DSP architecture built around **Node**, **Port**,
  **Graph**, and **Engine** abstractions.
- Hierarchical composition through **subgraphs and composite nodes**.
- Reusable components ranging from individual DSP operations to complete
  application subsystems.
- Common processing model for **real-time and offline** execution.
- **Sample-accurate event scheduling** within audio processing blocks.
- Separation of continuous audio streams from MIDI, automation, transport,
  and other discrete events.
- Platform-independent processing core with interchangeable audio I/O
  backends.
- Real-time-oriented execution model with a clear separation between control
  and audio processing domains.
- C++ DSP core with future Python bindings focused on composition,
  configuration, scripting, and control.
- Architecture intended to let independently developed GRAFITE components
  and applications interoperate inside larger processing graphs.

## Contents

- [Design philosophy](#design-philosophy)
- [Audio and event processing](#audio-and-event-processing)
- [Processing model](#processing-model)
- [Core components](#core-components)
- [Composite nodes and subgraphs](#composite-nodes-and-subgraphs)
- [Planned DSP library](#planned-dsp-library)
- [Python integration](#python-integration)
- [Repository structure](#repository-structure)
- [Current implementation](#current-implementation)
- [Getting started](#getting-started)
- [Build system](#build-system)
- [Development roadmap](#development-roadmap)
- [Non-goals of the core](#non-goals-of-the-core)
- [Contributing](#contributing)
- [License](#license)

## Design philosophy

GRAFITE is built around a small set of architectural principles.

### Modular by design

Audio processing is represented as a graph of independent processing
components. A component may be a simple DSP operation, such as a gain stage
or filter, or a more complex processing subsystem.

The fundamental abstractions are:

- **Node** — an individual processing unit;
- **Port** — an input or output endpoint of a node;
- **Graph** — the network describing nodes and their connections;
- **Engine** — the execution environment responsible for processing the graph.

The intention is to keep these abstractions sufficiently general that the
same engine can support effects processors, synthesizers, mixers, loopers,
sequencers, analysis tools, and other audio applications.

### Reusable and interoperable applications

GRAFITE is designed so that modularity does not stop at individual DSP
components.

A complete processing graph can be encapsulated as a composite node or
subgraph. This allows a complex component, or potentially an entire
application, to expose a simple set of ports and be reused inside a larger
graph.

Conceptually:

```text
Input -> [ Pedalboard ] -> [ Looper ] -> [ Mixer ] -> Output
              |                |             |
           subgraph          subgraph       subgraph
```

A pedalboard, synthesizer, looper, mixer, or other application can therefore
be designed both as a standalone program and as a component that can
interoperate with other GRAFITE-based systems.

### Platform-independent processing core

The DSP core must not depend on a particular operating system or audio API.

Platform-specific audio I/O is isolated from the processing core. The
`AudioProcessor` interface decouples offline WAV and native CoreAudio output
drivers from the `Engine`; `EngineAudioProcessor` adapts the Engine to that
interface. CoreAudio is the first experimental native real-time backend,
because hardware validation currently takes place on macOS. ALSA on Linux
is planned next, followed by other optional backends such as JACK, WASAPI,
ASIO, PipeWire, or network audio.

The portable core must not require platform-specific audio APIs.

### Real-time and offline processing

The same nodes and processing graphs are intended to operate both in
real-time and offline.

A DSP node should not need to know whether its input originates from an audio
device, a file, a generated buffer, or another source. Likewise, its output
may eventually be sent to hardware, stored in a file, passed to another
application, or inspected by a test environment.

Conceptually:

```text
                  +----------------+
                  |     Graph      |
                  +-------+--------+
                          |
                 +--------+--------+
                 |                 |
             Real-time           Offline
                 |                 |
           audio backend      file / buffer
```

This separation also makes it possible to test DSP components independently
of physical audio hardware.

### Real-time-safe architecture

The real-time processing path is designed around predictable execution.

Operations such as dynamic memory allocation, blocking synchronization, file
I/O, logging, and other potentially unbounded operations should not occur in
the real-time processing thread.

GRAFITE therefore distinguishes between two execution domains:

```text
Control / non-real-time             Audio / real-time
-----------------------             -----------------

create nodes                         process blocks
configure graph                     process audio
connect ports                       process events
allocate resources
load external data
prepare state
```

Configuration and structural changes are prepared outside the audio thread
and made available to the real-time engine through mechanisms that preserve
deterministic processing.

The exact synchronization and graph-update mechanisms will be developed as
the engine architecture matures.

## Audio and event processing

GRAFITE treats continuous audio data and discrete events as separate types of
streams.

Audio is processed in blocks, while events carry timing information relative
to those blocks. This permits sample-accurate event scheduling.

For example:

```text
frame      0                                      255
           |----------------------------------------|
audio      ==========================================
events          ^             ^                 ^
               31            142               231
```

Events may eventually represent:

- MIDI messages;
- parameter changes and automation;
- transport information;
- triggers;
- control messages;
- application-specific events.

Audio and event streams remain distinct but share the same processing clock
and timeline.

## Processing model

At the center of the processing API is a processing context associated with
each block.

A `ProcessContext` is expected to provide the information required by a node
to process the current block, including concepts such as:

- audio buffers;
- event buffers;
- number of frames;
- sample rate;
- absolute frame position;
- timing and transport state.

Nodes operate exclusively on this processing representation and do not
directly interact with hardware APIs.

This creates a processing chain conceptually similar to:

```text
Backend / source
      |
      v
+-------------+
|   Engine    |
+------+------+
       |
       v
+-------------+
|    Graph    |
+------+------+
       |
       +----> Node ----> Node ----> Node
                              |
                              +----> ...
```

## Core components

The initial architecture is organized around the following components.

### AudioBuffer

`AudioBuffer` represents blocks of audio samples used by the DSP engine.

Its design will account for channel organization, sample representation,
memory ownership and non-owning views, and the requirements of real-time
processing.

The internal DSP representation is intentionally separated from the sample
format used by external audio devices.

### Port

Ports represent node inputs and outputs.

GRAFITE will support at least two logical stream domains:

- audio ports;
- event ports.

Ports define how nodes are connected without exposing the implementation of
the underlying audio backend.

### Node

A `Node` is the fundamental processing unit.

Nodes may implement simple operations such as gain or filtering, generators
such as oscillators, routing operations such as mixing, event processing, or
complex subsystems.

Nodes are expected to follow a lifecycle broadly based on preparation,
processing, and reset operations.

### Graph

A `Graph` describes the processing topology.

It is responsible for representing nodes and their connections and for
providing the information required to determine a valid processing order.

Graph construction and modification belong to the non-real-time control
domain.

### Engine

The `Engine` coordinates execution of a graph.

Its responsibilities include processing blocks, coordinating timing and
scheduling, and interfacing the platform-independent processing model with
external I/O mechanisms.

The exact boundary between the engine, graph scheduler, and backend
implementations will be refined during development.

### Events

Events are timestamped relative to the audio processing timeline and can
occur at arbitrary sample offsets inside a processing block.

MIDI will be one supported event type rather than a special case embedded in
the audio architecture.

### Backends

Backends connect GRAFITE to external audio systems.

The backend layer is intentionally separated from the core so that different
platforms and execution environments can use the same processing graph.

Offline WAV processing and experimental CoreAudio stereo output on macOS
are implemented. ALSA and JACK are planned; CoreAudio input capture is not
yet implemented.

## Composite nodes and subgraphs

Hierarchical composition is a central design goal.

A graph should be able to expose selected inputs and outputs and behave as a
single node inside another graph:

```text
                 Composite Node
        +-----------------------------+
audio ->|  Node -> Node -> Node       |-> audio
event ->|      \-> Node ->/          |-> event
        +-----------------------------+
```

This enables reusable processing modules at multiple scales, from individual
effects to complete application subsystems.

Hierarchical graphs also provide a natural foundation for future
configuration, serialization, graphical editors, and application-level
composition.

## Planned DSP library

GRAFITE will include a collection of standard reusable DSP nodes. Initial
components will remain deliberately small while the core architecture is
validated.

Early nodes are expected to include basic components such as:

- gain;
- mixing;
- pass-through and routing utilities.

The DSP library can later grow into areas such as filtering, dynamics,
delays, modulation, synthesis, analysis, and other processing categories
without changing the fundamental graph model.

## Python integration

The real-time engine and DSP path are implemented in C++.

Python bindings are planned for higher-level tasks such as:

- graph construction;
- configuration;
- scripting;
- interactive experimentation;
- application control;
- offline workflows.

Python is not intended to execute sample-level DSP operations in the
real-time processing path.

A future workflow may therefore look conceptually like:

```text
Python
   |
   | configuration and control
   v
C++ Graph / Engine
   |
   | real-time processing
   v
C++ DSP
```

## Repository structure

The implementation is currently predominantly header-only:

```text
grafite/
├── .github/workflows/ci.yml
├── include/grafite/
│   ├── core/             # Buffers, nodes, graphs, engine
│   ├── event/            # Event interfaces
│   └── io/               # AudioProcessor, WAV and CoreAudio
├── tests/
│   ├── core/
│   ├── event/
│   └── io/
├── examples/             # WAV gain and CoreAudio programs
├── CMakeLists.txt
└── README.md
```

Platform-specific code is isolated from the portable processing core.

## Current implementation

Implemented and covered by deterministic software tests:

- Audio and event buffers, typed ports, and node lifecycle.
- Graph validation, compilation, topological scheduling and execution.
- External graph audio I/O and variable-size block processing.
- `Engine`, `AudioProcessor`, and `EngineAudioProcessor`.
- PCM16 WAV reader/writer, offline `FileAudioBackend`, and end-to-end gain
  processing.
- CMake builds and CTest on Ubuntu (GCC and Clang) and macOS (Apple Clang).

An experimental CoreAudio stereo output backend and 440 Hz oscillator
example have also been validated manually on macOS hardware.

Not yet implemented: CoreAudio capture, ALSA, JACK, multi-graph scheduling,
composite-node execution, Python bindings, and GUI/control APIs.

## Getting started

GRAFITE requires CMake 3.16 or later and a C++20 compiler.

```sh
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

### Offline WAV example

Process a PCM16 WAV with a gain of 0.5:

```sh
./build/grafite_wav_gain input.wav output.wav 0.5
```

The example uses `FileAudioBackend` and `EngineAudioProcessor`.

### CoreAudio examples (macOS only)

```sh
./build/grafite_coreaudio_probe
./build/grafite_coreaudio_sine
```

The probe checks AudioUnit availability. The sine example plays a quiet
440 Hz stereo tone for approximately three seconds. Lower speaker volume
before running it. Audio playback is not exercised by CI.

## Build system

CMake defines the portable `grafite_core` interface target and optional
tests and examples. Relevant configuration switches are:

- `GRAFITE_BUILD_TESTS` (default `ON`).
- `GRAFITE_BUILD_EXAMPLES` (default `ON`).
- `GRAFITE_BUILD_COREAUDIO` (default `ON`, effective only on macOS
  when examples are enabled).

The [C++ CI workflow](https://github.com/klunk386/grafite/actions/workflows/ci.yml)
runs for pushes to `develop` and pull requests targeting `develop` or
`main`. It compiles the core, tests, and examples on Ubuntu/GCC,
Ubuntu/Clang, and macOS/Apple Clang, then runs deterministic CTest
software tests. Hardware-dependent audio execution is excluded.

The badge at the top reports the combined workflow result on `develop`,
not physical audio-device compatibility or measured real-time latency.

## Development roadmap

Development proceeds in small, tested increments on `develop`.

1. Harden CoreAudio output: device format, callback behavior, lifecycle,
   and real-time error handling.
2. Add CoreAudio capture and validate live input → DSP → output.
3. Implement ALSA on Linux using the same backend-independent interface.
4. Develop multi-graph scheduling and execution domains, keeping logical
   graphs independent of physical audio threads.
5. Extend DSP, event/MIDI applications, control APIs, optional daemon
   orchestration, and embedded or remote user interfaces.

Composite nodes, larger DSP libraries, and Python bindings remain design
goals rather than completed features.

## Non-goals of the core

GRAFITE is not intended to hard-code the architecture of a particular audio
application.

The core should therefore not assume that it is running a pedalboard,
synthesizer, DAW, mixer, sequencer, or looper. These are applications and
compositions that can be built using the same underlying engine.

Likewise, platform-specific audio APIs, user interfaces, file formats, and
application-specific behavior should not leak into the fundamental DSP
abstractions unless they represent genuinely general concepts.

## Contributing

GRAFITE is at an early stage of development. Contribution guidelines, coding
conventions, issue templates, and pull-request procedures will be documented
as the implementation matures.

Architectural changes should preserve the central goals of modularity,
real-time safety, portability, reuse, and interoperability.

## License

GRAFITE is free software distributed under the **GNU Lesser General Public
License, version 3 or later**.

SPDX-License-Identifier: `LGPL-3.0-or-later`

See the `LICENSE` file for the complete license terms.

Copyright (c) 2026 GRAFITE contributors.
