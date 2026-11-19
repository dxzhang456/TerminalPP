#ifndef SMBPROVIDER_H
#define SMBPROVIDER_H

#include "cloudprovider.h"
#include <QString>

// Windows / Samba shared-folder provider.
//
// Qt (and Windows itself) exposes SMB shares as ordinary filesystem paths
// once addressed via a UNC path: "\\server\share\sub\dir". The Win32 I/O
// layer underneath QDir/QFile negotiates whichever SMB dialect the server
// offers (SMB1/CIFS, SMB2, SMB3/3.1.1) completely transparently — there is no
// SMB-version selection needed at the application level, which is why this
// provider works against "all share versions" without extra protocol code.
//
// On Linux, the same UNC-style path ("//server/share/...") is accepted by
// Samba's `smbclient`/`cifs-utils` mount conventions; this provider instead
// expects the share to already be reachable as a local path (either a UNC
// path Windows resolves natively, or a path under an existing `mount -t cifs`
// mount point on Linux — see the project's network-mount tooling for how such
// mounts are created). This keeps the implementation purely Qt/stdlib-based
// with no extra SMB client library dependency.
//
// All operations run synchronously on local/LAN I/O (which is fast) but are
// dispatched via QTimer::singleShot(0, ...) so they still return through the
// normal async signal path the rest of the app (CloudBrowserTab, transfer
// tracking) expects.
class SmbShareProvider : public CloudProvider
{
    Q_OBJECT
public:
    explicit SmbShareProvider(QObject *parent = nullptr) : CloudProvider(parent) {}

    // basePath like "\\\\server\\share" (Windows) or "/mnt/server/share" (an
    // already-mounted Samba share on Linux). user/pass are accepted for
    // symmetry with the other providers; on Windows a UNC path already
    // authenticated via `net use`/Credential Manager needs no further
    // authentication here, so they are currently informational only.
    void setEndpoint(const QString &basePath, const QString &user, const QString &pass) {
        m_base = basePath; m_user = user; m_pass = pass;
    }

    QString providerName() const override { return QStringLiteral("Shared Folder"); }
    QString rootPath() const override { return QStringLiteral("/"); }

    void list(const QString &path) override;
    void download(const QString &remotePath, const QString &localPath) override;
    void resolveStreamUrl(const QString &remotePath) override;
    void upload(const QString &localPath, const QString &remoteDir) override;
    void makeDir(const QString &remoteDir, const QString &name) override;
    void remove(const QString &remotePath) override;
    void rename(const QString &remotePath, const QString &newName) override;
    // Abort an in-flight copy loop (download/upload). The SMB copy runs on the
    // GUI thread in 1 MiB chunks; the loop pumps events between chunks and
    // checks this flag so Cancel/Remove stops it promptly.
    void cancelTransfers() override { m_cancel = true; }

private:
    QString absPath(const QString &relPath) const;
    QString m_base, m_user, m_pass;
    bool m_cancel = false;   // set by cancelTransfers(); polled by copy loops
};

#endif // SMBPROVIDER_H