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
- [ ] Re-verify HapM playback/alpha with ffplay, Unreal HAP decoder, and the Adobe Hap encoder.
- [ ] Implement HapR/HapH decoding when texture support is ready (HapR may stay unsupported in Unreal).
- [ ] Keep build/test coverage for the HAP formats and Snappy path.
