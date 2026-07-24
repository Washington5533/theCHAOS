@echo off
setlocal enabledelayedexpansion
title Chaos Pendulum - Release Builder

echo ============================================
echo   Chaos Pendulum Release Build
echo ============================================
echo.

:: ── Find MSBuild ──────────────────────────────
set MSBUILD=
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2^>nul`) do (
    set MSBUILD=%%i
)
if "!MSBUILD!"=="" (
    echo [WARN] vswhere not found, trying PATH...
    where msbuild >nul 2>&1
    if !errorlevel! neq 0 (
        echo [ERROR] MSBuild not found! Run from VS Developer Command Prompt.
        exit /b 1
    )
    set MSBUILD=msbuild
)
echo MSBuild: !MSBUILD!
echo.

:: ── Clean previous release ────────────────────
set RELEASE_DIR=%~dp0..\release
if exist "!RELEASE_DIR!" (
    echo Cleaning previous release...
    rmdir /s /q "!RELEASE_DIR!"
)
mkdir "!RELEASE_DIR!\SDL2"
mkdir "!RELEASE_DIR!\EasyX"

:: ── Build SDL2 Version ────────────────────────
echo.
echo [1/4] Building SDL2 Release x64...
"!MSBUILD!" "%~dp0..\Game\Game.slnx" ^
    /p:Configuration=Release ^
    /p:Platform=x64 ^
    /v:minimal
if !errorlevel! neq 0 (
    echo [ERROR] SDL2 build failed!
    exit /b 1
)
echo [OK] SDL2 build succeeded.

:: ── Build EasyX Version ───────────────────────
echo.
echo [2/4] Building EasyX Release x64...
"!MSBUILD!" "%~dp0..\Game-easyx\Game-easyx.slnx" ^
    /p:Configuration=Release ^
    /p:Platform=x64 ^
    /v:minimal
if !errorlevel! neq 0 (
    echo [WARN] EasyX build failed (may need EasyX SDK installed)
    goto :package
)
echo [OK] EasyX build succeeded.

:: ── Package ───────────────────────────────────
:package
echo.
echo [3/4] Packaging SDL2 release...

:: Copy SDL2 exe
set SDL2_EXE=%~dp0..\Game\Game\x64\Release\Game.exe
if exist "!SDL2_EXE!" (
    copy /y "!SDL2_EXE!" "!RELEASE_DIR!\SDL2\" >nul
    echo   Game.exe
) else (
    echo   [WARN] Game.exe not found at !SDL2_EXE!
    echo   Checking Debug build...
    set SDL2_EXE=%~dp0..\Game\Game\x64\Debug\Game.exe
    if exist "!SDL2_EXE!" (
        copy /y "!SDL2_EXE!" "!RELEASE_DIR!\SDL2\" >nul
        echo   Game.exe (Debug fallback)
    )
)

:: Copy SDL2 DLLs
for %%d in ("%~dp0..\Game\Game\SDL2.dll" "%~dp0..\Game\Game\SDL2_ttf.dll") do (
    if exist %%d (
        copy /y %%d "!RELEASE_DIR!\SDL2\" >nul
        echo   %%~nxd
    )
)

:: Copy EasyX exe + assets
set EASYX_EXE=%~dp0..\Game-easyx\Game\x64\Release\Game.exe
if exist "!EASYX_EXE!" (
    copy /y "!EASYX_EXE!" "!RELEASE_DIR!\EasyX\" >nul
    echo   EasyX\Game.exe
    xcopy /y /q "%~dp0..\Game-easyx\assets\*" "!RELEASE_DIR!\EasyX\assets\" >nul 2>&1
    echo   EasyX\assets\
) else (
    echo   [WARN] EasyX Game.exe not found
)

:: ── Copy README ───────────────────────────────
echo.
echo [4/4] Copying README...
copy /y "%~dp0..\README.md" "!RELEASE_DIR!\" >nul

:: ── Summary ────────────────────────────────────
echo.
echo ============================================
echo   Release built: !RELEASE_DIR!
echo ============================================
dir /s /b "!RELEASE_DIR!"
echo.
echo Done! Upload release\SDL2\ and release\EasyX\ to GitHub Releases.
pause
