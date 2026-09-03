#!/bin/bash
# Build x265 with alpha support for FFmpeg, as a 8+10+12 bit multilib.
# This replaces the MSYS2 package, which has neither alpha nor multilib.
#
# Why multilib matters here: x265 only implements the alpha input path for
# 8-bit pictures when the library itself is compiled at X265_DEPTH == 8
# (see source/common/picyuv.cpp, PicYuv::copyFromPicture). In a 10-bit-only
# build, feeding libx265 a yuva420p frame silently produces an alpha layer
# filled with the base layer luma instead of the alpha plane. FFmpeg selects
# the library by input depth (x265_api_get(depth), libavcodec/libx265.c), so
# shipping all three depths in one DLL is what actually fixes 8-bit alpha.
#
# Run this from an MSYS2 MinGW64 terminal.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

X265_VERSION="${X265_VERSION:-4.2}"
X265_ROOT="${X265_ROOT:-$SCRIPT_DIR/thirdparty/x265}"
X265_URL="${X265_URL:-https://bitbucket.org/multicoreware/x265_git/downloads/x265_${X265_VERSION}.tar.gz}"
BUILD_ROOT="$X265_ROOT/build_mingw"
INSTALL_PREFIX="${X265_INSTALL_PREFIX:-${MINGW_PREFIX:-/mingw64}}"
X265_BIN="$INSTALL_PREFIX/bin/x265"

LOG_FILE="${X265_BUILD_LOG:-$SCRIPT_DIR/build-msys-prepare-x265-with-alpha.log}"
mkdir -p "$(dirname "$LOG_FILE")"
: > "$LOG_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1

echo "=========================================="
echo "x265 ${X265_VERSION} - multilib 8+10+12 bit, ALPHA enabled"
echo "=========================================="
echo "Date:      $(date)"
echo "Source:    $X265_ROOT"
echo "Build:     $BUILD_ROOT"
echo "Install:   $INSTALL_PREFIX"
echo ""

# ---------------------------------------------------------------------------
# Step 0: source tree
# ---------------------------------------------------------------------------
if [ ! -f "$X265_ROOT/source/CMakeLists.txt" ]; then
    if [ -d "$X265_ROOT" ] && [ -n "$(ls -A "$X265_ROOT" 2>/dev/null)" ]; then
        echo "ERROR: $X265_ROOT exists but does not look like an x265 checkout."
        echo "Remove it or set X265_ROOT to a valid source tree."
        exit 1
    fi
    echo "Step 0: Downloading x265 ${X265_VERSION}..."
    mkdir -p "$X265_ROOT"
    TARBALL="$(mktemp -t x265-XXXXXX.tar.gz)"
    curl -fL -o "$TARBALL" "$X265_URL"
    tar -xzf "$TARBALL" -C "$X265_ROOT" --strip-components=1
    rm -f "$TARBALL"
    echo "  Extracted to $X265_ROOT"
else
    echo "Step 0: Using vendored source at $X265_ROOT"
fi

if [ -f "$X265_ROOT/x265Version.txt" ]; then
    echo "  releasetag: $(awk -F': *' '/releasetag/{print $2}' "$X265_ROOT/x265Version.txt")"
fi
echo ""

# ---------------------------------------------------------------------------
# Step 1: dependencies
# ---------------------------------------------------------------------------
echo "Step 1: Checking dependencies..."
MISSING=""
for tool in cmake make nasm g++; do
    if command -v "$tool" >/dev/null 2>&1; then
        echo "  [OK] $tool"
    else
        echo "  [MISSING] $tool"
        MISSING="$MISSING $tool"
    fi
done
if [ -n "$MISSING" ]; then
    echo ""
    echo "Install the missing tools, e.g.:"
    echo "  pacman -S --needed mingw-w64-x86_64-cmake mingw-w64-x86_64-nasm \\"
    echo "                     mingw-w64-x86_64-gcc make"
    exit 1
fi
echo ""

# ---------------------------------------------------------------------------
# Step 2: configure/build the three depths
# ---------------------------------------------------------------------------
# Options shared by all three passes. ENABLE_ALPHA must be identical
# everywhere: it drives MAX_LAYERS / MAX_SCALABLE_LAYERS, which size internal
# structures. Mixing values across the linked libraries would corrupt them.
COMMON_OPTS=(
    -G "MSYS Makefiles"
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    -DCMAKE_BUILD_TYPE=Release
    -DENABLE_ALPHA=ON
)

CPU_CORES="$(nproc)"
echo "Step 2: Building with $CPU_CORES cores..."
echo ""

rm -rf "$BUILD_ROOT"
mkdir -p "$BUILD_ROOT/8bit" "$BUILD_ROOT/10bit" "$BUILD_ROOT/12bit"

# --- 12-bit: static, no public C API (namespaced as x265_12bit) ---
echo "--- 12-bit static library ---"
cd "$BUILD_ROOT/12bit"
cmake "${COMMON_OPTS[@]}" \
    -DHIGH_BIT_DEPTH=ON \
    -DMAIN12=ON \
    -DEXPORT_C_API=OFF \
    -DENABLE_SHARED=OFF \
    -DENABLE_CLI=OFF \
    "$X265_ROOT/source"
make -j"$CPU_CORES"
echo ""

# --- 10-bit: static, no public C API (namespaced as x265_10bit) ---
echo "--- 10-bit static library ---"
cd "$BUILD_ROOT/10bit"
cmake "${COMMON_OPTS[@]}" \
    -DHIGH_BIT_DEPTH=ON \
    -DEXPORT_C_API=OFF \
    -DENABLE_SHARED=OFF \
    -DENABLE_CLI=OFF \
    "$X265_ROOT/source"
make -j"$CPU_CORES"
echo ""

# --- 8-bit: shared library + CLI, links the two static libs above ---
# EXTRA_LINK_FLAGS=-L. resolves against this build directory, so the 10/12-bit
# archives are copied in under the names x265 expects.
echo "--- 8-bit shared library (links 10-bit and 12-bit) ---"
cd "$BUILD_ROOT/8bit"
cp -f "$BUILD_ROOT/10bit/libx265.a" libx265_main10.a
cp -f "$BUILD_ROOT/12bit/libx265.a" libx265_main12.a
cmake "${COMMON_OPTS[@]}" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -DENABLE_SHARED=ON \
    -DENABLE_CLI=ON \
    -DLINKED_10BIT=ON \
    -DLINKED_12BIT=ON \
    -DEXTRA_LIB="x265_main10.a;x265_main12.a" \
    -DEXTRA_LINK_FLAGS="-L." \
    "$X265_ROOT/source"
make -j"$CPU_CORES"
echo ""

# ---------------------------------------------------------------------------
# Step 3: install
# ---------------------------------------------------------------------------
echo "Step 3: Installing to $INSTALL_PREFIX..."
make install
echo ""

# ---------------------------------------------------------------------------
# Step 4: verify
# ---------------------------------------------------------------------------
echo "Step 4: Verification..."
FAILED=0

CONFIG_H="$INSTALL_PREFIX/include/x265_config.h"
if [ -f "$CONFIG_H" ]; then
    echo "  x265_config.h: $(grep -E '^#define (X265_BUILD|ENABLE_ALPHA)' "$CONFIG_H" | tr '\n' ' ')"
    if grep -qE '^#define ENABLE_ALPHA 1' "$CONFIG_H"; then
        echo "  [OK] ENABLE_ALPHA is exported as 1 (FFmpeg will see MAX_LAYERS=2)"
    else
        echo "  [FAIL] ENABLE_ALPHA is not 1 in the installed header"
        FAILED=1
    fi
else
    echo "  [FAIL] $CONFIG_H not found"
    FAILED=1
fi

if command -v "$X265_BIN" >/dev/null 2>&1; then
    VERSION_LINE="$("$X265_BIN" --version 2>&1 | grep -i 'build info' || true)"
    echo "  $VERSION_LINE"
    if echo "$VERSION_LINE" | grep -q '8bit+10bit+12bit'; then
        echo "  [OK] multilib 8+10+12 bit"
    else
        echo "  [FAIL] not a multilib build - 8-bit alpha input will be wrong"
        FAILED=1
    fi

    if "$X265_BIN" --help 2>&1 | grep -qi -- '--alpha'; then
        echo "  [OK] --alpha option present"
    else
        echo "  [FAIL] --alpha option missing"
        FAILED=1
    fi
else
    echo "  [FAIL] $X265_BIN not found"
    FAILED=1
fi

echo ""
if [ "$FAILED" -ne 0 ]; then
    echo "=========================================="
    echo "x265 installed, but verification FAILED"
    echo "=========================================="
    exit 1
fi

echo "=========================================="
echo "x265 ${X265_VERSION} multilib + alpha installed"
echo "=========================================="
echo ""
echo "Note: 'pacman -S mingw-w64-x86_64-x265' would overwrite this build."
echo "      Re-run this script if that happens."
echo ""
echo "Now rebuild FFmpeg with:"
echo "  ./build-msys-shared.sh"
echo ""
