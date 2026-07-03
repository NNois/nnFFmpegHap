#!/bin/bash
# Prepare the NewTek NDI SDK 6 for the MinGW64 FFmpeg build.
# Copies the SDK headers and creates a MinGW import library (libndi)
# for Processing.NDI.Lib.x64.dll, plus the runtime DLL for bundling.
#
# Usage: bash prepare-ndisdk.sh [path-to-NDI-SDK]
#        (default: /c/ff/NDISDK/NDI 6 SDK, override with $NDI_SDK_DIR)
# Output: include/            (SDK headers)
#         build_mingw/libndi.dll.a  (import library, linked with -lndi)
#         build_mingw/Processing.NDI.Lib.x64.dll (runtime, to bundle)
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
NDI_SDK_DIR="${1:-${NDI_SDK_DIR:-/c/ff/NDISDK/NDI 6 SDK}}"
OUT_DIR="$SCRIPT_DIR/build_mingw"
INC_DIR="$SCRIPT_DIR/include"
DLL_NAME="Processing.NDI.Lib.x64.dll"

echo "=========================================="
echo "Preparing NewTek NDI SDK for MinGW64"
echo "=========================================="
echo "NDI_SDK_DIR=$NDI_SDK_DIR"
echo ""

if [ ! -f "$NDI_SDK_DIR/Include/Processing.NDI.Lib.h" ]; then
    echo "ERROR: NDI SDK not found at '$NDI_SDK_DIR'."
    echo "  Pass the SDK path as first argument or set NDI_SDK_DIR."
    exit 1
fi

mkdir -p "$OUT_DIR" "$INC_DIR"

echo "Step 1: Copying SDK headers..."
cp -v "$NDI_SDK_DIR/Include/"*.h "$INC_DIR/"

echo ""
echo "Step 2: Copying runtime DLL..."
cp -v "$NDI_SDK_DIR/Bin/x64/$DLL_NAME" "$OUT_DIR/"
cp -v "$NDI_SDK_DIR/Bin/x64/Processing.NDI.Lib.Licenses.txt" "$OUT_DIR/" 2>/dev/null || true

echo ""
echo "Step 3: Creating MinGW import library..."
rm -f "$OUT_DIR/libndi.dll.a" "$OUT_DIR/libndi.a"
if command -v gendef >/dev/null 2>&1; then
    ( cd "$OUT_DIR" && gendef "$DLL_NAME" )
    dlltool -d "$OUT_DIR/${DLL_NAME%.dll}.def" -D "$DLL_NAME" -l "$OUT_DIR/libndi.dll.a"
    rm -f "$OUT_DIR/${DLL_NAME%.dll}.def"
    echo "✓ Import library created with gendef/dlltool: build_mingw/libndi.dll.a"
else
    # Fallback: binutils ld can consume MSVC import libraries directly.
    cp -v "$NDI_SDK_DIR/Lib/x64/Processing.NDI.Lib.x64.lib" "$OUT_DIR/libndi.a"
    echo "⚠️  gendef not found (pacman -S mingw-w64-x86_64-tools-git)."
    echo "   Using the MSVC import library as build_mingw/libndi.a instead."
fi

echo ""
echo "=========================================="
echo "✓ NDI SDK prepared"
echo "=========================================="
echo "Headers:    $INC_DIR"
echo "Libraries:  $OUT_DIR"
echo ""
echo "build-msys-shared.sh will now pick it up automatically"
echo "(--enable-libndi_newtek)."
