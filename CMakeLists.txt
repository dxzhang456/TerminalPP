cmake_minimum_required(VERSION 3.19)

project(TerminalPP VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt6 modules. SerialPort is required for the UART / serial feature.
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    PrintSupport
    SerialPort
)

qt_standard_project_setup()

# --- libssh2 (native SSH client, no external ssh.exe/plink dependency) ---
# Try CMake config package first, then pkg-config, then a manual hint.
find_package(Libssh2 CONFIG QUIET)
if (NOT Libssh2_FOUND)
    find_package(libssh2 CONFIG QUIET)
endif()

set(LIBSSH2_TARGET "")
if (TARGET Libssh2::libssh2)
    set(LIBSSH2_TARGET Libssh2::libssh2)
elseif (TARGET libssh2::libssh2)
    set(LIBSSH2_TARGET libssh2::libssh2)
endif()

if (LIBSSH2_TARGET STREQUAL "")
    # pkg-config fallback (common on Linux / MSYS2).
    find_package(PkgConfig QUIET)
    if (PkgConfig_FOUND)
        pkg_check_modules(LIBSSH2 QUIET libssh2)
    endif()
endif()

set(PROJECT_SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h

    # --- Core (i18n, clipboard history, preferences, secret storage) ---
    src/core/i18n.cpp
    src/core/i18n.h
    src/core/cliphistory.cpp
    src/core/cliphistory.h
    src/core/cliphistorydialog.cpp
    src/core/cliphistorydialog.h
    src/core/secretstore.cpp
    src/core/secretstore.h

    # --- Compare (Beyond Compare-style) ---
    src/compare/diffengine.cpp
    src/compare/diffengine.h
    src/compare/textcomparetab.cpp
    src/compare/textcomparetab.h
    src/compare/foldercomparetab.cpp
    src/compare/foldercomparetab.h
    src/compare/hexcomparetab.cpp
    src/compare/hexcomparetab.h
    src/compare/imagecomparetab.cpp
    src/compare/imagecomparetab.h
    src/compare/comparedialog.cpp
    src/compare/comparedialog.h

    # --- Code editor ---
    src/editor/codeeditor.cpp
    src/editor/codeeditor.h
    src/editor/syntaxhighlighter.cpp
    src/editor/syntaxhighlighter.h
    src/editor/finddialog.cpp
    src/editor/finddialog.h
    src/editor/gotodialog.cpp
    src/editor/gotodialog.h
    src/editor/editortab.cpp
    src/editor/editortab.h
    src/editor/thememanager.cpp
    src/editor/thememanager.h

    # --- Terminal core (shared ANSI/VT100 engine) ---
    src/terminal/terminalwidget.cpp
    src/terminal/terminalwidget.h
    src/terminal/ansiparser.cpp
    src/terminal/ansiparser.h

    # --- SSH terminal ---
    src/terminal/sshsession.cpp
    src/terminal/sshsession.h
    src/terminal/sshtab.cpp
    src/terminal/sshtab.h
    src/terminal/sessiondialog.cpp
    src/terminal/sessiondialog.h
    src/terminal/sessionmanager.cpp
    src/terminal/sessionmanager.h

    # --- Serial / UART terminal ---
    src/terminal/serialsession.cpp
    src/terminal/serialsession.h
    src/terminal/serialtab.cpp
    src/terminal/serialtab.h
    src/terminal/serialdialog.cpp
    src/terminal/serialdialog.h
)

# Windows application icon (compiled from resources/app.rc -> icon.ico).
set(APP_ICON_RESOURCE "")
if (WIN32)
    set(APP_ICON_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
endif()

qt_add_executable(TerminalPP
    WIN32
    ${PROJECT_SOURCES}
    resources/resources.qrc
    ${APP_ICON_RESOURCE}
)

target_include_directories(TerminalPP PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(TerminalPP PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::PrintSupport
    Qt6::SerialPort
)

# Link libssh2 (and its include dirs) via whichever mechanism was found.
if (NOT LIBSSH2_TARGET STREQUAL "")
    target_link_libraries(TerminalPP PRIVATE ${LIBSSH2_TARGET})
elseif (LIBSSH2_FOUND)
    target_include_directories(TerminalPP PRIVATE ${LIBSSH2_INCLUDE_DIRS})
    target_link_libraries(TerminalPP PRIVATE ${LIBSSH2_LIBRARIES})
    target_link_directories(TerminalPP PRIVATE ${LIBSSH2_LIBRARY_DIRS})
else()
    # Last resort: assume headers/libs are on the default search path.
    message(WARNING "libssh2 not found via CMake config or pkg-config; "
                    "assuming it is on the compiler/linker default paths. "
                    "Set -DCMAKE_PREFIX_PATH to your libssh2 install if linking fails.")
    target_link_libraries(TerminalPP PRIVATE ssh2)
endif()

# Winsock + DPAPI (CryptProtectData) on Windows.
if (WIN32)
    target_link_libraries(TerminalPP PRIVATE ws2_32 crypt32)
endif()

# Installation / deployment
install(TARGETS TerminalPP
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION bin
)