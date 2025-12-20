# FFmpeg (HAP fork)

## Build quick start
- Terminal: MSYS2 MINGW64.
- Install deps: `./build-install-dependencies.sh`.
- Build portable tools: `./build-static.sh` (bundles DLLs).
- Copy with DLLs: `./build-copy-with-dlls.sh /c/path/to/app`.

## What changed
- HAP: fixed HapQ YCoCg transform, added HapA (alpha-only), and rebuilt HapM (Hap Q Alpha) multi-texture/chunk headers to match the spec.
- Toolchain: Snappy/HAP enabled plus libvfw32 and build utilities wired into the latest build scripts.
- Codecs kept: x264, x265, libvpx, vorbis/opus/lame, plus the usual FFmpeg stack.

## HAP status
- Working: Hap1 (DXT1), Hap5 (DXT5), HapY (DXT5-YCoCg scaled), HapA (RGTC1 alpha-only), HapM (DXT5-YCoCg + RGTC1 alpha).
- Future prep: MOV tags added for HapR (`Hap7`) and Hap HDR (`HapH`); decoding is not implemented yet.
- Quick checks: `./ffmpeg.exe -encoders | grep hap` and `./ffmpeg.exe -decoders | grep hap`.
- Encode examples: `-c:v hap -format hap|hap_alpha|hap_q|hap_a|hap_m` (optionally add `-chunks N`).

## DONE WORK (HAP)
- HapA alpha-only support (details: `libavcodec/hapenc.c`, `libavcodec/hapdec.c`, `libavformat/isom_tags.c`).
- HapQ YCoCg transform fix (details: `libavcodec/texturedsp.c` and HAP decode path).
- HapM multi-texture headers and chunk layout (details: `libavcodec/hapenc.c`, `libavcodec/hapdec.c`, `libavcodec/bsf/hapqa_extract.c`).

## TODO
- [ ] Implement HapR/HapH decoding when texture support is ready (HapR may stay unsupported in Unreal).
- [ ] Keep build/test coverage for the HAP formats and Snappy path.
- [ ] HAP R (FFmpeg): BC5 textures (4 channels); FFmpeg already compresses BC5 internally in `libavcodec/texturedsp.c` and related modules (no external lib).
- [ ] HAP R (Unreal): update HAPLib to latest `C:\ff\hap\source\hap.h` and `C:\ff\hap\source\hap.c` (Unreal ships `C:\Program Files\Epic Games\UE_5.7\Engine\Source\ThirdParty\HAPMedia\HAPLib\include\hap.h`).
- [ ] HAP R (Unreal) example decode (raw BC5):

```c
unsigned long outputBufferBytes;
HapDecode(inputBuffer, inputBufferBytes,
          HapTextureFormat_RGTC1_Red, // or RGTC2
          outputBuffer, &outputBufferBytes);
```
