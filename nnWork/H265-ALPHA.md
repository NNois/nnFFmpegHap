# H.265 / HEVC avec canal alpha

Support de l'alpha HEVC via un **x265 buildé maison** (`ENABLE_ALPHA=ON`, multilib
8+10+12 bits), car le paquet MSYS2 `x265` n'a ni l'alpha ni le multilib.

> ✅ **État (2026-09-03) : décodage OK, encodage OK.**
> Le décodage multi-couche est arrivé en amont dans FFmpeg — il fonctionne
> nativement sur cette build 8.1.2, sans patch. L'encodage exige un x265
> **multilib**, sinon l'alpha 8 bits est silencieusement corrompu (voir plus bas).

Les notes de [README_HEVC-Alpha.md](README_HEVC-Alpha.md) décrivent l'état de
janvier 2026 et une roadmap d'implémentation du décodage : **obsolètes**,
gardées pour l'historique. Le décodeur amont fait déjà le travail.

---

## Build

```bash
# 1) x265 multilib 8+10+12 avec alpha (remplace le paquet MSYS2, va dans /mingw64)
./build-msys-prepare-x265-with-alpha.sh

# 2) FFmpeg (détecte le x265 alpha via pkg-config)
./build-msys-shared.sh
```

Source x265 vendorisée dans `thirdparty/x265` (4.2). Pour changer de version :
`X265_VERSION=4.3 ./build-msys-prepare-x265-with-alpha.sh` (télécharge si absente).

Côté MSVC : `build-msvc-x265.bat` produit la même chose en statique. Note que
`build-msvc.sh` **n'active pas** libx265 — cette build est décode-only pour Unity.

Vérifier :
```bash
/mingw64/bin/x265 --version          # doit afficher 8bit+10bit+12bit
/mingw64/bin/x265 --help | grep -- --alpha
ffmpeg -h encoder=libx265 | grep -i yuva      # yuva420p yuva420p10le
```

---

## Pourquoi le multilib est obligatoire

x265 n'implémente le chemin d'entrée alpha 8 bits **que** si la bibliothèque
elle-même est compilée à `X265_DEPTH == 8` (`source/common/picyuv.cpp`,
`PicYuv::copyFromPicture` : le bloc `#if ENABLE_ALPHA` n'existe que dans la
branche `#else /* Case for (X265_DEPTH == 8) */`).

Avec un x265 10 bits seul, `libx265` accepte `yuva420p` sans broncher et remplit
la couche alpha avec **le luma de la couche de base**. Aucun warning.

FFmpeg choisit la bibliothèque selon la profondeur d'entrée
(`ctx->api = x265_api_get(desc->comp[0].depth)`, `libavcodec/libx265.c`), donc
embarquer les trois profondeurs dans la même DLL est ce qui rend l'alpha 8 bits
correct. Ce n'est pas un réglage de confort.

Mesures de l'aller-retour, alpha constant à 255 sur 320x240 :

| x265 | débit couche alpha | plan A redécodé |
|---|---|---|
| 10 bits seul, entrée `yuva420p` | 293 kb/s | min=5 max=221 ❌ |
| 10 bits seul, entrée `yuva420p10le` | 7 kb/s | min=254 max=255 ✅ |
| multilib, les deux | 7 kb/s | min=254 max=255 ✅ |

Un alpha plat qui coûte ~300 kb/s est le symptôme : la couche encode une image,
pas un aplat.

---

## Encodage

```bash
ffmpeg -y -i in_4444.mov -c:v libx265 -crf 22 -tag:v hvc1 -an out.mov
```

FFmpeg détecte le plan alpha et passe l'encodeur en `yuva420p` / `yuva420p10le`.
Preuve dans les logs x265 : **deux** jeux de stats (couche base + couche alpha),
et `tools: ... alpha` dans l'en-tête.

```
encoded N frames ... 2177 kb/s   <- base
encoded N frames ...  471 kb/s   <- alpha
```

---

## Décodage

Fonctionne nativement, en MOV comme en Annex-B brut :

```bash
ffmpeg -i out.mov -vf format=yuva420p,alphaextract -frames:v 1 alpha.png
```

⚠️ **`ffprobe` ne montre pas l'alpha** et ce n'est pas un bug :

```
$ ffprobe out.mov
Stream ... hevc (Main 10), yuv420p10le      <-- couche de base uniquement
```

Le `pix_fmt` au niveau du flux décrit la couche de base ; l'alpha n'apparaît
qu'après décodage effectif. C'est cette ligne qui avait fait conclure à tort au
« décodage KO » en janvier. Le vrai test :

```bash
ffmpeg -v verbose -i out.mov -frames:v 1 -f null - 2>&1 | grep pixfmt
# pixfmt:yuva420p10le   <-- l'alpha est bien là
```

Deux warnings sont **cosmétiques** et n'empêchent rien :
- `Multiple scalability types presented` — x265 signale MULTIVIEW *et*
  AUXILIARY alors que le flux n'est que de l'alpha ; commenté tel quel dans
  `libavcodec/hevc/ps.c`.
- `Unknown profile bitstream` — simple log du PTL de la couche alpha
  (`ps.c:281`), niveau warning faute de nom de profil connu.

Côté implémentation : `ff_hevc_is_alpha_video()` (`libavcodec/hevc/hevcdec.c`)
et `replace_alpha_plane()` (`libavcodec/hevc/refs.c`), qui fait décoder la
couche 1 directement dans le plan 3 de la frame de base.

Le décodage matériel (dxva2/d3d11va/vulkan) porte sur la couche de base ; le
chemin alpha testé est logiciel.

---

## Notes

- x265 utilisé : **4.2** (`X265_BUILD 216`), multilib 8+10+12, shared dans
  `/mingw64`, source dans `thirdparty/x265`.
- La 4.2 exporte `ENABLE_ALPHA` dans `x265_config.h` (pas la 4.1), ce qui rend
  `MAX_LAYERS` cohérent entre FFmpeg et la bibliothèque.
- `pacman -S mingw-w64-x86_64-x265` écrase cette build : relancer le script.
- `build-msys-shared.sh` fait un préflight x265 et signale un x265 non-multilib.
- Autres formats à alpha disponibles : ProRes 4444, VP9/WebM, FFV1, et côté
  textures GPU **Hap Alpha / Hap M** (voir [HAP.md](HAP.md)).
