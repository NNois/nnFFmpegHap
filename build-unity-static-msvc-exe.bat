@echo off
setlocal

set "FFMPEG_DIR=C:\ff\ff"
if not exist "%FFMPEG_DIR%" (
  echo ERROR: FFmpeg dir not found: %FFMPEG_DIR%
  exit /b 1
)

echo Cleaning %FFMPEG_DIR%\build-unity-static-msvc ...
if exist "%FFMPEG_DIR%\build-unity-static-msvc" (
  rd /s /q "%FFMPEG_DIR%\build-unity-static-msvc"
)
mkdir "%FFMPEG_DIR%\build-unity-static-msvc" >nul 2>&1

set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
  echo ERROR: vcvars64.bat not found at %VCVARS%
  echo Please adjust the path to your Visual Studio installation.
  exit /b 1
)

call "%VCVARS%"
if errorlevel 1 (
  echo ERROR: vcvars64.bat failed.
  exit /b 1
)

set "MSYS_BASH=C:\tools\msys64\usr\bin\bash.exe"
if not exist "%MSYS_BASH%" (
  echo ERROR: bash.exe not found at %MSYS_BASH%
  echo Please adjust the path to your MSYS2 installation.
  exit /b 1
)

set "MSVC_BIN_WIN=%VCToolsInstallDir%bin\Hostx64\x64"
set "WINSDK_BIN_WIN=%WindowsSdkDir%bin\%WindowsSDKVersion%\x64"

pushd "%FFMPEG_DIR%"
rem echo Building dav1d...
rem call "%FFMPEG_DIR%\build-dav1d-msvc.bat"
rem if errorlevel 1 (
rem   echo ERROR: dav1d build failed.
rem   popd
rem   exit /b 1
rem )
"%MSYS_BASH%" -lc "MSVC_BIN_WIN=\"%MSVC_BIN_WIN%\"; WINSDK_BIN_WIN=\"%WINSDK_BIN_WIN%\"; MSVC_BIN=\"$(cygpath -u \"$MSVC_BIN_WIN\")\"; WINSDK_BIN=\"$(cygpath -u \"$WINSDK_BIN_WIN\")\"; export PATH=\"$MSVC_BIN:$WINSDK_BIN:$PATH\"; export INCLUDE=\"$INCLUDE\"; export LIB=\"$LIB\"; cd /c/ff/ff && ./build-unity-static-msvc-exe.sh"
popd
endlocal
