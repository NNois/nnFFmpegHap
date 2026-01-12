#!/bin/bash
# Test BC6 avec image simple

# Créer une image de test colorée en PNG
echo "=== Création image test ==="
ffmpeg -f lavfi -i "color=c=red:s=256x256:d=0.1,format=rgba" -vframes 1 test_red.png -y 2>&1 | tail -3

# Vérifier que le fichier existe
ls -lh test_red.png

# Test 1: Encoder avec HAP H quality=100
echo ""
echo "=== Encodage HAP H quality=100 avec RGBF16 ==="
ffmpeg -i test_red.png -c:v hap -format hap_h -bc6_quality 100 -pix_fmt rgbf16 test_haph_q100.mov -y 2>&1 | tail -8

# Test 2: Encoder avec HAP H quality=0
echo ""
echo "=== Encodage HAP H quality=0 avec RGBF16 ==="
ffmpeg -i test_red.png -c:v hap -format hap_h -bc6_quality 0 -pix_fmt rgbf16 test_haph_q0.mov -y 2>&1 | tail -8

# Vérifier les fichiers créés
echo ""
echo "=== Fichiers créés ==="
ls -lh test_haph_*.mov test_red.png 2>&1 | awk '{print $5, $9}'

# Décoder et comparer
echo ""
echo "=== Test décodage q100 ==="
ffmpeg -i test_haph_q100.mov -vframes 1 -f null - -v error 2>&1

echo "=== Test décodage q0 ==="
ffmpeg -i test_haph_q0.mov -vframes 1 -f null - -v error 2>&1
