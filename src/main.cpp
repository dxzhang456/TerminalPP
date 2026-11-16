#include <QApplication>
#include <QStyleFactory>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("TerminalPP");
    QApplication::setOrganizationName("TerminalPP");
    QApplication::setApplicationVersion("1.0.0");

    // Use Fusion for a consistent cross-theme look (dark themes rely on it).
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    MainWindow window;
    window.show();

    // Open files passed on the command line.
    for (int i = 1; i < argc; ++i) {
        window.openFile(QString::fromLocal8Bit(argv[i]));
    }

    return app.exec();
}