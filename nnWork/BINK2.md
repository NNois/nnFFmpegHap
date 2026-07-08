# Bink Video 2 dans FFmpeg

Décodeur `binkvideo2` (`AV_CODEC_ID_BINKVIDEO2`) pour les fichiers **Bink 2**
(conteneur `.bk2`, signature `KB2`, révisions `i`/`j`/`k`/`n`). Le décodage
s'appuie sur le **SDK RAD Bink2** (`expand2.c`) lié statiquement.

> Build : `../build-msys-prepare-bink2sdk.sh` (compile `libbink2sdk.a`) puis
> `../build-msys-shared.sh`.
> Sans le SDK, `build-msys-shared.sh` avertit et le décodeur Bink2 n'a pas le
> chemin de décodage complet (pas de deblocking).

Vérifier :
```bash
ffmpeg -hide_banner -decoders | grep binkvideo2
```

---

## Utilisation

Le démuxeur `bink` détecte automatiquement Bink et Bink2 :
```bash
# Décoder un .bk2 vers un mov intermédiaire
ffmpeg -i movie.bk2 -c:v prores_ks -profile:v 3 movie.mov

# Extraire des frames PNG
ffmpeg -i movie.bk2 frame_%05d.png

# Inspecter
ffprobe movie.bk2
```

> Le décodeur est **intra + lossy**. Il n'y a **pas d'encodeur** Bink2 dans
> FFmpeg (format propriétaire RAD ; l'encodage se fait avec les outils RAD).

---

## Comment c'est construit

`build-msys-prepare-bink2sdk.sh` compile les sources du SDK
(`thirdparty/bink2sdk/src/`) en une lib statique `libbink2sdk.a` avec les
défines RAD requis :

```
-DUSING_EGT -DINC_BINK2 -DNO_BINK20 -DNTELEMETRY
-D__RADFINAL__ -D__RADINSTATICLIB__
```

Unités compilées : `expand2.c` (décodeur complet), `binkcomm.c`,
`binkmarkers.c`, `x86_cpu.c`, `win32_rrCpu.c`, plus `bink2sdk_shims.c`
(shims atomics/mémoire pour MinGW).

`build-msys-shared.sh` détecte `libbink2sdk.a` et ajoute `-I.../src`,
`-L.../build_mingw`, `-lbink2sdk` au link de FFmpeg. Le glue FFmpeg est
`libavcodec/bink2.c` (`#define USING_EGT` avant d'inclure les headers SDK).

---

## Notes

- Sources SDK versionnées dans `thirdparty/bink2sdk/src/` ;
  `build_mingw/` (objets + lib) ignoré par git.
- Lib **statique** → rien à bundler au runtime.
- `long_name` du codec : « Bink video 2 ».
