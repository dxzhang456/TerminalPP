#-------------------------------------------------
# Terminal++ qmake project (alternative to CMake)
# Build with: qmake && mingw32-make
#-------------------------------------------------

QT += core gui widgets network printsupport serialport

greaterThan(QT_MAJOR_VERSION, 5): QT += core

CONFIG += c++17

TARGET = TerminalPP
TEMPLATE = app

# Treat the src directory as an include root.
INCLUDEPATH += src

# ---- libssh2 (native SSH, no external ssh.exe/plink) ----
# Linux / macOS: use pkg-config. Windows: set LIBSSH2_DIR to your install.
unix {
    CONFIG += link_pkgconfig
    PKGCONFIG += libssh2
}
win32 {
    CONFIG += windows
    # Point LIBSSH2_DIR at your libssh2 install (headers in include/, lib in lib/).
    isEmpty(LIBSSH2_DIR): LIBSSH2_DIR = $$(LIBSSH2_DIR)
    !isEmpty(LIBSSH2_DIR) {
        INCLUDEPATH += $$LIBSSH2_DIR/include
        LIBS += -L$$LIBSSH2_DIR/lib
    }
    LIBS += -lssh2 -lmagic -lws2_32 -lcrypt32 -lmpr
}

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/core/i18n.cpp \
    src/core/cliphistory.cpp \
    src/core/cliphistorydialog.cpp \
    src/core/secretstore.cpp \
    src/core/fileassoc.cpp \
    src/core/magicdetector.cpp \
    src/core/errorlogger.cpp \
    src/compare/diffengine.cpp \
    src/compare/textcomparetab.cpp \
    src/compare/foldercomparetab.cpp \
    src/compare/hexcomparetab.cpp \
    src/compare/imagecomparetab.cpp \
    src/compare/comparedialog.cpp \
    src/cloud/clouddrivestore.cpp \
    src/cloud/restproviders.cpp \
    src/cloud/cloudconnectdialog.cpp \
    src/cloud/cloudbrowsertab.cpp \
    src/cloud/streamproxy.cpp \
    src/cloud/transfermanager.cpp \
    src/cloud/transferpanel.cpp \
    src/cloud/smbprovider.cpp \
    src/cloud/sftpprovider.cpp \
    src/cloud/ftpprovider.cpp \
    src/editor/codeeditor.cpp \
    src/editor/syntaxhighlighter.cpp \
    src/editor/finddialog.cpp \
    src/editor/gotodialog.cpp \
    src/editor/editortab.cpp \
    src/editor/hexeditortab.cpp \
    src/editor/imageeditortab.cpp \
    src/editor/thememanager.cpp \
    src/terminal/terminalwidget.cpp \
    src/terminal/ansiparser.cpp \
    src/terminal/sshsession.cpp \
    src/terminal/sshtab.cpp \
    src/terminal/sessiondialog.cpp \
    src/terminal/sessionmanager.cpp \
    src/terminal/serialsession.cpp \
    src/terminal/serialtab.cpp \
    src/terminal/serialdialog.cpp

HEADERS += \
    src/mainwindow.h \
    src/core/i18n.h \
    src/core/cliphistory.h \
    src/core/cliphistorydialog.h \
    src/core/secretstore.h \
    src/core/fileassoc.h \
    src/core/magicdetector.h \
    src/core/errorlogger.h \
    src/compare/diffengine.h \
    src/compare/textcomparetab.h \
    src/compare/foldercomparetab.h \
    src/compare/hexcomparetab.h \
    src/compare/imagecomparetab.h \
    src/compare/comparedialog.h \
    src/cloud/cloudprovider.h \
    src/cloud/clouddrivestore.h \
    src/cloud/restproviders.h \
    src/cloud/cloudconnectdialog.h \
    src/cloud/cloudbrowsertab.h \
    src/cloud/streamproxy.h \
    src/cloud/transfermanager.h \
    src/cloud/transferpanel.h \
    src/cloud/smbprovider.h \
    src/cloud/sftpprovider.h \
    src/cloud/ftpprovider.h \
    src/editor/codeeditor.h \
    src/editor/syntaxhighlighter.h \
    src/editor/finddialog.h \
    src/editor/gotodialog.h \
    src/editor/editortab.h \
    src/editor/hexeditortab.h \
    src/editor/imageeditortab.h \
    src/editor/thememanager.h \
    src/terminal/terminalwidget.h \
    src/terminal/ansiparser.h \
    src/terminal/sshsession.h \
    src/terminal/sshtab.h \
    src/terminal/sessiondialog.h \
    src/terminal/sessionmanager.h \
    src/terminal/serialsession.h \
    src/terminal/serialtab.h \
    src/terminal/serialdialog.h

RESOURCES += \
    resources/resources.qrc

# Windows application icon (Explorer/taskbar exe icon).
win32 {
    RC_FILE = resources/app.rc
}
