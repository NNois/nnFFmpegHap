#!/bin/bash
# Build basis_universal MINIMAL static library for FFmpeg HAP encoder
# Only compiles bc7f (BC7) + bc6hf (BC6H) encoders — no KTX2, no UASTC, no ZSTD, no ETC1S
# Run this from a MSYS2 MinGW64 terminal

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

LOG_FILE="build-basisu-bc7f-bc6hf.log"
: > "$LOG_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1

echo "=========================================="
echo "basis_universal - MINIMAL bc7f+bc6hf Build"
echo "=========================================="
echo "Date: $(date)"
echo "Directory: $SCRIPT_DIR"
echo ""

# Step 1: Check dependencies
echo "Step 1: Checking dependencies..."
MISSING=""

if ! command -v g++ >/dev/null 2>&1; then
    MISSING="$MISSING mingw-w64-x86_64-gcc"
    echo "  [MISSING] g++"
else
    echo "  [OK] g++ $(g++ -dumpversion)"
fi

if ! command -v ar >/dev/null 2>&1; then
    MISSING="$MISSING mingw-w64-x86_64-binutils"
    echo "  [MISSING] ar"
else
    echo "  [OK] ar"
fi

if [ -n "$MISSING" ]; then
    echo ""
    echo "Installing missing packages:$MISSING"
    pacman -S --noconfirm $MISSING
    echo ""
fi

# Step 2: Clean
BUILD_DIR="build_mingw"
echo ""
echo "Step 2: Cleaning previous build..."
rm -rf "$BUILD_DIR" 2>/dev/null || true
mkdir -p "$BUILD_DIR"
echo "  Done."

# Step 3: Compile only what we need
echo ""
echo "Step 3: Compiling minimal source files..."

CXX="g++"
CXXFLAGS="-O2 -std=c++17 -fno-strict-aliasing -DBASISU_SUPPORT_SSE=1 -msse4.1"
INCLUDES="-I$SCRIPT_DIR/encoder -I$SCRIPT_DIR/transcoder -I$SCRIPT_DIR"

# Source files needed (3 from basis_universal + 1 wrapper):
#
#   basisu_transcoder.cpp  — bc7f (fast_pack_bc7_*), bc6hf (fast_encode_bc6h),
#                            basisu_transcoder_init(), bc6h_enc_init()
#   basisu_bc7enc.cpp      — bc7enc_compress_block_init() (lookup tables for bc7f)
#   basisu_kernels_sse.cpp — detect_sse41()
#   basisu_wrapper.cpp     — C wrapper for FFmpeg (basisu_init, encode_block, set_quality)
#
# NOT compiled (not needed):
#   basisu_enc.cpp, basisu_comp.cpp, basisu_frontend.cpp, basisu_backend.cpp,
#   basisu_etc.cpp, basisu_astc_hdr_common.cpp, basisu_astc_hdr_6x6_enc.cpp,
#   basisu_astc_ldr_common.cpp, basisu_astc_ldr_encode.cpp,
#   basisu_uastc_enc.cpp, basisu_uastc_hdr_4x4_enc.cpp,
#   basisu_gpu_texture.cpp, basisu_ssim.cpp, basisu_pvrtc1_4.cpp,
#   basisu_resampler.cpp, basisu_resample_filters.cpp, basisu_basis_file.cpp,
#   basisu_opencl.cpp, jpgd.cpp, pvpngreader.cpp, tinyexr.cpp, zstd.c

SOURCES=(
    "transcoder/basisu_transcoder.cpp"
    "encoder/basisu_bc7enc.cpp"
    "encoder/basisu_kernels_sse.cpp"
)

OBJECTS=()

for src in "${SOURCES[@]}"; do
    obj="$BUILD_DIR/$(basename "$src" | sed 's/\.cpp$/.o/; s/\.c$/.o/')"
    echo "  Compiling $src ..."
    $CXX $CXXFLAGS $INCLUDES -c -o "$obj" "$src"
    OBJECTS+=("$obj")
    echo "    [OK] $obj"
done

# Step 4: Compile FFmpeg wrapper
echo ""
echo "Step 4: Compiling basisu_wrapper.cpp..."
FFMPEG_ROOT="$SCRIPT_DIR/../.."
WRAPPER_SRC="$FFMPEG_ROOT/libavcodec/basisu_wrapper.cpp"

if [ ! -f "$WRAPPER_SRC" ]; then
    echo "  [FAIL] basisu_wrapper.cpp not found at $WRAPPER_SRC"
    exit 1
fi

$CXX $CXXFLAGS $INCLUDES -c -o "$BUILD_DIR/basisu_wrapper.o" "$WRAPPER_SRC"
OBJECTS+=("$BUILD_DIR/basisu_wrapper.o")
echo "  [OK] basisu_wrapper.o"

# Step 5: Create static library
echo ""
echo "Step 5: Creating static library..."
ar rcs "$BUILD_DIR/libbasisu_encoder.a" "${OBJECTS[@]}"
echo "  [OK] libbasisu_encoder.a"

# Step 6: Verify
echo ""
echo "Step 6: Verifying build output..."

LIB_FILE="$BUILD_DIR/libbasisu_encoder.a"
if [ -f "$LIB_FILE" ]; then
    LIB_SIZE=$(du -h "$LIB_FILE" | cut -f1)
    OBJ_COUNT=${#OBJECTS[@]}
    echo "  [OK] Static library: $LIB_FILE ($LIB_SIZE, $OBJ_COUNT objects)"
    echo ""
    echo "  Objects:"
    ar t "$LIB_FILE" | while read obj; do
        echo "    $obj"
    done
    echo ""
    echo "  Symbols check (bc7f/bc6hf):"
    nm "$LIB_FILE" 2>/dev/null | grep -E "fast_pack_bc7_auto_rgba|fast_encode_bc6h[^_]|basisu_init|basisu_bc7_encode|basisu_bc6h_encode" | head -10
    echo ""
    echo "=========================================="
    echo "BUILD SUCCESS"
    echo "=========================================="
    echo ""
    echo "Library: $SCRIPT_DIR/$LIB_FILE"
    echo ""
    echo "Next step: run build-msys-shared.sh from /c/ff/ff/ to build FFmpeg"
else
    echo "  [FAIL] No static library found"
    echo ""
    echo "=========================================="
    echo "BUILD FAILED"
    echo "=========================================="
    exit 1
fi
