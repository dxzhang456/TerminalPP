#include <QApplication>
#include <QGuiApplication>
#include <QStyleFactory>
#include <QIcon>
#include <QTimer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QDir>
#include <QFileInfo>
#include "mainwindow.h"
#include "core/i18n.h"
#include "core/cliphistory.h"
#include "core/errorlogger.h"
#include "cloud/cloudprovider.h"

// ---------------------------------------------------------------------------
// Single-instance plumbing.
//
// Terminal++ runs as ONE process. When the user double-clicks a file (or uses
// Explorer's "Open with"), Windows launches the exe again with the file path
// on the command line. Instead of spawning a new window, that second launch:
//   1. connects to the primary instance's local socket ("TerminalPP-singleton"),
//   2. sends the newline-separated absolute file paths, and
//   3. exits immediately.
// The primary instance receives the list and opens each file as a NEW TAB in
// its existing window (raising/activating the window), exactly as requested:
// "这个应该在原来打开的terminapp中新增一个tab 在这个tab打开这个文件".
// ---------------------------------------------------------------------------
static const char *kServerName = "TerminalPP-singleton-ipc";

// Convert command-line args into a list of absolute file paths (skip options).
static QStringList collectFileArgs(int argc, char *argv[])
{
    QStringList out;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg.startsWith('-'))
            continue;
        out << QFileInfo(arg).absoluteFilePath();
    }
    return out;
}

// Try to hand our file list to an already-running primary instance. Returns
// true if a primary was found and the message was delivered (so this process
// should exit without creating a window).
static bool forwardToPrimary(const QStringList &files)
{
    QLocalSocket socket;
    socket.connectToServer(kServerName);
    if (!socket.waitForConnected(300))
        return false;   // no primary instance running

    // Payload: newline-separated paths (may be empty to just raise the window).
    const QByteArray payload = files.join('\n').toUtf8();
    socket.write(payload);
    socket.flush();
    socket.waitForBytesWritten(300);
    socket.disconnectFromServer();
    if (socket.state() != QLocalSocket::UnconnectedState)
        socket.waitForDisconnected(300);
    return true;
}

int main(int argc, char *argv[])
{
    ErrorLogger::initialize();

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

    QApplication app(argc, argv);

    ErrorLogger::logError(QStringLiteral("Startup"),
                          QStringLiteral("Application started. Log file: %1")
                              .arg(ErrorLogger::logFilePath()));

    qRegisterMetaType<QVector<CloudEntry>>("QVector<CloudEntry>");
    QApplication::setApplicationName("TerminalPP");
    QApplication::setOrganizationName("TerminalPP");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QApplication::setWindowIcon(QIcon(":/icon.png"));

    // --- Single-instance check (BEFORE building the window) ---
    // If a primary instance is already running, forward our file list to it and
    // quit immediately so a second window is never created.
    const QStringList fileArgs = collectFileArgs(argc, argv);
    if (forwardToPrimary(fileArgs)) {
        // Delivered to the primary; this launch is done.
        return 0;
    }

    I18n::instance()->initialize();

    MainWindow window;
    window.show();

    // Become the primary instance: listen for future launches and open the
    // files they forward as new tabs in THIS window.
    // Remove any stale socket left by a previous crash before listening.
    QLocalServer::removeServer(kServerName);
    auto *server = new QLocalServer(&app);
    if (server->listen(kServerName)) {
        QObject::connect(server, &QLocalServer::newConnection, &window, [server, &window]() {
            while (QLocalSocket *client = server->nextPendingConnection()) {
                QObject::connect(client, &QLocalSocket::readyRead, &window, [client, &window]() {
                    const QByteArray data = client->readAll();
                    const QString text = QString::fromUtf8(data);
                    QStringList files;
                    for (const QString &p : text.split('\n', Qt::SkipEmptyParts))
                        files << p;
                    window.raiseAndOpenFiles(files);
                    client->disconnectFromServer();
                    client->deleteLater();
                });
            }
        });
    }

    QTimer::singleShot(0, []() { ClipHistory::instance()->initialize(); });

    // Open files passed on the command line of THIS (primary) launch.
    bool openedAny = false;
    for (const QString &f : fileArgs) {
        if (window.openFile(f))
            openedAny = true;
    }
    if (openedAny)
        window.closeInitialEmptyTab();

    return app.exec();
}