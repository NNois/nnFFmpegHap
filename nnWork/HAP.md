# HAP dans FFmpeg

Codec HAP (textures GPU-ready, décodage temps réel) — encodeur `hap` amélioré
dans cette build : ajout de Hap R (BC7), Hap HDR (BC6H), Hap Alpha-Only, Hap M,
et correction de la transformation YCoCg de Hap Q.

> BC7 (Hap R) et BC6H (Hap H) sont encodés via **basis_universal**
> (voir [BASIS_UNIVERSAL.md](BASIS_UNIVERSAL.md)), lié statiquement.
> La seconde passe de compression utilise **Snappy** (`--enable-libsnappy`).

Vérifier :
```bash
ffmpeg -hide_banner -h encoder=hap
ffmpeg -hide_banner -encoders | grep hap
```

---

## Variantes (`-format`)

| `-format`   | Nom          | Compression texture              | FourCC | Entrée         |
|-------------|--------------|----------------------------------|--------|----------------|
| `hap`       | Hap 1        | BC1 (DXT1)                       | `Hap1` | RGB            |
| `hap_alpha` | Hap Alpha    | BC3 (DXT5)                       | `Hap5` | RGBA           |
| `hap_q`     | Hap Q        | YCoCg scaled DXT5 (BC3)          | `HapY` | RGB            |
| `hap_m`     | Hap Q Alpha  | YCoCg DXT5 + BC4 (alpha séparé)  | `HapM` | RGBA           |
| `hap_a`     | Hap Alpha-Only | BC4 (alpha seul)               | `HapA` | (canal alpha)  |
| `hap_r`     | Hap R        | BC7 (BPTC)                      | `Hap7` | RGBA           |
| `hap_h`     | Hap HDR      | BC6H (BPTC float, unsigned)     | `HapH` | RGB half-float |

Le conteneur habituel est MOV/MP4 (`.mov`).

---

## Options d'encodage

| Option | Déf. | Portée | Description |
|--------|------|--------|-------------|
| `-format` | `hap` | — | Variante (table ci-dessus). |
| `-bc7_quality` | 3 | Hap R | Qualité BC7, `0`..`4` (0 = rapide, 4 = lent/meilleur). |
| `-bc6_quality` | 2 | Hap H | Qualité BC6H, `0`..`4`. |
| `-chunks` | 1 | toutes | Découpe en N chunks pour décodage multi-thread. |
| `-compressor` | `snappy` | toutes | 2ᵉ passe : `snappy` ou `none`. |

**Pixel formats d'entrée** : `rgba` (ou `gray8` pour l'alpha-only) pour toutes
les variantes SDR ; `rgbf16` / `rgba64le` / `rgba64be` pour **Hap HDR**.
Utiliser `format=` en amont pour convertir.

---

## Exemples

### Hap 1 (base, plus léger)
```bash
ffmpeg -i in.mov -c:v hap -format hap out_hap1.mov
```

### Hap Q (meilleure qualité couleur)
```bash
ffmpeg -i in.mov -c:v hap -format hap_q out_hapq.mov
```

### Hap Alpha / Hap M (avec alpha)
```bash
ffmpeg -i in_rgba.mov -vf "format=rgba" -c:v hap -format hap_alpha out_hap5.mov
ffmpeg -i in_rgba.mov -vf "format=rgba" -c:v hap -format hap_m     out_hapm.mov
```

### Hap R (BC7, haute qualité)
```bash
ffmpeg -i in.mov -vf "format=rgba" -c:v hap -format hap_r -bc7_quality 4 out_hap7.mov
```

### Hap HDR (BC6H, half-float)
```bash
ffmpeg -i in_hdr.exr -vf "format=rgbf16" -c:v hap -format hap_h -bc6_quality 3 out_haph.mov
```

### Multi-chunk (décodage parallèle côté lecteur)
```bash
ffmpeg -i in.mov -c:v hap -format hap_q -chunks 4 out.mov
```

---

## Décodage

Le décodeur `hap` gère toutes les variantes (Hap1/5/Y/M/A/7). Exemple :
```bash
ffmpeg -i out_hap7.mov -pix_fmt rgba frame_%04d.png
```
> `HapH` (BC6H HDR) : les MOV tags sont reconnus mais le **décodage HDR** n'est
> pas implémenté côté FFmpeg (encode only).

---

## Notes

- **Hap R / BC7** : l'encodeur (bc7enc via basis_universal) produit les modes
  1/5/6/7. Le décodeur BC7 de FFmpeg gère ces modes ; modes 0/2/3/4 = TODO.
- **Hap Q** : la transformation YCoCg (scaled) a été corrigée dans cette build.
- **Hap M** = « Hap Q Alpha » (multi-texture : YCoCg DXT5 + BC4 alpha), headers
  chunk conformes à la spec.
- Détails d'implémentation et checklist : voir [README.md](README.md) et
  [PLAN.md](PLAN.md).
