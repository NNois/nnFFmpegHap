# KTX2 dans FFmpeg — images BC7 avec leurs mip-maps

Encodeur et décodeur `ktx2` (`AV_CODEC_ID_KTX2`) pour le **KTX 2.0** de Khronos : une image fixe en **BC7**
(`VK_FORMAT_BC7_SRGB_BLOCK`, ou `_UNORM_BLOCK` pour une carte de données), ses **mip-maps** jusqu'à 1 × 1, chaque
niveau compressé en **Zstandard** (supercompression KTX2 n° 2). C'est la version « broadcast » d'une image d'AdFlocon
(NOTE-80, variante « A bis ») : le moteur envoie ces blocs tels quels au GPU, sans décodage ni calcul de mip-maps ; le
décodeur rend ces fichiers lisibles par `ffplay` et `mpv` (lecteur autonome).

> Build : rien à préparer de plus que basis_universal (`../build-msys-prepare-basis-universal.sh`) — le BC7 est celui
> du Hap R, et **zstd est la copie que basis_universal compile dans `libbasisu_encoder.a`** (`zstd/zstd.c`, 1.5.7).
> Ne pas lier en plus la libzstd du système : chaque `ZSTD_*` serait défini deux fois. `../build-msys-shared.sh`
> vérifie l'en-tête `thirdparty/basis_universal/zstd/zstd.h` et refuse un build sans KTX2.

Vérifier :
```bash
ffmpeg -hide_banner -codecs | grep ktx2      # DEVIL. ktx2  Khronos KTX2 texture (BC7, mip levels, zstd)
```

---

## Encoder

`image2` reconnaît l'extension `.ktx2` :
```bash
ffmpeg -i photo.jpg -frames:v 1 -update 1 photo.ktx2
ffmpeg -i photo.png -mip_sharpen 5 -frames:v 1 -update 1 photo.ktx2
ffmpeg -i normal.png -srgb 0 -frames:v 1 -update 1 normal.ktx2          # carte de données : BC7_UNORM
```

| Option | Défaut | |
|---|---|---|
| `mipmaps` | 1 | les mip-maps jusqu'à 1 × 1 ; `0` = le niveau 0 seul (un KTX2 à un niveau) |
| `mip_sharpen` | 0 | netteté des mip-maps, « Sharpen 0..10 » d'Unreal ; `0` = la moyenne 2 × 2 |
| `srgb` | 1 | image sRGB : BC7_SRGB, mip-maps calculées en lumière linéaire ; `0` = BC7_UNORM, valeurs brutes |
| `bc7_quality` | 3 | qualité BC7 de basis_universal, 0 (le plus rapide) à 4 |
| `zstd_level` | 15 | niveau Zstandard de chaque mip-map ; `0` = niveaux stockés bruts (alignés sur 16 octets) |
| `kv` | — | clés `adtv.*` en plus (`-kv "adtv.recipe=broadcast:adtv.source=…"`) ; `KTX*`/`ktx*` refusées (réservées) |

- **Les mip-maps** sont celles du moteur d'AdFlocon (`src/engine/media/mipSharpenMath.ts`, porté ligne à ligne) : la
  chaîne des moyennes 2 × 2 descend, chaque niveau écrit en est une version accentuée, couleur de la moyenne mise à la
  luminance accentuée (sans dérive de couleur), bords serrés ; tailles `max(1, ⌊taille / 2⌋)`. Vérifié contre le noyau
  du moteur : à cran égal, l'écart est celui du BC7 (37–39 dB) ; à cran différent, 26–27 dB.
- **Un niveau dont la taille n'est pas un multiple de 4** est complété en répétant sa dernière ligne et colonne.
- **L'alpha** : une image qui en a passe par le chemin RGBA du BC7 ; une image opaque se décode avec un alpha de
  **255 exactement** (voir plus bas).
- **Les clés écrites** : `KTXwriter`, `adtv.bc7Quality`, `adtv.displayAspect`, `adtv.mipmaps`, `adtv.mipSharpen` (avec
  mip-maps), puis `kv`.
- **La proportion d'affichage** (`adtv.displayAspect`, « 5:3 ») : la largeur × SAR sur la hauteur, réduite. Une chaîne
  `setsar=1,scale=ceil(iw/4)*4:ceil(ih/4)*4` (AdFlocon) porte au multiple de 4 que WebGPU exige d'une texture BC, et le
  SAR garde la proportion de l'image d'avant : un 30 × 18 devient une texture 32 × 20 montrée en 5:3. Le décodeur la
  remet en SAR (ffplay, mpv : la bonne proportion) ; le moteur d'AdFlocon met l'image en page à cette proportion.
- **Vitesse** (VS1, 32 cœurs) : 1920 × 1280 avec ses 11 niveaux ≈ 470 ms ; le BC7 tourne en tranches sur les threads.

## Lire

```bash
ffplay photo.ktx2
ffplay -mip_level 3 photo.ktx2            # un niveau de mip-map (max(1, taille >> 3))
mpv photo.ktx2
ffmpeg -i photo.ktx2 photo.png
ffprobe -v verbose photo.ktx2             # les clés et le nombre de niveaux
```

- Le décodeur lit le niveau 0 (ou `mip_level`, le dernier s'il est au-delà), brut ou zstd, et le décode en RGBA avec
  le décodeur BC7 du Hap R ; `color_trc` sRGB ou linéaire selon le format, primaires BT.709.
- Lecture seule du BC7 2D (pas de tableau, de cube ni de profondeur, pas de Basis/UASTC) : un autre KTX2 est refusé
  avec un message clair.
- Un fichier est reconnu par son extension (`image2`) **et par son contenu** (`ktx2_pipe`, l'identifiant de 12 octets).
- mpv n'a **pas** besoin d'être recompilé : `AV_CODEC_ID_KTX2` est ajouté après `AV_CODEC_ID_BINKVIDEO2`, aucun
  identifiant existant ne bouge.

## Vérifié

- Structure relue contre la spécification (en-tête, index des niveaux du plus petit au plus grand dans le fichier,
  DFD BC7, clés triées, chaque niveau décompressé à sa taille) et par `ktx-parse` (Khronos / three, le parseur du
  `KTX2Loader`).
- Qualité : photo 1920 × 1280, BC7 qualité 3 ≈ 38 dB (35 dB en qualité 1) ; zstd gagne ≈ 23 % sur une photo, ≈ 50 %
  sur une image à grandes zones transparentes.

## L'alpha d'une image opaque (et le Hap R)

basis_universal, aux qualités BC7 **3 et 4**, active `cPackBC7FlagPBitOptMode6` (« alpha on fully opaque blocks may
be 254 however ») : des blocs en mode 6 portent un sommet alpha 127 avec un bit de parité 0, soit **254** — sur une
photo opaque de 1920 × 1280, **28,6 % des pixels**, en `ktx2` comme en **Hap R** (même encodeur, mêmes qualités).
Corrigé le 27 novembre 2026 dans les deux encodeurs (`libavcodec/bc7opaque.h`) : **bloc par bloc**, un bloc dont les
16 pixels sources sont opaques voit ses bits de parité alpha remis à 1 (modes 6 et 7) — il se décode à 255 exactement ;
la couleur de ce sommet bouge de 1/255 (mode 6) ou 1/63 (mode 7). Les blocs transparents ne changent pas. Les films Hap R
déjà encodés ne sont pas réencodés (NN : « micro bug quasi invisible »).
