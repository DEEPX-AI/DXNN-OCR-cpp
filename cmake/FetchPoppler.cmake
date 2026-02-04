# ========================================
# Poppler Build Configuration
# ========================================
# This file compiles Poppler from git submodule source.
# 
# Poppler is a PDF rendering library based on xpdf-3.0 code base.
# It provides a C++ API (poppler-cpp) for PDF manipulation.
#
# Usage:
#   include(${CMAKE_SOURCE_DIR}/cmake/FetchPoppler.cmake)
#   target_link_libraries(my_target poppler-cpp)
#
# Dependencies (Ubuntu/Debian):
#   sudo apt-get install -y libfontconfig1-dev libfreetype6-dev \
#       libjpeg-dev libpng-dev libtiff-dev libopenjp2-7-dev libboost-dev
#
# Submodule setup:
#   git submodule add https://gitlab.freedesktop.org/poppler/poppler.git 3rd-party/poppler
#   cd 3rd-party/poppler && git checkout poppler-24.02.0

# ========================================
# Configuration Options
# ========================================
# Poppler version (should match the git tag checked out in the submodule)
set(POPPLER_VERSION "24.02.0" CACHE STRING "Poppler version (git tag)")

# Poppler directories (source is now the submodule root)
set(POPPLER_ROOT_DIR "${CMAKE_SOURCE_DIR}/3rd-party/poppler")
set(POPPLER_SOURCE_DIR "${POPPLER_ROOT_DIR}")  # Submodule root is the source
set(POPPLER_BUILD_DIR "${POPPLER_ROOT_DIR}/build")
set(POPPLER_INSTALL_DIR "${POPPLER_ROOT_DIR}/install")

message(STATUS "========================================")
message(STATUS "Poppler Configuration")
message(STATUS "========================================")
message(STATUS "  Version: ${POPPLER_VERSION}")
message(STATUS "  Root dir: ${POPPLER_ROOT_DIR}")
message(STATUS "  Source dir: ${POPPLER_SOURCE_DIR}")
message(STATUS "  Build dir: ${POPPLER_BUILD_DIR}")
message(STATUS "  Install dir: ${POPPLER_INSTALL_DIR}")
message(STATUS "  Source: Git submodule")

# ========================================
# Check System Dependencies
# ========================================
find_package(PkgConfig REQUIRED)

# Required dependencies
pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
pkg_check_modules(FREETYPE REQUIRED freetype2)

# Optional dependencies (for better PDF support)
pkg_check_modules(JPEG libjpeg)
pkg_check_modules(PNG libpng)
pkg_check_modules(TIFF libtiff-4)
pkg_check_modules(OPENJPEG libopenjp2)

message(STATUS "  Fontconfig: ${FONTCONFIG_VERSION}")
message(STATUS "  Freetype: ${FREETYPE_VERSION}")

# ========================================
# Check if Poppler is already built
# ========================================
set(POPPLER_LIBRARY_FILE "${POPPLER_INSTALL_DIR}/lib/libpoppler.a")
set(POPPLER_CPP_LIBRARY_FILE "${POPPLER_INSTALL_DIR}/lib/libpoppler-cpp.a")
set(POPPLER_HEADER_FILE "${POPPLER_INSTALL_DIR}/include/poppler/cpp/poppler-document.h")
set(POPPLER_NEED_BUILD TRUE)

if(EXISTS "${POPPLER_LIBRARY_FILE}" AND EXISTS "${POPPLER_CPP_LIBRARY_FILE}" AND EXISTS "${POPPLER_HEADER_FILE}")
    # Check version file
    set(POPPLER_VERSION_FILE "${POPPLER_INSTALL_DIR}/VERSION")
    if(EXISTS "${POPPLER_VERSION_FILE}")
        file(READ "${POPPLER_VERSION_FILE}" POPPLER_EXISTING_VERSION)
        string(STRIP "${POPPLER_EXISTING_VERSION}" POPPLER_EXISTING_VERSION)
        if("${POPPLER_EXISTING_VERSION}" STREQUAL "${POPPLER_VERSION}")
            set(POPPLER_NEED_BUILD FALSE)
            message(STATUS "  Poppler ${POPPLER_VERSION} already built, skipping")
        else()
            message(STATUS "  Existing version ${POPPLER_EXISTING_VERSION} differs from ${POPPLER_VERSION}, rebuilding")
        endif()
    endif()
endif()

# ========================================
# Build Poppler from Git Submodule
# ========================================
if(POPPLER_NEED_BUILD)
    message(STATUS "Building Poppler from git submodule...")
    
    # Check if submodule source exists
    if(NOT EXISTS "${POPPLER_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR 
            "Poppler source not found at: ${POPPLER_SOURCE_DIR}\n"
            "Please initialize the git submodule:\n"
            "  git submodule update --init 3rd-party/poppler\n"
            "  cd 3rd-party/poppler && git checkout poppler-${POPPLER_VERSION}"
        )
    endif()
    
    # Create build and install directories
    file(MAKE_DIRECTORY "${POPPLER_BUILD_DIR}")
    file(MAKE_DIRECTORY "${POPPLER_INSTALL_DIR}")
    
    # Configure Poppler with CMake
    message(STATUS "Configuring Poppler...")
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            -S "${POPPLER_SOURCE_DIR}"
            -B "${POPPLER_BUILD_DIR}"
            -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_INSTALL_PREFIX=${POPPLER_INSTALL_DIR}
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            # Enable C++ binding
            -DENABLE_CPP=ON
            # Disable unnecessary features for OCR
            -DENABLE_UTILS=OFF
            -DENABLE_GLIB=OFF
            -DENABLE_GOBJECT_INTROSPECTION=OFF
            -DENABLE_QT5=OFF
            -DENABLE_QT6=OFF
            -DENABLE_LIBCURL=OFF
            -DENABLE_BOOST=OFF
            # Build static libraries
            -DBUILD_SHARED_LIBS=OFF
            # Enable Splash renderer (for bitmap rendering)
            -DENABLE_SPLASH=ON
            -DSPLASH_CMYK=OFF
            # Disable optional dependencies to simplify build
            -DENABLE_LIBOPENJPEG=none
            -DENABLE_LIBTIFF=OFF
            -DENABLE_NSS3=OFF
            -DENABLE_GPGME=OFF
            -DENABLE_LCMS=OFF
            -DENABLE_ZLIB_UNCOMPRESS=OFF
            # Disable all tests
            -DBUILD_GTK_TESTS=OFF
            -DBUILD_QT5_TESTS=OFF
            -DBUILD_QT6_TESTS=OFF
            -DBUILD_CPP_TESTS=OFF
            -DBUILD_MANUAL_TESTS=OFF
            -DTESTDATADIR=""
        WORKING_DIRECTORY "${POPPLER_BUILD_DIR}"
        RESULT_VARIABLE CMAKE_RESULT
        OUTPUT_VARIABLE CMAKE_OUTPUT
        ERROR_VARIABLE CMAKE_ERROR
    )
    
    if(NOT CMAKE_RESULT EQUAL 0)
        message(FATAL_ERROR 
            "Failed to configure Poppler:\n${CMAKE_ERROR}\n"
            "Make sure all dependencies are installed:\n"
            "  sudo apt-get install -y libfontconfig1-dev libfreetype6-dev \\\n"
            "      libjpeg-dev libpng-dev libtiff-dev libopenjp2-7-dev"
        )
    endif()
    
    # Build Poppler
    message(STATUS "Building Poppler (this may take a few minutes)...")
    include(ProcessorCount)
    ProcessorCount(NPROC)
    if(NPROC EQUAL 0)
        set(NPROC 4)
    endif()
    
    execute_process(
        COMMAND ${CMAKE_COMMAND} --build "${POPPLER_BUILD_DIR}" --parallel ${NPROC}
        WORKING_DIRECTORY "${POPPLER_BUILD_DIR}"
        RESULT_VARIABLE BUILD_RESULT
        OUTPUT_VARIABLE BUILD_OUTPUT
        ERROR_VARIABLE BUILD_ERROR
    )
    
    if(NOT BUILD_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to build Poppler:\n${BUILD_ERROR}")
    endif()
    
    # Install Poppler
    message(STATUS "Installing Poppler to: ${POPPLER_INSTALL_DIR}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} --install "${POPPLER_BUILD_DIR}"
        WORKING_DIRECTORY "${POPPLER_BUILD_DIR}"
        RESULT_VARIABLE INSTALL_RESULT
    )
    
    if(NOT INSTALL_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to install Poppler")
    endif()
    
    # Write version file
    file(WRITE "${POPPLER_INSTALL_DIR}/VERSION" "${POPPLER_VERSION}")
    
    message(STATUS "Poppler built and installed successfully")
endif()

# ========================================
# Set Poppler paths
# ========================================
set(POPPLER_ROOT "${POPPLER_INSTALL_DIR}")

# Find include directory
find_path(Poppler_INCLUDE_DIR
    NAMES "poppler/cpp/poppler-document.h"
    PATHS "${POPPLER_ROOT}"
    PATH_SUFFIXES "include"
    NO_DEFAULT_PATH
)

# Find libraries
find_library(Poppler_LIBRARY
    NAMES "poppler"
    PATHS "${POPPLER_ROOT}"
    PATH_SUFFIXES "lib" "lib64"
    NO_DEFAULT_PATH
)

find_library(Poppler_CPP_LIBRARY
    NAMES "poppler-cpp"
    PATHS "${POPPLER_ROOT}"
    PATH_SUFFIXES "lib" "lib64"
    NO_DEFAULT_PATH
)

# Read version
if(EXISTS "${POPPLER_ROOT}/VERSION")
    file(READ "${POPPLER_ROOT}/VERSION" Poppler_VERSION)
    string(STRIP "${Poppler_VERSION}" Poppler_VERSION)
else()
    set(Poppler_VERSION "${POPPLER_VERSION}")
endif()

# ========================================
# Create imported targets
# ========================================
if(Poppler_LIBRARY AND Poppler_CPP_LIBRARY AND Poppler_INCLUDE_DIR)
    # Main poppler library
    add_library(poppler STATIC IMPORTED GLOBAL)
    set_target_properties(poppler PROPERTIES
        IMPORTED_LOCATION "${Poppler_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Poppler_INCLUDE_DIR}"
    )
    
    # Poppler C++ binding library
    add_library(poppler-cpp STATIC IMPORTED GLOBAL)
    set_target_properties(poppler-cpp PROPERTIES
        IMPORTED_LOCATION "${Poppler_CPP_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Poppler_INCLUDE_DIR};${Poppler_INCLUDE_DIR}/poppler"
        INTERFACE_LINK_LIBRARIES "poppler;${FONTCONFIG_LIBRARIES};${FREETYPE_LIBRARIES};pthread"
    )
    
    set(Poppler_FOUND TRUE)
endif()

# ========================================
# Verification and Status
# ========================================
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Poppler
    REQUIRED_VARS Poppler_LIBRARY Poppler_CPP_LIBRARY Poppler_INCLUDE_DIR
    VERSION_VAR Poppler_VERSION
)

if(Poppler_FOUND)
    message(STATUS "========================================")
    message(STATUS "✓ Poppler configured successfully")
    message(STATUS "  Library: ${Poppler_LIBRARY}")
    message(STATUS "  C++ Library: ${Poppler_CPP_LIBRARY}")
    message(STATUS "  Include: ${Poppler_INCLUDE_DIR}")
    message(STATUS "  Version: ${Poppler_VERSION}")
    message(STATUS "========================================")
    
    # Export variables for compatibility
    set(POPPLER_INCLUDE_DIRS "${Poppler_INCLUDE_DIR}" CACHE INTERNAL "Poppler include directories")
    set(POPPLER_LIBRARIES "${Poppler_CPP_LIBRARY};${Poppler_LIBRARY}" CACHE INTERNAL "Poppler libraries")
else()
    message(FATAL_ERROR 
        "Failed to configure Poppler.\n"
        "Expected files at: ${POPPLER_ROOT}\n"
        "Please check the build logs above."
    )
endif()
