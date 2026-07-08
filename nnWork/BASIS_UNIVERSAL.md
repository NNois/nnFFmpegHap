# basis_universal (backend BC7 / BC6H de HAP)

`basis_universal` (Binomial) est la lib d'encodage de textures GPU utilisée
comme **backend d'encodage** pour :
- **Hap R** → BC7 (BPTC)
- **Hap HDR** → BC6H (BPTC float)

Elle **remplace** l'ancien `bc7enc` (BC7) et `ISPCTextureCompressor` /
`bc6enc` (BC6H) — plus de DLL `ispc_texcomp.dll` externe. Voir [HAP.md](HAP.md)
pour l'usage côté encodeur, et [PLAN.md](PLAN.md) pour le plan de migration.

---

## Architecture

FFmpeg est en C, basis_universal en C++ (`basisu` / `basist`). Le pont est un
**wrapper C** dans `libavcodec/` :

- `basisu_wrapper.h` / `basisu_wrapper.cpp` — API C `extern "C"` :
  - BC7 (Hap R) : `basisu_bc7_init()`, encodage bloc via
    `fast_pack_bc7_auto_rgba()` du transcoder ; qualité `0..4`
    (`BASISU_BC7_QUALITY_MAX = 4`).
  - BC6H (Hap H) : encode en ASTC HDR 4×4 puis transcode en BC6H ; qualité `0..4`.

Les **décodeurs** BC7/BC6H de FFmpeg (`bc7dec.c`, `bc6dec.c`, `bc6h_common.c`)
restent inchangés et indépendants.

---

## Build

```bash
./build-msys-prepare-basis-universal.sh
```

Ce script :
1. Clone `basis_universal` dans `thirdparty/basis_universal/` (si absent).
2. Configure via CMake/Ninja en **statique** :
   `-DBASISU_STATIC=TRUE -DBASISU_SSE=TRUE -DBASISU_ZSTD=TRUE`
   (`-DBASISU_OPENCL=FALSE -DBASISU_EXAMPLES=FALSE`).
3. Build la cible `basisu_encoder` → `build_mingw/libbasisu_encoder.a`.
4. Compile `libavcodec/basisu_wrapper.cpp` (`-std=c++17 -msse4.1`) et l'ajoute
   dans `libbasisu_encoder.a`.

`build-msys-shared.sh` **exige** cette lib (sinon il s'arrête) et ajoute :
```
-I.../encoder -I.../transcoder -I...          (cflags)
-L.../build_mingw -lbasisu_encoder -lstdc++ -lpthread   (link)
```

---

## Notes

- Lib **statique** → aucun DLL runtime à bundler.
- Thread-safe au niveau bloc (`fast_pack_bc7_*`, `astc_hdr_4x4_enc_block()`).
- Checkout basis_universal non versionné ; `build_mingw/` ignoré par git.
- La qualité BC6H (ASTC HDR → BC6H) peut différer légèrement de l'ancien
  ISPCTextureCompressor (algorithme plus moderne) — c'est attendu.
