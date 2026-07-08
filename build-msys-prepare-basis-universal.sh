#!/bin/bash
# Prepare and build basis_universal as a static library for FFmpeg integration.
# Run this from an MSYS2 MinGW64 terminal.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASISU_ROOT="${BASISU_ROOT:-$SCRIPT_DIR/thirdparty/basis_universal}"
BASISU_REPO_URL="${BASISU_REPO_URL:-https://github.com/BinomialLLC/basis_universal.git}"
LOG_FILE="${BASISU_BUILD_LOG:-$SCRIPT_DIR/build-msys-prepare-basis-universal.log}"

mkdir -p "$(dirname "$BASISU_ROOT")" "$(dirname "$LOG_FILE")"
: > "$LOG_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1

echo "=========================================="
echo "basis_universal - Static Library Build"
echo "=========================================="
echo "Date: $(date)"
echo "Repository: $BASISU_REPO_URL"
echo "Directory: $BASISU_ROOT"
echo ""

if [ ! -d "$BASISU_ROOT" ]; then
    echo "Step 0: Cloning basis_universal..."
    if ! command -v git >/dev/null 2>&1; then
        echo "  [MISSING] git"
        pacman -S --noconfirm git
    fi
    git clone "$BASISU_REPO_URL" "$BASISU_ROOT"
    echo ""
elif [ ! -f "$BASISU_ROOT/CMakeLists.txt" ]; then
    echo "ERROR: $BASISU_ROOT exists but does not look like a basis_universal checkout."
    echo "Remove it or set BASISU_ROOT to a valid checkout."
    exit 1
fi

cd "$BASISU_ROOT"

echo "Step 1: Checking dependencies..."
MISSING=""

if command -v git >/dev/null 2>&1; then
    echo "  [OK] git $(git --version | awk '{print $3}')"
fi

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

echo ""
echo "Step 2: Cleaning previous build..."
rm -rf build_mingw 2>/dev/null || true
echo "  Done."

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

echo ""
echo "Step 5: Compiling basisu_wrapper.cpp..."
WRAPPER_SRC="$SCRIPT_DIR/libavcodec/basisu_wrapper.cpp"

if [ ! -f "$WRAPPER_SRC" ]; then
    echo "  [FAIL] basisu_wrapper.cpp not found at $WRAPPER_SRC"
    exit 1
fi

g++ -c -O2 -fno-strict-aliasing -std=c++17 \
    -DBASISU_SUPPORT_SSE=1 -msse4.1 \
    -I"$BASISU_ROOT/encoder" \
    -I"$BASISU_ROOT/transcoder" \
    -I"$BASISU_ROOT" \
    -o build_mingw/basisu_wrapper.o \
    "$WRAPPER_SRC"

echo "  [OK] basisu_wrapper.o compiled"

ar rs build_mingw/libbasisu_encoder.a build_mingw/basisu_wrapper.o
echo "  [OK] basisu_wrapper.o added to libbasisu_encoder.a"

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
    echo "Library: $BASISU_ROOT/$LIB_FILE"
    echo ""
    echo "Next step: run ./build-msys-shared.sh"
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
    echo "Check $LOG_FILE for details."
    exit 1
fi
