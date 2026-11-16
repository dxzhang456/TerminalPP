<#
    Terminal++ build script (MinGW + Qt6)

    Examples:
        .\build.ps1                 # configure + build (Release)
        .\build.ps1 -Clean          # wipe build dir first
        .\build.ps1 -Deploy         # build + windeployqt
        .\build.ps1 -QtDir C:\Qt\6.6.0\mingw_64

    The script auto-detects a MinGW Qt6 install under C:\Qt if -QtDir
    is not supplied, and prefers the qmake build if available.
#>

param(
    [string]$QtDir = "",
    [string]$MingwDir = "",
    [string]$Libssh2Dir = "",   # libssh2 install root (contains include\ and lib\)
    [switch]$Clean,
    [switch]$Deploy,
    [switch]$UseQmake
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$buildDir  = Join-Path $scriptDir "build"

function Find-QtDir {
    if ($QtDir -ne "") { return $QtDir }
    if ($env:QT_DIR)   { return $env:QT_DIR }

    $candidates = Get-ChildItem -Path "C:\Qt" -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^6\.' } |
        Sort-Object Name -Descending
    foreach ($c in $candidates) {
        $mingw = Get-ChildItem -Path $c.FullName -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -like 'mingw*' } | Select-Object -First 1
        if ($mingw) { return $mingw.FullName }
    }
    throw "Qt6 (MinGW) not found. Pass -QtDir C:\Qt\6.x.x\mingw_64"
}

function Find-MingwDir {
    param($qt)
    if ($MingwDir -ne "") { return $MingwDir }
    if ($env:MINGW_DIR)   { return $env:MINGW_DIR }

    $tools = Get-ChildItem -Path "C:\Qt\Tools" -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like 'mingw*' } |
        Sort-Object Name -Descending | Select-Object -First 1
    if ($tools) { return (Join-Path $tools.FullName "bin") }

    throw "MinGW toolchain not found under C:\Qt\Tools. Pass -MingwDir."
}

$qt    = Find-QtDir
$mingw = Find-MingwDir $qt

Write-Host "Qt dir   : $qt"
Write-Host "MinGW dir: $mingw"

$env:PATH = "$qt\bin;$mingw;$env:PATH"

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "[clean] Removing $buildDir"
    Remove-Item -Recurse -Force $buildDir
}

if ($UseQmake) {
    # ---------- qmake path ----------
    New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
    Push-Location $buildDir
    Write-Host "[qmake] Generating Makefiles..."
    & "$qt\bin\qmake.exe" "$scriptDir\TerminalPP.pro"
    Write-Host "[make] Building..."
    & mingw32-make -j
    Pop-Location
    $exe = Join-Path $buildDir "release\TerminalPP.exe"
    if (-not (Test-Path $exe)) { $exe = Join-Path $buildDir "TerminalPP.exe" }
} else {
    # ---------- CMake path ----------
    Write-Host "[1/2] Configuring (CMake, MinGW Makefiles)..."
    $prefix = $qt
    if ($Libssh2Dir -ne "") { $prefix = "$qt;$Libssh2Dir" }
    elseif ($env:LIBSSH2_DIR) { $prefix = "$qt;$($env:LIBSSH2_DIR)" }
    & cmake -S $scriptDir -B $buildDir -G "MinGW Makefiles" `
        -DCMAKE_BUILD_TYPE=Release `
        "-DCMAKE_PREFIX_PATH=$prefix"
    if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed." }

    Write-Host "[2/2] Building..."
    & cmake --build $buildDir --config Release -j
    if ($LASTEXITCODE -ne 0) { throw "Build failed." }

    $exe = Join-Path $buildDir "TerminalPP.exe"
}

if (-not (Test-Path $exe)) {
    throw "Build finished but executable not found at $exe"
}
Write-Host "Build succeeded: $exe" -ForegroundColor Green

if ($Deploy) {
    Write-Host "[deploy] Running windeployqt..."
    & "$qt\bin\windeployqt.exe" --release $exe
    Write-Host "[deploy] Qt runtime bundled." -ForegroundColor Green
}

Write-Host ""
Write-Host "Run: $exe"