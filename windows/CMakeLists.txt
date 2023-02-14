cmake_minimum_required(VERSION 3.15)

project(windows VERSION 1.0.0)

add_library(windows INTERFACE)
add_library(bbmp::windows ALIAS windows)
target_sources(windows INTERFACE src/bbmp_ChildProcess.cpp)
target_include_directories(windows INTERFACE src)
target_compile_features(windows INTERFACE cxx_std_20)

option(BBMPU_BUILD_TESTS "Add test target" OFF)

if(("${CMAKE_SOURCE_DIR}" STREQUAL "${nvidia_gpu_temp_reader_SOURCE_DIR}") OR BBMPU_BUILD_TESTS)
    add_executable(test_child_process test/TestChildProcess.cpp)

    add_executable(child_process_test test/ChildProcessTest.cpp)
    target_link_libraries(child_process_test PRIVATE bbmp::windows)
    add_dependencies(child_process_test test_child_process)
endif()
