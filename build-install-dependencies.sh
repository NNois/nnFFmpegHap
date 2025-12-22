#!/bin/bash
# Install all MSYS2 dependencies for FFmpeg compilation with HAP support
# FFmpeg - Alternative Development Edition
# Run this in MSYS2 MINGW64 terminal

set -e

echo "=========================================="
echo "FFmpeg - Alternative Development Edition"
echo "Build Dependencies Installer"
echo "=========================================="
echo ""

# Check if we're in MSYS2
if ! command -v pacman &> /dev/null; then
    echo "❌ ERROR: pacman not found!"
    echo ""
    echo "You must run this script in MSYS2 MINGW64 terminal."
    echo "Please open MSYS2 MINGW64 and try again."
    exit 1
fi

echo "✓ Running in MSYS2 environment"
echo ""

# Update package database
echo "Step 1: Updating package database..."
pacman -Sy --noconfirm

echo ""
echo "Step 2: Installing build tools..."
pacman -S --needed --noconfirm \
    mingw-w64-x86_64-gcc \
    mingw-w64-x86_64-yasm \
    mingw-w64-x86_64-nasm \
    mingw-w64-x86_64-cmake \
    mingw-w64-x86_64-pkgconf \
    make \
    diffutils

echo ""
echo "Step 3: Installing codec libraries..."
pacman -S --needed --noconfirm \
    mingw-w64-x86_64-snappy \
    mingw-w64-x86_64-x264 \
    mingw-w64-x86_64-x265 \
    mingw-w64-x86_64-libvpx \
    mingw-w64-x86_64-libvorbis \
    mingw-w64-x86_64-opus \
    mingw-w64-x86_64-lame

echo ""
echo "Step 4: Installing additional libraries..."
pacman -S --needed --noconfirm \
    mingw-w64-x86_64-openexr \
    mingw-w64-x86_64-libwebp \
    mingw-w64-x86_64-SDL2 \
    mingw-w64-x86_64-zlib \
    mingw-w64-x86_64-bzip2 \
    mingw-w64-x86_64-zimg

echo ""
echo ""
echo "=========================================="
echo "✓ All Dependencies Installed!"
echo "=========================================="
echo ""
echo "Installed packages:"
echo "  Build Tools:"
echo "    - GCC compiler"
echo "    - YASM & NASM assemblers"
echo "    - CMake & pkgconf"
echo ""
echo "  Codec Libraries:"
echo "    - Snappy (for HAP encoder) ⭐"
echo "    - x264 (H.264 encoder)"
echo "    - x265 (H.265 encoder)"
echo "    - libvpx (VP8/VP9 encoder)"
echo "    - libvorbis (Vorbis audio)"
echo "    - Opus (Opus audio)"
echo "    - LAME (MP3 encoder)"
echo ""
echo "  Additional Libraries:"
echo "    - OpenEXR (EXR support)"
echo "    - libwebp (WebP support)"
echo "    - SDL2 (video playback for ffplay)"
echo "    - zlib & bzip2 (compression)"
echo "    - zimg (zscale filter for high-quality scaling)"
echo ""
echo ""
echo "You can now build FFmpeg with:"
echo "  ./build-ffmpeg.sh"
echo ""
echo "Run it now? (Y/n)"
read -r RUN_BUILD
if [ -z "$RUN_BUILD" ] || [ "$RUN_BUILD" = "y" ] || [ "$RUN_BUILD" = "Y" ]; then
    ./build-ffmpeg.sh
fi
echo ""
