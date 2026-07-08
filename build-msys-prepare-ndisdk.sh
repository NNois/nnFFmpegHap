#!/bin/bash
# Prepare the NewTek NDI SDK 6 for the MinGW64 FFmpeg build.
# Copies the SDK headers and creates a MinGW import library (libndi)
# for Processing.NDI.Lib.x64.dll, plus the runtime DLL for bundling.
#
# Usage: bash build-msys-prepare-ndisdk.sh [path-to-NDI-SDK]
#        (default: thirdparty/ndisdk/NDI 6 SDK, override with $NDI_SDK_DIR)
# Output: thirdparty/ndisdk/include/            (SDK headers)
#         thirdparty/ndisdk/build_mingw/libndi.dll.a  (import library, linked with -lndi)
#         thirdparty/ndisdk/build_mingw/Processing.NDI.Lib.x64.dll (runtime, to bundle)
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
NDISDK_ROOT="${NDISDK_ROOT:-$SCRIPT_DIR/thirdparty/ndisdk}"
LOCAL_NDI_SDK_DIR="$NDISDK_ROOT/NDI 6 SDK"
NDI_SDK_DIR="${1:-${NDI_SDK_DIR:-$LOCAL_NDI_SDK_DIR}}"
OUT_DIR="$NDISDK_ROOT/build_mingw"
INC_DIR="$NDISDK_ROOT/include"
LOG_FILE="${NDISDK_PREPARE_LOG:-$SCRIPT_DIR/build-msys-prepare-ndisdk.log}"
DLL_NAME="Processing.NDI.Lib.x64.dll"

mkdir -p "$(dirname "$LOG_FILE")"
: > "$LOG_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1

echo "=========================================="
echo "Preparing NewTek NDI SDK for MinGW64"
echo "=========================================="
echo "Date: $(date)"
echo "NDI_SDK_DIR=$NDI_SDK_DIR"
echo "NDISDK_ROOT=$NDISDK_ROOT"
echo "Log: $LOG_FILE"
echo ""

if [ ! -f "$NDI_SDK_DIR/Include/Processing.NDI.Lib.h" ]; then
    if [ -f "$LOCAL_NDI_SDK_DIR/Include/Processing.NDI.Lib.h" ]; then
        echo "Requested NDI SDK path not found, using local SDK:"
        echo "  $LOCAL_NDI_SDK_DIR"
        NDI_SDK_DIR="$LOCAL_NDI_SDK_DIR"
    else
        DETECTED_NDI_SDK_DIR="$(find "$NDISDK_ROOT" -maxdepth 3 -path '*/Include/Processing.NDI.Lib.h' -print -quit 2>/dev/null | sed 's#/Include/Processing.NDI.Lib.h$##')"
        if [ -n "$DETECTED_NDI_SDK_DIR" ]; then
            echo "Requested NDI SDK path not found, using detected local SDK:"
            echo "  $DETECTED_NDI_SDK_DIR"
            NDI_SDK_DIR="$DETECTED_NDI_SDK_DIR"
        fi
    fi
fi

if [ ! -f "$NDI_SDK_DIR/Include/Processing.NDI.Lib.h" ]; then
    echo "ERROR: NDI SDK not found at '$NDI_SDK_DIR'."
    echo "  Pass the SDK path as first argument or set NDI_SDK_DIR."
    echo "  Local expected path: $LOCAL_NDI_SDK_DIR"
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
    # gendef absent: use the SDK's MSVC x64 import library directly.
    # This is fully supported on x64 - MinGW ld/dlltool consume the MSVC .lib,
    # which exports undecorated extern "C" symbols plus __imp_ thunks that
    # satisfy the header's __declspec(dllimport) declarations. No gendef needed.
    cp -v "$NDI_SDK_DIR/Lib/x64/Processing.NDI.Lib.x64.lib" "$OUT_DIR/libndi.a"
    echo "ℹ️  gendef not installed; using the SDK's MSVC x64 import library as"
    echo "   build_mingw/libndi.a (works as-is with MinGW on x64)."
fi

echo ""
echo "=========================================="
echo "✓ NDI SDK prepared"
echo "=========================================="
echo "Headers:    $INC_DIR"
echo "Libraries:  $OUT_DIR"
echo "Log:        $LOG_FILE"
echo ""
echo "build-msys-shared.sh will now pick it up automatically"
echo "(--enable-libndi_newtek)."
