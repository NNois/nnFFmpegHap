#!/bin/bash
# Verification script for your FFmpeg build

echo "========================================"
echo "FFmpeg Build Verification"
echo "========================================"
echo ""

echo "1. FFmpeg Version:"
echo "-------------------"
./ffmpeg -version | head -6
echo ""

echo "2. HAP Encoder Support:"
echo "-----------------------"
./ffmpeg -hide_banner -encoders 2>/dev/null | grep "hap"
echo ""

echo "3. HAP Encoder Options:"
echo "-----------------------"
./ffmpeg -hide_banner -h encoder=hap 2>/dev/null | grep -A 15 "Encoder hap"
echo ""

echo "4. All Requested Codecs:"
echo "------------------------"
echo "Checking for:"
./ffmpeg -hide_banner -codecs 2>/dev/null | grep -E " (h264|hevc|vp8|vp9|exr|qtrle)" | head -10
echo ""
echo "Audio codecs:"
./ffmpeg -hide_banner -codecs 2>/dev/null | grep -E "(vorbis|opus|mp3)" | head -10
echo ""

echo "5. Library Configuration:"
echo "-------------------------"
./ffmpeg -hide_banner -buildconf 2>/dev/null | grep -E "enable-lib"
echo ""

echo "========================================"
echo "To test HAP-Q encoding with your fix:"
echo "========================================"
echo "./ffmpeg -i input.mov -c:v hap -format hap_q output.mov"
echo ""
