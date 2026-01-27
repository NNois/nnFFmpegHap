@echo off
setlocal

set "DAV1D_SRC=C:\ff\dav1d-src"
set "DAV1D_INSTALL=C:\ff\ff\build-unity-static-msvc\dav1d-msvc"
set "DAV1D_VER=1.4.3"
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

meson setup build-msvc --prefix="%DAV1D_INSTALL%" --default-library=static --buildtype=release -Db_vscrt=mt || exit /b 1
meson compile -C build-msvc || exit /b 1
meson install -C build-msvc || exit /b 1

popd

echo.
echo dav1d build complete.
echo Installed to: %DAV1D_INSTALL%
endlocal
