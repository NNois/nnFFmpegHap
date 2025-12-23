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
cp -v ffmpeg.exe "$DEST_DIR/"
cp -v ffplay.exe "$DEST_DIR/"
cp -v ffprobe.exe "$DEST_DIR/"

# Copy DLLs - check both local directory and mingw64
echo ""
echo "Copying required DLLs..."

copy_dlls_from_dir_or_ldd() {
    local dll_dir="$1"
    local label="$2"
    shift 2
    local ldd_targets=("$@")

    shopt -s nullglob
    local dlls=("$dll_dir"/*.dll)
    if [ ${#dlls[@]} -gt 0 ]; then
        echo "Found DLLs in $label:"
        for dll in "${dlls[@]}"; do
            cp -v "$dll" "$DEST_DIR/"
        done
    else
        echo "No DLLs found in $label; finding DLLs from MINGW64..."
        local required_dlls
        required_dlls=$(ldd "${ldd_targets[@]}" 2>/dev/null | grep mingw64 | awk '{print $3}' | sort -u)
        for dll in $required_dlls; do
            if [ -f "$dll" ]; then
                cp -v "$dll" "$DEST_DIR/"
            fi
        done
    fi
    shopt -u nullglob
}

# Copy DLLs from the install bin dir, then from the local build dir.
copy_dlls_from_dir_or_ldd "$FFMPEG_BIN" "$FFMPEG_BIN" "$FFMPEG_BIN/ffprobe.exe" "$FFMPEG_BIN/ffplay.exe"
copy_dlls_from_dir_or_ldd "." "build directory" "./ffprobe.exe" "./ffplay.exe"

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

# Ask if user wants to build mpv now
if [ -d "/c/ff/mpv" ]; then
    echo "=========================================="
    echo "Build mpv with updated FFmpeg?"
    echo "=========================================="
    read -p "Do you want to build mpv now? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo ""
        echo "Starting mpv build..."
        cd /c/ff/mpv
        if [ -f "./build-mpv.sh" ]; then
            ./build-mpv.sh
        else
            echo "Warning: build-mpv.sh not found in /c/ff/mpv"
            echo "You can build mpv manually by running:"
            echo "  cd /c/ff/mpv && meson compile -C build"
        fi
    else
        echo "Skipping mpv build."
        echo "To build mpv later, run:"
        echo "  cd /c/ff/mpv && ./build-mpv.sh"
    fi
fi

