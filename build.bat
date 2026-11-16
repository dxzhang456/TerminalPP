@echo off
REM ============================================================
REM  Terminal++ build script (MinGW + Qt6, CMake generator)
REM
REM  Usage:
REM     build.bat              - configure + build (Release)
REM     build.bat clean        - delete the build directory first
REM     build.bat deploy       - build + run windeployqt to bundle Qt DLLs
REM
REM  Prerequisites (adjust QT_DIR / MINGW_DIR below to your install):
REM     - Qt 6.x for MinGW (e.g. C:\Qt\6.6.0\mingw_64)
REM     - MinGW-w64 toolchain that ships with Qt
REM       (e.g. C:\Qt\Tools\mingw1120_64\bin)
REM     - CMake 3.19+ (bundled with Qt or standalone)
REM     - Qt SerialPort module installed (via Qt Maintenance Tool)
REM     - libssh2 for MinGW (headers + libssh2.a/.dll); set LIBSSH2_DIR
REM       to its install root (containing include\ and lib\).
REM       Easiest via MSYS2:  pacman -S mingw-w64-x86_64-libssh2
REM ============================================================

setlocal enabledelayedexpansion

REM ---- EDIT THESE PATHS TO MATCH YOUR INSTALL ---------------
if "%QT_DIR%"=="" set "QT_DIR=C:\Qt\6.6.0\mingw_64"
if "%MINGW_DIR%"=="" set "MINGW_DIR=C:\Qt\Tools\mingw1120_64\bin"
REM Optional: libssh2 install root (contains include\ and lib\).
REM if "%LIBSSH2_DIR%"=="" set "LIBSSH2_DIR=C:\libssh2"
REM -----------------------------------------------------------

set "SCRIPT_DIR=%~dp0"
set "BUILD_DIR=%SCRIPT_DIR%build"

REM Put Qt + MinGW on PATH for this shell.
set "PATH=%QT_DIR%\bin;%MINGW_DIR%;%PATH%"

if /i "%1"=="clean" (
    echo [clean] Removing build directory...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    shift
)

set "PREFIX=%QT_DIR%"
if not "%LIBSSH2_DIR%"=="" set "PREFIX=%QT_DIR%;%LIBSSH2_DIR%"

echo [1/3] Configuring with CMake (MinGW Makefiles)...
cmake -S "%SCRIPT_DIR%" -B "%BUILD_DIR%" ^
    -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH="%PREFIX%"
if errorlevel 1 (
    echo *** CMake configuration failed. Check QT_DIR / MINGW_DIR. ***
    exit /b 1
)

echo [2/3] Building...
cmake --build "%BUILD_DIR%" --config Release -j
if errorlevel 1 (
    echo *** Build failed. ***
    exit /b 1
)

echo [3/3] Build succeeded: "%BUILD_DIR%\TerminalPP.exe"

if /i "%1"=="deploy" (
    echo [deploy] Running windeployqt...
    windeployqt --release "%BUILD_DIR%\TerminalPP.exe"
    echo [deploy] Qt runtime DLLs bundled next to TerminalPP.exe
)

echo.
echo Done. Run:  "%BUILD_DIR%\TerminalPP.exe"
endlocal