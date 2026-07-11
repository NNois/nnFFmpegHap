#!/bin/bash
# Install all MSYS2 dependencies for FFmpeg compilation with HAP support
# FFmpeg -  ♥♥ Alternative Development Broadcast Edition ♥♥ - 8.1.2
# Run this in MSYS2 MINGW64 terminal

set -e

echo "=========================================="
echo "FFmpeg -  ♥♥ Alternative Development Broadcast Edition ♥♥ - 8.1.2"
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

# Ask about x265 alpha support
echo ""
echo "⚠️  IMPORTANT: x265 Alpha Channel Support"
echo ""
echo "The MSYS2 package 'mingw-w64-x86_64-x265' does NOT support alpha channels."
echo "If you need H.265 with alpha (yuva420p, yuva420p10le), you must build x265 from source."
echo ""
echo "Options:"
echo "  1) Install MSYS2 x265 (NO alpha support, quick install)"
echo "  2) Build x265 from source (WITH alpha support, takes ~5 minutes)"
echo ""
read -p "Choose [1/2] (default: 1): " X265_CHOICE
X265_CHOICE=${X265_CHOICE:-1}

if [ "$X265_CHOICE" = "2" ]; then
    echo ""
    echo "✓ Will build x265 with alpha support after installing other dependencies"
    BUILD_X265_FROM_SOURCE=true
else
    echo ""
    echo "✓ Installing MSYS2 x265 package (no alpha support)"
    BUILD_X265_FROM_SOURCE=false
fi

echo ""
if [ "$BUILD_X265_FROM_SOURCE" = "false" ]; then
    pacman -S --needed --noconfirm \
        mingw-w64-x86_64-snappy \
        mingw-w64-x86_64-x264 \
        mingw-w64-x86_64-x265 \
        mingw-w64-x86_64-libvpx \
        mingw-w64-x86_64-aom \
        mingw-w64-x86_64-svt-av1 \
        mingw-w64-x86_64-dav1d \
        mingw-w64-x86_64-libvorbis \
        mingw-w64-x86_64-opus \
        mingw-w64-x86_64-lame \
        mingw-w64-x86_64-fdk-aac
else
    # Install all codecs EXCEPT x265 (we'll build it from source)
    pacman -S --needed --noconfirm \
        mingw-w64-x86_64-snappy \
        mingw-w64-x86_64-x264 \
        mingw-w64-x86_64-libvpx \
        mingw-w64-x86_64-aom \
        mingw-w64-x86_64-svt-av1 \
        mingw-w64-x86_64-dav1d \
        mingw-w64-x86_64-libvorbis \
        mingw-w64-x86_64-opus \
        mingw-w64-x86_64-lame \
        mingw-w64-x86_64-fdk-aac
fi

echo ""
echo "Step 4: Installing additional libraries..."
pacman -S --needed --noconfirm \
    mingw-w64-x86_64-openexr \
    mingw-w64-x86_64-libwebp \
    mingw-w64-x86_64-SDL2 \
    mingw-w64-x86_64-zlib \
    mingw-w64-x86_64-bzip2 \
    mingw-w64-x86_64-zimg \
    mingw-w64-x86_64-srt

echo ""
echo "Step 5: Installing Vulkan libraries..."
pacman -S --needed --noconfirm \
    mingw-w64-x86_64-vulkan-headers \
    mingw-w64-x86_64-vulkan-loader \
    mingw-w64-x86_64-shaderc

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
if [ "$BUILD_X265_FROM_SOURCE" = "true" ]; then
    echo "    - x265 (H.265 encoder) - WILL BE BUILT WITH ALPHA SUPPORT ⭐"
else
    echo "    - x265 (H.265 encoder) - NO alpha support"
fi
echo "    - libvpx (VP8/VP9 encoder)"
echo "    - libaom (AV1 reference encoder)"
echo "    - SVT-AV1 (fast AV1 encoder)"
echo "    - dav1d (fast AV1 decoder)"
echo "    - libvorbis (Vorbis audio)"
echo "    - Opus (Opus audio)"
echo "    - LAME (MP3 encoder)"
echo "    - FDK-AAC (high-quality AAC encoder)"
echo ""
echo "  Additional Libraries:"
echo "    - OpenEXR (EXR support)"
echo "    - libwebp (WebP support)"
echo "    - SDL2 (video playback for ffplay)"
echo "    - zlib & bzip2 (compression)"
echo "    - zimg (zscale filter for high-quality scaling)"
echo "    - libsrt (Secure Reliable Transport protocol)"
echo ""
echo "  Vulkan Libraries:"
echo "    - Vulkan headers & loader (GPU acceleration)"
echo "    - Shaderc (SPIR-V shader compiler)"
echo ""
echo ""

if [ "$BUILD_X265_FROM_SOURCE" = "true" ]; then
    echo "Next steps:"
    echo "  1) ./build-msys-prepare-x265-with-alpha.sh  (build x265 with alpha support)"
    echo "  2) ./build-msys-shared.sh           (build FFmpeg)"
    echo ""
    echo "Run x265 build now? (Y/n)"
    read -r RUN_X265
    if [ -z "$RUN_X265" ] || [ "$RUN_X265" = "y" ] || [ "$RUN_X265" = "Y" ]; then
        ./build-msys-prepare-x265-with-alpha.sh
    fi
else
    echo "You can now build FFmpeg with:"
    echo "  ./build-msys-shared.sh"
    echo ""
    echo "Run it now? (Y/n)"
    read -r RUN_BUILD
    if [ -z "$RUN_BUILD" ] || [ "$RUN_BUILD" = "y" ] || [ "$RUN_BUILD" = "Y" ]; then
        ./build-msys-shared.sh
    fi
fi
echo ""
