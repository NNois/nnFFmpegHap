#!/usr/bin/env bash
# Apply HAP patch series to any FFmpeg source tree.

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"

is_ffmpeg_src() {
    [ -f "$1/configure" ] && [ -d "$1/libavcodec" ] && [ -d "$1/libavformat" ]
}

usage() {
    echo "Usage: $0 /path/to/ffmpeg [path/to/patches]"
    echo ""
    echo "Defaults:"
    echo "  - ffmpeg src: current directory if it looks like FFmpeg"
    echo "  - patches: directory containing 0001..0004 .patch files"
}

FFMPEG_SRC=""
PATCHES_DIR=""

if [ $# -ge 1 ]; then
    FFMPEG_SRC="$1"
elif is_ffmpeg_src "$PWD"; then
    FFMPEG_SRC="$PWD"
elif is_ffmpeg_src "$SCRIPT_DIR/../.."; then
    FFMPEG_SRC="$SCRIPT_DIR/../.."
fi

if [ $# -ge 2 ]; then
    PATCHES_DIR="$2"
elif [ -f "$SCRIPT_DIR/0001-hapq-ycocg-transform.patch" ]; then
    PATCHES_DIR="$SCRIPT_DIR"
elif [ -f "$SCRIPT_DIR/nnHapWork/0001-hapq-ycocg-transform.patch" ]; then
    PATCHES_DIR="$SCRIPT_DIR/nnHapWork"
elif [ -f "$SCRIPT_DIR/ff/nnHapWork/0001-hapq-ycocg-transform.patch" ]; then
    PATCHES_DIR="$SCRIPT_DIR/ff/nnHapWork"
fi

if [ -z "$FFMPEG_SRC" ] || ! is_ffmpeg_src "$FFMPEG_SRC"; then
    echo "Error: FFmpeg source not found."
    usage
    exit 1
fi

if [ -z "$PATCHES_DIR" ] || [ ! -d "$PATCHES_DIR" ]; then
    echo "Error: Patch directory not found."
    usage
    exit 1
fi

PATCHES=(
    "0001-hapq-ycocg-transform.patch"
    "0002-hapa-alpha-only.patch"
    "0003-hapm-qalpha.patch"
    "0004-hap7-bc7.patch"
)

echo "========================================="
echo "Applying HAP patch series"
echo "========================================="
echo "FFmpeg source: $FFMPEG_SRC"
echo "Patch dir:     $PATCHES_DIR"
echo ""

missing=0
for p in "${PATCHES[@]}"; do
    if [ ! -f "$PATCHES_DIR/$p" ]; then
        echo "Missing patch: $PATCHES_DIR/$p"
        missing=1
    fi
done
if [ "$missing" -ne 0 ]; then
    exit 1
fi

if ! command -v git >/dev/null 2>&1; then
    echo "Error: git not found (required for git apply)."
    exit 1
fi

echo "Step 1: Removing old bc7enc16 files if present..."
rm -f "$FFMPEG_SRC/libavcodec/bc7enc16.c" "$FFMPEG_SRC/libavcodec/bc7enc16.h"
echo "V Cleanup done"
echo ""

echo "Step 2: Applying patches..."
for p in "${PATCHES[@]}"; do
    patch_path="$PATCHES_DIR/$p"
    if git -C "$FFMPEG_SRC" apply --reverse --check "$patch_path" >/dev/null 2>&1; then
        echo "  - $p already applied (skipping)"
        continue
    fi
    git -C "$FFMPEG_SRC" apply --check "$patch_path"
    git -C "$FFMPEG_SRC" apply "$patch_path"
    echo "  - applied $p"
done
echo "V Patches applied"
echo ""

echo "========================================="
echo "V HAP patch series applied"
echo "========================================="
echo ""
echo "Next steps:"
echo "  cd $FFMPEG_SRC"
echo "  ./build-static.sh"
echo ""
