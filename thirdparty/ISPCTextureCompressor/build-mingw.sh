#!/bin/bash
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$ROOT/ispc_texcomp"
BUILD_DIR="$ROOT/build"
OUT_DIR="$ROOT/lib"

ISPC_BIN="${ISPC_BIN:-$ROOT/ISPC/win/ispc.exe}"
if [ ! -x "$ISPC_BIN" ]; then
    if command -v ispc >/dev/null 2>&1; then
        ISPC_BIN="$(command -v ispc)"
    else
        echo "ERROR: ispc compiler not found. Put ispc.exe in $ROOT/ISPC/win or set ISPC_BIN."
        exit 1
    fi
fi

CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--O2 -std=c++17 -I$ROOT -I$SRC_DIR}"
ISPC_ARCH="${ISPC_ARCH:-x86-64}"
ISPC_TARGETS="${ISPC_TARGETS:-sse2,avx}"
ISPC_FLAGS="${ISPC_FLAGS:---arch=$ISPC_ARCH --target=$ISPC_TARGETS -O2 --opt=fast-math}"

mkdir -p "$BUILD_DIR" "$OUT_DIR"

echo "Building ISPC kernels..."
"$ISPC_BIN" $ISPC_FLAGS -h "$SRC_DIR/kernel_ispc.h" -o "$SRC_DIR/kernel_ispc.o" "$SRC_DIR/kernel.ispc"
"$ISPC_BIN" $ISPC_FLAGS -h "$SRC_DIR/kernel_astc_ispc.h" -o "$SRC_DIR/kernel_astc_ispc.o" "$SRC_DIR/kernel_astc.ispc"

echo "Building C++ objects..."
$CXX $CXXFLAGS -c "$SRC_DIR/ispc_texcomp.cpp" -o "$BUILD_DIR/ispc_texcomp.o"
$CXX $CXXFLAGS -c "$SRC_DIR/ispc_texcomp_astc.cpp" -o "$BUILD_DIR/ispc_texcomp_astc.o"

echo "Building chkstk shim for MinGW..."
cat > "$BUILD_DIR/chkstk.S" <<'EOF'
/* __chkstk for x86_64 MinGW - probes stack pages so the OS can grow the stack.
   Called by ISPC-generated code when a function needs > 4KB of stack.
   On entry: rax = number of bytes needed.
   Must preserve all registers except rax and r11. */
    .text
    .globl __chkstk
    .def __chkstk; .scl 2; .type 32; .endef
__chkstk:
    push    %rcx
    push    %rax
    cmp     $0x1000, %rax
    lea     0x18(%rsp), %rcx    /* original rsp before call+pushes */
    jb      .Ldone
.Lloop:
    sub     $0x1000, %rcx
    test    %rcx, (%rcx)        /* touch the page (stack probe) */
    sub     $0x1000, %rax
    cmp     $0x1000, %rax
    ja      .Lloop
.Ldone:
    sub     %rax, %rcx
    test    %rcx, (%rcx)
    pop     %rax
    pop     %rcx
    ret
EOF
$CXX -c "$BUILD_DIR/chkstk.S" -o "$BUILD_DIR/chkstk.o"

echo "Linking DLL..."
$CXX -shared -o "$OUT_DIR/ispc_texcomp.dll" \
    "$BUILD_DIR/ispc_texcomp.o" \
    "$BUILD_DIR/ispc_texcomp_astc.o" \
    "$BUILD_DIR/chkstk.o" \
    "$SRC_DIR"/kernel_ispc*.o \
    "$SRC_DIR"/kernel_astc_ispc*.o \
    "$SRC_DIR/ispc_texcomp.def" \
    -Wl,--out-implib,"$OUT_DIR/libispc_texcomp.a"

echo "Built $OUT_DIR/ispc_texcomp.dll and $OUT_DIR/libispc_texcomp.a"
