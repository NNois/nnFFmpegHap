#!/bin/bash
# Build the RAD Bink2 SDK expand2.c as a static library for MinGW64
# Usage: bash build-msys-prepare-bink2sdk.sh
# Output: thirdparty/bink2sdk/build_mingw/libbink2sdk.a (static library)
#         build-msys-prepare-bink2sdk.log (build log)
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BINK2SDK_ROOT="${BINK2SDK_ROOT:-$SCRIPT_DIR/thirdparty/bink2sdk}"
SDK_SRC="$BINK2SDK_ROOT/src"
OUT_DIR="$BINK2SDK_ROOT/build_mingw"
LOG_FILE="${BINK2SDK_BUILD_LOG:-$SCRIPT_DIR/build-msys-prepare-bink2sdk.log}"

mkdir -p "$OUT_DIR"
: > "$LOG_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1

CC="${CC:-gcc}"
AR="${AR:-ar}"

echo "=========================================="
echo "Building RAD Bink2 SDK static library"
echo "=========================================="
echo "CC=$CC"
echo "SDK_SRC=$SDK_SRC"
echo "OUT_DIR=$OUT_DIR"
echo ""

CFLAGS="-O2 -msse2 -mssse3 \
  -DUSING_EGT \
  -DINC_BINK2 \
  -DNO_BINK20 \
  -DNTELEMETRY \
  -D__RADFINAL__ \
  -D__RADINSTATICLIB__ \
  -I${SDK_SRC} \
  -I${BINK2SDK_ROOT} \
  -Wno-all -w"

echo "CFLAGS=$CFLAGS"
echo ""

echo "[1/6] Compiling expand2.c (full bink2 decoder)..."
$CC $CFLAGS -c "${SDK_SRC}/expand2.c" -o "$OUT_DIR/expand2.o"
echo "  OK"

echo "[2/6] Compiling binkcomm.c (slice utilities)..."
$CC $CFLAGS -c "${SDK_SRC}/binkcomm.c" -o "$OUT_DIR/binkcomm.o"
echo "  OK"

echo "[3/6] Compiling binkmarkers.c (version checks)..."
$CC $CFLAGS -c "${SDK_SRC}/binkmarkers.c" -o "$OUT_DIR/binkmarkers.o"
echo "  OK"

echo "[4/6] Compiling x86_cpu.c (CPU detection)..."
$CC $CFLAGS -c "${SDK_SRC}/x86_cpu.c" -o "$OUT_DIR/x86_cpu.o"
echo "  OK"

echo "[5/6] Compiling win32_rrCpu.c (CPU init)..."
$CC $CFLAGS -c "${SDK_SRC}/win32_rrCpu.c" -o "$OUT_DIR/win32_rrCpu.o"
echo "  OK"

echo "[6/6] Compiling bink2sdk_shims.c (MinGW atomics/mem shims)..."
$CC $CFLAGS -c "${BINK2SDK_ROOT}/bink2sdk_shims.c" -o "$OUT_DIR/bink2sdk_shims.o"
echo "  OK"

echo ""
echo "Creating static library..."
$AR rcs "$OUT_DIR/libbink2sdk.a" \
    "$OUT_DIR/expand2.o" \
    "$OUT_DIR/binkcomm.o" \
    "$OUT_DIR/binkmarkers.o" \
    "$OUT_DIR/x86_cpu.o" \
    "$OUT_DIR/win32_rrCpu.o" \
    "$OUT_DIR/bink2sdk_shims.o"

echo ""
echo "=========================================="
echo "SUCCESS: $OUT_DIR/libbink2sdk.a"
echo "=========================================="
