cmake_minimum_required(VERSION 3.15)

project(NvGpuTempReader VERSION 1.0.0)

add_library(NvGpuTempReader INTERFACE)
add_library(bbmp::NvGpuTempReader ALIAS NvGpuTempReader)
target_sources(NvGpuTempReader INTERFACE Sources/NvGpuTempReader.cpp)
target_compile_features(NvGpuTempReader INTERFACE cxx_std_20)
target_include_directories(NvGpuTempReader INTERFACE Sources)

add_subdirectory(../nvapi ${CMAKE_CURRENT_BINARY_DIR}/nvapi)
target_link_libraries(NvGpuTempReader INTERFACE nvapi::nvapi)

option(BBMPU_BUILD_TESTS "Add test target" OFF)

if(("${CMAKE_SOURCE_DIR}" STREQUAL "${NvGpuTempReader_SOURCE_DIR}") OR BBMPU_BUILD_TESTS)
    add_executable(read_nvidia_gpu_temperature Test/ReadNvidiaGpuTemp.cpp)
    target_link_libraries(read_nvidia_gpu_temperature PRIVATE bbmp::NvGpuTempReader)
endif()
