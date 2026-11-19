#ifndef RESTPROVIDERS_H
#define RESTPROVIDERS_H

#include "cloudprovider.h"
#include <QString>
#include <QHash>
#include <functional>

class QNetworkAccessManager;

// ---------------------------------------------------------------------------
// Token-based REST cloud providers. Each subclass speaks its service's HTTP
// API using a bearer access token (or, for WebDAV, URL + basic auth). The
// token/credentials are supplied via the connect dialog and stored in the
// instance. All share one QNetworkAccessManager.
// ---------------------------------------------------------------------------

// Common base holding the network manager + auth token and small helpers.
class RestCloudBase : public CloudProvider
{
    Q_OBJECT
public:
    explicit RestCloudBase(QObject *parent = nullptr);

    void setAccessToken(const QString &token) { m_token = token; }
    QString accessToken() const { return m_token; }

protected:
    QNetworkAccessManager *m_net = nullptr;
    QString m_token;

    // Download a resolved absolute URL to a local path, wiring progress/finish.
    // Follows HTTP redirects (needed for Baidu dlink 302) and lets callers add
    // custom headers (e.g. User-Agent: pan.baidu.com).
    void downloadUrl(const QString &url, const QString &remotePath,
                     const QString &localPath,
                     const QList<QPair<QByteArray, QByteArray>> &headers = {});

    // PUT a local file to an absolute URL (used by OneDrive/WebDAV uploads).
    // Streams the file so large uploads don't buffer in memory, and reports
    // uploadProgress()/uploadFinished()/errorOccurred().
    void uploadPut(const QString &url, const QString &localPath,
                   const QString &remoteDir,
                   const QList<QPair<QByteArray, QByteArray>> &headers = {});
};

// ---- OneDrive (Microsoft Graph) ----
class OneDriveProvider : public RestCloudBase
{
    Q_OBJECT
public:
    using RestCloudBase::RestCloudBase;
    QString providerName() const override { return QStringLiteral("OneDrive"); }
    void list(const QString &path) override;
    void download(const QString &remotePath, const QString &localPath) override;
    void resolveStreamUrl(const QString &remotePath) override;
    void upload(const QString &localPath, const QString &remoteDir) override;
    void makeDir(const QString &remoteDir, const QString &name) override;
    void remove(const QString &remotePath) override;
    void rename(const QString &remotePath, const QString &newName) override;
};

// ---- Google Drive (Drive v3) ----
class GoogleDriveProvider : public RestCloudBase
{
    Q_OBJECT
public:
    using RestCloudBase::RestCloudBase;
    QString providerName() const override { return QStringLiteral("Google Drive"); }
    // Google Drive uses folder *ids*; rootPath() is the special id "root".
    QString rootPath() const override { return QStringLiteral("root"); }
    void list(const QString &folderId) override;
    void download(const QString &fileId, const QString &localPath) override;
    void resolveStreamUrl(const QString &fileId) override;
    void upload(const QString &localPath, const QString &remoteDir) override;
    void makeDir(const QString &remoteDir, const QString &name) override;
    void remove(const QString &remotePath) override;
    void rename(const QString &remotePath, const QString &newName) override;
    // The media endpoint needs the bearer token; the StreamProxy injects it.
    QList<QPair<QByteArray, QByteArray>> streamHeaders() const override {
        return {{ "Authorization", ("Bearer " + m_token).toUtf8() }};
    }
};

// ---- Dropbox (v2) ----
class DropboxProvider : public RestCloudBase
{
    Q_OBJECT
public:
    using RestCloudBase::RestCloudBase;
    QString providerName() const override { return QStringLiteral("Dropbox"); }
    void list(const QString &path) override;
    void download(const QString &remotePath, const QString &localPath) override;
    void resolveStreamUrl(const QString &remotePath) override;
    void upload(const QString &localPath, const QString &remoteDir) override;
    void makeDir(const QString &remoteDir, const QString &name) override;
    void remove(const QString &remotePath) override;
    void rename(const QString &remotePath, const QString &newName) override;
};

// ---- Baidu Pan (PCS / OpenAPI) ----
class BaiduPanProvider : public RestCloudBase
{
    Q_OBJECT
public:
    using RestCloudBase::RestCloudBase;
    QString providerName() const override { return QStringLiteral("Baidu Pan"); }
    void list(const QString &path) override;
    void download(const QString &remotePath, const QString &localPath) override;
    void resolveStreamUrl(const QString &remotePath) override;
    // Baidu rejects the dlink unless the User-Agent is exactly "pan.baidu.com"
    // (otherwise: error_code 31326 "user is not authorized"). External players
    // cannot send this, so the StreamProxy adds it on their behalf.
    QList<QPair<QByteArray, QByteArray>> streamHeaders() const override {
        return {{ "User-Agent", "pan.baidu.com" }};
    }
    void upload(const QString &localPath, const QString &remoteDir) override;
    void makeDir(const QString &remoteDir, const QString &name) override;
    void remove(const QString &remotePath) override;
    void rename(const QString &remotePath, const QString &newName) override;
private:
    // Resolve a file's temporary download link (dlink) from its fs_id, then
    // invoke `onDlink(url)`. Baidu needs the fs_id, not the path.
    void resolveDlink(const QString &remotePath,
                      std::function<void(const QString &)> onDlink);
    QHash<QString, QString> m_fsIdByPath;   // path -> fs_id, filled by list()
};

// ---- Generic WebDAV (Nutstore/坚果云, self-hosted, etc.) ----
class WebDavProvider : public RestCloudBase
{
    Q_OBJECT
public:
    explicit WebDavProvider(QObject *parent = nullptr) : RestCloudBase(parent) {}
    QString providerName() const override { return QStringLiteral("WebDAV"); }
    // baseUrl like "https://dav.jianguoyun.com/dav/"; user/password basic auth.
    void setEndpoint(const QString &baseUrl, const QString &user, const QString &pass) {
        m_base = baseUrl; m_user = user; m_pass = pass;
    }
    void list(const QString &path) override;
    void download(const QString &remotePath, const QString &localPath) override;
    void resolveStreamUrl(const QString &remotePath) override;
    void upload(const QString &localPath, const QString &remoteDir) override;
    void makeDir(const QString &remoteDir, const QString &name) override;
    void remove(const QString &remotePath) override;
    void rename(const QString &remotePath, const QString &newName) override;
private:
    QByteArray basicAuthHeader() const;
    QString absUrl(const QString &path) const;
    QString m_base, m_user, m_pass;
};

#endif // RESTPROVIDERS_H