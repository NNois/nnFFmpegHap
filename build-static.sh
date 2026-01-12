#!/bin/bash
# Rebuild FFmpeg with fully static linking (no DLL dependencies)
# FFmpeg - Alternative Development Edition
# This creates larger executables but they work from PowerShell/CMD without MSYS2

set -e

LOG_FILE="${FFMPEG_BUILD_LOG:-build-static.log}"
: > "$LOG_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1

echo "=========================================="
echo "FFmpeg - Alternative Development Edition"
echo "Rebuilding with STATIC linking (portable)"
echo "=========================================="
echo ""

GPURTBC6H_ROOT="${GPURTBC6H_ROOT:-$PWD/thirdparty/GPURealTimeBC6H}"
GPURTBC6H_CONFIG=""
GPURTBC6H_CFLAGS=""
GPURTBC6H_LDFLAGS=""
GPURTBC6H_INCLUDE=""

if [ -f "$GPURTBC6H_ROOT/include/GPURealTimeBC6H-c.h" ]; then
    GPURTBC6H_INCLUDE="$GPURTBC6H_ROOT/include"
elif [ -f "$PWD/GPURealTimeBC6H-c.h" ]; then
    GPURTBC6H_INCLUDE="$PWD"
elif [ -f "$GPURTBC6H_ROOT/GPURealTimeBC6H-c.h" ]; then
    GPURTBC6H_INCLUDE="$GPURTBC6H_ROOT"
fi

if [ -n "$GPURTBC6H_INCLUDE" ]; then
    GPURTBC6H_CONFIG="--enable-libgpurealtimebc6h"
    GPURTBC6H_CFLAGS="-I$GPURTBC6H_INCLUDE"
    GPURTBC6H_LDFLAGS="-L$GPURTBC6H_ROOT/lib"
else
    echo "Note: GPURealTimeBC6H not found at $GPURTBC6H_ROOT; Hap H GPU path disabled."
fi

echo "Step 1: Cleaning previous build..."
make clean 2>/dev/null || true
rm -f config.h config.log ffbuild/config.mak 2>/dev/null || true
rm -rf ./build 2>/dev/null || true
rm -f ./*.dll 2>/dev/null || true

echo ""
echo "Step 2: Configuring build options (STATIC BUILD):"
echo "  - Core: --enable-gpl --enable-version3 --disable-debug"
echo "  - Link: --enable-static --disable-shared"
echo "  - Video: --enable-libx264 --enable-libx265 --enable-libvpx --enable-libzimg"
echo "  - Audio: --enable-libvorbis --enable-libopus --enable-libmp3lame"
echo "  - HAP: --enable-libsnappy $GPURTBC6H_CONFIG"
echo "  - CFLAGS: -O3 $GPURTBC6H_CFLAGS"
echo ""

# Note: Full static linking is not possible with MINGW64
# We'll build normally and bundle required DLLs
./configure \
    --enable-gpl \
    --enable-version3 \
    --disable-debug \
    --enable-static \
    --disable-shared \
    --prefix=./build \
    --enable-libsnappy \
    --enable-libx264 \
    --enable-libx265 \
    --enable-libvpx \
    --enable-libvorbis \
    --enable-libopus \
    --enable-libmp3lame \
    --enable-libzimg \
    $GPURTBC6H_CONFIG \
    --extra-cflags="-O3 $GPURTBC6H_CFLAGS" \
    --extra-ldflags="$GPURTBC6H_LDFLAGS"

echo ""
echo "Step 3: Verifying HAP is enabled..."
grep "CONFIG_HAP_ENCODER" config.h && echo "✓ HAP encoder enabled" || echo "✗ HAP encoder NOT enabled"
grep "CONFIG_HAP_DECODER" config.h && echo "✓ HAP decoder enabled" || echo "✗ HAP decoder NOT enabled"

echo ""
echo "Step 4: Building FFmpeg..."
CPU_CORES=$(nproc)
echo "Using $CPU_CORES CPU cores..."
make -j$CPU_CORES

echo ""
echo "=========================================="
echo "✓ Build Complete!"
echo "=========================================="
echo ""
echo "FFmpeg - Alternative Development Edition"
echo "Build type: PORTABLE (with bundled DLLs)"
echo "Build features:"
echo "  - HapM support (FourCC: 'HapM')"
echo "  - HapA support (FourCC: 'HapA')"
echo "  - Fixed YCoCg color transform"
echo "  - ProRes encoder (prores, prores_aw, prores_ks)"
echo ""

# Check built executables
echo "Built executables:"
ls -lh ffmpeg.exe ffplay.exe ffprobe.exe 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}'

echo ""
echo "Step 5: Copying GPURealTimeBC6H runtime assets..."
GPURTBC6H_LIB_DIR="$GPURTBC6H_ROOT/lib"
if [ -f "$GPURTBC6H_LIB_DIR/GPURealTimeBC6H.dll" ]; then
    cp -v "$GPURTBC6H_LIB_DIR/GPURealTimeBC6H.dll" ./
else
    echo "Note: GPURealTimeBC6H.dll not found in $GPURTBC6H_LIB_DIR"
fi
if [ -f "$GPURTBC6H_LIB_DIR/compress.hlsl" ]; then
    cp -v "$GPURTBC6H_LIB_DIR/compress.hlsl" ./
else
    echo "Note: compress.hlsl not found in $GPURTBC6H_LIB_DIR"
fi

echo ""
echo "Step 6: Bundling required DLLs..."
echo ""

# Copy required MINGW64 DLLs to current directory
REQUIRED_DLLS=$(ldd ffprobe.exe | grep mingw64 | awk '{print $3}')
DLL_COUNT=0

for dll in $REQUIRED_DLLS; do
    if [ -f "$dll" ]; then
        dll_name=$(basename "$dll")
        cp -v "$dll" ./
        DLL_COUNT=$((DLL_COUNT + 1))
    fi
done

echo ""
echo "✓ Bundled $DLL_COUNT DLLs"
echo ""

echo "Testing HAP encoder:"
./ffmpeg -hide_banner -encoders 2>/dev/null | grep hap || echo "HAP not found in encoders"

echo ""
echo "Testing ProRes encoder:"
./ffmpeg -hide_banner -encoders 2>/dev/null | grep prores || echo "ProRes not found in encoders"

echo ""
echo "=========================================="
echo "Alternative Development Edition"
echo "Portable Build Complete"
echo "=========================================="
echo ""
echo "Files in this directory (executables + DLLs):"
ls -lh *.exe *.dll 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}'
echo ""
echo "These files can be copied together to any location"
echo "and will work from PowerShell, CMD, and Python apps"
echo "without requiring MSYS2."
echo ""
echo "To deploy to your app, use:"
echo "  ./build-copy-with-dlls-static.sh /c/AD/nnTools/tools/ffmpeg"
echo ""
echo "Run it now? (y/N)"
read -r RUN_COPY
if [ "$RUN_COPY" = "y" ] || [ "$RUN_COPY" = "Y" ]; then
    ./build-copy-with-dlls-static.sh /c/AD/nnTools/tools/ffmpeg
fi
echo ""
