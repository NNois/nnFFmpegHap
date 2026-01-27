@echo off
setlocal

set "X264_SRC=C:\ff\x264-src"
set "X264_INSTALL=C:\ff\ff\build-unity-static-msvc\x264-msvc"
set "X264_GIT=https://code.videolan.org/videolan/x264.git"
set "X264_REF=stable"

set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
  echo ERROR: vcvars64.bat not found at %VCVARS%
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

where git >nul 2>&1
if errorlevel 1 (
  echo ERROR: git not found in PATH.
  exit /b 1
)

echo Cleaning previous x264 build artifacts...
if exist "%X264_INSTALL%" (
  rd /s /q "%X264_INSTALL%"
)

if not exist "%X264_SRC%" mkdir "%X264_SRC%"
if not exist "%X264_SRC%\x264" (
  echo Cloning x264 (%X264_REF%)...
  git clone --depth 1 -b %X264_REF% %X264_GIT% "%X264_SRC%\x264" || exit /b 1
)

if not exist "%X264_SRC%\x264" (
  echo ERROR: x264 source not found at %X264_SRC%\x264
  exit /b 1
)

set "MSVC_BIN_WIN=%VCToolsInstallDir%bin\Hostx64\x64"
set "WINSDK_BIN_WIN=%WindowsSdkDir%bin\%WindowsSDKVersion%\x64"

echo Building x264 with MSVC (via MSYS2)...
pushd "%X264_SRC%\x264"

"%MSYS_BASH%" -lc "MSVC_BIN_WIN=\"%MSVC_BIN_WIN%\"; WINSDK_BIN_WIN=\"%WINSDK_BIN_WIN%\"; MSVC_BIN=\"$(cygpath -u \"$MSVC_BIN_WIN\")\"; WINSDK_BIN=\"$(cygpath -u \"$WINSDK_BIN_WIN\")\"; export PATH=\"$MSVC_BIN:$WINSDK_BIN:$PATH\"; export INCLUDE=\"$INCLUDE\"; export LIB=\"$LIB\"; export CC=cl; export AR=lib; export LD=link; export MSYS2_ARG_CONV_EXCL='*'; make distclean 2>/dev/null || true; ./configure --enable-static --disable-cli --disable-asm --prefix=/c/ff/ff/build-unity-static-msvc/x264-msvc --extra-cflags=\"/MT\" --extra-ldflags=\"/MT\" && make -j$(nproc) && make install" || exit /b 1

popd

if not exist "%X264_INSTALL%\lib" mkdir "%X264_INSTALL%\lib" >nul 2>&1
if exist "%X264_INSTALL%\lib\libx264.lib" (
  copy /Y "%X264_INSTALL%\lib\libx264.lib" "%X264_INSTALL%\lib\x264.lib" >nul
)

echo.
echo x264 build complete.
echo Installed to: %X264_INSTALL%
endlocal
