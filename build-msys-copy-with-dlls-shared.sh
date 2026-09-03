#!/bin/bash
# Copy FFmpeg executables with required DLLs to make them portable
# FFmpeg -  ♥♥ Alternative Development Broadcast Edition ♥♥ - 8.1.2

set -e

FFMPEG_PREFIX="${FFMPEG_PREFIX:-./build}"
FFMPEG_BIN="$FFMPEG_PREFIX/bin"
ISPCTEXCOMP_ROOT="${ISPCTEXCOMP_ROOT:-$PWD/thirdparty/ISPCTextureCompressor}"

# Explicit destination as $1 = copy there directly (no prompts). Without
# argument, each KNOWN destination is offered with a y/N prompt below.
DEST_DIR="${1:-}"
NNTOOLS_DIR="${NNTOOLS_DIR:-/c/AD/nnTools/tools/ffmpeg}"
FLOCON_DIR="${FLOCON_DIR:-/c/AD/AdFlocon/libs/ffmpeg}"

if [ -n "$DEST_DIR" ] && [ ! -d "$DEST_DIR" ]; then
    echo "Error: Destination directory does not exist: $DEST_DIR"
    exit 1
fi
if [ ! -d "$FFMPEG_BIN" ]; then
    echo "Error: FFmpeg install bin not found: $FFMPEG_BIN"
    exit 1
fi

echo "=========================================="
echo "FFmpeg -  ♥♥ Alternative Development Broadcast Edition ♥♥ - 8.1.2"
echo "Copying with DLLs"
echo "=========================================="
echo ""

copy_dlls_from_dir_or_ldd() {
    local dest="$1"
    local dll_dir="$2"
    local label="$3"
    shift 3
    local ldd_targets=("$@")

    shopt -s nullglob
    local dlls=("$dll_dir"/*.dll)
    if [ ${#dlls[@]} -gt 0 ]; then
        echo "Found DLLs in $label:"
        for dll in "${dlls[@]}"; do
            cp -v "$dll" "$dest/"
        done
    else
        echo "No DLLs found in $label; finding DLLs from MINGW64..."
        local required_dlls
        required_dlls=$(ldd "${ldd_targets[@]}" 2>/dev/null | grep mingw64 | awk '{print $3}' | sort -u)
        for dll in $required_dlls; do
            if [ -f "$dll" ]; then
                cp -v "$dll" "$dest/"
            fi
        done
    fi
    shopt -u nullglob
}

# The whole portable bundle (executables + DLLs + extras) into ONE directory.
copy_bundle() {
    local dest="$1"

    echo "Copying executables to $dest..."
    cp -v "$FFMPEG_BIN/ffmpeg.exe" "$dest/"
    cp -v "$FFMPEG_BIN/ffplay.exe" "$dest/"
    cp -v "$FFMPEG_BIN/ffprobe.exe" "$dest/"

    echo ""
    echo "Copying required DLLs..."
    copy_dlls_from_dir_or_ldd "$dest" "$FFMPEG_BIN" "$FFMPEG_BIN" "$FFMPEG_BIN/ffprobe.exe" "$FFMPEG_BIN/ffplay.exe"

    # Copy ISPCTextureCompressor runtime assets if present.
    if [ -f "$FFMPEG_BIN/ispc_texcomp.dll" ]; then
        cp -v "$FFMPEG_BIN/ispc_texcomp.dll" "$dest/"
    elif [ -f "$ISPCTEXCOMP_ROOT/lib/ispc_texcomp.dll" ]; then
        cp -v "$ISPCTEXCOMP_ROOT/lib/ispc_texcomp.dll" "$dest/"
    fi

    # Ensure SDL2.dll is included for ffplay (not linked by ffprobe)
    if [ ! -f "$dest/SDL2.dll" ]; then
        for sdl in /mingw64/bin/SDL2.dll /c/msys64/mingw64/bin/SDL2.dll; do
            if [ -f "$sdl" ]; then
                cp -v "$sdl" "$dest/"
                break
            fi
        done
    fi
}

report_dest() {
    local dest="$1"
    echo ""
    echo "✓ Copied to: $dest"
    ls -lh "$dest"/{ffmpeg.exe,ffplay.exe,ffprobe.exe,*.dll} 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}'
    echo ""
}

# Prompted copy towards one known destination (skipped if absent/declined).
offer_copy() {
    local dest="$1"
    local label="$2"
    if [ ! -d "$dest" ]; then
        echo "($label absent — ignoré : $dest)"
        return
    fi
    echo "=========================================="
    echo "$label : $dest"
    echo "=========================================="
    read -p "Copy into $label? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        copy_bundle "$dest"
        report_dest "$dest"
    else
        echo "Skipping $label (later: $0 \"$dest\")."
        echo ""
    fi
}

if [ -n "$DEST_DIR" ]; then
    # Explicit destination: direct copy, no questions asked.
    copy_bundle "$DEST_DIR"
    report_dest "$DEST_DIR"
else
    # AD tools (shared CLI toolbox) then AdFlocon libs/ffmpeg/ (the runtime the
    # app and its NSIS installer embed — mpv.exe there is built separately).
    offer_copy "$NNTOOLS_DIR" "AD tools (nnTools)"
    offer_copy "$FLOCON_DIR" "AdFlocon libs/ffmpeg"
fi

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

