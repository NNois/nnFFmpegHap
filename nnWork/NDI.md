# NDI dans FFmpeg (libndi_newtek)

Support NDI (NewTek / Network Device Interface) réintégré dans cette build.
Device d'entrée **et** de sortie : `libndi_newtek`.

> Build : voir `../build-msys-prepare-ndisdk.sh` puis `../build-msys-shared.sh`.
> Le runtime `Processing.NDI.Lib.x64.dll` est bundlé à côté des exécutables
> (ou installer le « NDI Runtime » système).

Vérifier que c'est présent :
```bash
ffmpeg -hide_banner -devices | grep libndi_newtek
```

---

## Entrée (réception)

Le nom de fichier est le **nom de la source NDI** (format `MACHINE (SOURCE)`),
tel que listé par `-find_sources`.

### Lister les sources du réseau
```bash
ffmpeg -f libndi_newtek -find_sources 1 -i dummy
```
Inclure des machines hors mDNS (unicast) :
```bash
ffmpeg -f libndi_newtek -extra_ips "192.168.10.10,192.168.10.11" -find_sources 1 -i dummy
```

### Recevoir une source
```bash
ffmpeg -f libndi_newtek -i "MACHINE (Nom Source)" -c:v h264 -c:a aac out.mp4
```

### Prévisualiser (faible latence)
```bash
ffplay -fflags nobuffer -flags low_delay -framedrop \
       -f libndi_newtek -bandwidth 0 -i "MACHINE (Nom Source)"
```

### Recevoir avec mpv
mpv charge le FFmpeg custom de cette build (DLLs partagées), donc le device NDI
s'ouvre via le protocole `av://` (`av://<device>:<nom source>`) :
```bash
mpv av://libndi_newtek:"MACHINE (Nom Source)"
```
Passer des options du device via `--demuxer-lavf-o` (ex. bande passante basse) :
```bash
mpv --demuxer-lavf-o=bandwidth=1 av://libndi_newtek:"MACHINE (Nom Source)"
```
Faible latence :
```bash
mpv --profile=low-latency --no-cache --untimed \
    av://libndi_newtek:"MACHINE (Nom Source)"
```

### Options d'entrée
| Option | Déf. | Description |
|--------|------|-------------|
| `find_sources` | 0 | Lister les sources trouvées puis quitter. |
| `bandwidth` | 0 | `0` = haute, `1` = basse (proxy), `2` = audio seul. |
| `wait_sources` | 0.5s | Temps d'attente que la liste des sources se stabilise. |
| `allow_video_fields` | 1 | Si `0`, tout est reçu en progressif. |
| `extra_ips` | – | IPs (séparées par virgule) à scanner en plus du mDNS. |

---

## Sortie (émission)

Le nom de fichier est le **nom NDI** annoncé sur le réseau.
NDI attend nativement du `uyvy422` ; formats acceptés : `uyvy422`, `bgra`,
`bgr0`, `rgba`, `rgb0`. Il faut donc convertir le pixel format en amont.

### Émettre un fichier en NDI
```bash
ffmpeg -re -i entree.mov \
       -vf "format=uyvy422" -f libndi_newtek "FFMPEG Sortie 1"
```

### Ré-encoder / re-scaler puis émettre
```bash
ffmpeg -re -i entree.mov \
       -vf "scale=1280:720,fps=50,format=uyvy422" \
       -f libndi_newtek "FFMPEG Sortie 1"
```

### Relais NDI → NDI
```bash
ffmpeg -f libndi_newtek -i "MACHINE (Source A)" \
       -vf "format=uyvy422" -f libndi_newtek "Source B"
```

### Options de sortie
| Option | Déf. | Description |
|--------|------|-------------|
| `reference_level` | 0 | Niveau de référence audio en dB (headroom au-dessus de +4 dBU). |
| `clock_video` | 0 | Cadence l'émission sur l'horloge vidéo. |
| `clock_audio` | 0 | Cadence l'émission sur l'horloge audio. |

Exemple avec cadence sur l'horloge vidéo (émission temps réel régulière) :
```bash
ffmpeg -re -i entree.mov -vf "format=uyvy422" \
       -f libndi_newtek -clock_video 1 "FFMPEG Sortie 1"
```

---

## Notes

- **Alpha** : à la réception, une source UYVA (avec alpha) est décodée en
  `uyvy422` et le canal alpha est ignoré (un warning est émis). En émission,
  `bgra`/`rgba` transportent l'alpha.
- **Audio** : PCM 16 bits interleavé côté FFmpeg (conversion faite en interne).
- **Latence** : pour du monitoring, `-fflags nobuffer -flags low_delay
  -framedrop` côté `ffplay` ; `bandwidth 1` réduit la bande passante (flux proxy).
- **Licence** : device NDI = build `--enable-nonfree` (déjà le cas ici).
