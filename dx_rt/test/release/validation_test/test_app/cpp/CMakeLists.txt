# CMakeLists for DXRT Validation Test
# Integrated with main dx_rt_fork build system

# Note: cmake_minimum_required, project, and C++ standard are handled by parent CMakeLists.txt

# Set build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "-std=c++14 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

# Target executable
set(TARGET_NAME validation_test)

# Source files
set(SOURCES
    validation_test.cpp
    src/generator.cpp
    src/executor.cpp
    src/input_utils.cpp
    src/utils.cpp
    src/bitmatcher.cpp
    src/executorManager.cpp
    src/test_manager.cpp
)

# Header files (for IDE support)
set(HEADERS
    include/generator.h
    include/executor.h
    include/input_utils.h
    include/utils.h
    include/bitmatcher.h
    include/executorManager.h
    include/test_manager.h
)

# Create executable
add_executable(${TARGET_NAME} ${SOURCES} ${HEADERS})

# Set output directory to match main build system (bin directory)
set_target_properties(${TARGET_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

# DXRT root directory and cmake functions are already included by parent CMakeLists.txt

# Include directories
target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Add DXRT library using the project's macro
add_dxrt(${TARGET_NAME})

# Using rapidjson instead of nlohmann_json (rapidjson is header-only and included with DXRT)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${TARGET_NAME} PRIVATE -fPIC)
endif()

# Custom target for building and running
add_custom_target(run_validation
    COMMAND ${TARGET_NAME} --help
    DEPENDS ${TARGET_NAME}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running validation test"
)

# Installation (follows main build system pattern)
install(TARGETS ${TARGET_NAME} DESTINATION bin)
install(TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_SOURCE_DIR}/bin)

# Print build information
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Target: ${TARGET_NAME}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
