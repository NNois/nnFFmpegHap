#!/bin/bash
# Build x265 with alpha support for FFmpeg
# This replaces the MSYS2 package which doesn't have alpha support

set -e

echo "=========================================="
echo "Building x265 with ALPHA SUPPORT"
echo "=========================================="
echo ""

X265_VERSION="4.1"
X265_DIR="x265_${X265_VERSION}"
X265_TARBALL="x265_${X265_VERSION}.tar.gz"
X265_URL="https://bitbucket.org/multicoreware/x265_git/downloads/${X265_TARBALL}"

# Download x265 source if not already present
if [ ! -d "$X265_DIR" ]; then
    echo "Step 1: Downloading x265 ${X265_VERSION}..."
    if [ ! -f "$X265_TARBALL" ]; then
        curl -L -o "$X265_TARBALL" "$X265_URL"
    fi
    
    echo "Step 2: Extracting..."
    tar -xzf "$X265_TARBALL"
else
    echo "✓ x265 source already present"
fi

cd "$X265_DIR/source"

echo ""
echo "Step 3: Patching CMakeLists.txt for modern CMake..."
# Fix CMake compatibility issues - create a properly ordered CMakeLists.txt
cat > CMakeLists.txt.new << 'CMAKEPATCH'
# Minimum CMake version (must be first)
cmake_minimum_required(VERSION 3.5)

# Project declaration
project(x265 C CXX)

# Remove old policy settings that are no longer supported
# cmake_policy(SET CMP0025 OLD)
# cmake_policy(SET CMP0054 OLD)

CMAKEPATCH

# Append the rest of the original file, skipping the old cmake_policy and cmake_minimum_required lines
tail -n +20 CMakeLists.txt | grep -v "^cmake_policy(SET CMP0025" | grep -v "^cmake_policy(SET CMP0054" | grep -v "^cmake_minimum_required" | grep -v "^project(x265" >> CMakeLists.txt.new
mv CMakeLists.txt.new CMakeLists.txt
echo "✓ CMakeLists.txt patched"

echo ""
echo "Step 4: Configuring x265 with alpha support..."
echo "  - HIGH_BIT_DEPTH=ON (10-bit and 12-bit support)"
echo "  - ENABLE_SHARED=ON (shared library)"
echo ""

mkdir -p build
cd build

cmake -G "MSYS Makefiles" \
    -DCMAKE_INSTALL_PREFIX=/mingw64 \
    -DENABLE_SHARED=ON \
    -DHIGH_BIT_DEPTH=ON \
    ..

echo ""
echo "Step 5: Building x265..."
CPU_CORES=$(nproc)
echo "Using $CPU_CORES CPU cores..."
make -j$CPU_CORES

echo ""
echo "Step 6: Installing x265 to /mingw64..."
make install

echo ""
echo "=========================================="
echo "✓ x265 with alpha support installed!"
echo "=========================================="
echo ""
echo "Now rebuild FFmpeg with:"
echo "  ./build-msys-shared.sh"
echo ""
