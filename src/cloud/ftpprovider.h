#ifndef FTPPROVIDER_H
#define FTPPROVIDER_H

#include "cloudprovider.h"
#include <QString>

// Minimal FTP client built directly on QTcpSocket (the classic RFC 959
// control-channel + PASV data-channel protocol) — no QFtp (removed from Qt5
// Network in Qt6) and no external ftp.exe dependency.
//
// All work happens on a dedicated worker thread (FtpWorker) that owns the
// control connection and opens a fresh PASV data connection for every
// transfer/listing, mirroring the SshSession/SshWorker and
// SftpProvider/SftpWorker split so the GUI thread never blocks.
class FtpProvider : public CloudProvider
{
    Q_OBJECT
public:
    explicit FtpProvider(QObject *parent = nullptr);
    ~FtpProvider() override;

    void setEndpoint(const QString &host, int port, const QString &user,
                     const QString &pass) {
        m_host = host; m_port = port; m_user = user; m_pass = pass;
    }

    QString providerName() const override { return QStringLiteral("FTP"); }
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

    QString m_host, m_user, m_pass;
    int m_port = 21;

    class FtpWorker *m_worker = nullptr;
    class QThread *m_thread = nullptr;
};

#endif // FTPPROVIDER_H