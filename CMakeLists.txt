cmake_minimum_required(VERSION 3.16)

project(
    grafite
    VERSION 0.1.0
    DESCRIPTION "General-purpose Real-time Audio Framework"
    LANGUAGES CXX
)

# -----------------------------------------------------------------------------
# Core library
# -----------------------------------------------------------------------------

add_library(grafite_core INTERFACE)

target_include_directories(
    grafite_core
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(
    grafite_core
    INTERFACE
        cxx_std_20
)


# -----------------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------------

option(GRAFITE_BUILD_TESTS "Build GRAFITE tests" ON)

if(GRAFITE_BUILD_TESTS)
    enable_testing()

    add_executable(
        test_audio_buffer
        tests/core/test_audio_buffer.cpp
    )

    target_link_libraries(
        test_audio_buffer
        PRIVATE
            grafite_core
    )

    add_test(
        NAME audio_buffer
        COMMAND test_audio_buffer
    )

    add_executable(
        test_port
        tests/core/test_port.cpp
    )

    target_link_libraries(
        test_port
        PRIVATE
            grafite_core
    )

    add_test(
        NAME port
        COMMAND test_port
    )

    add_executable(
        test_event_buffer
        tests/event/test_event_buffer.cpp
    )

    target_link_libraries(
        test_event_buffer
        PRIVATE
            grafite_core
    )

    add_test(
        NAME event_buffer
        COMMAND test_event_buffer
    )

    add_executable(
        test_process_context
        tests/core/test_process_context.cpp
    )

    target_link_libraries(
        test_process_context
        PRIVATE
            grafite_core
    )

    add_test(
        NAME process_context
        COMMAND test_process_context
    )

    add_executable(
        test_node
        tests/core/test_node.cpp
    )

    target_link_libraries(
        test_node
        PRIVATE
            grafite_core
    )

    add_test(
        NAME node
        COMMAND test_node
    )

    add_executable(
        test_graph
        tests/core/test_graph.cpp
    )

    target_link_libraries(
        test_graph
        PRIVATE
            grafite_core
    )

    add_test(
        NAME graph
        COMMAND test_graph
    )

    add_executable(
        test_compiled_graph
        tests/core/test_compiled_graph.cpp
    )

    target_link_libraries(
        test_compiled_graph
        PRIVATE
            grafite_core
    )

    add_test(
        NAME compiled_graph
        COMMAND test_compiled_graph
    )

    add_executable(
        test_engine
        tests/core/test_engine.cpp
    )

    target_link_libraries(
        test_engine
        PRIVATE
            grafite_core
    )

    add_test(
        NAME engine
        COMMAND test_engine
    )

    add_executable(
        test_graph_io
        tests/core/test_graph_io.cpp
    )

    target_link_libraries(
        test_graph_io
        PRIVATE
            grafite_core
    )

    add_test(
        NAME graph_io
        COMMAND test_graph_io
    )

    add_executable(
        test_wav_reader
        tests/io/test_wav_reader.cpp
    )

    target_link_libraries(
        test_wav_reader
        PRIVATE
            grafite_core
    )

    add_test(
        NAME wav_reader
        COMMAND test_wav_reader
    )

    add_executable(
        test_wav_writer
        tests/io/test_wav_writer.cpp
    )

    target_link_libraries(
        test_wav_writer
        PRIVATE
            grafite_core
    )

    add_test(
        NAME wav_writer
        COMMAND test_wav_writer
    )

    add_executable(
        test_engine_audio_processor
        tests/io/test_engine_audio_processor.cpp
    )

    target_link_libraries(
        test_engine_audio_processor
        PRIVATE
            grafite_core
    )

    add_test(
        NAME engine_audio_processor
        COMMAND test_engine_audio_processor
    )

    add_executable(
        test_planar_output_renderer
        tests/io/test_planar_output_renderer.cpp
    )

    target_link_libraries(
        test_planar_output_renderer
        PRIVATE
            grafite_core
    )

    add_test(
        NAME planar_output_renderer
        COMMAND test_planar_output_renderer
    )

    find_package(Threads REQUIRED)

    add_executable(
        test_audio_ring_buffer
        tests/io/test_audio_ring_buffer.cpp
    )

    target_link_libraries(
        test_audio_ring_buffer
        PRIVATE
            grafite_core
            Threads::Threads
    )

    add_test(
        NAME audio_ring_buffer
        COMMAND test_audio_ring_buffer
    )

    add_executable(
        test_linear_resampler
        tests/io/test_linear_resampler.cpp
    )

    target_link_libraries(
        test_linear_resampler
        PRIVATE
            grafite_core
    )

    add_test(
        NAME linear_resampler
        COMMAND test_linear_resampler
    )

    add_executable(
        test_multichannel_linear_resampler
        tests/io/test_multichannel_linear_resampler.cpp
    )

    target_link_libraries(
        test_multichannel_linear_resampler
        PRIVATE
            grafite_core
    )

    add_test(
        NAME multichannel_linear_resampler
        COMMAND test_multichannel_linear_resampler
    )

    add_executable(
        test_fifo_drift_controller
        tests/io/test_fifo_drift_controller.cpp
    )

    target_link_libraries(
        test_fifo_drift_controller
        PRIVATE
            grafite_core
    )

    add_test(
        NAME fifo_drift_controller
        COMMAND test_fifo_drift_controller
    )

    add_executable(
        test_fifo_drift_simulation
        tests/io/test_fifo_drift_simulation.cpp
    )

    target_link_libraries(
        test_fifo_drift_simulation
        PRIVATE
            grafite_core
    )

    add_test(
        NAME fifo_drift_simulation
        COMMAND test_fifo_drift_simulation
    )

    add_executable(
        test_fifo_drift_end_to_end
        tests/io/test_fifo_drift_end_to_end.cpp
    )

    target_link_libraries(
        test_fifo_drift_end_to_end
        PRIVATE
            grafite_core
    )

    add_test(
        NAME fifo_drift_end_to_end
        COMMAND test_fifo_drift_end_to_end
    )

    add_executable(
        test_channel_routing
        tests/io/test_channel_routing.cpp
    )

    target_link_libraries(
        test_channel_routing
        PRIVATE
            grafite_core
    )

    add_test(
        NAME channel_routing
        COMMAND test_channel_routing
    )

    add_executable(
        test_ring_buffer_playback_processor
        tests/io/test_ring_buffer_playback_processor.cpp
    )

    target_link_libraries(
        test_ring_buffer_playback_processor
        PRIVATE
            grafite_core
    )

    add_test(
        NAME ring_buffer_playback_processor
        COMMAND test_ring_buffer_playback_processor
    )

    add_executable(
        test_ring_buffer_engine_processor
        tests/io/test_ring_buffer_engine_processor.cpp
    )

    target_include_directories(
        test_ring_buffer_engine_processor
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/examples
    )

    target_link_libraries(
        test_ring_buffer_engine_processor
        PRIVATE
            grafite_core
    )

    add_test(
        NAME ring_buffer_engine_processor
        COMMAND test_ring_buffer_engine_processor
    )

    add_executable(
        test_wav_gain
        tests/io/test_wav_gain.cpp
    )

    target_include_directories(
        test_wav_gain
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/examples
    )

    target_link_libraries(
        test_wav_gain
        PRIVATE
            grafite_core
    )

    add_test(
        NAME wav_gain
        COMMAND test_wav_gain
    )


endif()

# -----------------------------------------------------------------------------
# Offline audio example
# -----------------------------------------------------------------------------

option(GRAFITE_BUILD_EXAMPLES "Build GRAFITE examples" ON)

if(GRAFITE_BUILD_EXAMPLES)
    add_executable(
        grafite_wav_gain
        examples/wav_gain.cpp
    )

    target_link_libraries(
        grafite_wav_gain
        PRIVATE
            grafite_core
    )
endif()

# -----------------------------------------------------------------------------
# Optional native CoreAudio probe (macOS only)
# -----------------------------------------------------------------------------

option(GRAFITE_BUILD_COREAUDIO "Build native CoreAudio components" ON)

if(APPLE AND GRAFITE_BUILD_COREAUDIO AND GRAFITE_BUILD_EXAMPLES)
    add_executable(
        grafite_coreaudio_probe
        examples/coreaudio_probe.cpp
    )

    add_executable(
        grafite_coreaudio_sine
        examples/coreaudio_sine.cpp
    )

    add_executable(
        grafite_coreaudio_devices
        examples/coreaudio_devices.cpp
    )

    add_executable(
        grafite_coreaudio_capture
        examples/coreaudio_capture.cpp
    )

    add_executable(
        grafite_coreaudio_loopback
        examples/coreaudio_loopback.cpp
    )

    add_executable(
        grafite_coreaudio_engine_loopback
        examples/coreaudio_engine_loopback.cpp
    )

    target_include_directories(
        grafite_coreaudio_engine_loopback
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/examples
    )

    target_link_libraries(
        grafite_coreaudio_engine_loopback
        PRIVATE
            grafite_core
            "-framework AudioToolbox"
            "-framework CoreAudio"
    )

    target_link_libraries(
        grafite_coreaudio_loopback
        PRIVATE
            grafite_core
            "-framework AudioToolbox"
            "-framework CoreAudio"
    )

    target_link_libraries(
        grafite_coreaudio_capture
        PRIVATE
            grafite_core
            "-framework AudioToolbox"
            "-framework CoreAudio"
    )

    target_link_libraries(
        grafite_coreaudio_devices
        PRIVATE
            grafite_core
            "-framework CoreAudio"
    )

    target_link_libraries(
        grafite_coreaudio_sine
        PRIVATE
            grafite_core
            "-framework AudioToolbox"
            "-framework CoreAudio"
    )

    target_link_libraries(
        grafite_coreaudio_probe
        PRIVATE
            grafite_core
            "-framework AudioToolbox"
            "-framework CoreAudio"
    )
endif()
