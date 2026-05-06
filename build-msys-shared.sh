#!/bin/bash
# Rebuild FFmpeg with shared DLLs (required for mpv to use custom codecs)
# FFmpeg - Alternative Development Edition
# This produces DLLs that must be bundled alongside the executables

set -e

LOG_FILE="${FFMPEG_BUILD_LOG:-build-msys-shared.log}"
: > "$LOG_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1

FFMPEG_PREFIX="${FFMPEG_PREFIX:-./build}"
FFMPEG_BIN="$FFMPEG_PREFIX/bin"
BASISU_ROOT="${BASISU_ROOT:-$PWD/thirdparty/basis_universal}"
BASISU_CFLAGS=""
BASISU_LDFLAGS=""
BASISU_LIBS=""

if [ -f "$BASISU_ROOT/build_mingw/libbasisu_encoder.a" ]; then
    BASISU_CFLAGS="-I$BASISU_ROOT/encoder -I$BASISU_ROOT/transcoder -I$BASISU_ROOT"
    BASISU_LDFLAGS="-L$BASISU_ROOT/build_mingw"
    BASISU_LIBS="-lbasisu_encoder -lstdc++ -lpthread"
    echo "basis_universal found: $BASISU_ROOT/build_mingw/libbasisu_encoder.a"
else
    echo "ERROR: basis_universal not built. Run thirdparty/basis_universal/build-basisu.sh first."
    exit 1
fi

BINK2SDK_ROOT="${BINK2SDK_ROOT:-$PWD/thirdparty/bink2sdk}"
BINK2SDK_CFLAGS=""
BINK2SDK_LDFLAGS=""
BINK2SDK_LIBS=""

if [ -f "$BINK2SDK_ROOT/build_mingw/libbink2sdk.a" ]; then
    BINK2SDK_CFLAGS="-I$BINK2SDK_ROOT/src"
    BINK2SDK_LDFLAGS="-L$BINK2SDK_ROOT/build_mingw"
    BINK2SDK_LIBS="-lbink2sdk"
    echo "bink2sdk found: $BINK2SDK_ROOT/build_mingw/libbink2sdk.a"
else
    echo "WARNING: bink2sdk not built. Bink2 decoder will use built-in (no deblocking)."
    echo "  Run thirdparty/bink2sdk/build-bink2sdk.sh to build it."
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
echo "  - HAP: --enable-libsnappy + basis_universal (BC7/BC6H static link)"
echo "  - Vulkan: --enable-vulkan --enable-libshaderc"
echo "  - Network: --enable-libsrt --enable-openssl"
echo "  - Hardware Decode: --enable-d3d11va --enable-d3d12va"
echo "  - CFLAGS: -O3 $BASISU_CFLAGS"
echo ""

# Preflight: show which x265 will be used
echo "Step 2.5: Preflight x265 check..."
if command -v pkg-config >/dev/null 2>&1; then
    if pkg-config --exists x265; then
        echo "✓ pkg-config x265 version: $(pkg-config --modversion x265)"
        echo "✓ pkg-config x265 libs: $(pkg-config --libs x265)"
        echo "✓ pkg-config x265 cflags: $(pkg-config --cflags x265)"
    else
        echo "⚠️  pkg-config cannot find x265"
    fi
else
    echo "⚠️  pkg-config not found"
fi
if command -v /mingw64/bin/x265 >/dev/null 2>&1; then
    /mingw64/bin/x265 --help 2>/dev/null | grep -qi "alpha" \
        && echo "✓ /mingw64/bin/x265 reports alpha support" \
        || echo "⚠️  /mingw64/bin/x265 does not mention alpha"
else
    echo "⚠️  /mingw64/bin/x265 not found"
fi
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
    --enable-libsrt \
    --enable-openssl \
    --enable-vulkan \
    --enable-libshaderc \
    --enable-d3d11va \
    --enable-d3d12va \
    --extra-cflags="-O3 $BASISU_CFLAGS $BINK2SDK_CFLAGS" \
    --extra-ldflags="$BASISU_LDFLAGS $BINK2SDK_LDFLAGS" \
    --extra-libs="$BASISU_LIBS $BINK2SDK_LIBS"

echo ""
echo "Step 3: Verifying HAP and Bink2 are enabled..."
grep -q "CONFIG_HAP_ENCODER=yes" ffbuild/config.mak && echo "✓ HAP encoder enabled" || echo "✗ HAP encoder NOT enabled"
grep -q "CONFIG_HAP_DECODER=yes" ffbuild/config.mak && echo "✓ HAP decoder enabled" || echo "✗ HAP decoder NOT enabled"
grep -q "CONFIG_BINK2_DECODER=yes" ffbuild/config.mak && echo "✓ Bink2 decoder enabled" || echo "✗ Bink2 decoder NOT enabled"

echo ""
echo "Step 4: Building FFmpeg..."
CPU_CORES=$(nproc)
echo "Using $CPU_CORES CPU cores..."
make -j$CPU_CORES

echo ""
echo "Step 5: Installing to $FFMPEG_PREFIX..."
make install

echo ""
echo "Step 6: basis_universal is statically linked, no runtime DLL needed."

echo ""
echo "=========================================="
echo "✓ Build Complete!"
echo "=========================================="
echo ""
echo "FFmpeg - Alternative Development Edition"
echo "Build type: SHARED DLLs (bundle required)"
echo "Build features:"
echo "  - Hap R support (FourCC: 'Hap7') - BC7 via basis_universal"
echo "  - Hap H support (FourCC: 'HapH') - BC6H via basis_universal"
echo "  - HapM support (FourCC: 'HapM')"
echo "  - HapA support (FourCC: 'HapA')"
echo "  - Fixed YCoCg color transform"
echo "  - Bink video 2 decoder (binkvideo2)"
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
echo "Testing Bink2 decoder:"
"$FFMPEG_BIN/ffmpeg" -hide_banner -decoders 2>/dev/null | grep binkvideo2 || echo "Bink2 not found in decoders"

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
