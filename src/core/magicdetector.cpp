#include "magicdetector.h"

#include <magic.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QImageReader>
#include <QMutex>
#include <QMutexLocker>
#include <QByteArray>

// ---------------------------------------------------------------------------
// Thread-safe wrapper around a libmagic `magic_t` cookie.
// Automatically locates the `magic.mgc` database file:
//   1. Next to the executable (<app_dir>/magic.mgc or <app_dir>/share/misc/magic.mgc)
//   2. In the MSYS2 mingw64 default location (/mingw64/share/misc/magic.mgc)
//   3. Default libmagic compiled-in path (NULL passed to magic_load)
// ---------------------------------------------------------------------------
namespace {

class MagicCookie
{
public:
    static MagicCookie &instance()
    {
        static MagicCookie inst;
        return inst;
    }

    QString getMime(const QString &filePath)
    {
        QMutexLocker lock(&m_mutex);
        if (!ensureMimeCookie())
            return QString();

        QByteArray nativePath = QDir::toNativeSeparators(filePath).toLocal8Bit();
        const char *res = magic_file(m_mimeMagic, nativePath.constData());
        if (!res) {
            // Try with UTF-8 if local 8-bit failed
            QByteArray utf8Path = filePath.toUtf8();
            res = magic_file(m_mimeMagic, utf8Path.constData());
        }
        return res ? QString::fromLatin1(res) : QString();
    }

    QString getDescription(const QString &filePath)
    {
        QMutexLocker lock(&m_mutex);
        if (!ensureDescCookie())
            return QString();

        QByteArray nativePath = QDir::toNativeSeparators(filePath).toLocal8Bit();
        const char *res = magic_file(m_descMagic, nativePath.constData());
        if (!res) {
            QByteArray utf8Path = filePath.toUtf8();
            res = magic_file(m_descMagic, utf8Path.constData());
        }
        return res ? QString::fromUtf8(res) : QString();
    }

private:
    MagicCookie() = default;
    ~MagicCookie()
    {
        if (m_mimeMagic) magic_close(m_mimeMagic);
        if (m_descMagic) magic_close(m_descMagic);
    }

    static QString findMagicDatabase()
    {
        const QString appDir = QCoreApplication::applicationDirPath();
        const QStringList candidates = {
            appDir + "/magic.mgc",
            appDir + "/share/misc/magic.mgc",
            "C:/msys64/mingw64/share/misc/magic.mgc",
            "/mingw64/share/misc/magic.mgc"
        };
        for (const QString &c : candidates) {
            if (QFile::exists(c))
                return QDir::toNativeSeparators(c);
        }
        return QString();
    }

    bool ensureMimeCookie()
    {
        if (m_mimeMagic)
            return true;
        m_mimeMagic = magic_open(MAGIC_MIME_TYPE | MAGIC_NO_CHECK_COMPRESS);
        if (!m_mimeMagic)
            return false;

        QString dbPath = findMagicDatabase();
        QByteArray dbBytes = dbPath.isEmpty() ? QByteArray() : dbPath.toLocal8Bit();
        const char *dbArg = dbBytes.isEmpty() ? nullptr : dbBytes.constData();

        if (magic_load(m_mimeMagic, dbArg) != 0) {
            // If explicit path failed, try NULL (default)
            if (dbArg != nullptr && magic_load(m_mimeMagic, nullptr) == 0)
                return true;
            magic_close(m_mimeMagic);
            m_mimeMagic = nullptr;
            return false;
        }
        return true;
    }

    bool ensureDescCookie()
    {
        if (m_descMagic)
            return true;
        m_descMagic = magic_open(MAGIC_NONE | MAGIC_NO_CHECK_COMPRESS);
        if (!m_descMagic)
            return false;

        QString dbPath = findMagicDatabase();
        QByteArray dbBytes = dbPath.isEmpty() ? QByteArray() : dbPath.toLocal8Bit();
        const char *dbArg = dbBytes.isEmpty() ? nullptr : dbBytes.constData();

        if (magic_load(m_descMagic, dbArg) != 0) {
            if (dbArg != nullptr && magic_load(m_descMagic, nullptr) == 0)
                return true;
            magic_close(m_descMagic);
            m_descMagic = nullptr;
            return false;
        }
        return true;
    }

    QMutex m_mutex;
    magic_t m_mimeMagic = nullptr;
    magic_t m_descMagic = nullptr;
};

// Fallback: heuristic byte inspection to distinguish text from binary if
// libmagic returns application/octet-stream or fails.
bool looksLikeTextFallback(const QString &filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return true; // Empty / unreadable -> default to text
    QByteArray sample = f.read(8192);
    f.close();

    if (sample.isEmpty())
        return true;

    // Check for NUL bytes or high proportion of binary control characters
    int suspicious = 0;
    for (int i = 0; i < sample.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(sample.at(i));
        if (c == 0)
            return false; // NUL byte: definitely binary
        if (c < 0x20 && c != '\t' && c != '\n' && c != '\r' && c != 0x0c && c != 0x1b)
            ++suspicious;
    }
    return (suspicious * 100 < sample.size());
}

} // namespace

QString MagicDetector::detectMime(const QString &filePath)
{
    QString mime = MagicCookie::instance().getMime(filePath);
    if (mime.isEmpty()) {
        QMimeDatabase db;
        mime = db.mimeTypeForFile(filePath, QMimeDatabase::MatchContent).name();
    }
    return mime;
}

QString MagicDetector::detectDescription(const QString &filePath)
{
    return MagicCookie::instance().getDescription(filePath);
}

MagicDetector::FileType MagicDetector::detectType(const QString &filePath)
{
    QFileInfo fi(filePath);
    if (!fi.exists())
        return Text;
    if (fi.size() == 0)
        return Text; // Empty files open in the text editor

    // 1. First consult libmagic for the MIME type
    QString mime = detectMime(filePath).trimmed().toLower();

    // 2. Classify based on MIME type
    if (mime.startsWith(QStringLiteral("image/"))) {
        // SVG can be treated either as image or text; QImageReader supports SVG
        // and user requested "如果是图片就按照图片格式打开"
        return Image;
    }

    if (mime.startsWith(QStringLiteral("text/")) ||
        mime == QStringLiteral("application/xml") ||
        mime == QStringLiteral("application/json") ||
        mime == QStringLiteral("application/javascript") ||
        mime == QStringLiteral("application/x-javascript") ||
        mime == QStringLiteral("application/x-sh") ||
        mime == QStringLiteral("application/x-shellscript") ||
        mime == QStringLiteral("application/x-python") ||
        mime == QStringLiteral("application/x-perl") ||
        mime == QStringLiteral("application/x-ruby") ||
        mime == QStringLiteral("application/x-php") ||
        mime == QStringLiteral("application/x-yaml") ||
        mime == QStringLiteral("application/x-toml") ||
        mime == QStringLiteral("application/x-subrip") ||
        mime.contains(QStringLiteral("xml")) ||
        mime.contains(QStringLiteral("json")) ||
        mime.contains(QStringLiteral("script"))) {
        return Text;
    }

    // Check if QImageReader can read it as an image (magic-based image sniff)
    QByteArray fmt = QImageReader::imageFormat(filePath);
    if (!fmt.isEmpty()) {
        return Image;
    }

    // 3. If libmagic returned generic "application/octet-stream" or similar,
    // double-check with the detailed description and content heuristics
    if (mime == QStringLiteral("application/octet-stream") || mime.isEmpty()) {
        QString desc = detectDescription(filePath).toLower();
        if (desc.contains(QStringLiteral("text")) ||
            desc.contains(QStringLiteral("source")) ||
            desc.contains(QStringLiteral("script")) ||
            desc.contains(QStringLiteral("ascii")) ||
            desc.contains(QStringLiteral("unicode")) ||
            desc.contains(QStringLiteral("utf-8")) ||
            desc.contains(QStringLiteral("csv"))) {
            return Text;
        }
        if (desc.contains(QStringLiteral("image")) ||
            desc.contains(QStringLiteral("bitmap")) ||
            desc.contains(QStringLiteral("png")) ||
            desc.contains(QStringLiteral("jpeg")) ||
            desc.contains(QStringLiteral("gif"))) {
            return Image;
        }

        // Byte inspection fallback: if it has no binary markers, treat as text
        if (looksLikeTextFallback(filePath))
            return Text;

        return Binary;
    }

    // Known binary MIME types (executables, archives, binaries, audio/video)
    return Binary;
}