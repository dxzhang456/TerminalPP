#include <QApplication>
#include <QGuiApplication>
#include <QStyleFactory>
#include <QIcon>
#include <QTimer>
#include "mainwindow.h"
#include "core/i18n.h"
#include "core/cliphistory.h"
#include "cloud/cloudprovider.h"

int main(int argc, char *argv[])
{
    // --- Startup optimizations (must run BEFORE QApplication is constructed) ---
    // Keep Qt from scanning extra plugin paths at launch — we ship exactly the
    // plugins we need right next to the executable, so skipping the system /
    // environment search saves measurable time during the first paint.
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // HighDpiScaleFactorRoundingPolicy is already set to PassThrough by Qt 6
    // on Windows; explicitly pin it so Fusion-styled toolbar icons avoid an
    // extra layout pass when a fractional DPI (e.g. 150%) is in use.
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

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
    // The full translation table is ONLY built when a non-English language is
    // active, so English launches skip ~500 hashtable inserts entirely.
    I18n::instance()->initialize();

    MainWindow window;
    window.show();

    // Defer anything that isn't needed for the first paint until after the
    // window is visible — clipboard history installs an app-wide listener,
    // and (future) non-critical background work is also enqueued here.
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
