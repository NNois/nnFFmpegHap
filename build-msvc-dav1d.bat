@echo off
setlocal EnableDelayedExpansion

set "DAV1D_SRC=C:\ff\dav1d-src"
set "DAV1D_INSTALL=C:\ff\ff\build-msvc\dav1d-msvc"
set "DAV1D_VER=1.5.3"
set "DAV1D_ZIP=dav1d-%DAV1D_VER%.zip"
set "DAV1D_URL=https://code.videolan.org/videolan/dav1d/-/archive/%DAV1D_VER%/dav1d-%DAV1D_VER%.zip"

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

where meson >nul 2>&1
if errorlevel 1 (
  echo ERROR: meson not found in PATH.
  echo Install with: pip install meson
  exit /b 1
)

where ninja >nul 2>&1
if errorlevel 1 (
  echo ERROR: ninja not found in PATH.
  echo Install with: pip install ninja
  exit /b 1
)

echo Cleaning previous dav1d build artifacts...
if exist "%DAV1D_INSTALL%" rd /s /q "%DAV1D_INSTALL%"

if not exist "%DAV1D_SRC%" mkdir "%DAV1D_SRC%"
if not exist "%DAV1D_SRC%\dav1d-%DAV1D_VER%" (
  echo Downloading dav1d %DAV1D_VER%...
  powershell -NoProfile -ExecutionPolicy Bypass -Command "Invoke-WebRequest -Uri '%DAV1D_URL%' -OutFile '%DAV1D_SRC%\%DAV1D_ZIP%'" || exit /b 1
  echo Extracting...
  powershell -NoProfile -ExecutionPolicy Bypass -Command "Expand-Archive -Force '%DAV1D_SRC%\%DAV1D_ZIP%' '%DAV1D_SRC%'" || exit /b 1
)

if not exist "%DAV1D_SRC%\dav1d-%DAV1D_VER%" (
  echo ERROR: dav1d source not found at %DAV1D_SRC%\dav1d-%DAV1D_VER%
  exit /b 1
)

pushd "%DAV1D_SRC%\dav1d-%DAV1D_VER%"
if exist "build-msvc" rd /s /q "build-msvc"

rem Get full paths to MSVC tools to avoid MSYS2 interference
set "CL_PATH=%VCToolsInstallDir%bin\Hostx64\x64\cl.exe"
set "LIB_PATH=%VCToolsInstallDir%bin\Hostx64\x64\lib.exe"

rem Create meson native file to force MSVC toolchain with full paths
echo [binaries] > msvc-native.ini
echo c = '%CL_PATH:\=\\%' >> msvc-native.ini
echo cpp = '%CL_PATH:\=\\%' >> msvc-native.ini
echo ar = '%LIB_PATH:\=\\%' >> msvc-native.ini
echo lib = '%LIB_PATH:\=\\%' >> msvc-native.ini
echo [built-in options] >> msvc-native.ini
echo c_args = ['/O2', '/MD', '/DNDEBUG', '/DDAV1D_BUILDING_DLL=0'] >> msvc-native.ini
echo cpp_args = ['/O2', '/MD', '/DNDEBUG', '/DDAV1D_BUILDING_DLL=0'] >> msvc-native.ini

echo.
echo Meson native file contents:
type msvc-native.ini
echo.

meson setup build-msvc --native-file=msvc-native.ini --prefix="%DAV1D_INSTALL%" --default-library=static --buildtype=release -Db_vscrt=md || exit /b 1
meson compile -C build-msvc || exit /b 1
meson install -C build-msvc || exit /b 1

popd

rem dav1d with MSVC should produce dav1d.lib directly
rem If libdav1d.a exists instead, the build used GCC - try to convert anyway
if not exist "%DAV1D_INSTALL%\lib\dav1d.lib" (
  if exist "%DAV1D_INSTALL%\lib\libdav1d.a" (
    echo WARNING: libdav1d.a found instead of dav1d.lib - meson may have used GCC
    echo Attempting to copy as dav1d.lib but this may not work with MSVC linker...
    copy "%DAV1D_INSTALL%\lib\libdav1d.a" "%DAV1D_INSTALL%\lib\dav1d.lib"
  )
)

if not exist "%DAV1D_INSTALL%\lib\dav1d.lib" (
  echo ERROR: dav1d.lib not found after build/convert.
  exit /b 1
)

echo.
echo dav1d build complete.
echo Installed to: %DAV1D_INSTALL%
endlocal
