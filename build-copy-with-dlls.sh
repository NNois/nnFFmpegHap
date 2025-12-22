#!/bin/bash
# Copy FFmpeg executables with required DLLs to make them portable
# FFmpeg - Alternative Development Edition

set -e

FFMPEG_PREFIX="${FFMPEG_PREFIX:-./build}"
FFMPEG_BIN="$FFMPEG_PREFIX/bin"

DEST_DIR="${1:-/c/AD/nnTools/tools/ffmpeg}"

if [ ! -d "$DEST_DIR" ]; then
    echo "Error: Destination directory does not exist: $DEST_DIR"
    exit 1
fi
if [ ! -d "$FFMPEG_BIN" ]; then
    echo "Error: FFmpeg install bin not found: $FFMPEG_BIN"
    exit 1
fi

echo "=========================================="
echo "FFmpeg - Alternative Development Edition"
echo "Copying with DLLs"
echo "=========================================="
echo ""

# Copy executables
echo "Copying executables..."
cp -v "$FFMPEG_BIN/ffmpeg.exe" "$DEST_DIR/"
cp -v "$FFMPEG_BIN/ffplay.exe" "$DEST_DIR/"
cp -v "$FFMPEG_BIN/ffprobe.exe" "$DEST_DIR/"

# Copy DLLs - check both local directory and mingw64
echo ""
echo "Copying required DLLs..."

# First, copy any DLLs from the install bin dir
shopt -s nullglob
FFMPEG_DLLS=("$FFMPEG_BIN"/*.dll)
if [ ${#FFMPEG_DLLS[@]} -gt 0 ]; then
    echo "Found DLLs in $FFMPEG_BIN:"
    for dll in "${FFMPEG_DLLS[@]}"; do
        cp -v "$dll" "$DEST_DIR/"
    done
else
    echo "No DLLs found in $FFMPEG_BIN"
fi

# Always copy dependencies from MINGW64
echo "Copying MINGW64 dependencies..."
REQUIRED_DLLS=$(ldd "$FFMPEG_BIN/ffprobe.exe" "$FFMPEG_BIN/ffplay.exe" 2>/dev/null | grep mingw64 | awk '{print $3}' | sort -u)
for dll in $REQUIRED_DLLS; do
    if [ -f "$dll" ]; then
        cp -v "$dll" "$DEST_DIR/"
    fi
done
shopt -u nullglob

# Ensure SDL2.dll is included for ffplay (not linked by ffprobe)
if [ ! -f "$DEST_DIR/SDL2.dll" ]; then
    for sdl in /mingw64/bin/SDL2.dll /c/msys64/mingw64/bin/SDL2.dll; do
        if [ -f "$sdl" ]; then
            cp -v "$sdl" "$DEST_DIR/"
            break
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
