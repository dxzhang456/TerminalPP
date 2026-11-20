#!/usr/bin/env bash
# =============================================================================
#  Terminal++ — Windows build via MSYS2 / MinGW-w64 (direct g++, no cmake/qmake)
#
#  This is the exact, verified procedure used to produce the shipped dist/.
#  It bypasses cmake/qmake (which can be unreliable on locked-down corporate
#  machines) and drives moc + rcc + g++ directly, then bundles every runtime
#  DLL + the Qt platform plugin into dist/ so the result is fully portable.
#
#  USAGE
#    bash build-msys2.sh build      # incremental: compile only (no clean)  [default]
#    bash build-msys2.sh clean      # clean only: remove build/ + dist/, do NOT compile
#    bash build-msys2.sh rebuild    # clean then build (full from-scratch build)
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

# ---- parse the build mode --------------------------------------------------
MODE="${1:-build}"
DO_CLEAN=0
DO_COMPILE=1
case "$MODE" in
    build|"") DO_CLEAN=0; DO_COMPILE=1 ;;   # incremental: compile only, no clean (default)
    clean)    DO_CLEAN=1; DO_COMPILE=0 ;;   # clean only: wipe, do NOT compile
    rebuild)  DO_CLEAN=1; DO_COMPILE=1 ;;   # clean then build
    *) echo "Unknown mode: '$MODE'"; \
       echo "Usage: build-msys2.sh [build|clean|rebuild]"; exit 1 ;;
esac

PROJ="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$PROJ/src"
OUT="$PROJ/build"
OBJ="$OUT/obj"
DIST="$PROJ/dist"
QTINC=/mingw64/include/qt6
# Qt6 host tools live under share/qt6/bin on MSYS2.
MOC=/mingw64/share/qt6/bin/moc.exe
RCC=/mingw64/share/qt6/bin/rcc.exe

# ---- clean (if requested) --------------------------------------------------
if [ "$DO_CLEAN" = "1" ]; then
    echo "[clean] removing build/ and dist/"
    rm -rf "$OUT" "$DIST"
fi
if [ "$DO_COMPILE" = "0" ]; then
    echo "DONE (clean only)."
    exit 0
fi

mkdir -p "$OUT/moc" "$OBJ" \
         "$DIST/platforms" "$DIST/styles" "$DIST/imageformats" \
         "$DIST/tls" "$DIST/networkinformation"

INCS="-I$SRC -I$OUT/moc -I$QTINC -I$QTINC/QtCore -I$QTINC/QtGui -I$QTINC/QtWidgets \
      -I$QTINC/QtNetwork -I$QTINC/QtPrintSupport -I$QTINC/QtSerialPort"
DEFS="-DQT_CORE_LIB -DQT_GUI_LIB -DQT_WIDGETS_LIB -DQT_NETWORK_LIB \
      -DQT_PRINTSUPPORT_LIB -DQT_SERIALPORT_LIB"
CXXFLAGS="-std=c++17 -O2 -DUNICODE -D_UNICODE $DEFS $INCS"

# newer_than A B  -> returns 0 (true) if A is newer than B, or B is missing.
newer_than() { [ ! -f "$2" ] || [ "$1" -nt "$2" ]; }

# ---- 1) moc on all Q_OBJECT headers (only when header changed) -------------
MOC_HEADERS=(
  "$SRC/mainwindow.h"
  "$SRC/core/i18n.h"              "$SRC/core/cliphistory.h"
  "$SRC/core/cliphistorydialog.h"
  "$SRC/compare/textcomparetab.h"   "$SRC/compare/foldercomparetab.h"
  "$SRC/compare/hexcomparetab.h"    "$SRC/compare/imagecomparetab.h"
  "$SRC/compare/comparedialog.h"
  "$SRC/cloud/cloudprovider.h"      "$SRC/cloud/restproviders.h"
  "$SRC/cloud/cloudconnectdialog.h" "$SRC/cloud/cloudbrowsertab.h"
  "$SRC/cloud/streamproxy.h"
  "$SRC/cloud/transfermanager.h"    "$SRC/cloud/transferpanel.h"
  "$SRC/cloud/smbprovider.h"        "$SRC/cloud/sftpprovider.h"
  "$SRC/cloud/ftpprovider.h"
  "$SRC/editor/codeeditor.h"      "$SRC/editor/syntaxhighlighter.h"
  "$SRC/editor/finddialog.h"      "$SRC/editor/gotodialog.h"
  "$SRC/editor/editortab.h"       "$SRC/editor/hexeditortab.h"
  "$SRC/editor/imageeditortab.h"
  "$SRC/terminal/terminalwidget.h" "$SRC/terminal/ansiparser.h"
  "$SRC/terminal/sshsession.h"     "$SRC/terminal/sshtab.h"
  "$SRC/terminal/sessiondialog.h"  "$SRC/terminal/serialsession.h"
  "$SRC/terminal/serialtab.h"      "$SRC/terminal/serialdialog.h"
)
echo "[1/4] moc"
MOC_CPP=()
for h in "${MOC_HEADERS[@]}"; do
    out="$OUT/moc/moc_$(basename "$h" .h).cpp"
    if newer_than "$h" "$out"; then
        "$MOC" $DEFS $INCS "$h" -o "$out" || { echo "moc failed: $h"; exit 2; }
    fi
    MOC_CPP+=("$out")
done

# Some .cpp files declare inline Q_OBJECT classes and #include their own
# "<base>.moc". Generate those into $OUT/moc so they are found via -I.
for pair in "compare/textcomparetab" "compare/imagecomparetab" "editor/hexeditortab" \
           "editor/imageeditortab" "cloud/sftpprovider" "cloud/ftpprovider"; do
    base="$(basename "$pair")"
    mocout="$OUT/moc/$base.moc"
    if newer_than "$SRC/$pair.cpp" "$mocout"; then
        "$MOC" $DEFS $INCS "$SRC/$pair.cpp" -o "$mocout" \
            || { echo "moc (inline) failed: $pair"; exit 2; }
    fi
done

# ---- 2) rcc (only when the .qrc changed) -----------------------------------
echo "[2/4] rcc"
USE_QRC=0
QRC_SRC="$PROJ/resources/resources.qrc"
QRC_CPP="$OUT/moc/qrc_resources.cpp"
if [ -f "$QRC_SRC" ]; then
    if newer_than "$QRC_SRC" "$QRC_CPP"; then
        "$RCC" "$QRC_SRC" -o "$QRC_CPP" 2>/dev/null && USE_QRC=1
    elif [ -f "$QRC_CPP" ]; then
        USE_QRC=1
    fi
fi

# ---- 2b) Windows icon: ensure icon.ico exists, compile app.rc via windres --
echo "[2b] windows icon"
if [ ! -f "$PROJ/resources/icon.ico" ] && [ -f "$PROJ/resources/icon.png" ]; then
    # Try to generate icon.ico from icon.png using Python/Pillow if available.
    python -c "from PIL import Image; Image.open(r'$PROJ/resources/icon.png').convert('RGBA').save(r'$PROJ/resources/icon.ico', sizes=[(16,16),(32,32),(48,48),(64,64),(128,128),(256,256)])" 2>/dev/null \
      || echo "  (Pillow not available — exe icon skipped; window icon still works via qrc)"
fi
RES_OBJ=""
if [ -f "$PROJ/resources/icon.ico" ]; then
    if newer_than "$PROJ/resources/app.rc" "$OBJ/app_rc.o" \
       || newer_than "$PROJ/resources/icon.ico" "$OBJ/app_rc.o"; then
        windres "$PROJ/resources/app.rc" -O coff -o "$OBJ/app_rc.o" \
            --include-dir "$PROJ/resources" 2>/dev/null \
            && echo "  icon resource compiled"
    fi
    [ -f "$OBJ/app_rc.o" ] && RES_OBJ="$OBJ/app_rc.o"
fi

# ---- 3) compile each source to an object (incremental), then link ----------
echo "[3/4] compile (incremental) + link"
SOURCES=(
  "$SRC/main.cpp" "$SRC/mainwindow.cpp"
  "$SRC"/core/*.cpp
  "$SRC"/compare/*.cpp
  "$SRC"/cloud/*.cpp
  "$SRC"/editor/*.cpp
  "$SRC"/terminal/*.cpp
)
SOURCES+=("${MOC_CPP[@]}")
[ "$USE_QRC" = "1" ] && SOURCES+=("$QRC_CPP")

# Map each .cpp to a unique .o under $OBJ (flatten path to avoid collisions).
OBJECTS=()
compiled=0
for s in "${SOURCES[@]}"; do
    [ -f "$s" ] || continue
    # Build a flat, collision-free object name from the path relative to $PROJ.
    rel="${s#$PROJ/}"
    flat="$(echo "$rel" | sed 's#[/\\]#__#g; s#\.cpp$#.o#')"
    o="$OBJ/$flat"
    if newer_than "$s" "$o"; then
        echo "  CC  $rel"
        g++ $CXXFLAGS -c "$s" -o "$o" || { echo "compile failed: $s"; exit 3; }
        compiled=$((compiled+1))
    fi
    OBJECTS+=("$o")
done
[ -n "$RES_OBJ" ] && OBJECTS+=("$RES_OBJ")
echo "  ($compiled source(s) (re)compiled)"

LIBS="-lQt6Widgets -lQt6Gui -lQt6Core -lQt6Network -lQt6PrintSupport \
      -lQt6SerialPort -lssh2 -lmagic -lws2_32 -lmswsock -lcrypt32 -lmpr"

# Link only if something changed or the exe is missing.
EXE="$OUT/TerminalPP.exe"
need_link=0
[ "$compiled" -gt 0 ] && need_link=1
[ ! -f "$EXE" ] && need_link=1
if [ "$need_link" = "1" ]; then
    echo "  LINK TerminalPP.exe"
    g++ -mwindows "${OBJECTS[@]}" -o "$EXE" -L/mingw64/lib $LIBS \
        || { echo "link failed"; exit 3; }
else
    echo "  (nothing changed — skipping link)"
fi

# ---- 4) deploy: bundle exe + all runtime DLLs + plugins into dist/ ----------
echo "[4/4] deploy -> dist/"
cp "$EXE" "$DIST/"
cp /mingw64/share/qt6/plugins/platforms/qwindows.dll "$DIST/platforms/" 2>/dev/null
for s in qmodernwindowsstyle.dll qwindowsvistastyle.dll; do
    cp "/mingw64/share/qt6/plugins/styles/$s" "$DIST/styles/" 2>/dev/null
done
for f in qgif.dll qico.dll qjpeg.dll qsvg.dll; do
    cp "/mingw64/share/qt6/plugins/imageformats/$f" "$DIST/imageformats/" 2>/dev/null
done
# TLS backend plugin — REQUIRED for HTTPS (QNetworkAccessManager over SSL).
# Without it Qt reports "TLS initialization failed" on every https request.
for f in qopensslbackend.dll qschannelbackend.dll qcertonlybackend.dll; do
    cp "/mingw64/share/qt6/plugins/tls/$f" "$DIST/tls/" 2>/dev/null
done
# Network information backends (optional but harmless).
cp /mingw64/share/qt6/plugins/networkinformation/*.dll "$DIST/networkinformation/" 2>/dev/null

# Recursively copy every mingw64 DLL referenced by the exe/dlls/plugins.
cd "$DIST"
for pass in 1 2 3 4 5 6; do
    changed=0
    for f in TerminalPP.exe *.dll platforms/*.dll styles/*.dll imageformats/*.dll tls/*.dll networkinformation/*.dll; do
        [ -f "$f" ] || continue
        for imp in $(objdump -p "$f" 2>/dev/null | awk '/DLL Name:/{print $3}'); do
            if [ -f "/mingw64/bin/$imp" ] && [ ! -f "$DIST/$imp" ]; then
                cp "/mingw64/bin/$imp" "$DIST/"; changed=1
            fi
        done
    done
    [ "$changed" = "0" ] && break
done
# Ensure gcc runtime + SSH/TLS/magic libs (OpenSSL for HTTPS, libmagic for file sniffing).
for d in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll \
         libssh2-1.dll libmagic-1.dll libssl-3-x64.dll libcrypto-3-x64.dll libz-1.dll zlib1.dll; do
    [ ! -f "$d" ] && [ -f "/mingw64/bin/$d" ] && cp "/mingw64/bin/$d" .
done
# Bundle the compiled libmagic database file (magic.mgc) next to the exe
mkdir -p "$DIST/share/misc"
[ -f "/mingw64/share/misc/magic.mgc" ] && cp "/mingw64/share/misc/magic.mgc" "$DIST/" 2>/dev/null
[ -f "/mingw64/share/misc/magic.mgc" ] && cp "/mingw64/share/misc/magic.mgc" "$DIST/share/misc/" 2>/dev/null
# OpenSSL may be named without the -x64 suffix on some MSYS2 installs.
for d in libssl-3.dll libcrypto-3.dll; do
    [ ! -f "$d" ] && [ -f "/mingw64/bin/$d" ] && cp "/mingw64/bin/$d" .
done

echo "DONE. Portable build in: $DIST/TerminalPP.exe  ($(ls *.dll | wc -l) DLLs bundled)"