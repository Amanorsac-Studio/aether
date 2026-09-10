@echo off
REM AETHER — Windows build (Visual Studio 2022). Run from the project folder.
REM Output: build\AETHER_artefacts\Release\VST3\AETHER.vst3  (also auto-copied to %COMMONPROGRAMFILES%\VST3)
cmake -B build -G "Visual Studio 17 2022" -A x64 || exit /b 1
cmake --build build --config Release --target AETHER_VST3 aether_dsp_test || exit /b 1
build\Release\aether_dsp_test.exe
echo.
echo Done. VST3 is in build\AETHER_artefacts\Release\VST3\
