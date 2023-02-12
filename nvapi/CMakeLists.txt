cmake_minimum_required(VERSION 3.15)

include(FetchContent)

project(nvapi VERSION 5.2.0)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(BUILD_64 1)
    set(BUILD_32 0)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(BUILD_32 1)
    set(BUILD_64 0)
endif()

add_library(nvapi INTERFACE)
add_library(nvapi::nvapi ALIAS nvapi)

target_include_directories(nvapi INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)

if (BUILD_64)
    target_link_libraries(nvapi INTERFACE ${CMAKE_CURRENT_LIST_DIR}/lib/amd64/nvapi64.lib)
elseif(BUILD_32)
    target_link_libraries(nvapi INTERFACE ${CMAKE_CURRENT_LIST_DIR}/lib/x86/nvapi.lib)
endif()
