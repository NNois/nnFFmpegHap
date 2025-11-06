#!/bin/bash
# Rebuild FFmpeg with HAP properly enabled
# Alternative Development Edition

set -e

echo "=========================================="
echo "FFmpeg - Alternative Development Edition"
echo "Rebuilding with HAP encoder enhancements"
echo "=========================================="
echo ""

echo "Step 1: Cleaning previous build..."
make clean 2>/dev/null || true
rm -f config.h config.log ffbuild/config.mak 2>/dev/null || true

echo ""
echo "Step 2: Configuring with HAP and all codecs..."
echo ""

# The key: Don't use --disable-everything, just enable what we need
# HAP encoder requires libsnappy to be explicitly enabled
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
    --extra-cflags="-O3"

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
echo "Testing HAP encoder:"
./ffmpeg -hide_banner -encoders 2>/dev/null | grep hap || echo "HAP not found in encoders"

echo ""
echo "Testing ProRes encoder:"
./ffmpeg -hide_banner -encoders 2>/dev/null | grep prores || echo "ProRes not found in encoders"

echo ""
echo "=========================================="
echo "Alternative Development Edition"
echo "=========================================="
echo ""
