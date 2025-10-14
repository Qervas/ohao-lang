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

echo ""
echo "✅ Build complete!"
echo ""

# Verify the bundle on macOS
if [[ "$OSTYPE" == "darwin"* ]] && [ -d "ohao-lang.app" ]; then
    echo "Verifying macOS bundle..."
    codesign --verify --deep --verbose=2 ohao-lang.app && echo "✅ Code signature valid!" || echo "⚠️ Code signature verification failed"
    echo ""
    echo "To run: open ohao-lang.app"
    echo "Or from build directory: ./ohao-lang.app/Contents/MacOS/ohao-lang"
else
    echo "To run: ./build/ohao-lang"
fi
