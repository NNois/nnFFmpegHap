@echo off
:: Test DeckLink "pur ffmpeg" (mire + bip vers la carte) ? voir test-decklink.ps1.
:: Usage :
::   test-decklink.cmd                                    (carte auto, 1080p50, 10 s)
::   test-decklink.cmd "DeckLink Mini Monitor 4K"
::   test-decklink.cmd "DeckLink" 1280x720 50 20
::   test-decklink.cmd external                           (keyer carte : fill+key sur les 2 SDI)
::   test-decklink.cmd "DeckLink 4K Extreme 12G" internal (keyer interne : incrust sur l'entree SDI)
:: Journal : test-decklink.log a cote du script.
setlocal EnableDelayedExpansion
set "ARGS="
set /a POS=0
:parse
if "%~1"=="" goto run
if /I "%~1"=="internal" (
    set "ARGS=!ARGS! -Keyer internal"
) else if /I "%~1"=="external" (
    set "ARGS=!ARGS! -Keyer external"
) else (
    set /a POS+=1
    if !POS!==1 set "ARGS=!ARGS! -Device "%~1""
    if !POS!==2 set "ARGS=!ARGS! -Size %~1"
    if !POS!==3 set "ARGS=!ARGS! -Rate %~1"
    if !POS!==4 set "ARGS=!ARGS! -Duration %~1"
)
shift
goto parse
:run
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0test-decklink.ps1" %ARGS%
echo.
pause
