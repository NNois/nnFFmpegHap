#!/bin/bash
# FFmpeg Full Build Script with HAP, x264, x265, VPX, EXR, and audio codecs
# Run this in MSYS2 MINGW64 terminal

set -e  # Exit on error

echo "=================================="
echo "FFmpeg Full Build with All Codecs"
echo "=================================="
echo ""

# Install all required development libraries
echo "Step 1: Installing dependencies..."
echo ""

pacman -S --needed --noconfirm \
    mingw-w64-x86_64-gcc \
    mingw-w64-x86_64-nasm \
    mingw-w64-x86_64-yasm \
    mingw-w64-x86_64-pkg-config \
    mingw-w64-x86_64-x264 \
    mingw-w64-x86_64-x265 \
    mingw-w64-x86_64-libvpx \
    mingw-w64-x86_64-openexr \
    mingw-w64-x86_64-libvorbis \
    mingw-w64-x86_64-opus \
    mingw-w64-x86_64-lame \
    make \
    diffutils

echo ""
echo "Step 2: Checking available codecs..."
echo ""

# Check what's installed
echo "Checking for libraries:"
pkg-config --exists x264 && echo "  ✓ x264 found" || echo "  ✗ x264 NOT found"
pkg-config --exists x265 && echo "  ✓ x265 found" || echo "  ✗ x265 NOT found"
pkg-config --exists vpx && echo "  ✓ libvpx found" || echo "  ✗ libvpx NOT found"
pkg-config --exists OpenEXR && echo "  ✓ OpenEXR found" || echo "  ✗ OpenEXR NOT found"
pkg-config --exists vorbis && echo "  ✓ vorbis found" || echo "  ✗ vorbis NOT found"
pkg-config --exists opus && echo "  ✓ opus found" || echo "  ✗ opus NOT found"
echo "  - libmp3lame (LAME) from package"
echo "  ⚠ fdk-aac: Not available in MSYS2 (license issues)"
echo "  ✓ HAP: Built-in FFmpeg (with your patches!)"
echo "  ✓ qtrle: Built-in FFmpeg"

echo ""
echo "Step 3: Cleaning previous build..."
make clean 2>/dev/null || true

echo ""
echo "Step 4: Configuring FFmpeg..."
echo ""

./configure \
    --enable-gpl \
    --enable-version3 \
    --disable-debug \
    --enable-static \
    --disable-shared \
    --prefix=./build \
    --enable-encoder=hap \
    --enable-decoder=hap \
    --enable-libx264 \
    --enable-libx265 \
    --enable-libvpx \
    --enable-libopencore-amrnb \
    --enable-libopencore-amrwb \
    --enable-libvorbis \
    --enable-libopus \
    --enable-libmp3lame \
    --enable-encoder=qtrle \
    --enable-decoder=qtrle \
    --enable-decoder=exr \
    --enable-muxer=mov \
    --enable-demuxer=mov

echo ""
echo "Step 5: Building FFmpeg (this will take several minutes)..."
echo ""

CPU_CORES=$(nproc)
echo "Using $CPU_CORES CPU cores for compilation..."
make -j$CPU_CORES

echo ""
echo "Step 6: Installing to ./build directory..."
make install

echo ""
echo "=================================="
echo "✓ Build Complete!"
echo "=================================="
echo ""
echo "Executables created:"
echo "  - ffmpeg.exe"
echo "  - ffplay.exe"
echo "  - ffprobe.exe"
echo ""
echo "To verify HAP support, run:"
echo "  ./ffmpeg.exe -encoders | grep hap"
echo "  ./ffmpeg.exe -decoders | grep hap"
echo ""
echo "To test your YCoCg fix:"
echo "  ./ffmpeg.exe -i input.mov -c:v hap -format hap_q output.mov"
echo ""
