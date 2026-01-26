@echo off
setlocal

set "ZLIB_SRC=C:\ff\zlib-src"
set "ZLIB_INSTALL=C:\ff\ff\build-unity-static-msvc\zlib-msvc"
set "ZLIB_VER=1.3.1"
set "ZLIB_ZIP=zlib131.zip"
set "ZLIB_URL=https://zlib.net/zlib131.zip"

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

echo Cleaning previous zlib build artifacts...
if exist "%ZLIB_INSTALL%" (
  rd /s /q "%ZLIB_INSTALL%"
)

if not exist "%ZLIB_SRC%" mkdir "%ZLIB_SRC%"
if not exist "%ZLIB_SRC%\zlib-%ZLIB_VER%" (
  echo Downloading zlib %ZLIB_VER%...
  powershell -NoProfile -ExecutionPolicy Bypass -Command "Invoke-WebRequest -Uri '%ZLIB_URL%' -OutFile '%ZLIB_SRC%\%ZLIB_ZIP%'" || exit /b 1
  echo Extracting...
  powershell -NoProfile -ExecutionPolicy Bypass -Command "Expand-Archive -Force '%ZLIB_SRC%\%ZLIB_ZIP%' '%ZLIB_SRC%'" || exit /b 1
)

if not exist "%ZLIB_SRC%\zlib-%ZLIB_VER%" (
  echo ERROR: zlib source not found at %ZLIB_SRC%\zlib-%ZLIB_VER%
  exit /b 1
)

pushd "%ZLIB_SRC%\zlib-%ZLIB_VER%"

echo Cleaning zlib source directory...
nmake -f win32\Makefile.msc clean 2>nul || rem

echo Building zlib with MSVC (pure C, no ASM)...
nmake -f win32\Makefile.msc || exit /b 1

popd

if not exist "%ZLIB_INSTALL%\include" mkdir "%ZLIB_INSTALL%\include"
if not exist "%ZLIB_INSTALL%\lib" mkdir "%ZLIB_INSTALL%\lib"

copy /Y "%ZLIB_SRC%\zlib-%ZLIB_VER%\zlib.h" "%ZLIB_INSTALL%\include\" >nul
copy /Y "%ZLIB_SRC%\zlib-%ZLIB_VER%\zconf.h" "%ZLIB_INSTALL%\include\" >nul

echo Patching zconf.h for MSVC compatibility...
powershell -NoProfile -ExecutionPolicy Bypass -Command "(Get-Content '%ZLIB_INSTALL%\include\zconf.h') -replace '#ifdef HAVE_UNISTD_H','#if 0 /* HAVE_UNISTD_H - disabled for MSVC */' | Set-Content '%ZLIB_INSTALL%\include\zconf.h'"

if exist "%ZLIB_SRC%\zlib-%ZLIB_VER%\zlib.lib" copy /Y "%ZLIB_SRC%\zlib-%ZLIB_VER%\zlib.lib" "%ZLIB_INSTALL%\lib\" >nul
if exist "%ZLIB_SRC%\zlib-%ZLIB_VER%\zlibstatic.lib" copy /Y "%ZLIB_SRC%\zlib-%ZLIB_VER%\zlibstatic.lib" "%ZLIB_INSTALL%\lib\" >nul

if exist "%ZLIB_SRC%\zlib-%ZLIB_VER%\zlib1.dll" copy /Y "%ZLIB_SRC%\zlib-%ZLIB_VER%\zlib1.dll" "%ZLIB_INSTALL%\lib\" >nul
if exist "%ZLIB_SRC%\zlib-%ZLIB_VER%\zlib1.dll" copy /Y "%ZLIB_SRC%\zlib-%ZLIB_VER%\zlib1.dll" "%ZLIB_INSTALL%\" >nul

echo.
echo Zlib build complete.
echo Installed to: %ZLIB_INSTALL%
endlocal
