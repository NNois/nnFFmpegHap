#!/bin/bash
# Copy FFmpeg MSVC build artifacts to Unity project
# Can be run standalone after build-msvc-exe.sh

set -e

echo "=========================================="
echo "Copy FFmpeg MSVC artifacts to Unity"
echo "=========================================="
echo ""

# Source paths
FFMPEG_PREFIX="./build-msvc"
ZLIB_INSTALL="/c/ff/ff/build-msvc/zlib-msvc"
VPX_INSTALL="/c/ff/ff/build-msvc/libvpx-msvc"
DAV1D_INSTALL="/c/ff/ff/build-msvc/dav1d-msvc"

# Destination path
UNITY_PROJECT_WIN="${UNITY_PROJECT_WIN:-J:\\2500_AdToolBox\\AdUnityPackages\\AdPlayer}"
UNITY_PROJECT_ROOT="$(cygpath -u "$UNITY_PROJECT_WIN" 2>/dev/null || echo "/j/2500_AdToolBox/AdUnityPackages/AdPlayer")"
UNITY_PLUGIN_FFMPEG="$UNITY_PROJECT_ROOT/Plugin/FFmpeg"

echo "Source: $FFMPEG_PREFIX"
echo "Destination: $UNITY_PLUGIN_FFMPEG"
echo ""

# Check if FFmpeg build exists
if [ ! -d "$FFMPEG_PREFIX/lib" ]; then
    echo "ERROR: FFmpeg build not found at $FFMPEG_PREFIX"
    echo "Run build-msvc-exe.bat first."
    exit 1
fi

# Create destination directories
mkdir -p "$UNITY_PLUGIN_FFMPEG/include" "$UNITY_PLUGIN_FFMPEG/lib"

echo "Copying FFmpeg headers and libs..."
if [ -d "$FFMPEG_PREFIX/include" ]; then
    cp -R "$FFMPEG_PREFIX/include/"* "$UNITY_PLUGIN_FFMPEG/include/"
    echo "  FFmpeg include: OK"
fi
if [ -d "$FFMPEG_PREFIX/lib" ]; then
    cp -R "$FFMPEG_PREFIX/lib/"* "$UNITY_PLUGIN_FFMPEG/lib/"
    echo "  FFmpeg lib: OK"
fi

echo "Copying zlib headers and libs..."
if [ -d "$ZLIB_INSTALL/include" ]; then
    cp -R "$ZLIB_INSTALL/include/"* "$UNITY_PLUGIN_FFMPEG/include/"
    echo "  zlib include: OK"
else
    echo "  zlib include: NOT FOUND"
fi
if [ -d "$ZLIB_INSTALL/lib" ]; then
    cp -R "$ZLIB_INSTALL/lib/"* "$UNITY_PLUGIN_FFMPEG/lib/"
    echo "  zlib lib: OK"
else
    echo "  zlib lib: NOT FOUND"
fi

echo "Copying libvpx headers and libs..."
if [ -d "$VPX_INSTALL/include" ]; then
    cp -R "$VPX_INSTALL/include/"* "$UNITY_PLUGIN_FFMPEG/include/"
    echo "  libvpx include: OK"
else
    echo "  libvpx include: NOT FOUND"
fi
if [ -d "$VPX_INSTALL/lib" ]; then
    cp -R "$VPX_INSTALL/lib/"* "$UNITY_PLUGIN_FFMPEG/lib/"
    echo "  libvpx lib: OK"
else
    echo "  libvpx lib: NOT FOUND"
fi

echo "Copying dav1d headers and libs..."
if [ -d "$DAV1D_INSTALL/include" ]; then
    cp -R "$DAV1D_INSTALL/include/"* "$UNITY_PLUGIN_FFMPEG/include/"
    echo "  dav1d include: OK"
else
    echo "  dav1d include: NOT FOUND (optional)"
fi
if [ -d "$DAV1D_INSTALL/lib" ]; then
    cp -R "$DAV1D_INSTALL/lib/"* "$UNITY_PLUGIN_FFMPEG/lib/"
    echo "  dav1d lib: OK"
else
    echo "  dav1d lib: NOT FOUND (optional)"
fi

echo ""
echo "=========================================="
echo "✓ Copy complete"
echo "=========================================="
echo ""
echo "Destination: $UNITY_PLUGIN_FFMPEG"
