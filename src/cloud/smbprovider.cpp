#include "smbprovider.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QTimer>
#include <QUrl>

#ifdef _WIN32
#include <windows.h>
#include <winnetwk.h>
#pragma comment(lib, "mpr.lib")
#endif

// ---------------------------------------------------------------------------
// On Windows, authenticate the UNC path with WNetAddConnection2 so the share
// is accessible under the supplied credentials without requiring the user to
// have already run `net use` or saved the credential in Windows Credential
// Manager. This is what makes "all share versions" (SMB1/2/3, any server
// that Windows itself can browse) work transparently: once the connection is
// established, every QDir/QFile call against the UNC path behaves exactly
// like a local path and Windows negotiates the SMB dialect internally.
static void ensureConnected(const QString &base, const QString &user, const QString &pass)
{
#ifdef _WIN32
    if (base.isEmpty() || !base.startsWith("\\\\"))
        return;
    NETRESOURCEW nr;
    memset(&nr, 0, sizeof(nr));
    std::wstring wbase = base.toStdWString();
    nr.dwType = RESOURCETYPE_DISK;
    nr.lpRemoteName = const_cast<wchar_t *>(wbase.c_str());

    const wchar_t *u = user.isEmpty() ? nullptr : (const wchar_t *)user.utf16();
    const wchar_t *p = pass.isEmpty() ? nullptr : (const wchar_t *)pass.utf16();
    // CONNECT_TEMPORARY avoids persisting the mapping across logons; errors
    // (e.g. ERROR_SESSION_CREDENTIAL_CONFLICT when a different credential is
    // already mapped to that server) are tolerated — the share may already
    // be reachable via an existing session.
    WNetAddConnection2W(&nr, p, u, CONNECT_TEMPORARY);
#else
    Q_UNUSED(base); Q_UNUSED(user); Q_UNUSED(pass);
#endif
}

QString SmbShareProvider::absPath(const QString &relPath) const
{
    QString base = m_base;
    // Normalize trailing separators.
    while (base.endsWith('/') || base.endsWith('\\'))
        base.chop(1);
    QString rel = relPath;
    if (rel == "/" || rel.isEmpty())
        return base;
    if (!rel.startsWith('/'))
        rel = "/" + rel;
    // UNC paths want backslashes; QDir/QFile accept forward slashes too, but
    // using backslashes matches what Windows Explorer/WNetAddConnection2 show.
    QString full = base + rel;
    return full;
}

void SmbShareProvider::list(const QString &path)
{
    ensureConnected(m_base, m_user, m_pass);
    const QString full = absPath(path);
    QTimer::singleShot(0, this, [this, path, full]() {
        QDir dir(full);
        if (!dir.exists()) {
            emit errorOccurred(tr("Cannot access shared folder: %1").arg(full));
            return;
        }
        QVector<CloudEntry> out;
        const QFileInfoList entries = dir.entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name);
        for (const QFileInfo &fi : entries) {
            CloudEntry e;
            e.name = fi.fileName();
            QString rel = path;
            if (!rel.endsWith('/')) rel += '/';
            rel += e.name;
            e.path = rel;
            e.isDir = fi.isDir();
            e.size = fi.size();
            e.modified = fi.lastModified();
            out.push_back(e);
        }
        emit listed(path, out);
    });
}

void SmbShareProvider::download(const QString &remotePath, const QString &localPath)
{
    ensureConnected(m_base, m_user, m_pass);
    const QString full = absPath(remotePath);
    QTimer::singleShot(0, this, [this, remotePath, localPath, full]() {
        QFile src(full);
        if (!src.open(QIODevice::ReadOnly)) {
            emit errorOccurred(tr("Cannot read: %1").arg(full));
            return;
        }
        QFile dst(localPath);
        if (!dst.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            emit errorOccurred(tr("Cannot write local file: %1").arg(localPath));
            src.close();
            return;
        }
        const qint64 total = src.size();
        qint64 done = 0;
        const qint64 chunk = 1 << 20; // 1 MiB chunks for progress reporting
        while (!src.atEnd()) {
            QByteArray buf = src.read(chunk);
            dst.write(buf);
            done += buf.size();
            emit downloadProgress(done, total);
        }
        src.close();
        dst.close();
        emit downloadFinished(remotePath, localPath);
    });
}

void SmbShareProvider::resolveStreamUrl(const QString &remotePath)
{
    // A UNC/local path IS already a directly playable "URL" for any system
    // media player (via QUrl::fromLocalFile), so no header-injecting relay is
    // needed here — unlike the cloud REST providers.
    ensureConnected(m_base, m_user, m_pass);
    const QString full = absPath(remotePath);
    emit streamUrlReady(remotePath, QUrl::fromLocalFile(full).toString());
}

void SmbShareProvider::upload(const QString &localPath, const QString &remoteDir)
{
    ensureConnected(m_base, m_user, m_pass);
    const QString name = QFileInfo(localPath).fileName();
    QString rel = remoteDir;
    if (!rel.endsWith('/')) rel += '/';
    rel += name;
    const QString full = absPath(rel);

    QTimer::singleShot(0, this, [this, localPath, remoteDir, full]() {
        QFile src(localPath);
        if (!src.open(QIODevice::ReadOnly)) {
            emit errorOccurred(tr("Cannot read local file: %1").arg(localPath));
            return;
        }
        QFile dst(full);
        if (!dst.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            emit errorOccurred(tr("Cannot write to shared folder: %1").arg(full));
            src.close();
            return;
        }
        const qint64 total = src.size();
        qint64 done = 0;
        const qint64 chunk = 1 << 20;
        while (!src.atEnd()) {
            QByteArray buf = src.read(chunk);
            dst.write(buf);
            done += buf.size();
            emit uploadProgress(localPath, done, total);
        }
        src.close();
        dst.close();
        emit uploadFinished(localPath, remoteDir);
    });
}

void SmbShareProvider::makeDir(const QString &remoteDir, const QString &name)
{
    ensureConnected(m_base, m_user, m_pass);
    const QString full = absPath(remoteDir);
    QTimer::singleShot(0, this, [this, full, name]() {
        QDir dir(full);
        if (dir.mkdir(name))
            emit operationFinished();
        else
            emit errorOccurred(tr("Cannot create folder: %1/%2").arg(full, name));
    });
}

void SmbShareProvider::remove(const QString &remotePath)
{
    ensureConnected(m_base, m_user, m_pass);
    const QString full = absPath(remotePath);
    QTimer::singleShot(0, this, [this, full]() {
        QFileInfo fi(full);
        bool ok = false;
        if (fi.isDir()) {
            QDir d(full);
            ok = d.removeRecursively();
        } else {
            ok = QFile::remove(full);
        }
        if (ok)
            emit operationFinished();
        else
            emit errorOccurred(tr("Cannot delete: %1").arg(full));
    });
}

void SmbShareProvider::rename(const QString &remotePath, const QString &newName)
{
    ensureConnected(m_base, m_user, m_pass);
    const QString full = absPath(remotePath);
    QTimer::singleShot(0, this, [this, full, newName]() {
        QFileInfo fi(full);
        const QString dest = fi.absolutePath() + "/" + newName;
        if (QFile::rename(full, dest))
            emit operationFinished();
        else
            emit errorOccurred(tr("Cannot rename: %1").arg(full));
    });
}