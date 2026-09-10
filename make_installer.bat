@echo off
setlocal
REM ============================================================
REM  AETHER - one-click Windows build + installer
REM
REM  Needs: Visual Studio 2022 or 2026 (Desktop C++ workload) and CMake 3.22+.
REM  Result: dist\AETHER_Setup_1.0.0.exe  (plus a .sha256 next to it)
REM ============================================================
cd /d "%~dp0"

set PRESET=windows-vs2026
set BUILDDIR=build\windows-vs2026
cmake --preset %PRESET% >nul 2>&1
if errorlevel 1 (
    echo Visual Studio 2026 generator unavailable - falling back to Visual Studio 2022...
    set PRESET=windows-vs2022
    set BUILDDIR=build\windows-vs2022
    cmake --preset windows-vs2022 || goto :fail
)

echo.
echo === Building AETHER VST3 ^(Release^) ===
cmake --build %BUILDDIR% --config Release --target AETHER_VST3 aether_dsp_test --parallel || goto :fail

echo.
echo === DSP self-test ===
"%BUILDDIR%\Release\aether_dsp_test.exe" || goto :fail

echo.
echo === Packaging installer ===
powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File "installer\build-installer.ps1" -BuildDir "%BUILDDIR%" || goto :fail

echo.
echo Done. Run dist\AETHER_Setup_1.0.0.exe to install AETHER, then rescan VST3 in your DAW.
exit /b 0

:fail
echo.
echo BUILD FAILED - see the messages above.
exit /b 1
