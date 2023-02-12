cmake_minimum_required(VERSION 3.15)

project(BBMPU VERSION 1.0.0)

set(BBMPU_BUILD_TESTS ON CACHE BOOL "Build tests in all bbmp util subprojects" FORCE)

add_subdirectory(NvGpuTempReader ${CMAKE_CURRENT_BINARY_DIR}/NvGpuTempReader)
