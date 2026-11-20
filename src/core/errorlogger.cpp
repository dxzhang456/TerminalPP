#include "errorlogger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QtGlobal>

#include <exception>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

QMutex s_logMutex;

void writeToLogFile(const QString &formattedLine)
{
    QMutexLocker locker(&s_logMutex);
    const QString path = ErrorLogger::logFilePath();
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        stream.setEncoding(QStringConverter::Utf8);
#endif
        stream << formattedLine << "\n";
        stream.flush();
        file.close();
    }
}

void qtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Filter out standard debug / info logs; capture warnings, criticals, fatals
    QString level;
    switch (type) {
    case QtDebugMsg:
        return; // normal debug messages are not errors
    case QtInfoMsg:
        return; // normal informational messages are not errors
    case QtWarningMsg:
        level = QStringLiteral("WARNING");
        break;
    case QtCriticalMsg:
        level = QStringLiteral("CRITICAL");
        break;
    case QtFatalMsg:
        level = QStringLiteral("FATAL");
        break;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    QString line = QStringLiteral("[%1] [%2] %3").arg(timestamp, level, msg);
    if (context.file && context.line > 0) {
        line += QStringLiteral(" (%1:%2, %3)").arg(QString::fromUtf8(context.file),
                                                    QString::number(context.line),
                                                    context.function ? QString::fromUtf8(context.function) : QString());
    }

    writeToLogFile(line);

    if (type == QtFatalMsg) {
        abort();
    }
}

#ifdef _WIN32
LONG WINAPI windowsUnhandledExceptionFilter(EXCEPTION_POINTERS *exPtrs)
{
    DWORD code = exPtrs && exPtrs->ExceptionRecord ? exPtrs->ExceptionRecord->ExceptionCode : 0;
    PVOID addr = exPtrs && exPtrs->ExceptionRecord ? exPtrs->ExceptionRecord->ExceptionAddress : nullptr;
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    const QString line = QStringLiteral("[%1] [FATAL_EXCEPTION] Windows Exception Code: 0x%2 at address: 0x%3")
                             .arg(timestamp,
                                  QString::number(code, 16).toUpper(),
                                  QString::number(reinterpret_cast<quintptr>(addr), 16).toUpper());
    writeToLogFile(line);
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

void standardTerminateHandler()
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    QString details;
    if (std::current_exception()) {
        try {
            throw;
        } catch (const std::exception &e) {
            details = QStringLiteral("std::exception: %1").arg(QString::fromUtf8(e.what()));
        } catch (...) {
            details = QStringLiteral("Unknown C++ unhandled exception");
        }
    } else {
        details = QStringLiteral("std::terminate called without an active exception");
    }

    const QString line = QStringLiteral("[%1] [FATAL_EXCEPTION] %2").arg(timestamp, details);
    writeToLogFile(line);
    abort();
}

} // namespace

QString ErrorLogger::logFilePath()
{
    // IMPORTANT: QCoreApplication::applicationDirPath() emits a qWarning()
    // if no QCoreApplication instance exists yet. Since this logger installs
    // its own qInstallMessageHandler(), that warning would be routed right
    // back into writeToLogFile() -> logFilePath() -> applicationDirPath()
    // -> warning again, recursing forever and deadlocking on s_logMutex
    // (a plain, non-recursive QMutex) before the GUI even starts. Guard
    // against that by checking QCoreApplication::instance() ourselves and
    // falling back to the current working directory (set by the OS loader
    // to the executable's own folder when launched by double-click) without
    // ever calling applicationDirPath() pre-construction.
    QString appDir;
    if (QCoreApplication::instance()) {
        appDir = QCoreApplication::applicationDirPath();
    }
    if (appDir.isEmpty()) {
        appDir = QDir::currentPath();
    }
    return QDir(appDir).filePath(QStringLiteral("error.log"));
}

void ErrorLogger::initialize()
{
    // Install Qt runtime message handler
    qInstallMessageHandler(qtMessageHandler);

    // Install C++ unhandled exception / termination handler
    std::set_terminate(standardTerminateHandler);

#ifdef _WIN32
    // Install Windows SEH unhandled exception filter for crashes / access violations
    SetUnhandledExceptionFilter(windowsUnhandledExceptionFilter);
#endif

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    writeToLogFile(QStringLiteral("[%1] [INFO] === Logging system initialized ===").arg(timestamp));
}

void ErrorLogger::logError(const QString &category, const QString &message)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    const QString line = QStringLiteral("[%1] [ERROR] [%2] %3").arg(timestamp, category, message);
    writeToLogFile(line);
}