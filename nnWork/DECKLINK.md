# DeckLink (Blackmagic) dans FFmpeg

Support des cartes Blackmagic (capture/lecture SDI/HDMI) via le device
`decklink` — déjà présent dans FFmpeg, activé ici avec le SDK DeckLink 16.0.

> Build : `../build-msys-prepare-decklinksdk.sh` (génère les headers via `widl`)
> puis `../build-msys-shared.sh`.
> **Runtime** : installer **Blackmagic Desktop Video** sur la machine cible
> (le driver COM est chargé au runtime — rien à bundler avec ffmpeg).

Vérifier que c'est présent :
```bash
ffmpeg -hide_banner -devices | grep decklink
```

---

## Lister les périphériques et formats

### Cartes détectées
```bash
ffmpeg -sources decklink
# ou :
ffmpeg -f decklink -list_devices 1 -i dummy
```

### Formats supportés par une carte
```bash
ffmpeg -f decklink -list_formats 1 -i "DeckLink SDI 4K"
```

---

## Entrée (capture)

Le nom de fichier est le **nom de la carte** (tel que listé ci-dessus).
```bash
ffmpeg -f decklink -i "DeckLink SDI 4K" -c:v h264 -c:a aac capture.mp4
```

Forcer un mode précis (sinon auto-détection) :
```bash
ffmpeg -f decklink -format_code Hp50 -i "DeckLink SDI 4K" out.mkv
```

Options d'entrée fréquentes :
| Option | Description |
|--------|-------------|
| `format_code` | Code du mode vidéo (ex. `Hp50`, `Hi59`, `pal `). Voir `-list_formats`. |
| `video_input` | Connecteur : `sdi`, `hdmi`, `optical_sdi`, `component`, `composite`, `s_video`. |
| `audio_input` | Source audio : `embedded`, `analog`, `aes_ebu`… |
| `channels` | Nombre de canaux audio (2, 8, 16). |
| `raw_format` | Format de pixel brut (`uyvy422`, `yuv422p10`, etc.). |

---

## Sortie (playout)

```bash
ffmpeg -re -i entree.mov \
       -pix_fmt uyvy422 -s 1920x1080 -r 25 \
       -f decklink "DeckLink SDI 4K"
```
La sortie DeckLink attend des frames non compressées à une cadence/format
compatibles avec la carte ; adapter `-s`, `-r`, `-pix_fmt` en amont.

Options de sortie utiles : `-preroll`, `-duplex_mode`, `-timing_offset`.

---

## Notes

- **Génération des headers** : côté Windows le SDK ne fournit que des `.idl`
  (COM). `build-msys-prepare-decklinksdk.sh` les compile en `DeckLinkAPI.h` +
  `DeckLinkAPI_i.c` avec `widl` (paquet MSYS2 `mingw-w64-x86_64-tools`).
- **SDK 16.0** = `BLACKMAGIC_DECKLINK_API_VERSION 0x10000000` : au-dessus du
  minimum FFmpeg (10.11) et sur les chemins de code « modernes ».
- **Patch de compat SDK 15+/16** : le SDK 15.0 (`0x0f000000`) a retiré
  `IDeckLinkVideoFrame::GetBytes()` (déplacé vers `IDeckLinkVideoBuffer`) et
  supprimé `IDeckLinkMemoryAllocator` / `SetVideoInputFrameMemoryAllocator`.
  FFmpeg 8.0 utilise l'ancienne API, donc `libavdevice/decklink_dec.cpp` et
  `decklink_enc.cpp` ont été patchés (gardés par
  `#if BLACKMAGIC_DECKLINK_API_VERSION`) :
  - capture : accès aux pixels via `IDeckLinkVideoBuffer` (QueryInterface) ;
  - allocateur mémoire custom désactivé en SDK 15+ (allocateur par défaut) ;
  - sortie : la frame custom implémente aussi `IDeckLinkVideoBuffer`.
  Le code reste rétro-compatible avec les SDK ≤ 14.x. La **sortie** (playout)
  sur SDK 16 est à vérifier en conditions réelles.
- **Threads** : la build utilise `--disable-w32threads --enable-pthreads`
  (winpthreads). C'est **obligatoire** pour DeckLink : les devices sont en C++
  et incluent `libavutil/thread.h` ; le chemin par défaut `w32threads`
  (`compat/w32pthreads.h`) n'est pas compilable en C++ (son `typedef pthread_t`
  entre en conflit avec le tag de struct). Il faut **les deux** flags :
  `w32threads` est activé par autodétection, et `--enable-pthreads` seul saute
  le bloc qui le désactiverait → configure meurt sur « pthreads requested but
  not found ». `libwinpthread-1.dll` est déjà bundlé.
- **Licence** : contrairement au NDI, `decklink` n'impose pas `--enable-nonfree`
  (mais la build l'active déjà pour d'autres composants).
- Sources `.idl` versionnées dans `thirdparty/decklinksdk/idl/` ; l'`include/`
  généré est ignoré par git (recréé par le script prepare).
