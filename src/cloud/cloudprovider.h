#ifndef CLOUDPROVIDER_H
#define CLOUDPROVIDER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QDateTime>
#include <QList>
#include <QPair>
#include <QByteArray>

// One entry (file or folder) in a cloud drive listing.
struct CloudEntry {
    QString name;        // display name
    QString path;        // provider-specific full path or id
    bool    isDir = false;
    qint64  size = 0;
    QDateTime modified;
    QString  downloadUrl; // optional direct/temporary URL (for streaming)
    QString  fsId;        // Baidu Pan fs_id (needed to resolve the dlink)
};

// Abstract interface every cloud-drive backend implements. All operations are
// asynchronous and report back through signals so the UI thread never blocks.
//
// Supported backends (concrete subclasses):
//   * OneDrive      (Microsoft Graph REST)
//   * Google Drive  (Drive v3 REST)
//   * Dropbox       (Dropbox v2 REST)
//   * Baidu Pan     (PCS / OpenAPI REST)
//   * Aliyun Pan    (OpenAPI REST)
//   * WebDAV        (generic; works for Nutstore/坚果云, self-hosted, etc.)
//
// Authentication is token-based: the user supplies an OAuth access token (or
// WebDAV URL + credentials) via the connect dialog; providers store it and
// attach it to every request. This keeps the app free of baked-in client
// secrets and of the Qt NetworkAuth dependency.
class CloudProvider : public QObject
{
    Q_OBJECT
public:
    explicit CloudProvider(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~CloudProvider() = default;

    // A short human-readable backend name, e.g. "OneDrive".
    virtual QString providerName() const = 0;

    // Root path/id used as the starting point of browsing.
    virtual QString rootPath() const { return QStringLiteral("/"); }

    // List the children of a folder (path or id). Emits listed() or errorOccurred().
    virtual void list(const QString &path) = 0;

    // Download a remote file to a local path. Emits downloadProgress() then
    // downloadFinished() / errorOccurred().
    virtual void download(const QString &remotePath, const QString &localPath) = 0;

    // Resolve a temporary, directly-playable URL for a file (used for video
    // streaming / "边下边播"). Emits streamUrlReady() or errorOccurred().
    // Default: not supported -> callers fall back to download.
    virtual void resolveStreamUrl(const QString &remotePath) {
        Q_UNUSED(remotePath);
        emit errorOccurred(tr("Streaming not supported by this provider."));
    }

    // Headers that MUST accompany a request to the URL reported by
    // streamUrlReady(). External players cannot send custom headers, so the
    // caller routes the stream through the local StreamProxy, which injects
    // these. Empty means the URL is directly playable.
    //
    // Baidu Pan requires "User-Agent: pan.baidu.com"; without it the dlink
    // answers {"error_code":31326,"error_msg":"user is not authorized"}.
    virtual QList<QPair<QByteArray, QByteArray>> streamHeaders() const {
        return {};
    }

    // Upload a local file into a remote folder. Emits uploadFinished().
    virtual void upload(const QString &localPath, const QString &remoteDir) {
        Q_UNUSED(localPath); Q_UNUSED(remoteDir);
        emit errorOccurred(tr("Upload not supported by this provider."));
    }

    // Delete a remote file/folder. Emits operationFinished().
    virtual void remove(const QString &remotePath) {
        Q_UNUSED(remotePath);
        emit errorOccurred(tr("Delete not supported by this provider."));
    }

    // Create a folder. Emits operationFinished().
    virtual void makeDir(const QString &remoteDir, const QString &name) {
        Q_UNUSED(remoteDir); Q_UNUSED(name);
        emit errorOccurred(tr("Create folder not supported by this provider."));
    }

signals:
    void listed(const QString &path, const QVector<CloudEntry> &entries);
    void downloadProgress(qint64 received, qint64 total);
    void downloadFinished(const QString &remotePath, const QString &localPath);
    // Per-file upload progress (local path identifies which file).
    void uploadProgress(const QString &localPath, qint64 sent, qint64 total);
    void uploadFinished(const QString &localPath, const QString &remoteDir);
    void operationFinished();
    void streamUrlReady(const QString &remotePath, const QString &url);
    void errorOccurred(const QString &message);
};

#endif // CLOUDPROVIDER_H