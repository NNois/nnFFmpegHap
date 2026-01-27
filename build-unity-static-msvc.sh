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
echo "Step 1: Cleaning previous FFmpeg build..."
make clean 2>/dev/null || true
rm -f config.h config.log ffbuild/config.mak 2>/dev/null || true

echo ""
echo "Step 2: Cleaning and rebuilding zlib with MSVC..."
# Always rebuild zlib to ensure fresh build with patches
cmd.exe /c "C:\\ff\\ff\\build-zlib-msvc.bat"
ZLIB_INSTALL="/c/ff/ff/build-unity-static-msvc/zlib-msvc"
if [ ! -f "$ZLIB_INSTALL/lib/zlib.lib" ] && [ ! -f "$ZLIB_INSTALL/lib/zlibstatic.lib" ]; then
  echo "ERROR: zlib build failed"
  exit 1
fi

echo ""
echo "Step 3: Cleaning and rebuilding libvpx with MSVC..."
cmd.exe /c "C:\\ff\\ff\\build-libvpx-msvc.bat"
VPX_INSTALL="/c/ff/ff/build-unity-static-msvc/libvpx-msvc"

# Rename vpxmd.lib to vpx.lib for FFmpeg compatibility
if [ -f "$VPX_INSTALL/lib/x64/vpxmd.lib" ] && [ ! -f "$VPX_INSTALL/lib/x64/vpx.lib" ]; then
  echo "Copying vpxmd.lib to vpx.lib for FFmpeg detection..."
  cp "$VPX_INSTALL/lib/x64/vpxmd.lib" "$VPX_INSTALL/lib/x64/vpx.lib"
fi

if [ ! -f "$VPX_INSTALL/lib/x64/vpx.lib" ] && [ ! -f "$VPX_INSTALL/lib/vpx.lib" ] && [ ! -f "$VPX_INSTALL/lib/libvpx.lib" ]; then
  echo "ERROR: libvpx build failed or vpx.lib not found"
  exit 1
fi

echo ""
echo "Step 4: Configuring minimal static build (MSVC toolchain)..."

target_prefix="./build-unity-static-msvc"

# Convert MSYS2 paths to Windows paths for MSVC
ZLIB_INSTALL_WIN="C:/ff/ff/build-unity-static-msvc/zlib-msvc"
ZLIB_CFLAGS="-IC:/ff/ff/build-unity-static-msvc/zlib-msvc/include"
ZLIB_LDFLAGS="-LIBPATH:C:/ff/ff/build-unity-static-msvc/zlib-msvc/lib"

# libvpx (required for VP8/VP9 with alpha)
VPX_ROOT_WIN="${VPX_ROOT:-C:\\ff\\ff\\build-unity-static-msvc\\libvpx-msvc}"
VPX_ROOT_UNIX="$(cygpath -u "$VPX_ROOT_WIN" 2>/dev/null || echo "")"
if [ -z "$VPX_ROOT_UNIX" ] || { [ ! -f "$VPX_ROOT_UNIX/lib/vpx.lib" ] && [ ! -f "$VPX_ROOT_UNIX/lib/libvpx.lib" ] && [ ! -f "$VPX_ROOT_UNIX/lib/x64/vpxmd.lib" ]; }; then
  echo "ERROR: libvpx not found. Build libvpx with MSVC and set VPX_ROOT (default: C:\\ff\\libvpx-msvc)."
  exit 1
fi
VPX_CFLAGS="-IC:/ff/ff/build-unity-static-msvc/libvpx-msvc/include"
if [ -f "$VPX_ROOT_UNIX/lib/x64/vpxmd.lib" ]; then
  VPX_LDFLAGS="-LIBPATH:C:/ff/ff/build-unity-static-msvc/libvpx-msvc/lib/x64"
else
  VPX_LDFLAGS="-LIBPATH:C:/ff/ff/build-unity-static-msvc/libvpx-msvc/lib"
fi
# Add vpx lib and required CRT/system libs for FFmpeg's check_lib tests
# vpx.lib was built with /MD and needs msvcrt.lib, but FFmpeg's test doesn't link it automatically
VPX_EXTRA_LIBS="vpx.lib"

./configure \
    --toolchain=msvc \
    --target-os=win64 \
    --arch=x86_64 \
    --disable-everything \
    --enable-static \
    --disable-shared \
    --disable-programs \
    --disable-x86asm \
    --enable-libvpx \
    --enable-avformat \
    --enable-avcodec \
    --enable-avutil \
    --enable-swscale \
    --enable-swresample \
    --enable-demuxer=mov,mp4,matroska,webm,image2,image2pipe,ogg,mp3 \
    --disable-decoder=vp8,vp9 \
    --enable-decoder=h264,hevc,libvpx_vp8,libvpx_vp9,av1,hap,prores,png,mjpeg,targa,pcm_s16le,pcm_s24le,pcm_s32le,pcm_f32le,pcm_f64le,aac,vorbis,mp3,mp3float \
    --enable-parser=h264,hevc,vp8,vp9,av1,vorbis,aac,mpegaudio \
    --enable-protocol=file,pipe \
    --enable-hwaccel=h264_d3d11va \
    --enable-hwaccel=h264_d3d11va2 \
    --enable-hwaccel=hevc_d3d11va \
    --enable-hwaccel=hevc_d3d11va2 \
    --enable-hwaccel=vp8_d3d11va \
    --enable-hwaccel=vp8_d3d11va2 \
    --enable-hwaccel=vp9_d3d11va \
    --enable-hwaccel=vp9_d3d11va2 \
    --enable-hwaccel=av1_d3d11va \
    --enable-hwaccel=av1_d3d11va2 \
    --enable-d3d11va \
    --enable-zlib \
    --disable-vulkan \
    --disable-libshaderc \
    --prefix="$target_prefix" \
    --extra-cflags="/O2 $ZLIB_CFLAGS $VPX_CFLAGS" \
    --extra-ldflags="$ZLIB_LDFLAGS $VPX_LDFLAGS" \
    --extra-libs="$VPX_EXTRA_LIBS"

echo ""
echo "Step 4: Building FFmpeg (static libs only)..."
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

echo "Copy artifacts to Unity project?"

echo "  - Headers+libs -> $UNITY_PLUGIN_FFMPEG"

echo "Run it now? (Y/n)"
read -r RUN_COPY
if [ -z "$RUN_COPY" ] || [ "$RUN_COPY" = "y" ] || [ "$RUN_COPY" = "Y" ]; then
    echo ""
    echo "Step 5: Copying headers and libs..."
    mkdir -p "$UNITY_PLUGIN_FFMPEG/include" "$UNITY_PLUGIN_FFMPEG/lib"

    # Copy FFmpeg headers and libs
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

    # Copy zlib headers and libs
    if [ -d "$ZLIB_INSTALL/include" ]; then
        cp -R "$ZLIB_INSTALL/include/"* "$UNITY_PLUGIN_FFMPEG/include/"
    else
        echo "Note: zlib include not found"
    fi
    if [ -d "$ZLIB_INSTALL/lib" ]; then
        cp -R "$ZLIB_INSTALL/lib/"* "$UNITY_PLUGIN_FFMPEG/lib/"
    else
        echo "Note: zlib lib not found"
    fi

    # Copy libvpx headers and libs
    if [ -d "$VPX_ROOT_UNIX/include" ]; then
        cp -R "$VPX_ROOT_UNIX/include/"* "$UNITY_PLUGIN_FFMPEG/include/"
    else
        echo "Note: libvpx include not found"
    fi
    if [ -d "$VPX_ROOT_UNIX/lib" ]; then
        cp -R "$VPX_ROOT_UNIX/lib/"* "$UNITY_PLUGIN_FFMPEG/lib/"
    else
        echo "Note: libvpx lib not found"
    fi

    echo ""
    echo "✓ Copy complete (FFmpeg + zlib + libvpx)"
fi

echo ""
echo "Run AdPlayer plugin build script?"
echo "  - $UNITY_PROJECT_ROOT/Plugin/Windows/build.sh"
echo "Run it now? (Y/n)"
read -r RUN_PLUGIN_BUILD
if [ -z "$RUN_PLUGIN_BUILD" ] || [ "$RUN_PLUGIN_BUILD" = "y" ] || [ "$RUN_PLUGIN_BUILD" = "Y" ]; then
    if [ -f "$UNITY_PROJECT_ROOT/Plugin/Windows/build.sh" ]; then
        bash "$UNITY_PROJECT_ROOT/Plugin/Windows/build.sh"
    else
        echo "Note: build.sh not found at $UNITY_PROJECT_ROOT/Plugin/Windows/build.sh"
    fi
fi
