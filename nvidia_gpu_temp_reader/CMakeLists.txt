cmake_minimum_required(VERSION 3.15)

project(nvidia_gpu_temp_reader VERSION 1.0.0)

add_library(nvidia_gpu_temp_reader INTERFACE)
add_library(bbmp::nvidia_gpu_temp_reader ALIAS nvidia_gpu_temp_reader)
target_sources(nvidia_gpu_temp_reader INTERFACE src/bbmp_NvGpuTempReader.cpp)
target_compile_features(nvidia_gpu_temp_reader INTERFACE cxx_std_20)
target_include_directories(nvidia_gpu_temp_reader INTERFACE src)

add_subdirectory(../nvapi ${CMAKE_CURRENT_BINARY_DIR}/nvapi)
target_link_libraries(nvidia_gpu_temp_reader INTERFACE nvapi::nvapi)

option(BBMPU_BUILD_TESTS "Add test target" OFF)

if(("${CMAKE_SOURCE_DIR}" STREQUAL "${nvidia_gpu_temp_reader_SOURCE_DIR}") OR BBMPU_BUILD_TESTS)
    add_executable(read_nvidia_gpu_temperature test/ReadNvidiaGpuTemperature.cpp)
    target_link_libraries(read_nvidia_gpu_temperature PRIVATE bbmp::nvidia_gpu_temp_reader)
endif()
