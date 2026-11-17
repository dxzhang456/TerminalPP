#!/usr/bin/env bash
# =============================================================================
#  Terminal++ — Windows build via MSYS2 / MinGW-w64 (direct g++, no cmake/qmake)
#
#  This is the exact, verified procedure used to produce the shipped dist/.
#  It bypasses cmake/qmake (which can be unreliable on locked-down corporate
#  machines) and drives moc + rcc + g++ directly, then bundles every runtime
#  DLL + the Qt platform plugin into dist/ so the result is fully portable.
#
#  Prerequisites (run once in an MSYS2 shell):
#     pacman -S --needed \
#        mingw-w64-x86_64-gcc \
#        mingw-w64-x86_64-qt6-base \
#        mingw-w64-x86_64-qt6-serialport \
#        mingw-w64-x86_64-libssh2 \
#        mingw-w64-x86_64-zlib mingw-w64-x86_64-zstd
#
#  Run:   C:\msys64\usr\bin\bash.exe -lc "bash /c/path/to/TerminalPP/build-msys2.sh"
# =============================================================================
set -u
export PATH="/mingw64/bin:/usr/bin:/bin"

PROJ="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$PROJ/src"
OUT="$PROJ/build"
DIST="$PROJ/dist"
QTINC=/mingw64/include/qt6
# Qt6 host tools live under share/qt6/bin on MSYS2.
MOC=/mingw64/share/qt6/bin/moc.exe
RCC=/mingw64/share/qt6/bin/rcc.exe

rm -rf "$OUT" "$DIST"
mkdir -p "$OUT/moc" "$DIST/platforms" "$DIST/styles" "$DIST/imageformats"

INCS="-I$SRC -I$QTINC -I$QTINC/QtCore -I$QTINC/QtGui -I$QTINC/QtWidgets \
      -I$QTINC/QtNetwork -I$QTINC/QtPrintSupport -I$QTINC/QtSerialPort"
DEFS="-DQT_CORE_LIB -DQT_GUI_LIB -DQT_WIDGETS_LIB -DQT_NETWORK_LIB \
      -DQT_PRINTSUPPORT_LIB -DQT_SERIALPORT_LIB"
CXXFLAGS="-std=c++17 -O2 -DUNICODE -D_UNICODE $DEFS $INCS"

# ---- 1) moc on all Q_OBJECT headers ----
MOC_HEADERS=(
  "$SRC/mainwindow.h"
  "$SRC/core/i18n.h"              "$SRC/core/cliphistory.h"
  "$SRC/core/cliphistorydialog.h"
  "$SRC/editor/codeeditor.h"      "$SRC/editor/syntaxhighlighter.h"
  "$SRC/editor/finddialog.h"      "$SRC/editor/gotodialog.h"
  "$SRC/editor/editortab.h"
  "$SRC/terminal/terminalwidget.h" "$SRC/terminal/ansiparser.h"
  "$SRC/terminal/sshsession.h"     "$SRC/terminal/sshtab.h"
  "$SRC/terminal/sessiondialog.h"  "$SRC/terminal/serialsession.h"
  "$SRC/terminal/serialtab.h"      "$SRC/terminal/serialdialog.h"
)
echo "[1/4] moc"
MOC_CPP=()
for h in "${MOC_HEADERS[@]}"; do
    out="$OUT/moc/moc_$(basename "$h" .h).cpp"
    "$MOC" $DEFS $INCS "$h" -o "$out" || { echo "moc failed: $h"; exit 2; }
    MOC_CPP+=("$out")
done

# ---- 2) rcc (optional) ----
echo "[2/4] rcc"
USE_QRC=0
if "$RCC" "$PROJ/resources/resources.qrc" -o "$OUT/moc/qrc_resources.cpp" 2>/dev/null; then
    USE_QRC=1
fi

# ---- 2b) Windows icon: ensure icon.ico exists, compile app.rc via windres ----
echo "[2b] windows icon"
if [ ! -f "$PROJ/resources/icon.ico" ] && [ -f "$PROJ/resources/icon.png" ]; then
    # Try to generate icon.ico from icon.png using Python/Pillow if available.
    python -c "from PIL import Image; Image.open(r'$PROJ/resources/icon.png').convert('RGBA').save(r'$PROJ/resources/icon.ico', sizes=[(16,16),(32,32),(48,48),(64,64),(128,128),(256,256)])" 2>/dev/null \
      || echo "  (Pillow not available — exe icon skipped; window icon still works via qrc)"
fi
RES_OBJ=""
if [ -f "$PROJ/resources/icon.ico" ]; then
    windres "$PROJ/resources/app.rc" -O coff -o "$OUT/moc/app_rc.o" \
        --include-dir "$PROJ/resources" 2>/dev/null \
        && RES_OBJ="$OUT/moc/app_rc.o" \
        && echo "  icon resource compiled"
fi

# ---- 3) compile + link ----
echo "[3/4] compile + link"
SOURCES=(
  "$SRC/main.cpp" "$SRC/mainwindow.cpp"
  "$SRC"/core/*.cpp
  "$SRC"/editor/*.cpp
  "$SRC"/terminal/*.cpp
)
SOURCES+=("${MOC_CPP[@]}")
[ "$USE_QRC" = "1" ] && SOURCES+=("$OUT/moc/qrc_resources.cpp")
[ -n "$RES_OBJ" ] && SOURCES+=("$RES_OBJ")

LIBS="-lQt6Widgets -lQt6Gui -lQt6Core -lQt6Network -lQt6PrintSupport \
      -lQt6SerialPort -lssh2 -lws2_32 -lmswsock"

g++ $CXXFLAGS -mwindows "${SOURCES[@]}" -o "$OUT/TerminalPP.exe" -L/mingw64/lib $LIBS \
    || { echo "link failed"; exit 3; }

# ---- 4) deploy: bundle exe + all runtime DLLs + plugins into dist/ ----
echo "[4/4] deploy -> dist/"
cp "$OUT/TerminalPP.exe" "$DIST/"
cp /mingw64/share/qt6/plugins/platforms/qwindows.dll "$DIST/platforms/" 2>/dev/null
for s in qmodernwindowsstyle.dll qwindowsvistastyle.dll; do
    cp "/mingw64/share/qt6/plugins/styles/$s" "$DIST/styles/" 2>/dev/null
done
for f in qgif.dll qico.dll qjpeg.dll qsvg.dll; do
    cp "/mingw64/share/qt6/plugins/imageformats/$f" "$DIST/imageformats/" 2>/dev/null
done

# Recursively copy every mingw64 DLL referenced by the exe/dlls/plugins.
cd "$DIST"
for pass in 1 2 3 4 5 6; do
    changed=0
    for f in TerminalPP.exe *.dll platforms/*.dll styles/*.dll imageformats/*.dll; do
        [ -f "$f" ] || continue
        for imp in $(objdump -p "$f" 2>/dev/null | awk '/DLL Name:/{print $3}'); do
            if [ -f "/mingw64/bin/$imp" ] && [ ! -f "$DIST/$imp" ]; then
                cp "/mingw64/bin/$imp" "$DIST/"; changed=1
            fi
        done
    done
    [ "$changed" = "0" ] && break
done
# Ensure gcc runtime + SSH/TLS libs.
for d in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll \
         libssh2-1.dll libssl-3-x64.dll libcrypto-3-x64.dll libz-1.dll zlib1.dll; do
    [ ! -f "$d" ] && [ -f "/mingw64/bin/$d" ] && cp "/mingw64/bin/$d" .
done

echo "DONE. Portable build in: $DIST/TerminalPP.exe  ($(ls *.dll | wc -l) DLLs bundled)"