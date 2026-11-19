#include <QApplication>
#include <QStyleFactory>
#include <QIcon>
#include <QTimer>
#include "mainwindow.h"
#include "core/i18n.h"
#include "core/cliphistory.h"
#include "cloud/cloudprovider.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Needed so QVector<CloudEntry> can be queued across threads (used by
    // SftpProvider's worker-thread model for list() results).
    qRegisterMetaType<QVector<CloudEntry>>("QVector<CloudEntry>");
    QApplication::setApplicationName("TerminalPP");
    QApplication::setOrganizationName("TerminalPP");
    QApplication::setApplicationVersion("1.0.0");

    // Use Fusion for a consistent cross-theme look (dark themes rely on it).
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    // Application / window / taskbar icon (embedded from resources/icon.png).
    QApplication::setWindowIcon(QIcon(":/icon.png"));

    // Load the persisted UI language (defaults to English on first launch).
    I18n::instance()->initialize();

    MainWindow window;
    window.show();

    // Defer clipboard-history tracking until after the window is shown so it
    // never adds to the perceived startup time (it installs an app-wide
    // clipboard listener which is irrelevant to the first paint).
    QTimer::singleShot(0, []() { ClipHistory::instance()->initialize(); });

    // Open files passed on the command line. If any file opens successfully,
    // discard the default empty "new" tab created at startup so double-clicking
    // a .txt just shows that file (no extra blank document).
    bool openedAny = false;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg.startsWith('-'))        // skip option-like args
            continue;
        if (window.openFile(arg))
            openedAny = true;
    }
    if (openedAny)
        window.closeInitialEmptyTab();

    return app.exec();
}
