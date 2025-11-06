#!/bin/bash
# Cross-compile for Windows from Linux/WSL

# Install dependencies (Ubuntu/Debian):
# sudo apt-get install build-essential mingw-w64 yasm nasm

./configure \
    --arch=x86_64 \
    --target-os=mingw32 \
    --cross-prefix=x86_64-w64-mingw32- \
    --enable-cross-compile \
    --enable-gpl \
    --enable-version3 \
    --disable-debug \
    --enable-static \
    --disable-shared \
    --pkg-config=pkg-config \
    --prefix=./build-win

make -j$(nproc)
make install

echo "Windows executables built in ./build-win/bin/"
