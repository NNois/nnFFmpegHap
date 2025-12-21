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
- Working: Hap1 (DXT1), Hap5 (DXT5), HapY (DXT5-YCoCg scaled), HapA (RGTC1 alpha-only), HapM (DXT5-YCoCg + RGTC1 alpha), HapR (Hap7, BC7 modes 1/6 only).
- Future prep: MOV tags added for Hap HDR (`HapH`); decoding is not implemented yet.
- Quick checks: `./ffmpeg.exe -encoders | grep hap` and `./ffmpeg.exe -decoders | grep hap`.
- Encode examples: `-c:v hap -format hap|hap_alpha|hap_q|hap_a|hap_m` (optionally add `-chunks N`).

## DONE WORK (HAP)
- HapA alpha-only support (details: `libavcodec/hapenc.c`, `libavcodec/hapdec.c`, `libavformat/isom_tags.c`).
- HapQ YCoCg transform fix (details: `libavcodec/texturedsp.c` and HAP decode path).
- HapM multi-texture headers and chunk layout (details: `libavcodec/hapenc.c`, `libavcodec/hapdec.c`, `libavcodec/bsf/hapqa_extract.c`).
- HapR (Hap7) encode/decode with BC7 modes 1/6 only (details: `libavcodec/hapenc.c`, `libavcodec/hapdec.c`, `libavcodec/bc7dec.c`).

## TODO
- [ ] Implement HapH decoding when texture support is ready (HapH may stay unsupported in Unreal).
- [ ] HAP R (FFmpeg): BC7 decode is limited to modes 1/6 (bc7enc16 output). Add modes 0/2/3/4/5/7 for full BC7 compatibility.
- [ ] HAP R (FFmpeg): evaluate `bc7enc` / `bc7enc_rdo` for better quality (RDO, broader mode use) vs `bc7enc16`.
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

## TODO: BC7 (HapR) status and work
- Decoder status: current FFmpeg BC7 decode is limited to modes 1 and 6 (matches `bc7enc16` output). Full compatibility needs modes 0/2/3/4/5/7.
- Encoder status: FFmpeg uses `bc7enc16` (fast, mode 1+6 only). Consider switching to or adding a choice for `bc7e` or `bc7enc_rdo` if we want more modes and/or RDO.
- Source refs: `C:\ff\bc7enc16` (original, fast single-file encoder, modes 1 and 6 only, strong on opaque + simple alpha, perceptual YCbCr metric, simple codebase); `C:\ff\bc7e` (Binomial, SIMD ISPC encoder, very fast vs ispc_texcomp, perceptual metrics, full modes, archived with latest in `bc7enc_rdo`); `C:\ff\bc7enc_rdo` (BC1-7 encoders with RDO + entropy reduction, ships latest `bc7e.ispc` optional and `bc7enc.cpp` fallback, includes `bc7decomp.cpp/.h`).
- BC7 modes quick notes: Mode 0 = 3 subsets, RGB, 4-bit endpoints + pbits, 3-bit indices (no alpha). Mode 1 = 2 subsets, RGB, 6-bit endpoints + shared pbits, 3-bit indices (no alpha). Mode 2 = 3 subsets, RGB, 5-bit endpoints, 2-bit indices (no alpha). Mode 3 = 2 subsets, RGB, 7-bit endpoints + pbits, 2-bit indices (no alpha). Mode 4 = 1 subset, RGBA, RGB 5-bit + A 6-bit endpoints, dual index sets, component rotation (alpha). Mode 5 = 1 subset, RGBA, RGB 7-bit + A 8-bit endpoints, 2-bit indices, component rotation (alpha). Mode 6 = 1 subset, RGBA, 7-bit endpoints + pbit (8-bit effective), 4-bit indices (alpha). Mode 7 = 2 subsets, RGBA, 5-bit endpoints + pbits, 2-bit indices (alpha).
