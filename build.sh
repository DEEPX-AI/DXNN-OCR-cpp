#!/bin/bash
# Build script - Usage: ./build.sh [release|debug] [clean] [test]

set -e

BUILD_TYPE="Release"
CLEAN_BUILD=false
BUILD_TESTS=false

for arg in "$@"; do
    case $arg in
        clean) CLEAN_BUILD=true ;;
        debug) BUILD_TYPE="Debug" ;;
        release) BUILD_TYPE="Release" ;;
        test) BUILD_TESTS=true ;;
        *) echo "Usage: ./build.sh [release|debug] [clean] [test]"; exit 1 ;;
    esac
done

BUILD_DIR="build_${BUILD_TYPE}"

if [ "$CLEAN_BUILD" = true ]; then
    rm -rf ${BUILD_DIR}
fi

mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

# Build OpenCV options
CMAKE_EXTRA_ARGS=""
if [ "${BUILD_OPENCV_FROM_SOURCE}" = "OFF" ]; then
    CMAKE_EXTRA_ARGS="-DBUILD_OPENCV_FROM_SOURCE=OFF"
    echo "Using system OpenCV"
fi

# Test options (using DXOCR_BUILD_TESTS to avoid conflict with OpenCV)
if [ "$BUILD_TESTS" = true ]; then
    CMAKE_EXTRA_ARGS="${CMAKE_EXTRA_ARGS} -DDXOCR_BUILD_TESTS=ON"
    echo "Building with unit tests enabled"
else
    CMAKE_EXTRA_ARGS="${CMAKE_EXTRA_ARGS} -DDXOCR_BUILD_TESTS=OFF"
fi

INSTALL_PREFIX="$(pwd)"

if [ "$BUILD_TYPE" = "Debug" ]; then
    cmake .. -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} ${CMAKE_EXTRA_ARGS}
else
    cmake .. -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} -DENABLE_DEBUG_INFO=ON ${CMAKE_EXTRA_ARGS}
fi
make -j$(nproc)

# make install
# 注意：跳过 make install，可执行文件已直接输出到 bin/ 目录
# 如需安装到系统目录，请手动执行: make install

echo "✅ Build complete: ${BUILD_DIR}/bin"

# Run tests if enabled
if [ "$BUILD_TESTS" = true ]; then
    echo ""
    echo "========================================="
    echo "Running unit tests..."
    echo "========================================="
    if [ -f "bin/server_tests" ]; then
        ./bin/server_tests --gtest_color=yes
    else
        echo "Warning: server_tests not found"
    fi
fi

