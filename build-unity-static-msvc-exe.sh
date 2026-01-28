#!/bin/bash
# MSVC-compatible minimal static FFmpeg build for Unity plugin usage
# Based on build-unity-static.sh, adapted for MSVC toolchain
# Requires: Visual Studio Build Tools env + MSYS2 shell

set -e

LOG_FILE="${FFMPEG_BUILD_LOG:-build-unity-static-msvc-exe.log}"
: > "$LOG_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1

# Prevent MSYS2 from mangling MSVC-style /O2 flags into paths
export MSYS2_ARG_CONV_EXCL="*"

echo "=========================================="
echo "FFmpeg - Unity Static EXE Build (MSVC)"
echo "=========================================="
echo ""

echo "Step 0: Checking toolchain..."
if ! command -v cl >/dev/null 2>&1; then
  echo "ERROR: cl.exe not found in PATH."
  echo "Launch MSYS2 from 'x64 Native Tools Command Prompt for VS' or run vcvars64.bat first."
  exit 1
fi

LINK_PATH=$(command -v link || true)
if [ -n "$LINK_PATH" ] && echo "$LINK_PATH" | grep -qi "/usr/bin/link"; then
  echo "ERROR: MSYS link.exe detected ($LINK_PATH)."
  echo "Remove/rename /usr/bin/link.exe or put VS link.exe earlier in PATH."
  exit 1
fi

echo "cl.exe: $(command -v cl)"
echo "link.exe: ${LINK_PATH:-not found}"

echo ""
echo "Step 1: Cleaning previous FFmpeg build..."
make clean 2>/dev/null || true
rm -f config.h config.log ffbuild/config.mak 2>/dev/null || true

echo ""
echo "Step 2: Checking pre-built dependencies..."
# Dependencies should be pre-built manually using build-*-msvc.bat scripts

ZLIB_INSTALL="/c/ff/ff/build-unity-static-msvc/zlib-msvc"
if [ ! -f "$ZLIB_INSTALL/lib/zlib.lib" ] && [ ! -f "$ZLIB_INSTALL/lib/zlibstatic.lib" ]; then
  echo "ERROR: zlib not found. Run build-zlib-msvc.bat first."
  exit 1
fi
echo "  zlib: OK"

VPX_INSTALL="/c/ff/ff/build-unity-static-msvc/libvpx-msvc"
# Rename vpxmd.lib to vpx.lib for FFmpeg compatibility if needed
if [ -f "$VPX_INSTALL/lib/x64/vpxmd.lib" ] && [ ! -f "$VPX_INSTALL/lib/x64/vpx.lib" ]; then
  echo "Copying vpxmd.lib to vpx.lib for FFmpeg detection..."
  cp "$VPX_INSTALL/lib/x64/vpxmd.lib" "$VPX_INSTALL/lib/x64/vpx.lib"
fi
if [ ! -f "$VPX_INSTALL/lib/x64/vpx.lib" ] && [ ! -f "$VPX_INSTALL/lib/vpx.lib" ] && [ ! -f "$VPX_INSTALL/lib/libvpx.lib" ]; then
  echo "ERROR: libvpx not found. Run build-libvpx-msvc.bat first."
  exit 1
fi
echo "  libvpx: OK"

SDL2_INSTALL="/c/ff/ff/build-unity-static-msvc/sdl2-msvc"
if [ ! -f "$SDL2_INSTALL/lib/SDL2-static.lib" ] && [ ! -f "$SDL2_INSTALL/lib/SDL2.lib" ]; then
  echo "ERROR: SDL2 not found. Run build-sdl2-msvc.bat first."
  exit 1
fi
echo "  SDL2: OK"

DAV1D_INSTALL="/c/ff/ff/build-unity-static-msvc/dav1d-msvc"
if [ ! -f "$DAV1D_INSTALL/lib/dav1d.lib" ]; then
  echo "  dav1d: NOT FOUND (optional, run build-dav1d-msvc.bat to enable)"
  DAV1D_ENABLED=0
else
  echo "  dav1d: OK"
  DAV1D_ENABLED=1
fi

# Provide sdl2-config for FFmpeg configure (MSYS2 environment)
SDL2_CONFIG_BIN="$SDL2_INSTALL/bin/sdl2-config"
mkdir -p "$SDL2_INSTALL/bin"
cat > "$SDL2_CONFIG_BIN" <<'EOF'
#!/bin/sh
prefix="C:/ff/ff/build-unity-static-msvc/sdl2-msvc"
libdir="$prefix/lib"
if [ -f "$libdir/SDL2-static.lib" ]; then
  sdl2lib="SDL2-static.lib"
else
  sdl2lib="SDL2.lib"
fi
case "$1" in
  --cflags) echo "-I${prefix}/include -I${prefix}/include/SDL2" ;;
  --libs) echo "-LIBPATH:${libdir} ${sdl2lib} SDL2main.lib user32.lib gdi32.lib winmm.lib imm32.lib ole32.lib oleaut32.lib version.lib uuid.lib setupapi.lib" ;;
  --version) echo "2.30.9" ;;
  *) echo "usage: $0 [--cflags|--libs|--version]" ; exit 1 ;;
esac
EOF
chmod +x "$SDL2_CONFIG_BIN"
export PATH="$SDL2_INSTALL/bin:$PATH"
export PKG_CONFIG_PATH="$SDL2_INSTALL/lib/pkgconfig:$PKG_CONFIG_PATH"

echo ""
echo "Step 3: Configuring minimal static build (MSVC toolchain)..."

target_prefix="./build-unity-static-msvc"

# Convert MSYS2 paths to Windows paths for MSVC
ZLIB_INSTALL_WIN="C:/ff/ff/build-unity-static-msvc/zlib-msvc"
ZLIB_CFLAGS="-IC:/ff/ff/build-unity-static-msvc/zlib-msvc/include"
ZLIB_LDFLAGS="-LIBPATH:C:/ff/ff/build-unity-static-msvc/zlib-msvc/lib"

# libvpx (required for VP8/VP9 with alpha)
VPX_ROOT_WIN="${VPX_ROOT:-C:\\ff\\ff\\build-unity-static-msvc\\libvpx-msvc}"
VPX_ROOT_UNIX="$(cygpath -u "$VPX_ROOT_WIN" 2>/dev/null || echo "")"
if [ -z "$VPX_ROOT_UNIX" ] || { [ ! -f "$VPX_ROOT_UNIX/lib/vpx.lib" ] && [ ! -f "$VPX_ROOT_UNIX/lib/libvpx.lib" ] && [ ! -f "$VPX_ROOT_UNIX/lib/x64/vpxmd.lib" ]; }; then
  echo "ERROR: libvpx not found. Build libvpx with MSVC and set VPX_ROOT (default: C:\\ff\\libvpx-msvc)."
  exit 1
fi
VPX_CFLAGS="-IC:/ff/ff/build-unity-static-msvc/libvpx-msvc/include"
if [ -f "$VPX_ROOT_UNIX/lib/x64/vpxmd.lib" ]; then
  VPX_LDFLAGS="-LIBPATH:C:/ff/ff/build-unity-static-msvc/libvpx-msvc/lib/x64"
else
  VPX_LDFLAGS="-LIBPATH:C:/ff/ff/build-unity-static-msvc/libvpx-msvc/lib"
fi
# Add vpx lib for FFmpeg's check_lib tests
VPX_EXTRA_LIBS="vpx.lib"

# SDL2 (for ffplay)
SDL2_CFLAGS="-IC:/ff/ff/build-unity-static-msvc/sdl2-msvc/include -IC:/ff/ff/build-unity-static-msvc/sdl2-msvc/include/SDL2"
SDL2_LDFLAGS="-LIBPATH:C:/ff/ff/build-unity-static-msvc/sdl2-msvc/lib"
if [ -f "/c/ff/ff/build-unity-static-msvc/sdl2-msvc/lib/SDL2-static.lib" ]; then
  SDL2_LIB_NAME="SDL2-static.lib"
else
  SDL2_LIB_NAME="SDL2.lib"
fi
SDL2_EXTRA_LIBS="SDL2main.lib $SDL2_LIB_NAME user32.lib gdi32.lib winmm.lib imm32.lib ole32.lib oleaut32.lib version.lib uuid.lib setupapi.lib"

# dav1d (fast AV1 decoder)
DAV1D_CFLAGS=""
DAV1D_LDFLAGS=""
DAV1D_EXTRA_LIBS=""
DAV1D_CONFIGURE=""
if [ "$DAV1D_ENABLED" = "1" ]; then
  DAV1D_CFLAGS="-IC:/ff/ff/build-unity-static-msvc/dav1d-msvc/include"
  DAV1D_LDFLAGS="-LIBPATH:C:/ff/ff/build-unity-static-msvc/dav1d-msvc/lib"
  DAV1D_EXTRA_LIBS="dav1d.lib"
  DAV1D_CONFIGURE="--enable-libdav1d"
fi

./configure \
    --toolchain=msvc \
    --target-os=win64 \
    --arch=x86_64 \
    --disable-everything \
    --enable-static \
    --disable-shared \
    --enable-ffmpeg \
    --enable-ffprobe \
    --enable-ffplay \
    --enable-avfilter \
    --enable-sdl2 \
    --disable-x86asm \
    --enable-gpl \
    --enable-libvpx \
    $DAV1D_CONFIGURE \
    --enable-avformat \
    --enable-avcodec \
    --enable-avutil \
    --enable-swscale \
    --enable-swresample \
    --enable-demuxer=mov,mp4,matroska,webm,image2,image2pipe,ogg,mp3 \
    --disable-decoder=vp8,vp9 \
    --enable-decoder=h264,hevc,libvpx_vp8,libvpx_vp9,libdav1d,hap,prores,png,mjpeg,targa,pcm_s16le,pcm_s24le,pcm_s32le,pcm_f32le,pcm_f64le,aac,vorbis,opus,mp3,mp3float \
    --enable-parser=h264,hevc,vp8,vp9,av1,vorbis,opus,aac,mpegaudio \
    --enable-filter=scale,pad,crop,format,aformat,aresample,buffer,buffersink,abuffer,abuffersink \
    --enable-protocol=file,pipe \
    --enable-hwaccel=h264_d3d11va \
    --enable-hwaccel=h264_d3d11va2 \
    --enable-hwaccel=hevc_d3d11va \
    --enable-hwaccel=hevc_d3d11va2 \
    --enable-hwaccel=vp8_d3d11va \
    --enable-hwaccel=vp8_d3d11va2 \
    --enable-hwaccel=vp9_d3d11va \
    --enable-hwaccel=vp9_d3d11va2 \
    --enable-hwaccel=av1_d3d11va \
    --enable-hwaccel=av1_d3d11va2 \
    --enable-d3d11va \
    --enable-zlib \
    --disable-vulkan \
    --disable-libshaderc \
    --prefix="$target_prefix" \
    --extra-cflags="/O2 $ZLIB_CFLAGS $VPX_CFLAGS $SDL2_CFLAGS $DAV1D_CFLAGS" \
    --extra-ldflags="$ZLIB_LDFLAGS $VPX_LDFLAGS $SDL2_LDFLAGS $DAV1D_LDFLAGS" \
    --extra-libs="$VPX_EXTRA_LIBS $SDL2_EXTRA_LIBS $DAV1D_EXTRA_LIBS"

echo ""
echo "Step 4: Building FFmpeg (static libs + tools)..."
CPU_CORES=$(nproc)
make -j"$CPU_CORES"
make install

echo ""
echo "=========================================="
echo "✓ Unity static MSVC EXE build complete"
echo "=========================================="
echo ""
echo "Output: $target_prefix"
echo ""
echo "To copy artifacts to Unity, run:"
echo "  ./build-unity-static-msvc-exe-copy.sh"
echo ""
echo "Run it now? (Y/n)"
read -r RUN_COPY
if [ -z "$RUN_COPY" ] || [ "$RUN_COPY" = "y" ] || [ "$RUN_COPY" = "Y" ]; then
    ./build-unity-static-msvc-exe-copy.sh
fi
