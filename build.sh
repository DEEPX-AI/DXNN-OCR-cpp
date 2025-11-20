#!/bin/bash
# Build script for DeepX OCR C++

set -e

# Parse arguments
BUILD_TYPE="Release"
CLEAN_BUILD=false

for arg in "$@"; do
    case $arg in
        clean|Clean|CLEAN)
            CLEAN_BUILD=true
            ;;
        Debug|debug|DEBUG)
            BUILD_TYPE="Debug"
            ;;
        Release|release|RELEASE)
            BUILD_TYPE="Release"
            ;;
        *)
            echo "Unknown argument: $arg"
            echo "Usage: bash build.sh [Release|Debug] [clean]"
            echo "  Release/Debug: Build type (default: Release)"
            echo "  clean: Remove build directory before building"
            exit 1
            ;;
    esac
done

BUILD_DIR="build_${BUILD_TYPE}"

# Clean build if requested
if [ "$CLEAN_BUILD" = true ]; then
    echo "========================================="
    echo "Cleaning build directory: ${BUILD_DIR}"
    echo "========================================="
    rm -rf ${BUILD_DIR}
    echo "Cleaned successfully!"
    echo ""
fi

echo "========================================="
echo "Building DeepX OCR C++ - ${BUILD_TYPE}"
echo "========================================="

# Create build directory
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

# Configure
cmake .. \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DENABLE_DEBUG_INFO=$([ "$BUILD_TYPE" = "Debug" ] && echo "ON" || echo "OFF")

# Build
echo ""
echo "Building..."
make -j$(nproc)

# Install
echo ""
echo "Installing to build/release..."
make install

echo ""
echo "========================================="
echo "Build completed successfully!"
echo "========================================="
echo "Executables in: ${BUILD_DIR}/release/bin"
echo "Libraries in: ${BUILD_DIR}/release/lib"

