@echo off
REM ============================================================
REM  Double-click this to publish AETHER to GitHub.
REM
REM  It hands off to init_repo.sh, which places the CI workflow,
REM  removes the superseded .NET installer files, commits, and
REM  pushes to Amanorsac-Studio/aether using your own saved Git
REM  credentials. Nothing here stores or prints a token.
REM ============================================================
cd /d "%~dp0"

set "BASH="
if exist "%PROGRAMFILES%\Git\bin\bash.exe" set "BASH=%PROGRAMFILES%\Git\bin\bash.exe"
if not defined BASH if exist "%PROGRAMFILES(x86)%\Git\bin\bash.exe" set "BASH=%PROGRAMFILES(x86)%\Git\bin\bash.exe"
if not defined BASH if exist "%LOCALAPPDATA%\Programs\Git\bin\bash.exe" set "BASH=%LOCALAPPDATA%\Programs\Git\bin\bash.exe"

if not defined BASH (
    echo Could not find Git Bash. Open Git Bash here and run:  ./init_repo.sh
    echo.
    pause
    exit /b 1
)

echo Running init_repo.sh with "%BASH%"
echo.
REM -c, not -lc: a login shell can change the working directory.
"%BASH%" -c "cd \"$(cygpath -u '%~dp0')\" && ./init_repo.sh"
set RC=%ERRORLEVEL%

echo.
if %RC%==0 (
    echo Done. https://github.com/Amanorsac-Studio/aether
) else (
    echo Finished with errors ^(exit %RC%^).
    echo A full transcript was written to push_log.txt in this folder.
)
echo.
pause
exit /b %RC%
