@echo off
setlocal

rem ---------------------------------------------------------------------------
rem x265 for the MSVC build: static 8+10+12 bit multilib, with alpha enabled.
rem
rem Multilib is not a nicety here. x265 only implements the alpha input path
rem for 8-bit pictures when the library itself is compiled at X265_DEPTH == 8
rem (source\common\picyuv.cpp, PicYuv::copyFromPicture). A 10-bit-only build
rem accepts yuva420p and silently fills the alpha layer with the base layer
rem luma instead. FFmpeg picks the library by input depth via
rem x265_api_get(depth) (libavcodec\libx265.c), so all three depths must ship
rem in the same archive for 8-bit alpha to be correct.
rem
rem Source is the tree vendored at thirdparty\x265, so this build stays on the
rem same x265 version as build-msys-prepare-x265-with-alpha.sh.
rem ---------------------------------------------------------------------------

set "REPO_DIR=%~dp0"
if "%X265_SRC%"=="" set "X265_SRC=%REPO_DIR%thirdparty\x265"
set "X265_BUILD=%REPO_DIR%build-msvc\x265-build"
set "X265_INSTALL=%REPO_DIR%build-msvc\x265-msvc"

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

where nasm >nul 2>&1
if errorlevel 1 (
  echo WARNING: nasm not found in PATH - x265 will be built without assembly
  echo          optimisations, which is several times slower.
)

if not exist "%X265_SRC%\source\CMakeLists.txt" (
  echo ERROR: x265 source not found at %X265_SRC%
  echo        Expected the tree vendored at thirdparty\x265.
  exit /b 1
)

echo.
echo ==========================================
echo x265 - static multilib 8+10+12 bit, ALPHA enabled
echo ==========================================
echo Source:  %X265_SRC%
echo Build:   %X265_BUILD%
echo Install: %X265_INSTALL%
echo.

echo Cleaning previous x265 build artifacts...
if exist "%X265_INSTALL%" rd /s /q "%X265_INSTALL%"
if exist "%X265_BUILD%" rd /s /q "%X265_BUILD%"
mkdir "%X265_BUILD%\8bit" || exit /b 1
mkdir "%X265_BUILD%\10bit" || exit /b 1
mkdir "%X265_BUILD%\12bit" || exit /b 1

rem Options shared by all three passes. ENABLE_ALPHA must be identical
rem everywhere: it drives MAX_LAYERS / MAX_SCALABLE_LAYERS, which size
rem internal structures. Mixing values across the combined archives would
rem corrupt them. Same reasoning for the CRT settings.
set COMMON_OPTS=-G "NMake Makefiles" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DENABLE_ALPHA=ON -DSTATIC_LINK_CRT=ON -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded

rem --- 12-bit: static, no public C API (namespaced as x265_12bit) ---
echo.
echo --- 12-bit static library ---
pushd "%X265_BUILD%\12bit"
cmake %COMMON_OPTS% -DHIGH_BIT_DEPTH=ON -DMAIN12=ON -DEXPORT_C_API=OFF -DENABLE_SHARED=OFF -DENABLE_CLI=OFF "%X265_SRC%\source" || exit /b 1
cmake --build . || exit /b 1
copy /y x265-static.lib "%X265_BUILD%\8bit\x265-static-main12.lib" >nul || exit /b 1
popd

rem --- 10-bit: static, no public C API (namespaced as x265_10bit) ---
echo.
echo --- 10-bit static library ---
pushd "%X265_BUILD%\10bit"
cmake %COMMON_OPTS% -DHIGH_BIT_DEPTH=ON -DEXPORT_C_API=OFF -DENABLE_SHARED=OFF -DENABLE_CLI=OFF "%X265_SRC%\source" || exit /b 1
cmake --build . || exit /b 1
copy /y x265-static.lib "%X265_BUILD%\8bit\x265-static-main10.lib" >nul || exit /b 1
popd

rem --- 8-bit: exports the public C API and dispatches to the two above ---
echo.
echo --- 8-bit static library (exports the C API) ---
pushd "%X265_BUILD%\8bit"
cmake %COMMON_OPTS% -DENABLE_SHARED=OFF -DENABLE_CLI=OFF -DLINKED_10BIT=ON -DLINKED_12BIT=ON -DEXTRA_LIB="x265-static-main10.lib;x265-static-main12.lib" -DCMAKE_INSTALL_PREFIX="%X265_INSTALL%" "%X265_SRC%\source" || exit /b 1
cmake --build . || exit /b 1

rem A static library records no dependencies, so the three archives have to be
rem merged by hand. Warnings 4006/4221 come from duplicate/empty objects and
rem are expected here.
move /y x265-static.lib x265-static-main.lib >nul || exit /b 1
LIB.EXE /ignore:4006 /ignore:4221 /OUT:x265-static.lib x265-static-main.lib x265-static-main10.lib x265-static-main12.lib || exit /b 1

rem Install after the merge so the combined archive is what gets published.
cmake --install . || exit /b 1
popd

rem FFmpeg's configure looks for x265.lib.
if exist "%X265_INSTALL%\lib\x265-static.lib" (
  copy /y "%X265_INSTALL%\lib\x265-static.lib" "%X265_INSTALL%\lib\x265.lib" >nul
) else (
  echo ERROR: %X265_INSTALL%\lib\x265-static.lib not found after install.
  exit /b 1
)

echo.
echo Verification...
findstr /c:"#define ENABLE_ALPHA 1" "%X265_INSTALL%\include\x265_config.h" >nul
if errorlevel 1 (
  echo   [FAIL] ENABLE_ALPHA is not 1 in the installed x265_config.h
  exit /b 1
)
echo   [OK] ENABLE_ALPHA exported as 1
for %%F in ("%X265_INSTALL%\lib\x265.lib") do echo   [OK] x265.lib merged, %%~zF bytes

echo.
echo ==========================================
echo x265 static multilib + alpha build complete.
echo ==========================================
echo Installed to: %X265_INSTALL%
echo.
echo Note: build-msvc.sh does not enable libx265 - that build is decode-only
echo       for Unity. To use this library, add to its configure line:
echo         --enable-libx265
echo         --enable-encoder=libx265
echo       plus -I%X265_INSTALL%\include in --extra-cflags and
echo       -LIBPATH:%X265_INSTALL%\lib in --extra-ldflags.
echo.
endlocal
