@echo off
setlocal EnableDelayedExpansion

set "DAV1D_SRC=C:\ff\dav1d-src"
set "DAV1D_INSTALL=C:\ff\ff\build-unity-static-msvc\dav1d-msvc"
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

set "CC=cl"
set "CXX=cl"
set "AR=lib"
set "LD=link"

meson setup build-msvc --prefix="%DAV1D_INSTALL%" --default-library=static --buildtype=release -Db_vscrt=mt || exit /b 1
meson compile -C build-msvc || exit /b 1
meson install -C build-msvc || exit /b 1

popd

if not exist "%DAV1D_INSTALL%\lib\dav1d.lib" (
  if exist "%DAV1D_INSTALL%\lib\libdav1d.a" (
    echo dav1d.lib not found; converting libdav1d.a to dav1d.lib...
    set "DAV1D_OBJ_DIR=%DAV1D_SRC%\dav1d-%DAV1D_VER%\build-msvc\dav1d-obj"
    if exist "%DAV1D_OBJ_DIR%" rd /s /q "%DAV1D_OBJ_DIR%"
    mkdir "%DAV1D_OBJ_DIR%"
    for /f "usebackq delims=" %%i in (`"%MSYS_BASH%" -lc "cygpath -u '%DAV1D_OBJ_DIR%'"`) do set "DAV1D_OBJ_DIR_UNIX=%%i"
    for /f "usebackq delims=" %%i in (`"%MSYS_BASH%" -lc "cygpath -u '%DAV1D_INSTALL%\\lib\\libdav1d.a'"`) do set "DAV1D_LIB_A_UNIX=%%i"
    "%MSYS_BASH%" -lc "mkdir -p \"%DAV1D_OBJ_DIR_UNIX%\"; cd \"%DAV1D_OBJ_DIR_UNIX%\" && ar x \"%DAV1D_LIB_A_UNIX%\" && find . -name '*.obj' -o -name '*.o' -print | while read -r f; do cygpath -w \"$f\"; done > objs.rsp" || exit /b 1
    lib.exe /nologo /OUT:"%DAV1D_INSTALL%\lib\dav1d.lib" @"%DAV1D_OBJ_DIR%\objs.rsp"
    if errorlevel 1 exit /b 1
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
