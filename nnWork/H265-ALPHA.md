# H.265 / HEVC avec canal alpha

Support de l'alpha HEVC via un **x265 buildé maison** (`ENABLE_ALPHA=ON`, multilib
8+10+12 bits), car le paquet MSYS2 `x265` n'a ni l'alpha ni le multilib.

> ✅ **État (2026-09-04) : décodage OK, encodage OK.**
> Le décodage multi-couche est arrivé en amont dans FFmpeg et fonctionne
> nativement sur cette build 8.1.2 pour les flux conformes. Les fichiers
> **Apple/VideoToolbox** exigent en plus un patch local, absent de toute 8.1.x
> (voir *Fichiers Apple*). L'encodage exige un x265 **multilib**, sinon l'alpha
> 8 bits est silencieusement corrompu (voir *Pourquoi le multilib*).

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

> 📌 **Où trouver les versions à jour** :
> <https://github.com/Multicorewareinc/x265/releases>
>
> x265 a migré sur GitHub. Le Bitbucket historique
> (`bitbucket.org/multicoreware/x265_git`) **s'arrête à la 4.2** et n'est plus
> alimenté — ne pas s'y fier pour juger de la dernière version. Le script
> télécharge l'asset `x265_<version>.tar.gz` de la release GitHub, qui a la
> même arborescence que les anciens tarballs Bitbucket.
>
> État des versions : 4.2 = `X265_BUILD 216` (vendorisée ici),
> **4.3 = `X265_BUILD 217`**, publiée le 31/07/2026.

Côté MSVC : `build-msvc-x265.bat` produit la même chose en statique. Note que
`build-msvc.sh` **n'active pas** libx265 — cette build est décode-only pour Unity.

Vérifier :
```bash
/mingw64/bin/x265 --version          # doit afficher 8bit+10bit+12bit
/mingw64/bin/x265 --help | grep -- --alpha

# La présence du 12 bits prouve que x265_api_get(12) répond, donc le multilib :
ffmpeg -h encoder=libx265 | grep "Supported pixel formats"
# ... yuv420p12le yuv422p12le yuv444p12le gbrp12le gray12le yuva420p yuva420p10le
```

Sans multilib la liste s'arrête au 10 bits, et `yuva420p` est un piège : il est
annoncé mais produit un alpha faux.

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

Mesures de l'aller-retour, alpha constant à 255 sur 320x240, relevées sur cette
build :

| x265 | débit couche alpha | plan A redécodé |
|---|---|---|
| 10 bits seul, entrée `yuva420p` | 293 kb/s | min=5 max=221 ❌ |
| 10 bits seul, entrée `yuva420p10le` | 7 kb/s | min=254 max=255 ✅ |
| **multilib 4.2, entrée `yuva420p`** | **7,1 kb/s** | **min=254 max=255 ✅** |

Un alpha plat qui coûte ~300 kb/s est le symptôme : la couche encode une image,
pas un aplat.

Deuxième signe que le multilib est actif : en 8 bits le flux **reste** en
`yuva420p`. Avec un x265 10 bits seul, `x265_api_get(8)` renvoie `NULL` et tout
était promu en Main 10, y compris un simple `-pix_fmt yuv420p`.

---

## Encodage

```bash
ffmpeg -y -i in_4444.mov -c:v libx265 -crf 22 -tag:v hvc1 -an out.mov
```

FFmpeg détecte le plan alpha et passe l'encodeur en `yuva420p` ou
`yuva420p10le` selon la profondeur de la source — les deux sont corrects depuis
le multilib. Preuve dans les logs x265 : **deux** jeux de stats (couche base +
couche alpha), et `tools: ... alpha` dans l'en-tête.

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

### Fichiers Apple / VideoToolbox — patch requis

Apple est de très loin le premier producteur de HEVC-alpha (iPhone, Final Cut,
Motion, exports macOS), mais ses premières versions génèrent des extensions VPS
non conformes. Le 8.1.2 d'origine échoue à les parser, **jette la couche alpha**
et se contente d'un avertissement :

```
[hevc] Ignoring unsupported VPS extension
pixfmt:yuv420p              <-- l'alpha a disparu, la vidéo se lit normalement
```

Le correctif amont existe en **FFmpeg 9.0** mais **n'est pas rétroporté** dans la
branche `release/8.1` : aucune 8.1.x ne l'aura jamais. Il est donc appliqué ici
en local : [0001-hevc-alpha-videotoolbox-workaround.patch](0001-hevc-alpha-videotoolbox-workaround.patch).

Trois modifications dans `libavcodec/hevc/ps.c` :
- deux `AVERROR_INVALIDDATA` → `AVERROR_PATCHWELCOME` dans `decode_vps_ext()`,
  pour que l'erreur devienne rattrapable ;
- dans `ff_hevc_decode_nal_vps()`, un repli qui **conserve** la couche alpha si
  elle a déjà été analysée, au lieu de forcer `nb_layers = 1`, et pose
  `poc_lsb_not_present` pour que les IDR de cette couche ne lisent pas
  `pic_order_cnt_lsb`.

Après patch, le message devient `Broken VPS extension, treating as alpha video`
et le flux sort en `yuva420p`. Validé sur un `.mov` VideoToolbox de 2021
(1280x720, hvc1) : plan alpha `min=0 max=255`, 18,6 % de pixels transparents et
60,8 % d'opaques — un vrai cache, pas un plan constant.

⚠️ `ffprobe` continue d'annoncer `yuv420p` sur ces fichiers : c'est normal, il
décrit la couche de base (voir plus haut). Le patch se vérifie sur le message
et sur `pixfmt:` en décodage, pas sur la ligne `Stream`.

Les fichiers produits par x265 sont conformes et n'ont jamais été concernés.

### Côté mpv — second patch, indépendant du précédent

Patcher FFmpeg ne suffit pas : mpv continuait d'afficher `A=none`. La cause
n'est pas dans le décodeur mais dans le **choix du format de sortie**.

`avcodec_default_get_format()` retourne le **dernier** format de la liste
(`libavcodec/decode.c`), par convention le format logiciel « simple ». Or
`hevcdec` propose `[hwaccels..., yuva420p, yuv420p]` : le dernier est donc celui
*sans* alpha. Et `setup_multilayer()` n'active la couche alpha que si le format
retenu porte `AV_PIX_FMT_FLAG_ALPHA` — l'alpha est donc abandonné avant même
d'être décodé.

| | choix du format |
|---|---|
| CLI ffmpeg (`fftools/ffmpeg_dec.c`) | **premier** non-hwaccel → `yuva420p` ✅ |
| `avcodec_default_get_format` | **dernier** → `yuv420p` ❌ |

Le CLI y échappe parce qu'il installe son propre `get_format()`. mpv n'en avait
aucun en décodage logiciel — il n'en pose un que pour le décodage matériel.
D'où l'écart sur le même fichier.

Correctif : [mpv-0001-vd_lavc-prefer-alpha-pixfmt.patch](mpv-0001-vd_lavc-prefer-alpha-pixfmt.patch),
à appliquer dans **`C:\ff\mpv`** (pas dans FFmpeg). Il ajoute un `get_format()`
logiciel qui privilégie un format à alpha quand le décodeur en propose un, et
retombe sur le comportement par défaut sinon.

Rien de tout ceci n'existe en amont chez mpv, et ce n'est pas propre au HEVC :
tout codec proposant une variante à alpha est concerné. C'est d'ailleurs pour
une raison voisine que VP8/VP9 avait dû être traité, mais côté FFmpeg
(cf. [0001-vp8-vp9-alpha-support.patch](0001-vp8-vp9-alpha-support.patch)).

### Côté adConvert (`C:\AD\nnTools\adConvert_app.py`)

Deux correctifs, hors de ce dépôt :

- **Extension `.mp4` → `.mov`** pour le preset `libx265`. HEVC-alpha est une
  convention Apple/QuickTime distribuée en `.mov` ; l'alpha survit techniquement
  en `.mp4`, mais QuickTime et Safari attendent `.mov`.
- **Suppression du bloc qui ajoutait `-x265-params alpha=1`.** FFmpeg négocie
  l'alpha tout seul depuis le pixel format (« ignore forced alpha option. The
  pixel format is all we need. » dans `libavcodec/libx265.c`) — cette option
  était donc redondante quand un plan alpha était présent, et une **panne pure**
  sinon : la détection faisait `"a" in pix_fmt`, vraie aussi pour `"gray"`, donc
  `libx265` refusait `alpha=1` avec `EINVAL` et **toute source en niveaux de
  gris échouait à s'encoder en H.265**. Le bloc est retiré, `-tag:v hvc1` reste.

Validé : encodage `.mov` HEVC-alpha (plan A identique à la source, relu par
mpv patché en `yuva420p`/`A=straight`) et encodage H.265 d'une source grise
(échouait avant, passe maintenant).

---

## Déploiement : le piège du 4.1 → 4.2

Le passage à 4.2 change **le nom du DLL et le symbole exporté** :

| | DLL | symbole |
|---|---|---|
| paquet MSYS2 (pacman) | `libx265-215.dll` | `x265_api_get_215` |
| build maison 4.1 | `libx265.dll` | `x265_api_get_215` |
| build maison 4.2 | `libx265.dll` | `x265_api_get_216` |
| build maison 4.3 | `libx265.dll` | `x265_api_get_217` |

Chaque montée de version de x265 incrémente ce symbole, donc **toute mise à jour
de x265 impose de rebuilder FFmpeg puis de redéployer le bundle**. Un bundle
déployé avec l'ancien `libx265.dll` casse immédiatement.

Un `avcodec-62.dll` compilé contre la 4.2 exige `x265_api_get_216`. S'il trouve
un ancien `libx265.dll` — dans le dossier de l'application, ou n'importe où dans
le `PATH` — Windows échoue avec :

```
The procedure entry point x265_api_get_216 could not be located
in the dynamic link library ...\avcodec-62.dll
```

Le remède est de **toujours embarquer** `libx265.dll` à côté des exécutables,
jamais de compter sur le `PATH` : le dossier de l'application est prioritaire
dans l'ordre de recherche de Windows. C'est ce que fait
`build-msys-copy-with-dlls-shared.sh`, qui résout les dépendances par `ldd`
depuis le dossier de destination et refuse de rendre la main si le bundle n'est
pas autonome.

Noter que `libx265-215.dll` (MSYS2) et `libx265.dll` (maison) portent des noms
différents : ils cohabitent sans conflit dans `/mingw64/bin`, ce qui rend le
problème invisible tant qu'on ne déploie pas.

---

## Notes

- x265 utilisé : **4.2** (`X265_BUILD 216`), multilib 8+10+12, shared dans
  `/mingw64`, source dans `thirdparty/x265`.
- La 4.2 exporte `ENABLE_ALPHA` dans `x265_config.h` (pas la 4.1), ce qui rend
  `MAX_LAYERS` cohérent entre FFmpeg et la bibliothèque.
- `pacman -S mingw-w64-x86_64-x265` ne remplace pas `libx265.dll` (son DLL
  s'appelle `libx265-215.dll`), mais il **écrase** `x265_config.h`, `x265.pc`,
  `libx265.dll.a` et `x265.exe`. FFmpeg se reconfigurerait alors sur
  `X265_BUILD 215` tout en liant une DLL 216. Relancer le script après toute
  mise à jour du paquet.
- `build-msys-shared.sh` fait un préflight x265 et signale un x265 non-multilib.
- Autres formats à alpha disponibles : ProRes 4444, VP9/WebM, FFV1, et côté
  textures GPU **Hap Alpha / Hap M** (voir [HAP.md](HAP.md)).
