#!/bin/bash
# Rebuild FFmpeg with shared DLLs (required for mpv to use custom codecs)
# FFmpeg - Alternative Development Edition
# This produces DLLs that must be bundled alongside the executables

set -e

FFMPEG_PREFIX="${FFMPEG_PREFIX:-./build}"
FFMPEG_BIN="$FFMPEG_PREFIX/bin"

echo "=========================================="
echo "FFmpeg - Alternative Development Edition"
echo "Rebuilding with SHARED DLLs (mpv-compatible)"
echo "=========================================="
echo ""

echo "Step 1: Cleaning previous build..."
make clean 2>/dev/null || true
rm -f config.h config.log ffbuild/config.mak 2>/dev/null || true
rm -rf "$FFMPEG_PREFIX" 2>/dev/null || true
rm -f ./*.dll 2>/dev/null || true

echo ""
echo "Step 2: Configuring build options (SHARED BUILD):"
echo "  - Core: --enable-gpl --enable-version3 --disable-debug"
echo "  - Link: --enable-shared --disable-static"
echo "  - Video: --enable-libx264 --enable-libx265 --enable-libvpx --enable-libzimg"
echo "  - Audio: --enable-libvorbis --enable-libopus --enable-libmp3lame"
echo "  - HAP: --enable-libsnappy"
echo "  - CFLAGS: -O3"
echo ""

# Note: Shared build is required so mpv can load custom FFmpeg codecs.
./configure \
    --enable-gpl \
    --enable-version3 \
    --disable-debug \
    --enable-shared \
    --disable-static \
    --prefix="$FFMPEG_PREFIX" \
    --enable-libsnappy \
    --enable-libx264 \
    --enable-libx265 \
    --enable-libvpx \
    --enable-libvorbis \
    --enable-libopus \
    --enable-libmp3lame \
    --enable-libzimg \
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
echo "Step 5: Installing to $FFMPEG_PREFIX..."
make install

echo ""
echo "=========================================="
echo "✓ Build Complete!"
echo "=========================================="
echo ""
echo "FFmpeg - Alternative Development Edition"
echo "Build type: SHARED DLLs (bundle required)"
echo "Build features:"
echo "  - HapM support (FourCC: 'HapM')"
echo "  - HapA support (FourCC: 'HapA')"
echo "  - Fixed YCoCg color transform"
echo "  - ProRes encoder (prores, prores_aw, prores_ks)"
echo ""

# Check built executables
echo "Built executables:"
ls -lh "$FFMPEG_BIN/ffmpeg.exe" "$FFMPEG_BIN/ffplay.exe" "$FFMPEG_BIN/ffprobe.exe" 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}'

echo ""
echo "Step 6: Bundling required DLLs..."
echo ""

# Copy required MINGW64 DLLs to the install bin dir
REQUIRED_DLLS=$(ldd "$FFMPEG_BIN/ffprobe.exe" | grep mingw64 | awk '{print $3}')
DLL_COUNT=0

for dll in $REQUIRED_DLLS; do
    if [ -f "$dll" ]; then
        cp -v "$dll" "$FFMPEG_BIN/"
        DLL_COUNT=$((DLL_COUNT + 1))
    fi
done

echo ""
echo "✓ Bundled $DLL_COUNT DLLs"
echo ""

echo "Testing HAP encoder:"
"$FFMPEG_BIN/ffmpeg" -hide_banner -encoders 2>/dev/null | grep hap || echo "HAP not found in encoders"

echo ""
echo "Testing ProRes encoder:"
"$FFMPEG_BIN/ffmpeg" -hide_banner -encoders 2>/dev/null | grep prores || echo "ProRes not found in encoders"

echo ""
echo "=========================================="
echo "Alternative Development Edition"
echo "Portable Build Complete"
echo "=========================================="
echo ""
echo "Files in $FFMPEG_BIN (executables + DLLs):"
ls -lh "$FFMPEG_BIN"/*.exe "$FFMPEG_BIN"/*.dll 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}'
echo ""
echo "These files can be copied together to any location"
echo "and will work from PowerShell, CMD, and Python apps"
echo "without requiring MSYS2."
echo ""
echo "To deploy to your app, use:"
echo "  ./build-copy-with-dlls-shared.sh /c/AD/nnTools/tools/ffmpeg"
echo ""
echo "Run it now? (Y/n)"
read -r RUN_COPY
RUN_COPY=${RUN_COPY:-Y}
if [ -z "$RUN_COPY" ] || [ "$RUN_COPY" = "y" ] || [ "$RUN_COPY" = "Y" ]; then
    ./build-copy-with-dlls-shared.sh /c/AD/nnTools/tools/ffmpeg
fi
echo ""
