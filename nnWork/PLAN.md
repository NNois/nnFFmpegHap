# Plan : Remplacer bc7enc/bc6enc par basis_universal

## Contexte

Actuellement FFmpeg utilise :
- **BC7 (Hap R / Hap7)** : `bc7enc.c` / `bc7enc.h` — encodeur C standalone de Richard Geldreich
- **BC6H (Hap H / HapH)** : `bc6enc.c` / `bc6enc.h` (Compressonator) + `ISPCTextureCompressor` (DLL externe pour l'encodage réel via `CompressBlocksBC6H`)
- **Décodeurs** : `bc7dec.c` / `bc6dec.c` / `bc6h_common.c` — indépendants, restent inchangés

On remplace par `thirdparty/basis_universal` (même auteur pour BC7).

## Architecture cible

basis_universal est une lib **C++** (namespaces `basisu` / `basist`). FFmpeg est en C.
On crée un **wrapper C** (`basisu_wrapper.cpp` / `basisu_wrapper.h`) dans `libavcodec/` qui expose des fonctions C simples.

### Pour BC7 (Hap R)
basis_universal contient `bc7enc_compress_block_init()` et la même logique que l'ancien bc7enc,
plus les fonctions `bc7f::fast_pack_bc7_auto_rgba()` dans le transcoder.
On utilisera les fonctions du **transcoder** (`basisu_transcoder_internal.h` namespace `bc7f`)
car elles offrent une API block-level propre : `fast_pack_bc7_auto_rgba(pBlock, pPixels, flags)`.

Quality 0-4 sera mappé aux flags `cPackBC7Flag*` du transcoder.

### Pour BC6H (Hap H)
basis_universal encode en ASTC HDR 4x4 puis **transcode automatiquement en BC6H**.
La fonction `astc_hdr_4x4_enc_block()` produit des résultats contenant `m_bc6h_block`.
Quality 0-4 sera mappé via `uastc_hdr_4x4_codec_options::set_quality_level(0..4)`.

**Ceci remplace ISPCTextureCompressor** — plus besoin de la DLL externe `ispc_texcomp.dll`.

## Étapes

### Étape 1 : Fixer le build MSVC de basis_universal
- Retarget les `.vcxproj` de v145 vers v143 (VS 2022)
- Builder `encoder_lib` en Release x64 → produit `encoder_lib.lib`

### Étape 2 : Builder basis_universal comme lib statique via CMake
Plutôt que les projets MSVC, on utilise CMake pour une intégration propre :
```bash
cd thirdparty/basis_universal
cmake -B build -G "Visual Studio 17 2022" -A x64 \
  -DBASISU_STATIC=TRUE -DBASISU_SSE=TRUE -DBASISU_ZSTD=TRUE \
  -DBASISU_OPENCL=FALSE -DBASISU_EXAMPLES=FALSE
cmake --build build --config Release --target basisu_encoder
```
Produit : `build/Release/basisu_encoder.lib`

### Étape 3 : Créer le wrapper C (`libavcodec/basisu_wrapper.cpp` / `.h`)

**basisu_wrapper.h** (C API) :
```c
// BC7 block encoding
void basisu_bc7_init(void);
int  basisu_bc7_encode_block(uint8_t *dst, ptrdiff_t stride, const uint8_t *block);
void basisu_bc7_set_quality(int quality); // 0-4

// BC6H block encoding
void basisu_bc6h_init(void);
int  basisu_bc6h_compress_frame(uint8_t *out, const uint8_t *rgba_u16,
                                 int width, int height, int quality);
```

**basisu_wrapper.cpp** (C++ implementation) :
- `#include` les headers basis_universal
- Appelle `basisu_encoder_init()` une fois
- BC7 : utilise `bc7f::fast_pack_bc7_auto_rgba()` (thread-safe par block)
- BC6H : itère sur les blocs 4x4, appelle `astc_hdr_4x4_enc_block()` pour chaque bloc, extrait `m_bc6h_block`
- Expose tout via `extern "C"`

### Étape 4 : Modifier hapenc.c

- Remplacer `#include "bc7enc.h"` par `#include "basisu_wrapper.h"`
- Remplacer `#include "ispc_texcomp.h"` et tout le bloc `#if CONFIG_LIBISPC_TEXCOMP`
- BC7 init : `basisu_bc7_init()` au lieu de `ff_bc7enc_init()`
- BC7 block : pointer `ctx->enc[0].tex_funct` vers la fonction wrapper
- BC6H : appeler `basisu_bc6h_compress_frame()` au lieu de `CompressBlocksBC6H()`
- Garder quality 0-4 pour les deux

### Étape 5 : Modifier le système de build FFmpeg

**configure** :
- Remplacer `--enable-libispc_texcomp` par `--enable-libbasisu`
- Changer `hap_encoder_deps` de `"libsnappy libispc_texcomp"` à `"libsnappy libbasisu"`
- Ajouter le check `require libbasisu basisu_wrapper.h basisu_bc7_init -lbasisu_encoder -lstdc++`

**libavcodec/Makefile** :
- Ligne 450 : remplacer `bc7enc.o` par `basisu_wrapper.o`
- Ajouter les règles pour compiler `.cpp`

**build-msys-shared.sh** :
- Remplacer les variables `ISPCTEXCOMP_*` par `BASISU_*`
- Pointer vers le `.lib` / `.a` compilé de basis_universal
- Supprimer la copie de `ispc_texcomp.dll`

### Étape 6 : Nettoyer

- Supprimer `libavcodec/bc7enc.c` et `libavcodec/bc7enc.h` (remplacés)
- Supprimer `libavcodec/bc6enc.c` et `libavcodec/bc6enc.h` (remplacés)
- Garder `bc7dec.c`, `bc6dec.c`, `bc6h_common.c` (décodeurs, indépendants)
- Supprimer les références à ISPCTextureCompressor dans les scripts

## Points d'attention

1. **C++ dans FFmpeg** : FFmpeg est un projet C. Le wrapper `.cpp` doit être compilé avec g++ et linké. Le Makefile de FFmpeg supporte déjà ça (voir précédents `.cpp` dans certains modules).

2. **Taille** : basis_universal est grosse (~41 fichiers source). On link la lib statique pré-compilée plutôt que de compiler les sources dans FFmpeg.

3. **Thread safety** : `bc7f::fast_pack_bc7_*` et `astc_hdr_4x4_enc_block()` sont documentés thread-safe.

4. **Qualité BC6H** : L'approche ASTC HDR → transcode BC6H peut donner des résultats légèrement différents de ISPCTextureCompressor. C'est normal — basis_universal utilise un algorithme plus moderne.

5. **Décodeurs inchangés** : `bc7dec.c`, `bc6dec.c`, `bc6h_common.c` restent les mêmes, ils sont indépendants des encodeurs.
