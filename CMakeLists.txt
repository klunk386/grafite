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

endif()