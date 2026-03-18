@echo off
setlocal

set "PROJECT_DIR=%~dp0"

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
if not exist "%MSYS_BASH%" set "MSYS_BASH=C:\Program Files\Git\bin\bash.exe"
if not exist "%MSYS_BASH%" (
  echo ERROR: bash.exe not found. Install Git for Windows or MSYS2.
  exit /b 1
)

set "MSVC_BIN_WIN=%VCToolsInstallDir%bin\Hostx64\x64"
set "WINSDK_BIN_WIN=%WindowsSdkDir%bin\%WindowsSDKVersion%\x64"

"%MSYS_BASH%" -lc "MSVC_BIN_WIN=\"%MSVC_BIN_WIN%\"; WINSDK_BIN_WIN=\"%WINSDK_BIN_WIN%\"; MSVC_BIN=\"$(cygpath -u \"$MSVC_BIN_WIN\")\"; WINSDK_BIN=\"$(cygpath -u \"$WINSDK_BIN_WIN\")\"; export PATH=\"$MSVC_BIN:$WINSDK_BIN:/mingw64/bin:$PATH\"; export INCLUDE=\"$INCLUDE\"; export LIB=\"$LIB\"; cd \"$(cygpath -u '%PROJECT_DIR%')\" && ./build-msvc-adobe.sh"

endlocal
