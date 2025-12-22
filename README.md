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

## HAP formats (quick map)
| HAP format     | Texture compression                                     | FourCC | Bits                     |
|----------------|---------------------------------------------------------|--------|--------------------------|
| HAP            | BC1 (DXT1)                                              | Hap1   | 8-bit RGB                |
| HAP Alpha      | BC3 (DXT5)                                              | Hap5   | 8-bit RGBA               |
| HAP Q          | Scaled YCoCg DXT5 (BC3 with color transform)            | HapY   | 8-bit RGB                |
| HAP Q Alpha    | Scaled YCoCg DXT5 (BC3) + BC4 (separate alpha)          | HapM   | 8-bit RGBA               |
| HAP Alpha-Only | BC4 (alpha only)                                        | HapA   | 8-bit A                  |
| HAP R          | BC7 (BPTC)                                              | Hap7   | 8-bit RGBA               |
| HAP HDR        | BC6U/BC6S (BPTC float, unsigned/signed)                 | HapH   | 16-bit half-float RGB    |

## HAP checklist
- [x] Working formats: Hap1 (DXT1), Hap5 (DXT5), HapY (DXT5-YCoCg scaled), HapA (RGTC1 alpha-only), HapM (DXT5-YCoCg + RGTC1 alpha), HapR (Hap7, BC7 modes 1/6 only).
- [x] HapA alpha-only support (details: `libavcodec/hapenc.c`, `libavcodec/hapdec.c`, `libavformat/isom_tags.c`).
- [x] HapQ YCoCg transform fix (details: `libavcodec/texturedsp.c` and HAP decode path).
- [x] HapM multi-texture headers and chunk layout (details: `libavcodec/hapenc.c`, `libavcodec/hapdec.c`, `libavcodec/bsf/hapqa_extract.c`).
- [x] HapR (Hap7) encode/decode with BC7 modes 1/6 only (details: `libavcodec/hapenc.c`, `libavcodec/hapdec.c`, `libavcodec/bc7dec.c`).
- [x] Quick checks: `./ffmpeg.exe -encoders | grep hap` and `./ffmpeg.exe -decoders | grep hap`.
- [x] Encode examples: `-c:v hap -format hap|hap_alpha|hap_q|hap_a|hap_m|hap_r` (optionally add `-chunks N`).
- [x] MOV tags added for Hap HDR (`HapH`) (tag only, no codec support).
- [ ] HAP R (FFmpeg): BC7 decode is limited to modes 1/6 (bc7enc16 output). Add modes 0/2/3/4/5/7 for full BC7 compatibility.
- [ ] HAP R (FFmpeg): evaluate `bc7enc` / `bc7enc_rdo` for better quality (RDO, broader mode use) vs `bc7enc16`.
- [ ] Keep build/test coverage for the HAP formats and Snappy path.
- [ ] Hap H decoding/encoding is not implemented yet.


## TODO: BC7 (HapR) status and work
- Decoder status: current FFmpeg BC7 decode is limited to modes 1 and 6 (matches `bc7enc16` output). Full compatibility needs modes 0/2/3/4/5/7.
- Encoder status: FFmpeg uses `bc7enc` with mode 1/6 only (mode 5/7 disabled). Consider `bc7e` or `bc7enc_rdo` if we want more modes and/or RDO.
- Source refs: `C:\ff\bc7enc16` (original, fast single-file encoder, modes 1 and 6 only, strong on opaque + simple alpha, perceptual YCbCr metric, simple codebase); `C:\ff\bc7e` (Binomial, SIMD ISPC encoder, very fast vs ispc_texcomp, perceptual metrics, full modes, archived with latest in `bc7enc_rdo`); `C:\ff\bc7enc_rdo` (BC1-7 encoders with RDO + entropy reduction, ships latest `bc7e.ispc` optional and `bc7enc.cpp` fallback, includes `bc7decomp.cpp/.h`).
- BC7 modes quick notes: Mode 0 = 3 subsets, RGB, 4-bit endpoints + pbits, 3-bit indices (no alpha). Mode 1 = 2 subsets, RGB, 6-bit endpoints + shared pbits, 3-bit indices (no alpha). Mode 2 = 3 subsets, RGB, 5-bit endpoints, 2-bit indices (no alpha). Mode 3 = 2 subsets, RGB, 7-bit endpoints + pbits, 2-bit indices (no alpha). Mode 4 = 1 subset, RGBA, RGB 5-bit + A 6-bit endpoints, dual index sets, component rotation (alpha). Mode 5 = 1 subset, RGBA, RGB 7-bit + A 8-bit endpoints, 2-bit indices, component rotation (alpha). Mode 6 = 1 subset, RGBA, 7-bit endpoints + pbit (8-bit effective), 4-bit indices (alpha). Mode 7 = 2 subsets, RGBA, 5-bit endpoints + pbits, 2-bit indices (alpha).


## Unreal engine notes:
- [ ] HAP R (Unreal): update HAPLib to latest `C:\ff\hap\source\hap.h` and `C:\ff\hap\source\hap.c` (Unreal ships `C:\Program Files\Epic Games\UE_5.7\Engine\Source\ThirdParty\HAPMedia\HAPLib\include\hap.h`).
- HAP R (Unreal) example decode (raw BC7):
```
const void *inputBuffer;      // Frame HAP R compressée
unsigned long inputBufferBytes; // Taille du buffer d'entrée
void *outputBuffer;           // Buffer de sortie pour BC7
unsigned long outputBufferBytes; // Taille allouée pour la sortie
unsigned long outputBufferBytesUsed; // Taille réellement utilisée
unsigned int outputTextureFormat;

// Décoder la frame HAP R (index 0 pour la première/seule texture)
unsigned int result = HapDecode(
    inputBuffer, inputBufferBytes,
    0,                    // index (0 pour la première texture)
    NULL,                 // callback (NULL si pas de multithreading)
    NULL,                 // info (contexte utilisateur pour callback)
    outputBuffer, outputBufferBytes,
    &outputBufferBytesUsed,
    &outputTextureFormat
);

if (result == HapResult_No_Error) {
    // Vérifier le format de texture
    if (outputTextureFormat == HapTextureFormat_RGBA_BPTC_UNORM) {
        // C'est bien du BC7 (BPTC) !
        // outputBuffer contient maintenant les données BC7 brutes
        // Prêtes à être uploadées vers une texture GPU dans Unreal
    }
}
```
