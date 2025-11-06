#!/bin/bash
# Minimal FFmpeg build with HAP support only

./configure \
    --disable-everything \
    --enable-encoder=hap \
    --enable-decoder=hap \
    --enable-muxer=mov \
    --enable-demuxer=mov \
    --enable-protocol=file \
    --enable-gpl \
    --disable-debug \
    --disable-shared \
    --enable-static

make -j$(nproc)

echo ""
echo "Minimal build complete!"
echo "Executables: ffmpeg.exe, ffplay.exe, ffprobe.exe"
