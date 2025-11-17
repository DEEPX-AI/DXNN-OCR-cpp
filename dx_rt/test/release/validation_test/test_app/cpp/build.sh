#!/bin/bash

# Build script for validation_test.cpp
# Usage: ./build.sh [clean]
#   ./build.sh       - Build the project
#   ./build.sh clean - Clean build directory

set -e  # Exit on any error

# Get the directory where the script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Check if clean option is provided
if [ "$1" = "clean" ]; then
    echo "Cleaning build directory in: $SCRIPT_DIR"
    if [ -d "build" ]; then
        echo "Removing build directory..."
        rm -rf build
        echo "Clean completed successfully!"
    else
        echo "Build directory does not exist, nothing to clean."
    fi
    exit 0
fi

echo "Building validation_test in: $SCRIPT_DIR"

# Check if CMakeLists.txt exists
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: CMakeLists.txt not found in current directory"
    exit 1
fi

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
else
    echo "Build directory already exists"
fi

# Navigate to build directory
cd build

echo "Running cmake..."
cmake ..

echo "Building with make..."
make

echo "Build completed successfully!"
echo "Executable should be available in: $SCRIPT_DIR/bin/"
