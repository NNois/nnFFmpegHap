#!/bin/bash
# Copy FFmpeg executables with required DLLs to make them portable
# FFmpeg - Alternative Development Edition

set -e

DEST_DIR="$1"

if [ -z "$DEST_DIR" ]; then
    echo "Usage: ./copy-with-dlls.sh <destination_directory>"
    echo ""
    echo "Example: ./copy-with-dlls.sh /c/AD/nnTools/tools/ffmpeg"
    exit 1
fi

if [ ! -d "$DEST_DIR" ]; then
    echo "Error: Destination directory does not exist: $DEST_DIR"
    exit 1
fi

echo "=========================================="
echo "FFmpeg - Alternative Development Edition"
echo "Copying with DLLs"
echo "=========================================="
echo ""

# Copy executables
echo "Copying executables..."
cp -v ffmpeg.exe "$DEST_DIR/"
cp -v ffplay.exe "$DEST_DIR/"
cp -v ffprobe.exe "$DEST_DIR/"

# Copy DLLs - check both local directory and mingw64
echo ""
echo "Copying required DLLs..."

# First, copy any DLLs already in the current directory (from rebuild-static.sh)
LOCAL_DLLS=$(ls *.dll 2>/dev/null || true)
if [ -n "$LOCAL_DLLS" ]; then
    echo "Found DLLs in build directory:"
    for dll in *.dll; do
        if [ -f "$dll" ]; then
            cp -v "$dll" "$DEST_DIR/"
        fi
    done
else
    # If no local DLLs, find them from mingw64 (for rebuild-with-hap.sh builds)
    echo "Finding DLLs from MINGW64..."
    REQUIRED_DLLS=$(ldd ffprobe.exe | grep mingw64 | awk '{print $3}')
    for dll in $REQUIRED_DLLS; do
        if [ -f "$dll" ]; then
            cp -v "$dll" "$DEST_DIR/"
        fi
    done
fi

echo ""
echo "=========================================="
echo "✓ Copy Complete!"
echo "=========================================="
echo ""
echo "Copied to: $DEST_DIR"
echo ""
echo "Files copied:"
ls -lh "$DEST_DIR"/{ffmpeg.exe,ffplay.exe,ffprobe.exe,*.dll} 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}'
echo ""
echo "You can now use these executables from PowerShell or CMD."
echo ""
