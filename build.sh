#!/bin/bash
# Build script - Usage: ./build.sh [release|debug] [clean]

set -e

BUILD_TYPE="Release"
CLEAN_BUILD=false

for arg in "$@"; do
    case $arg in
        clean) CLEAN_BUILD=true ;;
        debug) BUILD_TYPE="Debug" ;;
        release) BUILD_TYPE="Release" ;;
        *) echo "Usage: ./build.sh [release|debug] [clean]"; exit 1 ;;
    esac
done

BUILD_DIR="build_${BUILD_TYPE}"

if [ "$CLEAN_BUILD" = true ]; then
    rm -rf ${BUILD_DIR}
fi

mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

if [ "$BUILD_TYPE" = "Debug" ]; then
    cmake .. -DCMAKE_BUILD_TYPE=${BUILD_TYPE}
else
    cmake .. -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DENABLE_DEBUG_INFO=ON
fi
make -j$(nproc)
make install

echo "✅ Build complete: ${BUILD_DIR}/release/bin"

