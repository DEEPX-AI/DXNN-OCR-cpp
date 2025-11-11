#!/bin/bash
# Build script for DeepX OCR C++

set -e

BUILD_TYPE=${1:-Release}
BUILD_DIR="build_${BUILD_TYPE}"

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

