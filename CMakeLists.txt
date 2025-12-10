cmake_minimum_required(VERSION 3.14)
project(DeepXOCR VERSION 0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set default build type to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (default: Release)" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/release" CACHE PATH "Default install path" FORCE)
endif()

# Log level configuration
# Options: LOG_LEVEL_TRACE, LOG_LEVEL_DEBUG, LOG_LEVEL_INFO, LOG_LEVEL_WARN, LOG_LEVEL_ERROR, LOG_LEVEL_OFF
# Usage: cmake -DLOG_LEVEL=LOG_LEVEL_WARN ..
if(DEFINED LOG_LEVEL)
    message(STATUS "Log level set to: ${LOG_LEVEL}")
    add_compile_definitions(LOG_LEVEL=${LOG_LEVEL})
else()
    # Default: Use logger.hpp's automatic detection (DEBUG in Debug mode, INFO in Release)
    message(STATUS "Log level: Auto (DEBUG in Debug build, INFO in Release build)")
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

include(dx_func)

add_definitions(-DPROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}")
add_compile_options(-W -Wall -pthread -fPIC)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options(-fstrict-volatile-bitfields)
endif()

# ========================================
# Debug/Performance Profiling Configuration
# ========================================
if(CMAKE_BUILD_TYPE MATCHES "Debug")
    message(STATUS "Debug mode: Enabling debug symbols and profiling support")
    add_compile_options(-g3 -ggdb -rdynamic -fno-omit-frame-pointer)
    add_compile_options(-O1)
elseif(ENABLE_DEBUG_INFO)
    message(STATUS "Debug info enabled for profiling")
    add_compile_options(-g -rdynamic -fno-omit-frame-pointer)
endif()

# ========================================
# OpenCV Configuration
# ========================================
include(cmake/BuildOpenCV.cmake)

# Add 3rd-party libraries
add_subdirectory(3rd-party/json)

# Suppress warnings from 3rd-party libraries
set(CMAKE_CXX_FLAGS_BACKUP "${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")  # Disable all warnings
add_subdirectory(3rd-party/clipper2/CPP)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_BACKUP}")

# Global include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/3rd-party/json/include)
include_directories(${CMAKE_SOURCE_DIR}/3rd-party/spdlog/include)

# Use SYSTEM to suppress warnings from 3rd-party libraries
include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/3rd-party/clipper2/CPP/Clipper2Lib/include)

# Add subdirectories
add_subdirectory(src/common)
add_subdirectory(src/preprocessing)
add_subdirectory(src/detection)
add_subdirectory(src/classification)
add_subdirectory(src/recognition)
add_subdirectory(src/pipeline)
# add_subdirectory(src/ocr_engine)

# ========================================
# Test Subdirectory
# ========================================
add_subdirectory(test)

# ========================================
# Benchmark Subdirectory
# ========================================
add_subdirectory(benchmark)


# Build information
message(STATUS "========================================")
message(STATUS "DeepX OCR C++ Project Configuration")
message(STATUS "========================================")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "System Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Source Directory: ${CMAKE_SOURCE_DIR}")
message(STATUS "DXRT Installed: ${DXRT_INSTALLED_DIR}")
message(STATUS "OpenCV Version: ${OpenCV_VERSION}")
message(STATUS "========================================")
