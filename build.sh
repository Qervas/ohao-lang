#!/bin/bash
set -e

echo "Building Ohao Lang Qt Application..."

# Clean previous build
rm -rf build

# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build
# Use nproc on Linux, sysctl on macOS, fallback to 4
if command -v nproc > /dev/null 2>&1; then
    NPROC=$(nproc)
elif command -v sysctl > /dev/null 2>&1; then
    NPROC=$(sysctl -n hw.ncpu)
else
    NPROC=4
fi
make -j${NPROC}

echo "Build complete! Run with: ./build/ohao-lang"
