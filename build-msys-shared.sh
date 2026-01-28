#!/bin/bash
# Rebuild FFmpeg with shared DLLs (required for mpv to use custom codecs)
# FFmpeg - Alternative Development Edition
# This produces DLLs that must be bundled alongside the executables

set -e

LOG_FILE="${FFMPEG_BUILD_LOG:-build-shared.log}"
: > "$LOG_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1

FFMPEG_PREFIX="${FFMPEG_PREFIX:-./build}"
FFMPEG_BIN="$FFMPEG_PREFIX/bin"
ISPCTEXCOMP_ROOT="${ISPCTEXCOMP_ROOT:-$PWD/thirdparty/ISPCTextureCompressor}"
ISPCTEXCOMP_CONFIG=""
ISPCTEXCOMP_CFLAGS=""
ISPCTEXCOMP_LDFLAGS=""

if [ -f "$ISPCTEXCOMP_ROOT/ispc_texcomp/ispc_texcomp.h" ]; then
    ISPCTEXCOMP_CONFIG="--enable-libispc_texcomp"
    ISPCTEXCOMP_CFLAGS="-I$ISPCTEXCOMP_ROOT/ispc_texcomp"
    ISPCTEXCOMP_LDFLAGS="-L$ISPCTEXCOMP_ROOT/lib"
else
    echo "Note: ISPCTextureCompressor not found at $ISPCTEXCOMP_ROOT; Hap H ISPC path disabled."
fi

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
echo "  - Core: --enable-gpl --enable-version3 --enable-nonfree --disable-debug"
echo "  - Link: --enable-shared --disable-static"
echo "  - Video: --enable-libx264 --enable-libx265 --enable-libvpx --enable-libaom --enable-libsvtav1 --enable-libdav1d --enable-libzimg"
echo "  - Audio: --enable-libvorbis --enable-libopus --enable-libmp3lame --enable-libfdk-aac"
echo "  - HAP: --enable-libsnappy $ISPCTEXCOMP_CONFIG"
echo "  - Vulkan: --enable-vulkan --enable-libshaderc"
echo "  - Hardware Decode: --enable-d3d11va --enable-d3d12va"
echo "  - CFLAGS: -O3 $ISPCTEXCOMP_CFLAGS"
echo ""

# Note: Shared build is required so mpv can load custom FFmpeg codecs.
./configure \
    --enable-gpl \
    --enable-version3 \
    --enable-nonfree \
    --disable-debug \
    --enable-shared \
    --disable-static \
    --prefix="$FFMPEG_PREFIX" \
    --enable-zlib \
    --enable-libsnappy \
    --enable-libx264 \
    --enable-libx265 \
    --enable-libvpx \
    --enable-libaom \
    --enable-libsvtav1 \
    --enable-libdav1d \
    --enable-libvorbis \
    --enable-libopus \
    --enable-libmp3lame \
    --enable-libfdk-aac \
    --enable-libzimg \
    --enable-vulkan \
    --enable-libshaderc \
    --enable-d3d11va \
    --enable-d3d12va \
    $ISPCTEXCOMP_CONFIG \
    --extra-cflags="-O3 $ISPCTEXCOMP_CFLAGS" \
    --extra-ldflags="$ISPCTEXCOMP_LDFLAGS"

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
echo "Step 6: Copying ISPCTextureCompressor runtime assets..."
ISPCTEXCOMP_LIB_DIR="$ISPCTEXCOMP_ROOT/lib"
if [ -f "$ISPCTEXCOMP_LIB_DIR/ispc_texcomp.dll" ]; then
    cp -v "$ISPCTEXCOMP_LIB_DIR/ispc_texcomp.dll" "$FFMPEG_BIN/"
else
    echo "Note: ispc_texcomp.dll not found in $ISPCTEXCOMP_LIB_DIR"
fi

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
echo "Step 7: Bundling required DLLs..."
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
echo "  ./build-msys-copy-with-dlls-shared.sh /c/AD/nnTools/tools/ffmpeg"
echo ""
echo "Run it now? (Y/n)"
read -r RUN_COPY
RUN_COPY=${RUN_COPY:-Y}
if [ -z "$RUN_COPY" ] || [ "$RUN_COPY" = "y" ] || [ "$RUN_COPY" = "Y" ]; then
    ./build-msys-copy-with-dlls-shared.sh /c/AD/nnTools/tools/ffmpeg
fi
echo ""
