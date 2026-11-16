#!/usr/bin/env bash
# ============================================================
#  Terminal++ build script for Linux / macOS (Qt6 + libssh2)
#
#  Usage:
#     ./build.sh                 # configure + build (Release)
#     ./build.sh clean           # remove build dir first
#     ./build.sh run             # build then launch the app
#     ./build.sh qmake           # build via qmake instead of CMake
#
#  Dependencies (install first):
#     Debian/Ubuntu:
#       sudo apt install build-essential cmake qt6-base-dev \
#            qt6-serialport-dev libqt6printsupport6 \
#            libssh2-1-dev pkg-config
#     Fedora:
#       sudo dnf install gcc-c++ cmake qt6-qtbase-devel \
#            qt6-qtserialport-devel libssh2-devel pkgconf
#     Arch:
#       sudo pacman -S base-devel cmake qt6-base qt6-serialport \
#            libssh2 pkgconf
#     macOS (Homebrew):
#       brew install qt libssh2 cmake pkg-config
# ============================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

MODE="${1:-}"

if [ "$MODE" = "clean" ]; then
    echo "[clean] Removing $BUILD_DIR"
    rm -rf "$BUILD_DIR"
    MODE="${2:-}"
fi

# ---- sanity checks ----
command -v cmake >/dev/null 2>&1 || { echo "ERROR: cmake not found"; exit 1; }
if ! pkg-config --exists libssh2 2>/dev/null; then
    echo "WARNING: libssh2 not found via pkg-config."
    echo "         Install it, e.g.  sudo apt install libssh2-1-dev"
fi

QMAKE_BIN="$(command -v qmake6 || command -v qmake || true)"

if [ "$MODE" = "qmake" ]; then
    # ---------- qmake build ----------
    if [ -z "$QMAKE_BIN" ]; then
        echo "ERROR: qmake/qmake6 not found"; exit 1
    fi
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    echo "[qmake] $QMAKE_BIN"
    "$QMAKE_BIN" "$SCRIPT_DIR/TerminalPP.pro"
    echo "[make] building..."
    make -j"$(nproc 2>/dev/null || echo 4)"
    EXE="$BUILD_DIR/TerminalPP"
else
    # ---------- CMake build (default) ----------
    echo "[1/2] Configuring with CMake..."
    cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
    echo "[2/2] Building..."
    cmake --build "$BUILD_DIR" -j"$(nproc 2>/dev/null || echo 4)"
    EXE="$BUILD_DIR/TerminalPP"
fi

if [ ! -x "$EXE" ]; then
    echo "ERROR: build finished but $EXE not found"
    exit 1
fi

echo "Build succeeded: $EXE"

if [ "$MODE" = "run" ] || [ "$2" = "run" ]; then
    echo "[run] launching..."
    "$EXE"
fi

echo
echo "Run with: $EXE"