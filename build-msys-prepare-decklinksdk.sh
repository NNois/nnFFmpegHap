#!/bin/bash
# Prepare the Blackmagic DeckLink SDK for the MinGW64 FFmpeg build.
#
# On Windows the DeckLink SDK ships only COM .idl files (no ready-made
# DeckLinkAPI.h). FFmpeg's decklink code does:
#     #include <DeckLinkAPI.h>
#     #include <DeckLinkAPI_i.c>
# so we generate BOTH from the .idl using widl (the Wine MIDL compiler,
# provided by the MSYS2 package mingw-w64-x86_64-tools).
#
# DeckLink is accessed through COM: the driver DLL is loaded at runtime from
# the system-wide "Desktop Video" install. There is therefore NO import
# library to link and NO runtime DLL to bundle - only headers are needed
# (configure adds -lole32 -loleaut32 automatically on Windows).
#
# Usage: bash build-msys-prepare-decklinksdk.sh [path-to-DeckLink-SDK-Win-include]
#        The .idl sources live in thirdparty/decklinksdk/idl (already vendored);
#        pass a SDK path only to refresh them from a newer SDK.
# Output: thirdparty/decklinksdk/include/DeckLinkAPI.h    (generated)
#         thirdparty/decklinksdk/include/DeckLinkAPI_i.c  (generated, GUIDs)
#         thirdparty/decklinksdk/include/DeckLinkAPIVersion.h
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DECKLINK_ROOT="${DECKLINK_ROOT:-$SCRIPT_DIR/thirdparty/decklinksdk}"
IDL_DIR="$DECKLINK_ROOT/idl"
INC_DIR="$DECKLINK_ROOT/include"
LOG_FILE="${DECKLINK_PREPARE_LOG:-$SCRIPT_DIR/build-msys-prepare-decklinksdk.log}"

# Optional: refresh the vendored .idl from an external SDK copy passed as $1.
SDK_INCLUDE="${1:-$DECKLINK_SDK_INCLUDE}"

mkdir -p "$(dirname "$LOG_FILE")"
: > "$LOG_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1

echo "=========================================="
echo "Preparing Blackmagic DeckLink SDK for MinGW64"
echo "=========================================="
echo "Date: $(date)"
echo "DECKLINK_ROOT=$DECKLINK_ROOT"
echo "Log: $LOG_FILE"
echo ""

# Step 0: optionally refresh vendored .idl from an external SDK
if [ -n "$SDK_INCLUDE" ]; then
    if [ -f "$SDK_INCLUDE/DeckLinkAPI.idl" ]; then
        echo "Step 0: Refreshing vendored .idl from $SDK_INCLUDE ..."
        mkdir -p "$IDL_DIR"
        cp -v "$SDK_INCLUDE/"*.idl "$IDL_DIR/"
        cp -v "$SDK_INCLUDE/DeckLinkAPIVersion.h" "$IDL_DIR/"
        echo ""
    else
        echo "WARNING: '$SDK_INCLUDE/DeckLinkAPI.idl' not found, using vendored .idl."
        echo ""
    fi
fi

if [ ! -f "$IDL_DIR/DeckLinkAPI.idl" ]; then
    echo "ERROR: DeckLink .idl sources not found in $IDL_DIR."
    echo "  Re-run with the SDK Win/include path, e.g.:"
    echo "    bash $(basename "$0") \"/c/ff/DECKLINKSDK/Blackmagic DeckLink SDK 16.0/Win/include\""
    exit 1
fi

# Step 1: widl must be available
if ! command -v widl >/dev/null 2>&1; then
    echo "ERROR: widl not found."
    echo "  widl (the Wine MIDL compiler) generates DeckLinkAPI.h / _i.c from the .idl."
    echo "  Install it in MSYS2 MINGW64 with:"
    echo "      pacman -S mingw-w64-x86_64-tools"
    exit 1
fi
echo "widl: $(command -v widl)"
widl --version 2>/dev/null | head -1 || true
echo ""

mkdir -p "$INC_DIR"

# Step 2: DeckLinkAPIVersion.h must sit next to the generated header
echo "Step 2: Copying DeckLinkAPIVersion.h ..."
cp -v "$IDL_DIR/DeckLinkAPIVersion.h" "$INC_DIR/"
echo "  SDK version: $(grep -oE '"[0-9.]+"' "$IDL_DIR/DeckLinkAPIVersion.h" | head -1)"
echo ""

# Step 3: generate the header and the interface-identifier (GUID) file
#
# -Dbool=boolean: the DeckLink SDK's *deprecated* interfaces (DeckLinkAPI_v*.idl)
# use the C++ keyword `bool`, which widl's IDL grammar does not know (MS MIDL
# does), causing "type 'bool' not found in global namespace". We map it to the
# MIDL base type `boolean` (unsigned char, same 1-byte ABI) during IDL parsing.
# This only affects those deprecated interfaces - FFmpeg uses the current ones,
# whose DoesSupportVideoMode already takes `BOOL*` (== FFmpeg's DECKLINK_BOOL on
# Windows), so the generated header matches FFmpeg exactly.
echo "Step 3: Generating DeckLinkAPI.h and DeckLinkAPI_i.c with widl ..."
rm -f "$INC_DIR/DeckLinkAPI.h" "$INC_DIR/DeckLinkAPI_i.c"
(
    cd "$IDL_DIR"
    widl -I. -Dbool=boolean \
        -h -H "$INC_DIR/DeckLinkAPI.h" \
        -u -U "$INC_DIR/DeckLinkAPI_i.c" \
        DeckLinkAPI.idl
)

if [ ! -f "$INC_DIR/DeckLinkAPI.h" ] || [ ! -f "$INC_DIR/DeckLinkAPI_i.c" ]; then
    echo "ERROR: widl did not produce the expected files."
    exit 1
fi

echo "  Generated: $(basename "$INC_DIR")/DeckLinkAPI.h   ($(wc -l < "$INC_DIR/DeckLinkAPI.h") lines)"
echo "  Generated: $(basename "$INC_DIR")/DeckLinkAPI_i.c ($(wc -l < "$INC_DIR/DeckLinkAPI_i.c") lines)"

echo ""
echo "=========================================="
echo "✓ DeckLink SDK prepared"
echo "=========================================="
echo "Headers: $INC_DIR"
echo "Log:     $LOG_FILE"
echo ""
echo "build-msys-shared.sh will now pick it up automatically (--enable-decklink)."
echo "Runtime: install Blackmagic 'Desktop Video' on the target machine."
