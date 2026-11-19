#ifndef SFTPPROVIDER_H
#define SFTPPROVIDER_H

#include "cloudprovider.h"
#include <QString>

// Native SFTP provider built on libssh2 (the same library already linked for
// the SSH terminal feature) — no external sftp.exe / WinSCP dependency.
//
// Connection + authentication (password or private key) happen once on first
// use and are kept alive for the provider's lifetime; every list/download/
// upload/remove/rename/makeDir call reuses that session. All libssh2 calls
// run on a dedicated worker thread (SftpWorker) so the GUI thread never
// blocks on network I/O, mirroring the SshSession / SshWorker split used by
// the SSH terminal feature.
class SftpProvider : public CloudProvider
{
    Q_OBJECT
public:
    explicit SftpProvider(QObject *parent = nullptr);
    ~SftpProvider() override;

    // host/port/user + password OR private-key path (keyPath empty => password).
    void setEndpoint(const QString &host, int port, const QString &user,
                     const QString &pass, const QString &keyPath = QString()) {
        m_host = host; m_port = port; m_user = user; m_pass = pass; m_keyPath = keyPath;
    }

    QString providerName() const override { return QStringLiteral("SFTP"); }
    QString rootPath() const override { return QStringLiteral("/"); }

    void list(const QString &path) override;
    void download(const QString &remotePath, const QString &localPath) override;
    void resolveStreamUrl(const QString &remotePath) override;
    void upload(const QString &localPath, const QString &remoteDir) override;
    void makeDir(const QString &remoteDir, const QString &name) override;
    void remove(const QString &remotePath) override;
    void rename(const QString &remotePath, const QString &newName) override;
    // Signal the worker to abort an in-flight download/upload immediately.
    void cancelTransfers() override;

private slots:
    void onWorkerListed(const QString &path, const QVector<CloudEntry> &entries);
    void onWorkerError(const QString &msg);
    void onWorkerDownloadProgress(qint64 done, qint64 total);
    void onWorkerDownloadFinished(const QString &remotePath, const QString &localPath);
    void onWorkerUploadProgress(const QString &localPath, qint64 done, qint64 total);
    void onWorkerUploadFinished(const QString &localPath, const QString &remoteDir);
    void onWorkerOpFinished();

private:
    void ensureWorker();

    QString m_host, m_user, m_pass, m_keyPath;
    int m_port = 22;

    class SftpWorker *m_worker = nullptr;
    class QThread *m_thread = nullptr;
};

#endif // SFTPPROVIDER_H