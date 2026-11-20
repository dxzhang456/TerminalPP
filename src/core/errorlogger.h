#ifndef ERRORLOGGER_H
#define ERRORLOGGER_H

#include <QString>

// Application-wide error and exception logging system.
// Writes all runtime errors, critical issues, fatal faults, and unhandled
// exceptions into `error.log` located in the exact same directory as the
// executable file.
class ErrorLogger
{
public:
    // Initialize the logger: installs the Qt message handler and Windows / C++
    // unhandled exception handlers. Must be called early in main().
    static void initialize();

    // Log a custom error message explicitly.
    static void logError(const QString &category, const QString &message);

    // Path to the error.log file.
    static QString logFilePath();
};

#endif // ERRORLOGGER_H