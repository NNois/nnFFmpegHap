#!/bin/bash
# Test BC6 avec format d'entrée correct

echo "=== Test 1: Création vidéo source en RGBA64LE ==="
ffmpeg -f lavfi -i "color=c=red:s=256x256:d=0.1" -pix_fmt rgba64le -vframes 3 test_source.mkv -y 2>&1 | tail -5

ls -lh test_source.mkv

echo ""
echo "=== Test 2: Encodage HAP H (BC6U) quality=100 ==="
ffmpeg -i test_source.mkv -c:v hap -format hap_h -bc6_quality 100 -compressor none test_haph_q100.mov -y 2>&1 | tail -10

echo ""
echo "=== Test 3: Encodage HAP H (BC6U) quality=50 ==="
ffmpeg -i test_source.mkv -c:v hap -format hap_h -bc6_quality 50 -compressor none test_haph_q50.mov -y 2>&1 | tail -10

echo ""
echo "=== Test 4: Encodage HAP H (BC6U) quality=0 ==="
ffmpeg -i test_source.mkv -c:v hap -format hap_h -bc6_quality 0 -compressor none test_haph_q0.mov -y 2>&1 | tail -10

echo ""
echo "=== Comparaison tailles ==="
ls -lh test_haph_*.mov | awk '{print $5, $9}'

echo ""
echo "=== Vérification décodage ==="
ffmpeg -i test_haph_q100.mov -f null - 2>&1 | grep -E "(Stream|frame)"
