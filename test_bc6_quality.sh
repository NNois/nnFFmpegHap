#!/bin/bash
# Test pour diagnostiquer les problèmes de qualité BC6

# Créer une image de test HDR simple
echo "=== Création image test HDR ==="
ffmpeg -f lavfi -i testsrc=duration=1:size=256x256:rate=1 -pix_fmt rgbf32le -vframes 1 test_hdr.exr -y 2>&1 | tail -5

# Test 1: Encoder avec HAP H (BC6U) qualité max
echo ""
echo "=== Test HAP H (BC6U) quality=100 ==="
ffmpeg -i test_hdr.exr -c:v hap -format hap_h -bc6_quality 100 -pix_fmt rgbf16 test_haph_q100.mov -y 2>&1 | grep -E "(quality|bc6|frame|encoding)"

# Test 2: Encoder avec qualité réduite
echo ""
echo "=== Test HAP H quality=50 ==="
ffmpeg -i test_hdr.exr -c:v hap -format hap_h -bc6_quality 50 -pix_fmt rgbf16 test_haph_q50.mov -y 2>&1 | grep -E "(quality|bc6|frame|encoding)"

# Test 3: Encoder avec qualité minimale
echo ""
echo "=== Test HAP H quality=0 ==="
ffmpeg -i test_hdr.exr -c:v hap -format hap_h -bc6_quality 0 -pix_fmt rgbf16 test_haph_q0.mov -y 2>&1 | grep -E "(quality|bc6|frame|encoding)"

# Comparer les tailles
echo ""
echo "=== Comparaison tailles fichiers ==="
ls -lh test_haph_*.mov 2>&1 | awk '{print $5, $9}'
