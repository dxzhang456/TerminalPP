#include <QApplication>
#include <QStyleFactory>
#include <QIcon>
#include "mainwindow.h"
#include "core/i18n.h"
#include "core/cliphistory.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("TerminalPP");
    QApplication::setOrganizationName("TerminalPP");
    QApplication::setApplicationVersion("1.0.0");

    // Use Fusion for a consistent cross-theme look (dark themes rely on it).
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    // Application / window / taskbar icon (embedded from resources/icon.png).
    QApplication::setWindowIcon(QIcon(":/icon.png"));

    // Load the persisted UI language (defaults to English on first launch) and
    // start tracking the clipboard history.
    I18n::instance()->initialize();
    ClipHistory::instance()->initialize();

    MainWindow window;
    window.show();

    // Open files passed on the command line.
    for (int i = 1; i < argc; ++i) {
        window.openFile(QString::fromLocal8Bit(argv[i]));
    }

    return app.exec();
}