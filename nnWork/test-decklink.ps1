# ============================================================================
# Test DeckLink « pur ffmpeg » — indépendant de Flocon : envoie une MIRE
# (testsrc2 + bip 1 kHz) directement à la carte avec le ffmpeg embarqué, et
# journalise TOUT dans decklink-test.log (à côté du script).
#
# À lancer via test-decklink.cmd, ou :
#   powershell -File test-decklink.ps1 [-Device "DeckLink"] [-Size 1920x1080]
#              [-Rate 50] [-Duration 10]
#
# NOTE compatibilité : ce ffmpeg est compilé contre le SDK DeckLink 16.0 —
# la machine doit avoir des drivers Desktop Video >= 16.x (un 11.x liste
# parfois les cartes mais NE PEUT PAS ouvrir la sortie).
# Pour du 1080i50 : -Rate 25 et décommenter les options d'entrelacé plus bas.
# ============================================================================
param(
  [string]$Device = "DeckLink",
  [string]$Size = "1920x1080",
  [int]$Rate = 50,
  [int]$Duration = 10,
  # Keyer matériel (build ffmpeg patché AD) : "internal" = incrust sur l'entrée
  # SDI ; "external" = fill sur SDI A + key sur SDI B. La mire devient un carré
  # rouge sur fond TRANSPARENT (l'alpha est le key).
  [ValidateSet("off", "internal", "external")]
  [string]$Keyer = "off"
)

$ff = Join-Path $PSScriptRoot "ffmpeg.exe"
$log = Join-Path $PSScriptRoot "test-decklink.log"

# Tee vers le log ET l'écran (Out-Host : rien ne fuit dans le flux de retour
# des fonctions, sinon `$code = Run ...` capturerait tout le texte).
function Say([string]$text) { $text | Tee-Object -FilePath $log -Append | Out-Host }

function Run([string]$title, [string[]]$ffArgs, [switch]$MayFail) {
  Say ""
  Say "───────────────────────────────────────────────────────────────"
  Say "── $title"
  Say "── ffmpeg $($ffArgs -join ' ')"
  Say "───────────────────────────────────────────────────────────────"
  $out = & $ff @ffArgs 2>&1 | ForEach-Object { "$_" }
  $script:lastOutput = $out
  $out | Tee-Object -FilePath $log -Append | Out-Host
  $code = $LASTEXITCODE
  Say "── code de sortie : $code $(if ($code -eq 0) { '(OK)' } elseif ($MayFail) { '(informatif)' } else { '(ÉCHEC)' })"
  return $code
}

"=== Test DeckLink — $(Get-Date) ===" | Tee-Object -FilePath $log | Out-Host   # (re)crée le log
Say "ffmpeg : $ff"
Say "carte  : $Device   mire : $Size@$Rate   durée : $Duration s"

if (-not (Test-Path $ff)) {
  Say "ERREUR : ffmpeg.exe introuvable à côté du script."
  exit 1
}

# 1. Le build : la ligne "configuration:" doit contenir --enable-decklink.
Run "Version du build ffmpeg" @("-hide_banner", "-version") -MayFail | Out-Null

# 2. Les cartes vues par ffmpeg (sorties DeckLink).
Run "Cartes DeckLink détectées (sorties)" @("-hide_banner", "-sinks", "decklink") -MayFail | Out-Null

# Le nom exact compte (« DeckLink 4K Extreme 12G », pas « DeckLink ») : si le
# nom demandé n'est pas dans la liste, on bascule sur la première carte vue.
$cards = @($script:lastOutput |
  Where-Object { $_ -match "^\s*[0-9a-fA-F]+:[0-9a-fA-F]+:[0-9a-fA-F]+\s+\[(.+)\]" } |
  ForEach-Object { $Matches[1] })
if ($cards.Count -gt 0 -and $cards -notcontains $Device) {
  Say ""
  Say ">> Carte « $Device » introuvable — utilisation de « $($cards[0]) » (détectée)."
  $Device = $cards[0]
}

# 3. Les modes vidéo annoncés par la carte choisie (révèle aussi les erreurs
#    d'API : un driver trop vieux échoue déjà ici).
Run "Modes vidéo de « $Device »" @("-hide_banner", "-f", "decklink", "-list_formats", "1", "-i", $Device) -MayFail | Out-Null

# 4. LA MIRE : testsrc2 + bip 1 kHz → la carte. uyvy422 = le format natif SDI.
#    Avec -Keyer : carré rouge sur fond transparent, en BGRA (alpha = key).
if ($Keyer -eq "off") {
  $code = Run "MIRE $Size@$Rate pendant $Duration s → « $Device »" @(
    "-hide_banner", "-loglevel", "info", "-stats",
    "-f", "lavfi", "-i", "testsrc2=size=${Size}:rate=$Rate",
    "-f", "lavfi", "-i", "sine=frequency=1000:sample_rate=48000",
    "-t", "$Duration",
    "-pix_fmt", "uyvy422",
    "-c:a", "pcm_s16le", "-ac", "2",
    "-f", "decklink", $Device
  )
} else {
  $w = [int]($Size -split "x")[0]; $h = [int]($Size -split "x")[1]
  $box = "color=red:s=400x200:r=${Rate},format=rgba,pad=${w}:${h}:(ow-iw)/2:(oh-ih)/2:color=black@0.0"
  $code = Run "KEYER $Keyer — carré rouge sur alpha, $Size@$Rate, $Duration s → « $Device »" @(
    "-hide_banner", "-loglevel", "info", "-stats",
    "-f", "lavfi", "-i", $box,
    "-t", "$Duration",
    "-pix_fmt", "bgra",
    "-keyer", $Keyer,
    "-f", "decklink", $Device
  )
}

Say ""
if ($code -eq 0) {
  Say "VERDICT : OK — la mire est passée. Si l'écran SDI est resté noir,"
  Say "vérifier le câblage/connecteur de sortie (Fill) et le mode du moniteur."
} else {
  Say "VERDICT : ÉCHEC (code $code). Pistes, dans l'ordre :"
  Say " 1. Drivers Blackmagic Desktop Video >= 16.x requis (build SDK 16.0)."
  Say "    Version installée : Blackmagic Desktop Video Setup → À propos."
  Say " 2. Nom de carte exact : reprendre celui listé à l'étape « sinks »"
  Say "    (test-decklink.cmd ""Nom exact de la carte"")."
  Say " 3. Mode non supporté : choisir un Size@Rate listé à l'étape « modes »."
  Say " 4. Carte déjà ouverte par une autre appli (Media Express, OBS...)."
}
Say ""
Say "Journal complet : $log"
exit $code
