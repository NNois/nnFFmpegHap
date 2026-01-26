#!/bin/bash
# MSVC-compatible minimal static FFmpeg build for Unity plugin usage
# Based on build-unity-static.sh, adapted for MSVC toolchain
# Requires: Visual Studio Build Tools env + MSYS2 shell

set -e

LOG_FILE="${FFMPEG_BUILD_LOG:-build-unity-static-msvc.log}"
: > "$LOG_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1

# Prevent MSYS2 from mangling MSVC-style /O2 flags into paths
export MSYS2_ARG_CONV_EXCL="*"

echo "=========================================="
echo "FFmpeg - Unity Static Minimal Build (MSVC)"
echo "=========================================="
echo ""

echo "Step 0: Checking toolchain..."
if ! command -v cl >/dev/null 2>&1; then
  echo "ERROR: cl.exe not found in PATH."
  echo "Launch MSYS2 from 'x64 Native Tools Command Prompt for VS' or run vcvars64.bat first."
  exit 1
fi

LINK_PATH=$(command -v link || true)
if [ -n "$LINK_PATH" ] && echo "$LINK_PATH" | grep -qi "/usr/bin/link"; then
  echo "ERROR: MSYS link.exe detected ($LINK_PATH)."
  echo "Remove/rename /usr/bin/link.exe or put VS link.exe earlier in PATH."
  exit 1
fi

echo "cl.exe: $(command -v cl)"
echo "link.exe: ${LINK_PATH:-not found}"

echo ""
echo "Step 1: Cleaning previous build..."
make clean 2>/dev/null || true
rm -f config.h config.log ffbuild/config.mak 2>/dev/null || true
rm -rf ./build-unity-static-msvc 2>/dev/null || true

echo ""
echo "Step 2: Configuring minimal static build (MSVC toolchain)..."

target_prefix="./build-unity-static-msvc"

ZLIB_CFLAGS=""
ZLIB_LDFLAGS=""

./configure \
    --toolchain=msvc \
    --target-os=win64 \
    --arch=x86_64 \
    --disable-everything \
    --enable-static \
    --disable-shared \
    --disable-programs \
    --disable-x86asm \
    --enable-avformat \
    --enable-avcodec \
    --enable-avutil \
    --enable-swscale \
    --enable-swresample \
    --enable-demuxer=mov,mp4,matroska,webm,image2,image2pipe,ogg \
    --enable-decoder=h264,hevc,vp8,vp9,av1,hap,mjpeg,pcm_s16le,pcm_s24le,pcm_s32le,pcm_f32le,pcm_f64le,aac,vorbis,mp3,mp3float \
    --enable-parser=h264,hevc,vp8,vp9,av1,vorbis,aac,mpegaudio \
    --enable-protocol=file,pipe \
    --disable-d3d11va \
    --disable-zlib \
    --disable-bzlib \
    --disable-iconv \
    --disable-lzma \
    --disable-libxml2 \
    --disable-vulkan \
    --disable-vaapi \
    --disable-vdpau \
    --disable-gnutls \
    --disable-openssl \
    --disable-schannel \
    --disable-securetransport \
    --prefix="$target_prefix" \
    --extra-cflags="/O2" \
    --extra-ldflags=""

echo ""
echo "Step 3: Building FFmpeg (static libs only)..."
CPU_CORES=$(nproc)
make -j"$CPU_CORES"
make install

echo ""
echo "=========================================="
echo "✓ Unity static MSVC build complete"
echo "=========================================="
echo ""
echo "Output: $target_prefix"
echo ""

UNITY_PROJECT_WIN="${UNITY_PROJECT_WIN:-J:\\2500_AdToolBox\\AdUnityPackages\\AdPlayer}"
UNITY_PROJECT_ROOT="$(cygpath -u "$UNITY_PROJECT_WIN" 2>/dev/null || echo "/j/2500_AdToolBox/AdUnityPackages/AdPlayer")"
UNITY_PLUGIN_FFMPEG="$UNITY_PROJECT_ROOT/Plugin/FFmpeg"
UNITY_PACKAGE_WINDOWS="$UNITY_PROJECT_ROOT/Packages/com.ad.player/Plugin/Windows"

echo "Copy artifacts to Unity project?"

echo "  - Headers+libs -> $UNITY_PLUGIN_FFMPEG"

echo "Run it now? (y/N)"
read -r RUN_COPY
if [ "$RUN_COPY" = "y" ] || [ "$RUN_COPY" = "Y" ]; then
    echo ""
    echo "Step 4: Copying headers and libs..."
    mkdir -p "$UNITY_PLUGIN_FFMPEG/include" "$UNITY_PLUGIN_FFMPEG/lib"
    if [ -d "$target_prefix/include" ]; then
        cp -R "$target_prefix/include/"* "$UNITY_PLUGIN_FFMPEG/include/"
    else
        echo "Note: $target_prefix/include not found"
    fi
    if [ -d "$target_prefix/lib" ]; then
        cp -R "$target_prefix/lib/"* "$UNITY_PLUGIN_FFMPEG/lib/"
    else
        echo "Note: $target_prefix/lib not found"
    fi

    echo ""
    echo "✓ Copy complete"
fi
