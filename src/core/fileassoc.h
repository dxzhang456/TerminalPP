#ifndef FILEASSOC_H
#define FILEASSOC_H

#include <QString>
#include <QStringList>

// Windows file-association helper.
//
// Registers (or removes) Terminal++ as a handler for text / source-code /
// supported-image file extensions, using per-user keys under
//   HKCU\Software\Classes
// so NO administrator rights are required. Two capabilities are exposed, each
// independently add-able and removable:
//
//   1. "Open With" entry  — adds Terminal++ to the Explorer right-click
//      "Open with" sub-menu for the given extensions (HKCU\...\<ext>\OpenWithProgids
//      + a registered ProgID). Does NOT change the current default.
//
//   2. Default program    — makes Terminal++ the default handler (sets the
//      per-user UserChoice/ProgID default for the extension). Reversible.
//
// On non-Windows builds every method is a safe no-op returning false, so the
// calling UI can still compile and simply report "not supported on this OS".
class FileAssociations
{
public:
    // The three groups of extensions the UI offers (without leading dots).
    static QStringList textExtensions();
    static QStringList sourceCodeExtensions();
    static QStringList imageExtensions();
    // Convenience: text + source + image, de-duplicated.
    static QStringList allExtensions();

    // True on Windows (where association is implemented).
    static bool isSupported();

    // Ensure the Terminal++ ProgID ("TerminalPP.Document") exists under
    // HKCU\Software\Classes with an open command pointing at this exe.
    // Called automatically by the add* methods; safe to call repeatedly.
    static bool ensureProgIdRegistered();

    // --- "Open With" menu entry (does not change the default) ---------------
    // Add Terminal++ to the "Open with" list for each extension.
    static bool addToOpenWith(const QStringList &extensions);
    // Remove Terminal++ from the "Open with" list for each extension.
    static bool removeFromOpenWith(const QStringList &extensions);

    // --- Default program ----------------------------------------------------
    // Make Terminal++ the default handler for each extension (per-user).
    static bool setAsDefault(const QStringList &extensions);
    // Restore the previous/non-Terminal++ default for each extension by
    // clearing our per-user default ProgID association.
    static bool unsetAsDefault(const QStringList &extensions);

    // Query helpers for the UI (reflect current state).
    static bool isInOpenWith(const QString &extension);
    static bool isDefault(const QString &extension);

private:
    static QString exePath();          // this executable's path (native sep)
    static QString progId();           // "TerminalPP.Document"
    static QString openCommand();      // "\"<exe>\" \"%1\""
};

#endif // FILEASSOC_H