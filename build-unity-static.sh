#!/bin/bash
# Minimal static FFmpeg build for Unity plugin usage
# Focus: demux + decoders for H.264/H.265/VP8/VP9/AV1 + image sequences (png/jpg)
# Audio: PCM, AAC (libfdk-aac), Vorbis

set -e

LOG_FILE="${FFMPEG_BUILD_LOG:-build-unity-static.log}"
: > "$LOG_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1

echo "=========================================="
echo "FFmpeg - Unity Static Minimal Build"
echo "=========================================="
echo ""

echo "Step 1: Cleaning previous build..."
make clean 2>/dev/null || true
rm -f config.h config.log ffbuild/config.mak 2>/dev/null || true
rm -rf ./build-unity-static 2>/dev/null || true

echo ""
echo "Step 2: Configuring minimal static build..."

target_prefix="./build-unity-static"

./configure \
    --disable-everything \
    --enable-static \
    --disable-shared \
    --disable-programs \
    --enable-avformat \
    --enable-avcodec \
    --enable-avutil \
    --enable-demuxer=mov,mp4,matroska,webm,image2,image2pipe,ogg \
    --enable-decoder=h264,hevc,vp8,vp9,av1,png,mjpeg,pcm_s16le,pcm_s24le,pcm_s32le,pcm_f32le,pcm_f64le,libfdk_aac,vorbis \
    --enable-parser=h264,hevc,vp8,vp9,av1,vorbis \
    --enable-protocol=file,pipe \
    --enable-libfdk-aac \
    --enable-libvorbis \
    --enable-nonfree \
    --prefix="$target_prefix" \
    --extra-cflags="-O3" \
    --extra-ldflags=""

echo ""
echo "Step 3: Building FFmpeg (static libs only)..."
CPU_CORES=$(nproc)
make -j"$CPU_CORES"
make install

echo ""
echo "=========================================="
echo "✓ Unity static build complete"
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
echo "  - DLLs (if any) -> $UNITY_PACKAGE_WINDOWS"
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
    echo "Step 5: Copying DLLs (if any)..."
    if [ -d "$target_prefix/bin" ]; then
        mkdir -p "$UNITY_PACKAGE_WINDOWS"
        cp -v "$target_prefix/bin/"*.dll "$UNITY_PACKAGE_WINDOWS/" 2>/dev/null || true
    else
        echo "Note: $target_prefix/bin not found (static build usually has no DLLs)"
    fi
    echo ""
    echo "✓ Copy complete"
fi
