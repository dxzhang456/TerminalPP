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
#    bash build-msys2.sh build                  # incremental Win64 (default)
#    bash build-msys2.sh clean                  # wipe build/ + dist/
#    bash build-msys2.sh rebuild                # clean then build (Win64)
#    bash build-msys2.sh build --target=win32   # build 32-bit (MinGW32)
#    bash build-msys2.sh build --target=arm64   # build ARM64 (CLANGARM64)
#
#  For every target the matching portable OpenSSH-for-Windows under
#  thirdparty/openssh/ is copied into dist/openssh/ so the SSH-server
#  toggle in Terminal++ works out-of-the-box without administrator rights.
#
#  Prerequisites (run once in an MSYS2 shell):
#     pacman -S --needed \
#        mingw-w64-x86_64-gcc \
#        mingw-w64-x86_64-qt6-base \
#        mingw-w64-x86_64-qt6-serialport \
#        mingw-w64-x86_64-libssh2 \
#        mingw-w64-x86_64-zlib mingw-w64-x86_64-zstd
#  For non-Win64 targets the matching MSYS2 subsystem must be installed and
#  its Qt6 / toolchain packages must be present (mingw-w64-i686-* for win32,
#  mingw-w64-clang-aarch64-* for arm64). The script falls back with a clear
#  error if the needed toolchain is missing.
#
#  Run:   C:\msys64\usr\bin\bash.exe -lc "bash /c/path/to/TerminalPP/build-msys2.sh"
# =============================================================================
set -u

# ---- parse the build mode + optional target -------------------------------
MODE="build"
TARGET="win64"
for arg in "$@"; do
    case "$arg" in
        build|clean|rebuild) MODE="$arg" ;;
        --target=*)          TARGET="${arg#*=}" ;;
        *) echo "Unknown argument: '$arg'"; \
           echo "Usage: build-msys2.sh [build|clean|rebuild] [--target=win64|win32|arm64]"; \
           exit 1 ;;
    esac
done
DO_CLEAN=0
DO_COMPILE=1
case "$MODE" in
    build|"") DO_CLEAN=0; DO_COMPILE=1 ;;
    clean)    DO_CLEAN=1; DO_COMPILE=0 ;;
    rebuild)  DO_CLEAN=1; DO_COMPILE=1 ;;
    *) echo "Unknown mode: '$MODE'"; exit 1 ;;
esac

# ---- pick the MSYS2 subsystem for the requested target --------------------
# Each target needs its own /<prefix>/bin on PATH so moc/rcc/g++ come from
# that subsystem's Qt6, and the bundled runtime DLLs match the exe's ABI.
case "$TARGET" in
    win64)
        TC_PREFIX="/mingw64"
        TC_ARCH="x86_64"
        OBJFMT_RE='pei-x86-64'
        CXX_NAME="g++"
        OPENSSH_SUBDIR="OpenSSH-Win64"
        PDFIUM_SUBDIR="Pdfium-Win64"
        DIST_SUFFIX=""            # dist/ (default folder — keeps existing scripts happy)
        ;;
    win32)
        TC_PREFIX="/mingw32"
        TC_ARCH="i686"
        OBJFMT_RE='pei-i386'
        CXX_NAME="g++"
        OPENSSH_SUBDIR="OpenSSH-Win32"
        PDFIUM_SUBDIR="Pdfium-Win32"
        DIST_SUFFIX="-win32"
        ;;
    arm64)
        TC_PREFIX="/clangarm64"
        TC_ARCH="aarch64"
        OBJFMT_RE='pei-aarch64|pe-aarch64'
        CXX_NAME="clang++"
        OPENSSH_SUBDIR="OpenSSH-ARM64"
        PDFIUM_SUBDIR="Pdfium-Arm64"
        DIST_SUFFIX="-arm64"
        ;;
    *) echo "Unknown --target=$TARGET"; \
       echo "Supported targets: win64 (default), win32, arm64"; \
       exit 1 ;;
esac

# Sanity-check the chosen MSYS2 subsystem is actually installed.
if [ ! -d "$TC_PREFIX" ] || [ ! -x "$TC_PREFIX/bin/$CXX_NAME.exe" ]; then
    echo "ERROR: target '$TARGET' requires the MSYS2 subsystem at $TC_PREFIX"
    echo "       with a working $CXX_NAME.exe, but it was not found."
    echo ""
    echo "   Install it from an MSYS2 shell, e.g. for win32:"
    echo "     pacman -S --needed mingw-w64-i686-gcc mingw-w64-i686-qt6-base \\"
    echo "                         mingw-w64-i686-qt6-serialport \\"
    echo "                         mingw-w64-i686-libssh2 mingw-w64-i686-zlib"
    echo "   For arm64:"
    echo "     pacman -S --needed mingw-w64-clang-aarch64-gcc-compat \\"
    echo "                         mingw-w64-clang-aarch64-qt6-base \\"
    echo "                         mingw-w64-clang-aarch64-qt6-serialport"
    exit 1
fi

export PATH="$TC_PREFIX/bin:/usr/bin:/bin"

PROJ="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$PROJ/src"
OUT="$PROJ/build${DIST_SUFFIX}"
OBJ="$OUT/obj"
DIST="$PROJ/dist${DIST_SUFFIX}"
QTINC="$TC_PREFIX/include/qt6"
# Qt6 host tools live under share/qt6/bin on MSYS2.
MOC="$TC_PREFIX/share/qt6/bin/moc.exe"
RCC="$TC_PREFIX/share/qt6/bin/rcc.exe"

# ---- clean (if requested) --------------------------------------------------
if [ "$DO_CLEAN" = "1" ]; then
    echo "[clean] removing $OUT and $DIST"
    rm -rf "$OUT" "$DIST"
fi
if [ "$DO_COMPILE" = "0" ]; then
    echo "DONE (clean only)."
    exit 0
fi

echo "[config] target=$TARGET  toolchain=$TC_PREFIX  cxx=$CXX_NAME"

mkdir -p "$OUT/moc" "$OBJ" \
         "$DIST/platforms" "$DIST/styles" "$DIST/imageformats" \
         "$DIST/tls" "$DIST/networkinformation"

INCS="-I$SRC -I$OUT/moc -I$QTINC -I$QTINC/QtCore -I$QTINC/QtGui -I$QTINC/QtWidgets \
      -I$QTINC/QtNetwork -I$QTINC/QtPrintSupport -I$QTINC/QtSerialPort"

# NOTE: do NOT add -DQT_NO_KEYWORDS. Several headers (compare/*, cloud/*,
# pdf/*, etc.) use the lowercase `signals:` / `slots:` / `emit` keywords; the
# NO_KEYWORDS macro turns those back into errors ("statusMessage not a
# member of TextCompareTab", etc.).
CXXFLAGS="-O2 -pipe -fPIC \
          -DQT_CORE_LIB -DQT_GUI_LIB -DQT_WIDGETS_LIB -DQT_NETWORK_LIB \
          -DQT_PRINTSUPPORT_LIB -DQT_SERIALPORT_LIB"

# moc only understands preprocessor-style options (-D, -I, ...), so strip
# compiler-only flags like -O2 / -pipe / -fPIC that would otherwise make
# moc abort with "Unknown option 'O2'".
MOCFLAGS="-DQT_CORE_LIB -DQT_GUI_LIB -DQT_WIDGETS_LIB -DQT_NETWORK_LIB \
          -DQT_PRINTSUPPORT_LIB -DQT_SERIALPORT_LIB"

# ---- helpers -----------------------------------------------------------------
newer_than() {
    [ ! -f "$2" ] || [ "$1" -nt "$2" ]
}

# ---- 1) moc all headers that carry Q_OBJECT / Q_GADGET -----------------------
echo "[1/4] moc"
# NOTE: src/pdf/pdfium.{h,cpp} is a plain (non-QObject) class, so it needs no
# moc entry — it is picked up automatically by the "$SRC"/pdf/*.cpp glob in
# the compile step.
HEADERS=$(grep -lE 'Q_OBJECT|Q_GADGET' $(find "$SRC" -name '*.h') 2>/dev/null || true)
for h in $HEADERS; do
    out="$OUT/moc/moc_$(basename "$h" .h).cpp"
    if newer_than "$h" "$out"; then
        "$MOC" $MOCFLAGS $INCS "$h" -o "$out"
    fi
done

# Some .cpp files declare inline Q_OBJECT classes and #include their own
# generated moc at the bottom (e.g. imageeditortab.cpp -> imageeditortab.moc).
# For those the generated file is named "<basename>.moc" and lives inside
# the .cpp's own directory, not under $OUT/moc. We auto-detect every such
# .cpp by grepping for `#include "<base>.moc"` rather than maintaining a
# hand-written list that keeps getting out-of-sync with the codebase.
INLINE_MOC_CPPS=$(grep -rln '#include ".*\.moc"' "$SRC" 2>/dev/null || true)
for cpp in $INLINE_MOC_CPPS; do
    base="$(basename "$cpp" .cpp)"
    dir="$(dirname "$cpp")"
    mocout="$dir/$base.moc"
    if newer_than "$cpp" "$mocout"; then
        "$MOC" $MOCFLAGS $INCS "$cpp" -o "$mocout" -I"$SRC" -I"$dir"
    fi
done

# ---- 2) rcc the resources --------------------------------------------------
echo "[2/4] rcc"
QRC_CPP="$OUT/moc/qrc_resources.cpp"
if [ -f "$PROJ/resources/resources.qrc" ]; then
    if newer_than "$PROJ/resources/resources.qrc" "$QRC_CPP"; then
        "$RCC" --name resources "$PROJ/resources/resources.qrc" -o "$QRC_CPP"
    fi
fi

# ---- 2b) application icon (Windows resource) --------------------------------
echo "[2b] windows icon"
ICO_RC="$PROJ/resources/app.rc"
ICO_RES="$OUT/moc/app_res.o"
WINDRES="$TC_PREFIX/bin/windres.exe"
if [ ! -x "$WINDRES" ]; then
    # clang subsystems ship `llvm-windres` under the same name; fall back if
    # the plain name is absent.
    [ -x "$TC_PREFIX/bin/llvm-windres.exe" ] && WINDRES="$TC_PREFIX/bin/llvm-windres.exe"
fi
if [ -f "$ICO_RC" ] && [ -x "$WINDRES" ]; then
    if newer_than "$ICO_RC" "$ICO_RES"; then
        "$WINDRES" -i "$ICO_RC" -o "$ICO_RES" --output-format=coff 2>/dev/null || true
    fi
fi

# ---- 3) compile + link ------------------------------------------------------
echo "[3/4] compile (incremental) + link"
SOURCES=(
  "$SRC/main.cpp" "$SRC/mainwindow.cpp"
  "$SRC"/core/*.cpp
  "$SRC"/compare/*.cpp
  "$SRC"/cloud/*.cpp
  "$SRC"/pdf/*.cpp
  "$SRC"/editor/*.cpp
  "$SRC"/terminal/*.cpp
)
# All generated moc files are safely covered by one glob (empty is harmless).
MOCS=( "$OUT"/moc/moc_*.cpp )
[ -f "$QRC_CPP" ] && MOCS+=( "$QRC_CPP" )

# Map each .cpp to a unique .o under $OBJ (flatten path to avoid collisions).
OBJS=()
NEED_RECOMPILE=()
TOTAL_SRC=0
RECOMPILE_CNT=0
for src in "${SOURCES[@]}" "${MOCS[@]}"; do
    [ -f "$src" ] || continue
    rel="${src#$PROJ/}"
    flat="$(echo "$rel" | sed 's#[/\\]#__#g; s#\.cpp$#.o#')"
    obj="$OBJ/$flat"
    OBJS+=( "$obj" )
    TOTAL_SRC=$((TOTAL_SRC+1))
    if newer_than "$src" "$obj"; then
        NEED_RECOMPILE+=( "$src|$obj" )
        RECOMPILE_CNT=$((RECOMPILE_CNT+1))
    fi
done

# Also include the Windows resource object (icon) in the link.
[ -f "$ICO_RES" ] && OBJS+=( "$ICO_RES" )

CXX="$TC_PREFIX/bin/$CXX_NAME.exe"
if [ "$RECOMPILE_CNT" = "0" ]; then
    echo "  (0 source(s) (re)compiled)"
else
    echo "  compiling $RECOMPILE_CNT file(s) with -j16"
    IDX=0
    PIDS=()
    for pair in "${NEED_RECOMPILE[@]}"; do
        src="${pair%|*}"; obj="${pair#*|}"
        IDX=$((IDX+1))
        (
            if ! "$CXX" -std=gnu++17 $CXXFLAGS $INCS -c "$src" -o "$obj" 2>&1; then
                echo "COMPILE FAILED: $src"; exit 1
            fi
            echo "  [$IDX/$RECOMPILE_CNT] CC  ${src#$PROJ/}"
        ) &
        PIDS+=( $! )
        # Cap concurrency at 16.
        if [ "${#PIDS[@]}" -ge 16 ]; then
            wait "${PIDS[0]}" || exit 1
            PIDS=( "${PIDS[@]:1}" )
        fi
    done
    for p in "${PIDS[@]}"; do wait "$p" || exit 1; done
    echo "  ($RECOMPILE_CNT source(s) (re)compiled)"
fi

EXE="$OUT/TerminalPP.exe"
# Always relink if ANY object is newer than the exe, or the exe does not exist.
NEED_LINK=0
if [ ! -f "$EXE" ]; then NEED_LINK=1; fi
if [ "$NEED_LINK" = "0" ]; then
    for o in "${OBJS[@]}"; do
        if [ "$o" -nt "$EXE" ]; then NEED_LINK=1; break; fi
    done
fi

if [ "$NEED_LINK" = "0" ]; then
    echo "  (nothing changed — skipping link)"
else
    echo "  LINK TerminalPP.exe"
    # System libs needed by src/cloud (Winsock2 for TCP, mpr for
    # WNetAddConnection2W in smbprovider.cpp) and shell32 for the SSH-server
    # toggle's ShellExecuteExW("runas") UAC relaunch helper.
    LIBS="-L$TC_PREFIX/lib -lQt6Widgets -lQt6PrintSupport -lQt6Network \
          -lQt6Gui -lQt6SerialPort -lQt6Core -lssh2 -lz -lzstd -lmagic \
          -lws2_32 -lmpr -lshell32 -ladvapi32 -luser32 -lcrypt32"
    "$CXX" -Wl,-subsystem,windows "${OBJS[@]}" $LIBS -o "$EXE" 2>&1 | tee /tmp/link.log || \
        { echo "LINK FAILED"; exit 1; }
fi

# ---- 4) deploy: bundle exe + all runtime DLLs + plugins into dist/ ----------
echo "[4/4] deploy -> $DIST/"
cp "$EXE" "$DIST/"
cp "$TC_PREFIX/share/qt6/plugins/platforms/qwindows.dll" "$DIST/platforms/" 2>/dev/null
for s in qwindowsvistastyle.dll qmodernwindowsstyle.dll; do
    cp "$TC_PREFIX/share/qt6/plugins/styles/$s" "$DIST/styles/" 2>/dev/null
done
for f in qgif.dll qjpeg.dll qico.dll qsvg.dll qtiff.dll qwebp.dll; do
    cp "$TC_PREFIX/share/qt6/plugins/imageformats/$f" "$DIST/imageformats/" 2>/dev/null
done
# TLS backend for QNetworkAccessManager HTTPS requests (WebDAV, cloud OAuth,
# library-of-congress PDFs over https, etc).
for f in qopensslbackend.dll qschannelbackend.dll qcertonlybackend.dll; do
    cp "$TC_PREFIX/share/qt6/plugins/tls/$f" "$DIST/tls/" 2>/dev/null
done
cp "$TC_PREFIX/share/qt6/plugins/networkinformation/"*.dll "$DIST/networkinformation/" 2>/dev/null

# Copy every DLL the exe depends on (recursively) from the toolchain prefix.
cd "$DIST"
changed=1
while [ "$changed" = "1" ]; do
    changed=0
    for exe_or_dll in *.exe *.dll; do
        [ -f "$exe_or_dll" ] || continue
        for imp in $(objdump -p "$exe_or_dll" 2>/dev/null | grep 'DLL Name:' | awk '{print $3}'); do
            if [ -f "$TC_PREFIX/bin/$imp" ] && [ ! -f "$DIST/$imp" ]; then
                cp "$TC_PREFIX/bin/$imp" "$DIST/"; changed=1
            fi
        done
    done
done

# Known-good baseline: make sure these are present even if a nested dep missed them.
for d in libbrotlicommon.dll libbrotlidec.dll libbrotlienc.dll ; do
    [ ! -f "$d" ] && [ -f "$TC_PREFIX/bin/$d" ] && cp "$TC_PREFIX/bin/$d" .
done

mkdir -p "$DIST/share/misc"
[ -f "$TC_PREFIX/share/misc/magic.mgc" ] && cp "$TC_PREFIX/share/misc/magic.mgc" "$DIST/" 2>/dev/null
[ -f "$TC_PREFIX/share/misc/magic.mgc" ] && cp "$TC_PREFIX/share/misc/magic.mgc" "$DIST/share/misc/" 2>/dev/null

# Qt6 iconv dll on some MSYS2 builds is split:
for d in libiconv-2.dll libintl-8.dll ; do
    [ ! -f "$d" ] && [ -f "$TC_PREFIX/bin/$d" ] && cp "$TC_PREFIX/bin/$d" .
done

# ---- 4b) bundle pdfium.dll into dist/pdfium/ --------------------------------
# The PDF viewer renders pages with PDFium, loaded dynamically at run time.
# src/pdf/pdfium.cpp searches several locations and uses dist/pdfium/pdfium.dll
# as the LAST RESORT, so shipping a copy here guarantees real page rendering
# even on a machine that has no PDFium installed anywhere.
#
# Layout in the source tree:
#   thirdparty/pdfium/
#       Pdfium-Win64/pdfium.dll       (x86_64)
#       Pdfium-Win32/pdfium.dll       (i686)
#       Pdfium-Arm64/pdfium.dll       (aarch64)
# We pick the folder matching the current build target. For backward
# compatibility we also look at a few legacy / system locations (used by
# older setups).
mkdir -p "$DIST/pdfium"
PDFIUM_SRCS=(
    "$PROJ/thirdparty/pdfium/$PDFIUM_SUBDIR/pdfium.dll"
    "$PROJ/thirdparty/pdfium${DIST_SUFFIX}/pdfium.dll"
    "$PROJ/thirdparty/pdfium/pdfium.dll"
    "/c/Program Files/Netskope/EPDLP/win/pdfium.dll"
    "/c/Program Files (x86)/Netskope/EPDLP Deployment/x64/pdfium.dll"
    "/c/Program Files/Windows Defender Advanced Threat Protection/Classification/Dprt/pdfium.dll"
)
if [ ! -f "$DIST/pdfium/pdfium.dll" ]; then
    for src in "${PDFIUM_SRCS[@]}"; do
        if [ -f "$src" ]; then
            # Only take a DLL whose arch matches this build's exe.
            if objdump -f "$src" 2>/dev/null | grep -Eq "$OBJFMT_RE"; then
                cp "$src" "$DIST/pdfium/pdfium.dll" && \
                    echo "  pdfium.dll bundled from: $src"
                break
            fi
        fi
    done
fi
if [ -f "$DIST/pdfium/pdfium.dll" ]; then
    PDFIUM_SZ=$(stat -c %s "$DIST/pdfium/pdfium.dll" 2>/dev/null || echo "?")
    echo "  pdfium: $DIST/pdfium/pdfium.dll ($PDFIUM_SZ bytes)"
else
    echo "  pdfium: NOT bundled — put a matching ($TARGET) pdfium.dll in thirdparty/pdfium/$PDFIUM_SUBDIR/"
    echo "          (the viewer still works; pages fall back to the built-in engine)"
fi

# ---- 4b2) pre-create default host-this-PC folders ---------------------------
# The Share Folder toggle defaults to <TerminalPP.exe>/sharefolder and the SSH
# Server toggle defaults to <TerminalPP.exe>/home. These folders are normally
# created on-demand when the user ticks the matching menu entry, but we make
# them visible in dist/ up front so the shipped layout is self-documenting
# ("here is where the SMB share lives", "here is where SSH sessions land").
# A .keep sentinel keeps the directories around when the tree is zipped or
# synced via tools that drop empty folders.
mkdir -p "$DIST/sharefolder"
mkdir -p "$DIST/home"
for d in "$DIST/sharefolder" "$DIST/home"; do
    [ -f "$d/.keep" ] || : > "$d/.keep"
done

# ---- 4c) bundle portable OpenSSH for this target ----------------------------
# Terminal++'s "SSH Server (./home)" toggle looks for a bundled sshd.exe next
# to the program (in dist/openssh/) and spawns it in user mode on port 2222
# — no Administrator required. We ship the matching portable OpenSSH for the
# target arch from thirdparty/openssh/OpenSSH-<Variant>/.
OPENSSH_SRC="$PROJ/thirdparty/openssh/$OPENSSH_SUBDIR"
if [ -d "$OPENSSH_SRC" ]; then
    mkdir -p "$DIST/openssh"
    # Copy only the files Terminal++ needs at run time: sshd.exe, ssh-keygen,
    # sftp-server (so SFTP subsystem works) and the libcrypto that both depend
    # on. We skip the install-sshd.ps1 helpers because they target admin setup.
    for f in sshd.exe ssh-keygen.exe sftp-server.exe ssh-shellhost.exe \
             ssh-sk-helper.exe libcrypto.dll sshd_config_default moduli \
             LICENSE.txt NOTICE.txt ; do
        src="$OPENSSH_SRC/$f"
        [ -f "$src" ] && cp "$src" "$DIST/openssh/"
    done
    echo "  openssh: $DIST/openssh/ (from $OPENSSH_SRC)"
else
    echo "  openssh: NOT bundled — thirdparty/openssh/$OPENSSH_SUBDIR/ missing"
    echo "           (the SSH-server toggle still works via the system sshd or"
    echo "           by dropping an OpenSSH distribution into dist/openssh/)"
fi

cd "$DIST"
echo "DONE. Portable build in: $DIST/TerminalPP.exe  ($(ls *.dll | wc -l) DLLs bundled, target=$TARGET)"