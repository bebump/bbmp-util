cmake_minimum_required(VERSION 3.15)

project(BBMPU VERSION 1.0.0)

set(BBMPU_BUILD_TESTS ON CACHE BOOL "Build tests in all bbmp util subprojects" FORCE)

add_subdirectory(nvidia_gpu_temp_reader ${CMAKE_CURRENT_BINARY_DIR}/nvidia_gpu_temp_reader)
add_subdirectory(windows ${CMAKE_CURRENT_BINARY_DIR}/windows)
