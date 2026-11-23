#include "fileassoc.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

#ifdef _WIN32
#  include <windows.h>
#  include <shlobj.h>   // SHChangeNotify
#endif

// ---------------------------------------------------------------------------
// Extension groups
// ---------------------------------------------------------------------------
QStringList FileAssociations::textExtensions()
{
    return {
        "txt","text","md","markdown","log","csv","tsv","ini","cfg","conf",
        "config","properties","env","yaml","yml","toml","json","xml","rst",
        "nfo","srt","vtt","diff","patch"
    };
}

QStringList FileAssociations::sourceCodeExtensions()
{
    return {
        "c","cc","cpp","cxx","h","hh","hpp","hxx","inl","ipp","m","mm",
        "cs","java","kt","kts","scala","groovy","py","pyw","rb","pl","pm",
        "php","lua","tcl","r","jl","swift","go","rs","dart",
        "js","jsx","mjs","cjs","ts","tsx","vue","svelte",
        "sh","bash","zsh","bat","cmd","ps1","psm1",
        "sql","graphql","proto","thrift","f","f90","pas","vb","vbs",
        "erl","ex","exs","hs","ml","clj","lisp","el","nim","zig","v","vhd",
        "html","htm","css","scss","sass","less","jsp","asp","aspx",
        "cmake","gradle","pro","pri","qrc","ui","mk","makefile","dockerfile"
    };
}

QStringList FileAssociations::imageExtensions()
{
    return {
        "png","jpg","jpeg","bmp","gif","webp","tif","tiff","ico","svg"
    };
}

QStringList FileAssociations::pdfExtensions()
{
    return { "pdf" };
}

QStringList FileAssociations::allExtensions()
{
    QStringList all = textExtensions();
    all += sourceCodeExtensions();
    all += imageExtensions();
    all += pdfExtensions();
    all.removeDuplicates();
    return all;
}

// ---------------------------------------------------------------------------
// Internals
// ---------------------------------------------------------------------------
QString FileAssociations::exePath()
{
    return QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
}

QString FileAssociations::progId()
{
    return QStringLiteral("TerminalPP.Document");
}

QString FileAssociations::openCommand()
{
    return QString("\"%1\" \"%2\"").arg(exePath(), QStringLiteral("%1"));
}

bool FileAssociations::isSupported()
{
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

#ifdef _WIN32

// All registry work happens under HKCU\Software\Classes (per-user, no admin).
static const QString kClassesRoot =
    QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes");

bool FileAssociations::ensureProgIdRegistered()
{
    // HKCU\Software\Classes\TerminalPP.Document
    //   (Default)                      = "Terminal++ Document"
    //   DefaultIcon\(Default)          = "<exe>,0"
    //   shell\open\command\(Default)   = "<exe>" "%1"
    const QString base = kClassesRoot + "\\" + progId();
    {
        QSettings r(base, QSettings::NativeFormat);
        r.setValue(".", "Terminal++ Document");
    }
    {
        QSettings r(base + "\\DefaultIcon", QSettings::NativeFormat);
        r.setValue(".", QString("%1,0").arg(exePath()));
    }
    {
        QSettings r(base + "\\shell\\open\\command", QSettings::NativeFormat);
        r.setValue(".", openCommand());
    }
    {
        // Friendly name shown in the "Open with" list.
        QSettings r(base + "\\shell\\open", QSettings::NativeFormat);
        r.setValue("FriendlyAppName", "Terminal++");
    }
    return true;
}

// Notify the shell that associations changed so Explorer refreshes.
static void notifyShell()
{
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

bool FileAssociations::addToOpenWith(const QStringList &extensions)
{
    ensureProgIdRegistered();
    for (const QString &ext : extensions) {
        const QString e = ext.startsWith('.') ? ext : "." + ext;
        // HKCU\Software\Classes\<.ext>\OpenWithProgids\TerminalPP.Document = ""
        QSettings r(kClassesRoot + "\\" + e + "\\OpenWithProgids",
                    QSettings::NativeFormat);
        r.setValue(progId(), QString());   // empty REG_SZ value = membership
    }
    notifyShell();
    return true;
}

bool FileAssociations::removeFromOpenWith(const QStringList &extensions)
{
    for (const QString &ext : extensions) {
        const QString e = ext.startsWith('.') ? ext : "." + ext;
        QSettings r(kClassesRoot + "\\" + e + "\\OpenWithProgids",
                    QSettings::NativeFormat);
        r.remove(progId());
    }
    notifyShell();
    return true;
}

bool FileAssociations::setAsDefault(const QStringList &extensions)
{
    ensureProgIdRegistered();
    for (const QString &ext : extensions) {
        const QString e = ext.startsWith('.') ? ext : "." + ext;
        // Per-user default handler: HKCU\Software\Classes\<.ext>\(Default) = ProgID.
        // (Also list it under OpenWithProgids so it appears in the menu.)
        {
            QSettings r(kClassesRoot + "\\" + e, QSettings::NativeFormat);
            r.setValue(".", progId());
        }
        {
            QSettings r(kClassesRoot + "\\" + e + "\\OpenWithProgids",
                        QSettings::NativeFormat);
            r.setValue(progId(), QString());
        }
    }
    notifyShell();
    return true;
}

bool FileAssociations::unsetAsDefault(const QStringList &extensions)
{
    for (const QString &ext : extensions) {
        const QString e = ext.startsWith('.') ? ext : "." + ext;
        QSettings r(kClassesRoot + "\\" + e, QSettings::NativeFormat);
        // Only clear it if WE set it (don't clobber another app's ProgID).
        if (r.value(".").toString() == progId())
            r.remove(".");
    }
    notifyShell();
    return true;
}

bool FileAssociations::isInOpenWith(const QString &extension)
{
    const QString e = extension.startsWith('.') ? extension : "." + extension;
    QSettings r(kClassesRoot + "\\" + e + "\\OpenWithProgids",
                QSettings::NativeFormat);
    return r.contains(progId());
}

bool FileAssociations::isDefault(const QString &extension)
{
    const QString e = extension.startsWith('.') ? extension : "." + extension;
    QSettings r(kClassesRoot + "\\" + e, QSettings::NativeFormat);
    return r.value(".").toString() == progId();
}

#else  // ---- non-Windows: safe no-ops ----

bool FileAssociations::ensureProgIdRegistered() { return false; }
bool FileAssociations::addToOpenWith(const QStringList &) { return false; }
bool FileAssociations::removeFromOpenWith(const QStringList &) { return false; }
bool FileAssociations::setAsDefault(const QStringList &) { return false; }
bool FileAssociations::unsetAsDefault(const QStringList &) { return false; }
bool FileAssociations::isInOpenWith(const QString &) { return false; }
bool FileAssociations::isDefault(const QString &) { return false; }

#endif