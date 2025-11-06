#!/bin/bash
# FFmpeg Windows build script

# Clean previous build
make clean 2>/dev/null || true

# Configure for Windows with HAP support
./configure \
    --arch=x86_64 \
    --target-os=mingw32 \
    --cross-prefix=x86_64-w64-mingw32- \
    --enable-gpl \
    --enable-version3 \
    --disable-debug \
    --enable-static \
    --disable-shared \
    --prefix=./build \
    --enable-encoder=hap \
    --enable-decoder=hap \
    --enable-protocol=file

# Build
make -j$(nproc)

# Install to build directory
make install

echo ""
echo "Build complete! Executables are in:"
echo "  - ffmpeg.exe"
echo "  - ffplay.exe" 
echo "  - ffprobe.exe"
