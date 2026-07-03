#!/bin/bash
# Build x265 with alpha support for FFmpeg
# This replaces the MSYS2 package which doesn't have alpha support

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

LOG_FILE="${X265_BUILD_LOG:-$SCRIPT_DIR/build-msys-x265-with-alpha.log}"
mkdir -p "$(dirname "$LOG_FILE")"
: > "$LOG_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1

echo "=========================================="
echo "Building x265 with ALPHA SUPPORT"
echo "=========================================="
echo ""

X265_VERSION="4.1"
X265_DIR="x265_${X265_VERSION}"
X265_TARBALL="x265_${X265_VERSION}.tar.gz"
X265_TARBALL_PATH="$SCRIPT_DIR/$X265_TARBALL"
X265_URL="https://bitbucket.org/multicoreware/x265_git/downloads/${X265_TARBALL}"
INSTALL_PREFIX="${X265_INSTALL_PREFIX:-${MINGW_PREFIX:-/mingw64}}"
X265_BIN="$INSTALL_PREFIX/bin/x265"

# Download x265 source if not already present
if [ ! -d "$X265_DIR" ]; then
    echo "Step 1: Downloading x265 ${X265_VERSION}..."
    if [ ! -f "$X265_TARBALL_PATH" ]; then
        curl -L -o "$X265_TARBALL_PATH" "$X265_URL"
    fi
    
    echo "Step 2: Extracting..."
    tar -xzf "$X265_TARBALL_PATH"
else
    echo "✓ x265 source already present"
fi

cd "$X265_DIR/source"

echo ""
echo "Step 3: Restoring original CMakeLists.txt..."
# Ensure we use the pristine CMakeLists to avoid broken patches
if [ ! -f "$X265_TARBALL_PATH" ]; then
    echo "Tarball missing, downloading ${X265_TARBALL}..."
    curl -L -o "$X265_TARBALL_PATH" "$X265_URL"
fi
tar -xOf "$X265_TARBALL_PATH" "$X265_DIR/source/CMakeLists.txt" > CMakeLists.txt
echo "✓ CMakeLists.txt restored"

echo "Patching CMakeLists.txt for CMake 4.x..."
# Force required policies to NEW and move cmake_minimum_required before project()
sed -i 's/cmake_policy(SET CMP0025 OLD)/cmake_policy(SET CMP0025 NEW)/' CMakeLists.txt
sed -i 's/cmake_policy(SET CMP0054 OLD)/cmake_policy(SET CMP0054 NEW)/' CMakeLists.txt

awk '
BEGIN{min_done=0}
/^cmake_minimum_required/ && !min_done {min_line=$0; min_done=1; next}
/^project[[:space:]]*\(/ && min_done==0 {min_line="cmake_minimum_required (VERSION 2.8.8)"; min_done=1}
/^project[[:space:]]*\(/ && min_done==1 {
    print min_line
    print
    min_line=""
    next
}
{print}
END{if(min_line!="") print min_line}
' CMakeLists.txt > CMakeLists.txt.tmp
mv CMakeLists.txt.tmp CMakeLists.txt
echo "✓ CMakeLists.txt patched"

echo ""
echo "Step 4: Configuring x265 with alpha support..."
echo "  - HIGH_BIT_DEPTH=ON (10-bit and 12-bit support)"
echo "  - ENABLE_ALPHA=ON (alpha layer support)"
echo "  - ENABLE_SHARED=ON (shared library)"
echo ""

echo "Cleaning previous CMake cache..."
rm -rf build
rm -rf ../build

mkdir -p build
cd build

cmake -G "MSYS Makefiles" \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -DENABLE_SHARED=ON \
    -DENABLE_ALPHA=ON \
    -DHIGH_BIT_DEPTH=ON \
    ..

echo ""
echo "Step 5: Building x265..."
CPU_CORES=$(nproc)
echo "Using $CPU_CORES CPU cores..."
make -j$CPU_CORES

echo ""
echo "Step 6: Installing x265 to $INSTALL_PREFIX..."
make install

echo ""
echo "Step 7: Quick alpha capability check..."
if command -v "$X265_BIN" >/dev/null 2>&1; then
    "$X265_BIN" --version || true
    if "$X265_BIN" --help 2>/dev/null | grep -qi "alpha"; then
        echo "✓ x265 help mentions alpha support"
    else
        echo "⚠️  x265 help does not mention alpha (not definitive)"
    fi
else
    echo "⚠️  $X265_BIN not found, skipping alpha check"
fi

echo ""
echo "=========================================="
echo "✓ x265 with alpha support installed!"
echo "=========================================="
echo ""
echo "Now rebuild FFmpeg with:"
echo "  ./build-msys-shared.sh"
echo ""
