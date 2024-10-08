cmake_minimum_required(VERSION 3.15)

project(Northstar CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

set(NORTHSTAR_MAJOR 0)
set(NORTHSTAR_MINOR 0)

# Language specs
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VS_PLATFORM_TOOLSET v143)

# This determines the real binary root directory
set(NS_BINARY_DIR ${CMAKE_BINARY_DIR}/game)
# NOTE [Fifty]: Visual studio deems Build root with the value "${projectDir}"
# in CMakeSettings.json as invalid and defaults to using a temporary dir
# somewhere in %USER%/CMakeBuilds. To combat this we set it to "${projectDir}/build"
# and then link binaries in ${CMAKE_BINARY_DIR}/game. This means you can copy your
# game into ${CMAKE_BINARY_DIR}/game without it being cluttered up by cmake files.

message(STATUS "NS: Building to ${NS_BINARY_DIR}")


list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/primedev/cmake")
include(utils)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CURL_STATIC_CRT ON)

include_directories(primedev)
include_directories(primedev/thirdparty)

# Version
add_compile_definitions(NORTHSTAR_MAJOR=0)
add_compile_definitions(NORTHSTAR_MINOR=0)
add_compile_definitions(NORTHSTAR_VERSION="${NORTHSTAR_MAJOR}.${NORTHSTAR_MINOR}-dev")

# Targets
add_subdirectory(primedev)
