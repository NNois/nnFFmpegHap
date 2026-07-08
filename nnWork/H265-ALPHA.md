# H.265 / HEVC avec canal alpha

Support de l'alpha HEVC via un **x265 buildé maison** (`ENABLE_ALPHA=ON`,
`HIGH_BIT_DEPTH=ON`) car le paquet MSYS2 `x265` n'a pas l'alpha.

> ⚠️ **État : encodage OK, décodage KO.** L'alpha est bien encodé dans le
> fichier (deux couches x265), mais le décodeur HEVC de FFmpeg ne sait pas
> encore extraire la couche alpha (limitation amont, `AVERROR_PATCHWELCOME`).
> Analyse détaillée + plan d'implémentation : [README_HEVC-Alpha.md](README_HEVC-Alpha.md).

---

## Build

```bash
# 1) x265 avec alpha (remplace le paquet MSYS2, s'installe dans /mingw64)
./build-msys-prepare-x265-with-alpha.sh

# 2) FFmpeg (détecte le x265 alpha via pkg-config)
./build-msys-shared.sh
```

`build-msys-shared.sh` fait un préflight x265 (Step 2.5) et signale si le x265
détecté annonce l'alpha.

Vérifier :
```bash
/mingw64/bin/x265 --version
/mingw64/bin/x265 --fullhelp | grep -i alpha
ffmpeg -h encoder=libx265 | grep -i alpha
```

---

## Encodage (fonctionne)

Depuis une source avec alpha (ProRes 4444, etc.). FFmpeg détecte le plan alpha
et passe l'encodeur en `yuva420p10le` (le « A » = alpha) :

```bash
ffmpeg -y -i in_4444.mov -c:v libx265 -crf 22 -tag:v hvc1 -an out.mp4
```

Preuve dans les logs x265 : **deux** jeux de stats (couche base + couche alpha) :
```
encoded N frames ... 2177 kb/s   <- base
encoded N frames ...  471 kb/s   <- alpha
```

---

## Décodage (ne fonctionne pas encore)

```bash
ffprobe out.mp4
# Stream ... hevc (Main 10), yuv420p10le   <-- pas de "A", alpha perdu
# [hevc] Unknown profile bitstream / Multiple scalability types presented
```

L'alpha **est dans le fichier** mais n'est pas restitué. Pour le récupérer
aujourd'hui, garder la source (ProRes 4444) ou passer par un format à alpha
supporté (VP9/WebM, FFV1, ProRes 4444).

---

## Notes

- x265 utilisé : **4.1**, buildé en shared dans `/mingw64`.
- Le décodage multi-couche (HEVC Annex F/I) est à implémenter côté FFmpeg ;
  modèle de référence = l'alpha VP8/VP9 (décodeurs séparés + merge). Roadmap
  complète dans [README_HEVC-Alpha.md](README_HEVC-Alpha.md).
- Alternatives alpha qui marchent aujourd'hui : ProRes 4444, VP9/WebM, FFV1,
  et côté textures GPU **Hap Alpha / Hap M** (voir [HAP.md](HAP.md)).
