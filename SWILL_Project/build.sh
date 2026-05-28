#!/bin/bash
# SWILL Project — Automated Build Script for Linux/WSL (cross-compile)
# Note: Requires mingw-w64 for Windows target

set -e

echo "============================================"
echo "  SWILL Project - Cross-Compile Build Script"
echo "============================================"
echo ""

# Check dependencies
if ! command -v cmake &> /dev/null; then
    echo "[ERROR] CMake not found!"
    echo "Install: sudo apt-get install cmake"
    exit 1
fi

if ! command -v i686-w64-mingw32-g++ &> /dev/null; then
    echo "[ERROR] MinGW-w64 not found!"
    echo "Install: sudo apt-get install mingw-w64"
    exit 1
fi

# Navigate to project root
cd "$(dirname "$0")"

# Clean previous build
if [ -d "build" ]; then
    echo "[INFO] Cleaning previous build..."
    rm -rf build
fi

# Create build directory
echo "[INFO] Creating build directory..."
mkdir build
cd build

# Configure with CMake (cross-compile for Windows)
echo "[INFO] Configuring project (MinGW, Win32)..."
cmake .. \
    -G "Unix Makefiles" \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=i686-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++ \
    -DCMAKE_BUILD_TYPE=Release

# Build
echo "[INFO] Building Release version..."
cmake --build . --config Release -j$(nproc)

echo ""
echo "============================================"
echo "  Build completed successfully!"
echo "============================================"
echo ""
echo "Binaries located in:"
echo "  $(pwd)/bin/Release/"
echo ""
echo "Files:"
ls -la bin/Release/*.dll bin/Release/*.exe bin/Release/*.lib 2>/dev/null || true
echo ""

# Copy to output directory
mkdir -p ../output
cp bin/Release/SWILL_Payload.dll ../output/ 2>/dev/null || true
cp bin/Release/SWILL_Loader.exe ../output/ 2>/dev/null || true
cp bin/Release/SWILL_Core.lib ../output/ 2>/dev/null || true

echo "[INFO] Copies also available in: $(pwd)/../output/"
echo ""
