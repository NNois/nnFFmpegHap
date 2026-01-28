@echo off
setlocal

set "X265_SRC=C:\ff\x265-src"
set "X265_INSTALL=C:\ff\ff\build-msvc\x265-msvc"
set "X265_GIT=https://bitbucket.org/multicoreware/x265_git.git"
set "X265_REF=stable"

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

where git >nul 2>&1
if errorlevel 1 (
  echo ERROR: git not found in PATH.
  exit /b 1
)

where cmake >nul 2>&1
if errorlevel 1 (
  echo ERROR: cmake not found in PATH.
  exit /b 1
)

echo Cleaning previous x265 build artifacts...
if exist "%X265_INSTALL%" rd /s /q "%X265_INSTALL%"

if not exist "%X265_SRC%" mkdir "%X265_SRC%"
if not exist "%X265_SRC%\x265_git" (
  echo Cloning x265 (%X265_REF%)...
  git clone --depth 1 -b %X265_REF% %X265_GIT% "%X265_SRC%\x265_git"
  if errorlevel 1 exit /b 1
)

if not exist "%X265_SRC%\x265_git" (
  echo ERROR: x265 source not found at %X265_SRC%\x265_git
  exit /b 1
)

pushd "%X265_SRC%\x265_git"
if exist "build-msvc" rd /s /q "build-msvc"
mkdir "build-msvc"
pushd "build-msvc"

cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DENABLE_SHARED=OFF -DENABLE_CLI=OFF -DSTATIC_LINK_CRT=ON -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DCMAKE_INSTALL_PREFIX="%X265_INSTALL%" ..\source || exit /b 1
cmake --build . || exit /b 1
cmake --install . || exit /b 1

popd
popd

if not exist "%X265_INSTALL%\lib" mkdir "%X265_INSTALL%\lib" >nul 2>&1
if exist "%X265_INSTALL%\lib\x265-static.lib" (
  copy /Y "%X265_INSTALL%\lib\x265-static.lib" "%X265_INSTALL%\lib\x265.lib" >nul
)

echo.
echo x265 build complete.
echo Installed to: %X265_INSTALL%
endlocal
