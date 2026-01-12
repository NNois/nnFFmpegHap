#!/bin/bash
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$ROOT/src"
INC_DIR="$ROOT/include"
OUT_DIR="$ROOT/lib"

mkdir -p "$OUT_DIR"

if [ ! -f "$SRC_DIR/bin/compress.hlsl" ]; then
    echo "ERROR: compress.hlsl not found at $SRC_DIR/bin/compress.hlsl"
    exit 1
fi

cp -v "$SRC_DIR/bin/compress.hlsl" "$OUT_DIR/compress.hlsl"

g++ -shared -O2 -std=c++17 \
    -I"$INC_DIR" \
    -o "$OUT_DIR/GPURealTimeBC6H.dll" \
    "$SRC_DIR/gpurealtimebc6h.cpp" \
    -Wl,--out-implib,"$OUT_DIR/libGPURealTimeBC6H.a" \
    -ld3d11 -ld3dcompiler

echo "Built $OUT_DIR/GPURealTimeBC6H.dll and $OUT_DIR/libGPURealTimeBC6H.a"
