@echo off
setlocal

set "SDL2_SRC=C:\ff\sdl2-src"
set "SDL2_INSTALL=C:\ff\ff\build-msvc\sdl2-msvc"
set "SDL2_VER=2.30.9"
set "SDL2_ZIP=SDL2-%SDL2_VER%.zip"
set "SDL2_URL=https://www.libsdl.org/release/%SDL2_ZIP%"

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

where cmake >nul 2>&1
if errorlevel 1 (
  echo ERROR: cmake not found in PATH.
  exit /b 1
)

echo Cleaning previous SDL2 build artifacts...
if exist "%SDL2_INSTALL%" (
  rd /s /q "%SDL2_INSTALL%"
)

if not exist "%SDL2_SRC%" mkdir "%SDL2_SRC%"
if not exist "%SDL2_SRC%\SDL2-%SDL2_VER%" (
  echo Downloading SDL2 %SDL2_VER%...
  powershell -NoProfile -ExecutionPolicy Bypass -Command "Invoke-WebRequest -Uri '%SDL2_URL%' -OutFile '%SDL2_SRC%\%SDL2_ZIP%'" || exit /b 1
  echo Extracting...
  powershell -NoProfile -ExecutionPolicy Bypass -Command "Expand-Archive -Force '%SDL2_SRC%\%SDL2_ZIP%' '%SDL2_SRC%'" || exit /b 1
)

if not exist "%SDL2_SRC%\SDL2-%SDL2_VER%" (
  echo ERROR: SDL2 source not found at %SDL2_SRC%\SDL2-%SDL2_VER%
  exit /b 1
)

set "SDL2_BUILD=%SDL2_SRC%\SDL2-%SDL2_VER%\build-msvc"
if exist "%SDL2_BUILD%" rd /s /q "%SDL2_BUILD%"
mkdir "%SDL2_BUILD%" >nul 2>&1

echo Configuring SDL2 with CMake (MSVC)...
cmake -S "%SDL2_SRC%\SDL2-%SDL2_VER%" -B "%SDL2_BUILD%" -G "Visual Studio 17 2022" -A x64 ^
  -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF ^
  -DCMAKE_INSTALL_PREFIX="%SDL2_INSTALL%" ^
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded || exit /b 1

echo Building and installing SDL2...
cmake --build "%SDL2_BUILD%" --config Release --target install || exit /b 1

echo.
echo Generating MSVC-compatible sdl2.pc...
(
echo # sdl pkg-config source file ^(MSVC compatible^)
echo.
echo prefix=%SDL2_INSTALL:\=/%
echo exec_prefix=${prefix}
echo libdir=${exec_prefix}/lib
echo includedir=${prefix}/include
echo.
echo Name: sdl2
echo Description: Simple DirectMedia Layer
echo Version: %SDL2_VER%
echo Requires.private:
echo Conflicts:
echo Libs: -LIBPATH:${libdir} SDL2-static.lib SDL2main.lib kernel32.lib user32.lib gdi32.lib winmm.lib imm32.lib ole32.lib oleaut32.lib version.lib uuid.lib advapi32.lib setupapi.lib shell32.lib dinput8.lib dxguid.lib
echo Cflags: -I${includedir} -I${includedir}/SDL2
) > "%SDL2_INSTALL%\lib\pkgconfig\sdl2.pc"

echo.
echo SDL2 build complete.
echo Installed to: %SDL2_INSTALL%
endlocal
