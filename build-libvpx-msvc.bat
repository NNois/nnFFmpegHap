@echo off
setlocal

set "LIBVPX_SRC=C:\ff\libvpx-src"
set "LIBVPX_INSTALL=C:\ff\ff\build-unity-static-msvc\libvpx-msvc"
set "LIBVPX_VER=1.15.2"
set "LIBVPX_ZIP=libvpx-%LIBVPX_VER%.zip"
set "LIBVPX_URL=https://github.com/webmproject/libvpx/archive/refs/tags/v%LIBVPX_VER%.zip"

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
  exit /b 1
)

set "MSBUILD_BIN_WIN=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin"
set "MSBUILD_EXE=%MSBUILD_BIN_WIN%\MSBuild.exe"
if not exist "%MSBUILD_EXE%" (
  echo ERROR: MSBuild.exe not found at %MSBUILD_EXE%
  exit /b 1
)

echo Cleaning previous libvpx build artifacts...
if exist "%LIBVPX_INSTALL%" (
  rd /s /q "%LIBVPX_INSTALL%"
)

if not exist "%LIBVPX_SRC%" mkdir "%LIBVPX_SRC%"
if not exist "%LIBVPX_SRC%\libvpx-%LIBVPX_VER%" (
  echo Downloading libvpx %LIBVPX_VER%...
  powershell -NoProfile -ExecutionPolicy Bypass -Command "Invoke-WebRequest -Uri '%LIBVPX_URL%' -OutFile '%LIBVPX_SRC%\%LIBVPX_ZIP%'" || exit /b 1
  echo Extracting...
  powershell -NoProfile -ExecutionPolicy Bypass -Command "Expand-Archive -Force '%LIBVPX_SRC%\%LIBVPX_ZIP%' '%LIBVPX_SRC%'" || exit /b 1
)

if not exist "%LIBVPX_SRC%\libvpx-%LIBVPX_VER%" (
  echo ERROR: libvpx source not found at %LIBVPX_SRC%\libvpx-%LIBVPX_VER%
  exit /b 1
)

set "MSVC_BIN_WIN=%VCToolsInstallDir%bin\Hostx64\x64"
set "WINSDK_BIN_WIN=%WindowsSdkDir%bin\%WindowsSDKVersion%\x64"

echo Building libvpx with MSVC (via MSYS2)...
pushd "%LIBVPX_SRC%\libvpx-%LIBVPX_VER%"

"%MSYS_BASH%" -lc "MSVC_BIN_WIN=\"%MSVC_BIN_WIN%\"; WINSDK_BIN_WIN=\"%WINSDK_BIN_WIN%\"; MSBUILD_BIN_WIN=\"%MSBUILD_BIN_WIN%\"; MSVC_BIN=\"$(cygpath -u \"$MSVC_BIN_WIN\")\"; WINSDK_BIN=\"$(cygpath -u \"$WINSDK_BIN_WIN\")\"; MSBUILD_BIN=\"$(cygpath -u \"$MSBUILD_BIN_WIN\")\"; export PATH=\"$MSVC_BIN:$WINSDK_BIN:$MSBUILD_BIN:$PATH\"; export INCLUDE=\"$INCLUDE\"; export LIB=\"$LIB\"; rm -rf build-msvc; mkdir build-msvc; cd build-msvc; ../configure --target=x86_64-win64-vs17 --disable-examples --disable-tools --disable-unit-tests --disable-docs --disable-webm-io --enable-vp8 --enable-vp9 --prefix=/c/ff/ff/build-unity-static-msvc/libvpx-msvc && make -j1 vpx.vcxproj && sed -i 's|<RuntimeLibrary>MultiThreadedDLL</RuntimeLibrary>|<RuntimeLibrary>MultiThreaded</RuntimeLibrary>|g' vpx.vcxproj && sed -i 's|<RuntimeLibrary>MultiThreadedDebugDLL</RuntimeLibrary>|<RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary>|g' vpx.vcxproj && make -j$(nproc) && make install" || exit /b 1

popd

echo.
echo libvpx build complete.
echo Installed to: %LIBVPX_INSTALL%

if not exist "%LIBVPX_INSTALL%\lib" mkdir "%LIBVPX_INSTALL%\lib" >nul 2>&1
if exist "%LIBVPX_INSTALL%\lib\x64\vpxmd.lib" (
  copy /Y "%LIBVPX_INSTALL%\lib\x64\vpxmd.lib" "%LIBVPX_INSTALL%\lib\vpx.lib" >nul
)
endlocal
