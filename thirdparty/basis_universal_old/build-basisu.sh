#!/bin/bash
# Build basis_universal as a static library for FFmpeg integration
# Run this from a MSYS2 MinGW64 terminal

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

LOG_FILE="build-basisu.log"
: > "$LOG_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1

echo "=========================================="
echo "basis_universal - Static Library Build"
echo "=========================================="
echo "Date: $(date)"
echo "Directory: $SCRIPT_DIR"
echo ""

# Step 1: Check dependencies
echo "Step 1: Checking dependencies..."
MISSING=""

if ! command -v cmake >/dev/null 2>&1; then
    MISSING="$MISSING mingw-w64-x86_64-cmake"
    echo "  [MISSING] cmake"
else
    echo "  [OK] cmake $(cmake --version | head -1)"
fi

if ! command -v ninja >/dev/null 2>&1; then
    MISSING="$MISSING mingw-w64-x86_64-ninja"
    echo "  [MISSING] ninja"
else
    echo "  [OK] ninja $(ninja --version)"
fi

if ! command -v g++ >/dev/null 2>&1; then
    MISSING="$MISSING mingw-w64-x86_64-gcc"
    echo "  [MISSING] g++"
else
    echo "  [OK] g++ $(g++ -dumpversion)"
fi

if [ -n "$MISSING" ]; then
    echo ""
    echo "Installing missing packages:$MISSING"
    pacman -S --noconfirm $MISSING
    echo ""
fi

# Step 2: Clean previous build
echo ""
echo "Step 2: Cleaning previous build..."
rm -rf build_mingw 2>/dev/null || true
echo "  Done."

# Step 3: Configure with CMake
echo ""
echo "Step 3: Configuring with CMake..."
cmake -B build_mingw -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS="-O2 -fno-strict-aliasing" \
    -DCMAKE_CXX_FLAGS="-O2 -fno-strict-aliasing" \
    -DBASISU_STATIC=TRUE \
    -DBASISU_SSE=TRUE \
    -DBASISU_ZSTD=TRUE \
    -DBASISU_OPENCL=FALSE \
    -DBASISU_EXAMPLES=FALSE \
    -DBASISU_BUILD_PYTHON=FALSE

echo ""
echo "Step 4: Building (static library only, skipping basisu.exe)..."
cmake --build build_mingw --config Release --target basisu_encoder -j"$(nproc)"

# Step 5: Compile FFmpeg wrapper and add to library
echo ""
echo "Step 5: Compiling basisu_wrapper.cpp..."
FFMPEG_ROOT="$SCRIPT_DIR/../.."
WRAPPER_SRC="$FFMPEG_ROOT/libavcodec/basisu_wrapper.cpp"

if [ ! -f "$WRAPPER_SRC" ]; then
    echo "  [FAIL] basisu_wrapper.cpp not found at $WRAPPER_SRC"
    exit 1
fi

g++ -c -O2 -fno-strict-aliasing -std=c++17 \
    -DBASISU_SUPPORT_SSE=1 -msse4.1 \
    -I"$SCRIPT_DIR/encoder" \
    -I"$SCRIPT_DIR/transcoder" \
    -I"$SCRIPT_DIR" \
    -o build_mingw/basisu_wrapper.o \
    "$WRAPPER_SRC"

echo "  [OK] basisu_wrapper.o compiled"

# Add wrapper to the static library
ar rs build_mingw/libbasisu_encoder.a build_mingw/basisu_wrapper.o
echo "  [OK] basisu_wrapper.o added to libbasisu_encoder.a"

# Step 6: Verify output
echo ""
echo "Step 6: Verifying build output..."

LIB_FILE=""
if [ -f "build_mingw/libbasisu_encoder.a" ]; then
    LIB_FILE="build_mingw/libbasisu_encoder.a"
elif [ -f "build_mingw/libbasisu.a" ]; then
    LIB_FILE="build_mingw/libbasisu.a"
fi

if [ -n "$LIB_FILE" ]; then
    echo "  [OK] Static library: $LIB_FILE ($(du -h "$LIB_FILE" | cut -f1))"
    echo ""
    echo "  Symbols check (BC7/BC6H related):"
    nm "$LIB_FILE" 2>/dev/null | grep -i -E "bc7enc_compress_block_init|fast_pack_bc7|astc_hdr_4x4_enc_block|astc_hdr_enc_init|basisu_encoder_init" | head -20
    echo ""
    echo "=========================================="
    echo "BUILD SUCCESS"
    echo "=========================================="
    echo ""
    echo "Library: $SCRIPT_DIR/$LIB_FILE"
    echo ""
    echo "Next step: run build-msys-shared.sh from /c/ff/ff/ to build FFmpeg"
else
    echo "  [FAIL] No static library found in build_mingw/"
    echo ""
    echo "  Files in build_mingw/:"
    ls -lh build_mingw/*.a build_mingw/*.lib 2>/dev/null || echo "  (none)"
    echo ""
    echo "=========================================="
    echo "BUILD FAILED"
    echo "=========================================="
    echo ""
    echo "Check $SCRIPT_DIR/$LOG_FILE for details."
    exit 1
fi
