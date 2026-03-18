#!/bin/bash
# HAP Encoder Adobe CC - version 8
# Uses pre-built FFmpeg 4.x vcpkg static package (no FFmpeg build from source)
# Requires: Visual Studio 2022 env (via build-msvc-adobe.bat) + MSYS2/Git Bash

set -e

FFMPEG_SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ADOBE_DIR="/c/ff/hap-encoder-adobe-cc"
BUILD_ROOT="$FFMPEG_SRC/build-msvc-adobe"
BUILD_DIR="$BUILD_ROOT/plugin"
LOG_FILE="$FFMPEG_SRC/build-msvc-adobe.log"

# Pre-built FFmpeg 4.x vcpkg static package (avformat/avcodec/avutil only, no codec deps)
VCPKG_FFMPEG="/c/ff/ffmpeg-2021-12-07-0-lgpl21-x64-windows-static/installed/x64-windows-static"

# Setup logging
mkdir -p "$BUILD_ROOT"
: > "$LOG_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1

export MSYS2_ARG_CONV_EXCL="*"

echo "=========================================="
echo "HAP Encoder Adobe CC - Minimal Build v8"
echo "=========================================="
echo "Started: $(date)"
echo ""

# --- Step 0: Check toolchain ---
echo "Step 0: Checking toolchain..."
if ! command -v cl >/dev/null 2>&1; then
  echo "ERROR: cl.exe not found in PATH."
  echo "Run via build-msvc-adobe.bat which sources vcvars64.bat."
  exit 1
fi
LINK_PATH=$(command -v link || true)
if [ -n "$LINK_PATH" ] && echo "$LINK_PATH" | grep -qi "/usr/bin/link"; then
  echo "ERROR: MSYS link.exe shadows MSVC link.exe ($LINK_PATH)."
  exit 1
fi
if [ ! -d "$ADOBE_DIR" ]; then
  echo "ERROR: Adobe project not found at $ADOBE_DIR"
  exit 1
fi
if [ ! -f "$VCPKG_FFMPEG/lib/avcodec.lib" ]; then
  echo "ERROR: FFmpeg vcpkg package not found at $VCPKG_FFMPEG"
  exit 1
fi
echo "  cl.exe: $(command -v cl)"
echo "  Adobe project: $ADOBE_DIR"
echo "  FFmpeg vcpkg: $VCPKG_FFMPEG"

# --- Step 1: Prepare FFmpeg install dir from vcpkg ---
echo ""
echo "Step 1: Setting up FFmpeg from vcpkg package..."

FFMPEG_INSTALL="$BUILD_ROOT/ffmpeg-minimal"
mkdir -p "$FFMPEG_INSTALL/lib"
mkdir -p "$FFMPEG_INSTALL/include"

# Copy only the 3 libs we need
cp "$VCPKG_FFMPEG/lib/avcodec.lib"  "$FFMPEG_INSTALL/lib/"
cp "$VCPKG_FFMPEG/lib/avformat.lib" "$FFMPEG_INSTALL/lib/"
cp "$VCPKG_FFMPEG/lib/avutil.lib"   "$FFMPEG_INSTALL/lib/"

# Copy lame_stub.obj (satisfies mp3lame/mpglib symbols in avcodec)
if [ -f "$VCPKG_FFMPEG/lib/lame_stub.obj" ]; then
  cp "$VCPKG_FFMPEG/lib/lame_stub.obj" "$FFMPEG_INSTALL/lib/"
  echo "  Copied lame_stub.obj"
fi

# Copy headers
cp -r "$VCPKG_FFMPEG/include/libavcodec"  "$FFMPEG_INSTALL/include/"
cp -r "$VCPKG_FFMPEG/include/libavformat" "$FFMPEG_INSTALL/include/"
cp -r "$VCPKG_FFMPEG/include/libavutil"   "$FFMPEG_INSTALL/include/"

# Copy internal.h stub (needed by movie_writer.cpp, not installed by vcpkg)
INTERNAL_STUB="$ADOBE_DIR/external/foundation/external/ffmpeg/FFmpeg/libavformat/internal.h"
if [ -f "$INTERNAL_STUB" ]; then
  cp "$INTERNAL_STUB" "$FFMPEG_INSTALL/include/libavformat/internal.h"
  echo "  Copied internal.h stub"
fi

echo "  FFmpeg install ready: $FFMPEG_INSTALL"

# --- Step 2: Build the plugin ---
echo ""
echo "Step 2: Building HAP Encoder Adobe CC plugin..."
cd "$ADOBE_DIR"

if [ -d "$BUILD_DIR" ]; then
  echo "  Cleaning previous build..."
  rm -rf "$BUILD_DIR"
fi

BUILD_DIR_WIN="$(cygpath -m "$BUILD_DIR")"
ADOBE_DIR_WIN="$(cygpath -m "$ADOBE_DIR")"
FFMPEG_INSTALL_WIN="$(cygpath -m "$FFMPEG_INSTALL")"

cmake -B "$BUILD_DIR_WIN" \
      -G "Visual Studio 17 2022" \
      -A x64 \
      -S "$ADOBE_DIR_WIN" \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
      -DFFMPEG_MINIMAL_INSTALL="$FFMPEG_INSTALL_WIN"

cmake --build "$BUILD_DIR_WIN" --config Release --parallel

echo ""
echo "=========================================="
echo "Build SUCCEEDED: $(date)"
echo "=========================================="
echo ""
echo "Plugin output: $BUILD_DIR/Release/"
echo "Log: $LOG_FILE"
