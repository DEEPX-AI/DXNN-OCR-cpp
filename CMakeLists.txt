cmake_minimum_required(VERSION 3.14)
project(DeepXOCR VERSION 1.0.0 LANGUAGES CXX)

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

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

include(dx_func)

add_definitions(-DPROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}")
add_compile_options(-W -Wall -pthread -fPIC)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options(-fstrict-volatile-bitfields)
endif()

if(ENABLE_DEBUG_INFO)
    add_compile_options(-g -rdynamic)
endif()

# Find OpenCV
find_package(OpenCV REQUIRED)

# Add 3rd-party libraries
add_subdirectory(3rd-party/json)
add_subdirectory(3rd-party/clipper2/CPP)

# Global include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/3rd-party/json/include)
include_directories(${CMAKE_SOURCE_DIR}/3rd-party/clipper2/CPP/Clipper2Lib/include)

# Add subdirectories
add_subdirectory(src/common)
add_subdirectory(src/preprocessing)
add_subdirectory(src/detection)
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
