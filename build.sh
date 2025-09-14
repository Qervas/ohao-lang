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
make -j$(nproc)

echo "Build complete! Run with: ./build/ohao-lang"