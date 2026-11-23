#include "restproviders.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QXmlStreamReader>
#include <QCryptographicHash>
#include <QTimer>

// ===========================================================================
//  RestCloudBase
// ===========================================================================
RestCloudBase::RestCloudBase(QObject *parent)
    : CloudProvider(parent)
{
    m_net = new QNetworkAccessManager(this);
}

void RestCloudBase::trackReply(QNetworkReply *reply)
{
    if (reply)
        m_activeReplies.append(reply);
}

void RestCloudBase::untrackReply(QNetworkReply *reply)
{
    m_activeReplies.removeAll(reply);
    // Also drop any replies that have already been destroyed (QPointer == null).
    for (int i = m_activeReplies.size() - 1; i >= 0; --i)
        if (m_activeReplies[i].isNull())
            m_activeReplies.removeAt(i);
}

void RestCloudBase::cancelTransfers()
{
    // Abort every in-flight download/upload. abort() triggers each reply's
    // finished handler with an OperationCanceledError, which closes its file
    // and cleans up; the transfer manager has already marked the item Canceled.
    const auto replies = m_activeReplies;   // copy: abort() may mutate the list
    for (const QPointer<QNetworkReply> &r : replies) {
        if (!r.isNull() && r->isRunning())
            r->abort();
    }
    m_activeReplies.clear();
}

void RestCloudBase::downloadUrl(const QString &url, const QString &remotePath,
                                const QString &localPath,
                                const QList<QPair<QByteArray, QByteArray>> &headers)
{
    QNetworkRequest req((QUrl(url)));
    // Follow redirects automatically (Baidu dlink returns a 302).
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    for (const auto &h : headers)
        req.setRawHeader(h.first, h.second);
    if (!m_token.isEmpty() && headers.isEmpty())
        req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());

    QNetworkReply *reply = m_net->get(req);
    trackReply(reply);
    auto *file = new QFile(localPath, reply);
    if (!file->open(QIODevice::WriteOnly)) {
        emit errorOccurred(tr("Cannot write local file: %1").arg(localPath));
        untrackReply(reply);
        reply->deleteLater();
        return;
    }
    connect(reply, &QNetworkReply::readyRead, this, [reply, file]() {
        file->write(reply->readAll());
    });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 r, qint64 t) { emit downloadProgress(r, t); });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, file, remotePath, localPath]() {
        file->write(reply->readAll());
        file->flush();
        file->close();
        untrackReply(reply);
        if (reply->error() != QNetworkReply::NoError) {
            // A user Cancel aborts the reply (OperationCanceledError); remove
            // the partially-written local file and stay silent — the transfer
            // manager already marked the item Canceled.
            if (reply->error() == QNetworkReply::OperationCanceledError)
                QFile::remove(localPath);
            else
                emit errorOccurred(reply->errorString());
        } else {
            emit downloadFinished(remotePath, localPath);
        }
        reply->deleteLater();
    });
}

// PUT a local file to an absolute URL, streaming the file so large uploads do
// not buffer in memory. Reports uploadProgress()/uploadFinished()/errorOccurred.
void RestCloudBase::uploadPut(const QString &url, const QString &localPath,
                              const QString &remoteDir,
                              const QList<QPair<QByteArray, QByteArray>> &headers)
{
    auto *file = new QFile(localPath);
    if (!file->open(QIODevice::ReadOnly)) {
        emit errorOccurred(tr("Cannot read local file: %1").arg(localPath));
        delete file;
        return;
    }
    QNetworkRequest req((QUrl(url)));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::ContentLengthHeader, file->size());
    for (const auto &h : headers)
        req.setRawHeader(h.first, h.second);
    if (!m_token.isEmpty() && headers.isEmpty())
        req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());

    QNetworkReply *reply = m_net->put(req, file);
    trackReply(reply);
    file->setParent(reply);   // the file lives as long as the reply
    connect(reply, &QNetworkReply::uploadProgress, this,
            [this, localPath](qint64 sent, qint64 total) {
        emit uploadProgress(localPath, sent, total);
    });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, localPath, remoteDir]() {
        untrackReply(reply);
        if (reply->error() != QNetworkReply::NoError) {
            if (reply->error() != QNetworkReply::OperationCanceledError)
                emit errorOccurred(reply->errorString());
        } else {
            emit uploadFinished(localPath, remoteDir);
        }
        reply->deleteLater();
    });
}

// ===========================================================================
//  OneDrive — Microsoft Graph
//  List:   GET /me/drive/root/children  or  /me/drive/root:/{path}:/children
//  Content:GET .../content  (302 to a temporary URL)
// ===========================================================================
void OneDriveProvider::list(const QString &path)
{
    QString url;
    if (path.isEmpty() || path == "/" )
        url = "https://graph.microsoft.com/v1.0/me/drive/root/children";
    else
        url = QString("https://graph.microsoft.com/v1.0/me/drive/root:%1:/children")
                  .arg(path.startsWith('/') ? path : "/" + path);

    QNetworkRequest req((QUrl(url)));
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, path]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
            reply->deleteLater();
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QVector<CloudEntry> out;
        for (const QJsonValue &v : doc.object().value("value").toArray()) {
            QJsonObject o = v.toObject();
            CloudEntry e;
            e.name = o.value("name").toString();
            e.isDir = o.contains("folder");
            e.size = o.value("size").toVariant().toLongLong();
            e.modified = QDateTime::fromString(
                o.value("lastModifiedDateTime").toString(), Qt::ISODate);
            QString base = (path.isEmpty() || path == "/") ? "" : path;
            e.path = base + "/" + e.name;
            out.push_back(e);
        }
        emit listed(path, out);
        reply->deleteLater();
    });
}

void OneDriveProvider::download(const QString &remotePath, const QString &localPath)
{
    QString url = QString("https://graph.microsoft.com/v1.0/me/drive/root:%1:/content")
                      .arg(remotePath.startsWith('/') ? remotePath : "/" + remotePath);
    downloadUrl(url, remotePath, localPath,
                {{ "Authorization", ("Bearer " + m_token).toUtf8() }});
}

void OneDriveProvider::resolveStreamUrl(const QString &remotePath)
{
    // Graph returns @microsoft.graph.downloadUrl (a short-lived direct URL).
    QString url = QString("https://graph.microsoft.com/v1.0/me/drive/root:%1")
                      .arg(remotePath.startsWith('/') ? remotePath : "/" + remotePath);
    QNetworkRequest req((QUrl(url)));
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, remotePath]() {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
            QString u = o.value("@microsoft.graph.downloadUrl").toString();
            if (!u.isEmpty()) emit streamUrlReady(remotePath, u);
            else emit errorOccurred(tr("No stream URL available."));
        } else {
            emit errorOccurred(reply->errorString());
        }
        reply->deleteLater();
    });
}

void OneDriveProvider::upload(const QString &localPath, const QString &remoteDir)
{
    // PUT /me/drive/root:/{dir}/{name}:/content  (simple upload, < 250MB).
    const QString name = QFileInfo(localPath).fileName();
    QString dir = remoteDir;
    if (dir == "/" ) dir.clear();
    if (dir.startsWith('/')) dir = dir.mid(1);
    const QString remotePath = dir.isEmpty() ? ("/" + name) : ("/" + dir + "/" + name);
    const QString url =
        QString("https://graph.microsoft.com/v1.0/me/drive/root:%1:/content").arg(remotePath);
    uploadPut(url, localPath, remoteDir,
              {{ "Authorization", ("Bearer " + m_token).toUtf8() }});
}

void OneDriveProvider::makeDir(const QString &remoteDir, const QString &name)
{
    QString parent = remoteDir;
    QString url;
    if (parent.isEmpty() || parent == "/")
        url = "https://graph.microsoft.com/v1.0/me/drive/root/children";
    else
        url = QString("https://graph.microsoft.com/v1.0/me/drive/root:%1:/children")
                  .arg(parent.startsWith('/') ? parent : "/" + parent);
    QNetworkRequest req((QUrl(url)));
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject body;
    body["name"] = name;
    body["folder"] = QJsonObject();
    body["@microsoft.graph.conflictBehavior"] = "rename";
    QNetworkReply *reply = m_net->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) emit errorOccurred(reply->errorString());
        else emit operationFinished();
        reply->deleteLater();
    });
}

void OneDriveProvider::remove(const QString &remotePath)
{
    const QString p = remotePath.startsWith('/') ? remotePath : "/" + remotePath;
    QNetworkRequest req(QUrl(QString("https://graph.microsoft.com/v1.0/me/drive/root:%1").arg(p)));
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    QNetworkReply *reply = m_net->deleteResource(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) emit errorOccurred(reply->errorString());
        else emit operationFinished();
        reply->deleteLater();
    });
}

void OneDriveProvider::rename(const QString &remotePath, const QString &newName)
{
    const QString p = remotePath.startsWith('/') ? remotePath : "/" + remotePath;
    QNetworkRequest req(QUrl(QString("https://graph.microsoft.com/v1.0/me/drive/root:%1").arg(p)));
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject body;
    body["name"] = newName;
    QNetworkReply *reply = m_net->sendCustomRequest(
        req, "PATCH", QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) emit errorOccurred(reply->errorString());
        else emit operationFinished();
        reply->deleteLater();
    });
}

// ===========================================================================
//  Google Drive — Drive v3 (folders addressed by id)
// ===========================================================================
void GoogleDriveProvider::list(const QString &folderId)
{
    QUrl url("https://www.googleapis.com/drive/v3/files");
    QUrlQuery q;
    q.addQueryItem("q", QString("'%1' in parents and trashed=false")
                            .arg(folderId.isEmpty() ? "root" : folderId));
    q.addQueryItem("fields", "files(id,name,mimeType,size,modifiedTime)");
    q.addQueryItem("pageSize", "1000");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, folderId]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
            reply->deleteLater();
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QVector<CloudEntry> out;
        for (const QJsonValue &v : doc.object().value("files").toArray()) {
            QJsonObject o = v.toObject();
            CloudEntry e;
            e.name = o.value("name").toString();
            e.path = o.value("id").toString();   // id is the "path" for Drive
            e.isDir = o.value("mimeType").toString()
                          == "application/vnd.google-apps.folder";
            e.size = o.value("size").toVariant().toLongLong();
            e.modified = QDateTime::fromString(
                o.value("modifiedTime").toString(), Qt::ISODate);
            out.push_back(e);
        }
        emit listed(folderId, out);
        reply->deleteLater();
    });
}

void GoogleDriveProvider::download(const QString &fileId, const QString &localPath)
{
    QString url = QString("https://www.googleapis.com/drive/v3/files/%1?alt=media")
                      .arg(fileId);
    downloadUrl(url, fileId, localPath,
                {{ "Authorization", ("Bearer " + m_token).toUtf8() }});
}

void GoogleDriveProvider::resolveStreamUrl(const QString &fileId)
{
    // The media endpoint only works with an Authorization header. Players
    // cannot send one, so the caller routes this URL through the local
    // StreamProxy, which injects streamHeaders() on every request.
    emit streamUrlReady(fileId,
        QString("https://www.googleapis.com/drive/v3/files/%1?alt=media").arg(fileId));
}

void GoogleDriveProvider::upload(const QString &localPath, const QString &remoteDir)
{
    // Drive multipart upload: a JSON metadata part + the file bytes. The parent
    // folder id comes in via remoteDir (Drive addresses folders by id).
    const QString name = QFileInfo(localPath).fileName();
    QFile f(localPath);
    if (!f.open(QIODevice::ReadOnly)) {
        emit errorOccurred(tr("Cannot read local file: %1").arg(localPath));
        return;
    }
    const QByteArray data = f.readAll();
    f.close();

    const QByteArray boundary = "----TerminalPPBoundary7MA4YWxkTrZu0gW";
    QJsonObject meta;
    meta["name"] = name;
    if (!remoteDir.isEmpty() && remoteDir != "root")
        meta["parents"] = QJsonArray{ remoteDir };

    QByteArray body;
    body += "--" + boundary + "\r\n";
    body += "Content-Type: application/json; charset=UTF-8\r\n\r\n";
    body += QJsonDocument(meta).toJson(QJsonDocument::Compact) + "\r\n";
    body += "--" + boundary + "\r\n";
    body += "Content-Type: application/octet-stream\r\n\r\n";
    body += data + "\r\n";
    body += "--" + boundary + "--\r\n";

    QNetworkRequest req(QUrl("https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart"));
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    req.setRawHeader("Content-Type", "multipart/related; boundary=" + boundary);
    QNetworkReply *reply = m_net->post(req, body);
    connect(reply, &QNetworkReply::uploadProgress, this,
            [this, localPath](qint64 s, qint64 t) { emit uploadProgress(localPath, s, t); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, localPath, remoteDir]() {
        if (reply->error() != QNetworkReply::NoError) emit errorOccurred(reply->errorString());
        else emit uploadFinished(localPath, remoteDir);
        reply->deleteLater();
    });
}

void GoogleDriveProvider::makeDir(const QString &remoteDir, const QString &name)
{
    QNetworkRequest req(QUrl("https://www.googleapis.com/drive/v3/files"));
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject body;
    body["name"] = name;
    body["mimeType"] = "application/vnd.google-apps.folder";
    if (!remoteDir.isEmpty() && remoteDir != "root")
        body["parents"] = QJsonArray{ remoteDir };
    QNetworkReply *reply = m_net->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) emit errorOccurred(reply->errorString());
        else emit operationFinished();
        reply->deleteLater();
    });
}

void GoogleDriveProvider::remove(const QString &remotePath)
{
    // Drive "delete" is permanent; use trash update instead (safer default)
    // would require an extra call, so we issue a real delete as requested.
    QNetworkRequest req(QUrl(QString("https://www.googleapis.com/drive/v3/files/%1").arg(remotePath)));
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    QNetworkReply *reply = m_net->deleteResource(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) emit errorOccurred(reply->errorString());
        else emit operationFinished();
        reply->deleteLater();
    });
}

void GoogleDriveProvider::rename(const QString &remotePath, const QString &newName)
{
    QNetworkRequest req(QUrl(QString("https://www.googleapis.com/drive/v3/files/%1").arg(remotePath)));
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject body;
    body["name"] = newName;
    QNetworkReply *reply = m_net->sendCustomRequest(
        req, "PATCH", QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) emit errorOccurred(reply->errorString());
        else emit operationFinished();
        reply->deleteLater();
    });
}

// ===========================================================================
//  Dropbox — v2 RPC endpoints
// ===========================================================================
void DropboxProvider::list(const QString &path)
{
    QNetworkRequest req(QUrl("https://api.dropboxapi.com/2/files/list_folder"));
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject body;
    body["path"] = (path == "/" ? "" : path);
    body["recursive"] = false;
    QNetworkReply *reply = m_net->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, path]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
            reply->deleteLater();
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QVector<CloudEntry> out;
        for (const QJsonValue &v : doc.object().value("entries").toArray()) {
            QJsonObject o = v.toObject();
            CloudEntry e;
            e.name = o.value("name").toString();
            e.path = o.value("path_display").toString();
            e.isDir = o.value(".tag").toString() == "folder";
            e.size = o.value("size").toVariant().toLongLong();
            e.modified = QDateTime::fromString(
                o.value("server_modified").toString(), Qt::ISODate);
            out.push_back(e);
        }
        emit listed(path, out);
        reply->deleteLater();
    });
}

void DropboxProvider::download(const QString &remotePath, const QString &localPath)
{
    QNetworkRequest req(QUrl("https://content.dropboxapi.com/2/files/download"));
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    QJsonObject arg; arg["path"] = remotePath;
    req.setRawHeader("Dropbox-API-Arg", QJsonDocument(arg).toJson(QJsonDocument::Compact));

    QNetworkReply *reply = m_net->post(req, QByteArray());
    auto *file = new QFile(localPath, reply);
    if (!file->open(QIODevice::WriteOnly)) {
        emit errorOccurred(tr("Cannot write local file: %1").arg(localPath));
        reply->deleteLater();
        return;
    }
    connect(reply, &QNetworkReply::readyRead, this, [reply, file]() { file->write(reply->readAll()); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 r, qint64 t) { emit downloadProgress(r, t); });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, file, remotePath, localPath]() {
        file->write(reply->readAll()); file->flush(); file->close();
        if (reply->error() != QNetworkReply::NoError) emit errorOccurred(reply->errorString());
        else emit downloadFinished(remotePath, localPath);
        reply->deleteLater();
    });
}

void DropboxProvider::resolveStreamUrl(const QString &remotePath)
{
    QNetworkRequest req(QUrl("https://api.dropboxapi.com/2/files/get_temporary_link"));
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject body; body["path"] = remotePath;
    QNetworkReply *reply = m_net->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, remotePath]() {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
            QString u = o.value("link").toString();
            if (!u.isEmpty()) emit streamUrlReady(remotePath, u);
            else emit errorOccurred(tr("No stream URL available."));
        } else emit errorOccurred(reply->errorString());
        reply->deleteLater();
    });
}

void DropboxProvider::upload(const QString &localPath, const QString &remoteDir)
{
    const QString name = QFileInfo(localPath).fileName();
    QString dir = remoteDir;
    if (dir == "/") dir.clear();
    const QString remotePath = dir.isEmpty() ? ("/" + name)
                                             : (dir.endsWith('/') ? dir + name : dir + "/" + name);
    auto *file = new QFile(localPath);
    if (!file->open(QIODevice::ReadOnly)) {
        emit errorOccurred(tr("Cannot read local file: %1").arg(localPath));
        delete file;
        return;
    }
    QNetworkRequest req(QUrl("https://content.dropboxapi.com/2/files/upload"));
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    QJsonObject arg;
    arg["path"] = remotePath;
    arg["mode"] = "overwrite";
    arg["mute"] = true;
    req.setRawHeader("Dropbox-API-Arg", QJsonDocument(arg).toJson(QJsonDocument::Compact));
    req.setRawHeader("Content-Type", "application/octet-stream");
    req.setHeader(QNetworkRequest::ContentLengthHeader, file->size());
    QNetworkReply *reply = m_net->post(req, file);
    file->setParent(reply);
    connect(reply, &QNetworkReply::uploadProgress, this,
            [this, localPath](qint64 s, qint64 t) { emit uploadProgress(localPath, s, t); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, localPath, remoteDir]() {
        if (reply->error() != QNetworkReply::NoError) emit errorOccurred(reply->errorString());
        else emit uploadFinished(localPath, remoteDir);
        reply->deleteLater();
    });
}

void DropboxProvider::makeDir(const QString &remoteDir, const QString &name)
{
    QString dir = remoteDir;
    if (dir == "/") dir.clear();
    const QString path = dir.isEmpty() ? ("/" + name)
                                       : (dir.endsWith('/') ? dir + name : dir + "/" + name);
    QNetworkRequest req(QUrl("https://api.dropboxapi.com/2/files/create_folder_v2"));
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject body; body["path"] = path;
    QNetworkReply *reply = m_net->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) emit errorOccurred(reply->errorString());
        else emit operationFinished();
        reply->deleteLater();
    });
}

void DropboxProvider::remove(const QString &remotePath)
{
    QNetworkRequest req(QUrl("https://api.dropboxapi.com/2/files/delete_v2"));
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject body; body["path"] = remotePath;
    QNetworkReply *reply = m_net->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) emit errorOccurred(reply->errorString());
        else emit operationFinished();
        reply->deleteLater();
    });
}

void DropboxProvider::rename(const QString &remotePath, const QString &newName)
{
    // Move the file within its own parent folder, giving it a new name.
    QString parent = remotePath;
    int idx = parent.lastIndexOf('/');
    parent = (idx <= 0) ? "" : parent.left(idx);
    const QString newPath = parent + "/" + newName;

    QNetworkRequest req(QUrl("https://api.dropboxapi.com/2/files/move_v2"));
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject body;
    body["from_path"] = remotePath;
    body["to_path"] = newPath;
    QNetworkReply *reply = m_net->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) emit errorOccurred(reply->errorString());
        else emit operationFinished();
        reply->deleteLater();
    });
}

// ===========================================================================
//  Baidu Pan — PCS/OpenAPI (access_token as query param)
// ===========================================================================
void BaiduPanProvider::list(const QString &path)
{
    QUrl url("https://pan.baidu.com/rest/2.0/xpan/file");
    QUrlQuery q;
    q.addQueryItem("method", "list");
    q.addQueryItem("access_token", m_token);
    q.addQueryItem("dir", path.isEmpty() ? "/" : path);
    q.addQueryItem("web", "1");
    url.setQuery(q);
    QNetworkReply *reply = m_net->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, path]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
            reply->deleteLater();
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QVector<CloudEntry> out;
        for (const QJsonValue &v : doc.object().value("list").toArray()) {
            QJsonObject o = v.toObject();
            CloudEntry e;
            e.name = o.value("server_filename").toString();
            e.path = o.value("path").toString();
            e.isDir = o.value("isdir").toInt() == 1;
            e.size = o.value("size").toVariant().toLongLong();
            e.modified = QDateTime::fromSecsSinceEpoch(
                o.value("server_mtime").toVariant().toLongLong());
            e.fsId = QString::number(o.value("fs_id").toVariant().toLongLong());
            if (!e.isDir)
                m_fsIdByPath.insert(e.path, e.fsId);   // cache for dlink resolution
            out.push_back(e);
        }
        emit listed(path, out);
        reply->deleteLater();
    });
}

// Look up a single file's fs_id by its path and cache it.
//
// Baidu has no "metadata by path" call that returns an fs_id directly, so we
// list the file's parent directory (which does return fs_id for every entry)
// and cache every file we see. This is what makes a download immediately after
// an upload work: the freshly created file is not in the cache yet, and this
// silently repopulates it instead of failing with "fs_id unknown".
void BaiduPanProvider::fetchFsIdByPath(const QString &remotePath,
                                       std::function<void(bool)> done)
{
    // Parent directory of remotePath ("/a/b/c.txt" -> "/a/b").
    QString dir = remotePath;
    const int slash = dir.lastIndexOf('/');
    dir = (slash > 0) ? dir.left(slash) : QStringLiteral("/");

    QUrl url("https://pan.baidu.com/rest/2.0/xpan/file");
    QUrlQuery q;
    q.addQueryItem("method", "list");
    q.addQueryItem("access_token", m_token);
    q.addQueryItem("dir", dir);
    q.addQueryItem("web", "1");
    url.setQuery(q);

    QNetworkReply *reply = m_net->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, remotePath, done]() {
        bool found = false;
        if (reply->error() == QNetworkReply::NoError) {
            // The file name we are looking for, used as a fallback match when
            // the full paths differ only by normalisation (duplicated or
            // missing slashes, which Baidu rewrites server-side).
            QString wantName = remotePath;
            const int ws = wantName.lastIndexOf('/');
            if (ws >= 0)
                wantName = wantName.mid(ws + 1);

            const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            for (const QJsonValue &v : doc.object().value("list").toArray()) {
                const QJsonObject o = v.toObject();
                if (o.value("isdir").toInt() == 1)
                    continue;
                const QString p = o.value("path").toString();
                const QString id =
                    QString::number(o.value("fs_id").toVariant().toLongLong());
                if (p.isEmpty() || id.isEmpty())
                    continue;
                m_fsIdByPath.insert(p, id);         // refresh the whole folder
                if (p == remotePath) {
                    found = true;
                } else if (!found
                           && o.value("server_filename").toString() == wantName) {
                    // Same file, different path spelling: register it under the
                    // path the caller actually asked for so the retry hits.
                    m_fsIdByPath.insert(remotePath, id);
                    found = true;
                }
            }
        }
        reply->deleteLater();
        done(found);
    });
}

// Resolve the temporary dlink for a file via the xpan filemetas API, then
// call onDlink with the final, token-appended URL.
void BaiduPanProvider::resolveDlink(const QString &remotePath,
                                    std::function<void(const QString &)> onDlink,
                                    int retriesLeft)
{
    const QString fsId = m_fsIdByPath.value(remotePath);
    if (fsId.isEmpty()) {
        // Not cached — typically a file that was just uploaded in this session.
        // Re-list its parent folder to recover the fs_id, then retry, instead
        // of asking the user to refresh manually.
        if (retriesLeft > 0) {
            fetchFsIdByPath(remotePath, [this, remotePath, onDlink, retriesLeft](bool ok) {
                if (ok) {
                    resolveDlink(remotePath, onDlink, retriesLeft - 1);
                } else {
                    // Baidu commits an upload asynchronously, so the file can
                    // still be missing from the listing for a moment. Retry
                    // once more after a short delay before giving up.
                    if (retriesLeft > 1) {
                        QTimer::singleShot(1200, this,
                            [this, remotePath, onDlink, retriesLeft]() {
                                resolveDlink(remotePath, onDlink, retriesLeft - 1);
                            });
                    } else {
                        emit errorOccurred(
                            tr("Baidu Pan: could not resolve the download link for "
                               "\"%1\" (fs_id not found). The file may still be "
                               "finalising on the server — refresh the folder and "
                               "try again.").arg(remotePath));
                    }
                }
            });
            return;
        }
        emit errorOccurred(tr("Baidu Pan: fs_id unknown for this file; "
                              "re-open the folder and try again."));
        return;
    }
    QUrl url("https://pan.baidu.com/rest/2.0/xpan/multimedia");
    QUrlQuery q;
    q.addQueryItem("method", "filemetas");
    q.addQueryItem("access_token", m_token);
    q.addQueryItem("fsids", "[" + fsId + "]");
    q.addQueryItem("dlink", "1");
    url.setQuery(q);
    QNetworkReply *reply = m_net->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, onDlink]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
            reply->deleteLater();
            return;
        }
        QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        QString dlink;
        const QJsonArray list = o.value("list").toArray();
        if (!list.isEmpty())
            dlink = list.first().toObject().value("dlink").toString();
        reply->deleteLater();
        if (dlink.isEmpty()) {
            emit errorOccurred(tr("Baidu Pan: no download link (dlink) returned. "
                "Configure your Baidu app token with the download scope: in the "
                "Baidu developer console enable the \"netdisk\" (download) "
                "permission and re-authorize so the access_token is issued with "
                "scope=basic,netdisk."));
            return;
        }
        // The dlink itself needs the access_token appended to be fetchable.
        QUrl d(dlink);
        QUrlQuery dq(d);
        dq.addQueryItem("access_token", m_token);
        d.setQuery(dq);
        onDlink(d.toString(QUrl::FullyEncoded));
    });
}

void BaiduPanProvider::download(const QString &remotePath, const QString &localPath)
{
    resolveDlink(remotePath, [this, remotePath, localPath](const QString &url) {
        // Baidu requires a desktop/browser User-Agent for dlink downloads.
        downloadUrl(url, remotePath, localPath,
                    {{ "User-Agent", "pan.baidu.com" }});
    });
}

void BaiduPanProvider::resolveStreamUrl(const QString &remotePath)
{
    // Hand the resolved dlink to the caller. The dlink is NOT directly usable
    // by a browser/player: Baidu rejects any request whose User-Agent is not
    // "pan.baidu.com" with error_code 31326 ("user is not authorized").
    // The caller therefore republishes this URL through the local StreamProxy,
    // which injects streamHeaders() (the required User-Agent) upstream.
    resolveDlink(remotePath, [this, remotePath](const QString &url) {
        emit streamUrlReady(remotePath, url);
    });
}

void BaiduPanProvider::upload(const QString &localPath, const QString &remoteDir)
{
    // Baidu Pan uses the xpan 3-step upload: precreate -> superfile2 -> create.
    // The old PCS "method=upload" endpoint returns "forbidden" for modern
    // OpenAPI tokens, so we implement the supported flow here (single block;
    // works for files up to the per-block size, which is ample for typical use).
    const QString name = QFileInfo(localPath).fileName();
    QString dir = remoteDir.isEmpty() ? "/" : remoteDir;
    if (!dir.endsWith('/')) dir += '/';
    QString remotePath = dir + name;
    // Collapse any accidental double slashes (e.g. root "/" + "/folder/file"
    // produced "//folder/file"), which Baidu's xpan API rejects as an invalid
    // path and reports via an empty uploadid at precreate.
    while (remotePath.contains("//"))
        remotePath.replace("//", "/");

    QFile f(localPath);
    if (!f.open(QIODevice::ReadOnly)) {
        emit errorOccurred(tr("Cannot read local file: %1").arg(localPath));
        return;
    }
    const QByteArray data = f.readAll();
    f.close();
    const qint64 size = data.size();
    const QByteArray md5 = QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex();

    // ---- step 1: precreate ----
    QUrl pre("https://pan.baidu.com/rest/2.0/xpan/file");
    QUrlQuery pq; pq.addQueryItem("method", "precreate");
    pq.addQueryItem("access_token", m_token);
    pre.setQuery(pq);
    QUrlQuery preForm;
    preForm.addQueryItem("path", remotePath);
    preForm.addQueryItem("size", QString::number(size));
    preForm.addQueryItem("isdir", "0");
    preForm.addQueryItem("autoinit", "1");
    preForm.addQueryItem("rtype", "3");
    preForm.addQueryItem("block_list", QString("[\"%1\"]").arg(QString::fromUtf8(md5)));
    QNetworkRequest preReq(pre);
    preReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QNetworkReply *preReply = m_net->post(preReq, preForm.toString(QUrl::FullyEncoded).toUtf8());
    connect(preReply, &QNetworkReply::finished, this,
            [this, preReply, data, md5, remotePath, localPath, remoteDir, size]() {
        if (preReply->error() != QNetworkReply::NoError) {
            emit errorOccurred(preReply->errorString());
            preReply->deleteLater();
            return;
        }
        const QByteArray preRaw = preReply->readAll();
        QJsonObject o = QJsonDocument::fromJson(preRaw).object();
        preReply->deleteLater();
        const QString uploadid = o.value("uploadid").toString();
        const int preErrno = o.value("errno").toInt(0);
        if (uploadid.isEmpty()) {
            // Surface Baidu's actual errno so the failure is diagnosable.
            // Common causes:
            //   -6     : access_token invalid / missing the "netdisk" scope
            //   2 / 31034 : parameter error or request-rate limit
            //   111    : token expired
            // errno == 0 but empty uploadid can also mean the file already
            // exists identically (rapid-upload) — in that case Baidu returns
            // return_type==2 and the file is already there, so treat it as a
            // successful upload rather than an error.
            if (preErrno == 0 && o.value("return_type").toInt() == 2) {
                emit uploadFinished(localPath, remoteDir);
                return;
            }
            emit errorOccurred(tr("Baidu Pan upload precreate failed (errno=%1). "
                "Check that the access_token has the \"netdisk\" scope and that "
                "the target folder exists and your app allows uploads.")
                .arg(preErrno));
            return;
        }

        // ---- step 2: superfile2 (upload the single block) ----
        QUrl sf("https://d.pcs.baidu.com/rest/2.0/pcs/superfile2");
        QUrlQuery sq;
        sq.addQueryItem("method", "upload");
        sq.addQueryItem("access_token", m_token);
        sq.addQueryItem("type", "tmpfile");
        sq.addQueryItem("path", remotePath);
        sq.addQueryItem("uploadid", uploadid);
        sq.addQueryItem("partseq", "0");
        sf.setQuery(sq);

        const QByteArray boundary = "----TerminalPPBaiduBlock";
        QByteArray body;
        body += "--" + boundary + "\r\n";
        body += "Content-Disposition: form-data; name=\"file\"; filename=\"file\"\r\n";
        body += "Content-Type: application/octet-stream\r\n\r\n";
        body += data + "\r\n";
        body += "--" + boundary + "--\r\n";

        QNetworkRequest sfReq(sf);
        sfReq.setRawHeader("User-Agent", "pan.baidu.com");
        sfReq.setRawHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
        QNetworkReply *sfReply = m_net->post(sfReq, body);
        connect(sfReply, &QNetworkReply::uploadProgress, this,
                [this, localPath](qint64 s, qint64 t) { emit uploadProgress(localPath, s, t); });
        connect(sfReply, &QNetworkReply::finished, this,
                [this, sfReply, md5, remotePath, localPath, remoteDir, uploadid, size]() {
            if (sfReply->error() != QNetworkReply::NoError) {
                emit errorOccurred(sfReply->errorString());
                sfReply->deleteLater();
                return;
            }
            sfReply->deleteLater();

            // ---- step 3: create (commit the file) ----
            QUrl cr("https://pan.baidu.com/rest/2.0/xpan/file");
            QUrlQuery cq; cq.addQueryItem("method", "create");
            cq.addQueryItem("access_token", m_token);
            cr.setQuery(cq);
            QUrlQuery crForm;
            crForm.addQueryItem("path", remotePath);
            crForm.addQueryItem("size", QString::number(size));
            crForm.addQueryItem("isdir", "0");
            crForm.addQueryItem("rtype", "3");
            crForm.addQueryItem("uploadid", uploadid);
            crForm.addQueryItem("block_list", QString("[\"%1\"]").arg(QString::fromUtf8(md5)));
            QNetworkRequest crReq(cr);
            crReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
            QNetworkReply *crReply = m_net->post(crReq, crForm.toString(QUrl::FullyEncoded).toUtf8());
            connect(crReply, &QNetworkReply::finished, this,
                    [this, crReply, localPath, remoteDir, remotePath]() {
                QJsonObject r = QJsonDocument::fromJson(crReply->readAll()).object();
                const int errno_ = r.value("errno").toInt(0);
                if (crReply->error() != QNetworkReply::NoError)
                    emit errorOccurred(crReply->errorString());
                else if (errno_ != 0)
                    emit errorOccurred(tr("Baidu Pan create failed (errno=%1).").arg(errno_));
                else {
                    // Cache the new file's fs_id straight from the create
                    // response so an immediate download works without first
                    // refreshing the folder (this is what previously failed
                    // with "fs_id unknown / no dlink").
                    const qint64 newFsId = r.value("fs_id").toVariant().toLongLong();
                    if (newFsId != 0) {
                        const QString p = r.value("path").toString().isEmpty()
                                              ? remotePath
                                              : r.value("path").toString();
                        m_fsIdByPath.insert(p, QString::number(newFsId));
                    }
                    emit uploadFinished(localPath, remoteDir);
                }
                crReply->deleteLater();
            });
        });
    });
}

void BaiduPanProvider::makeDir(const QString &remoteDir, const QString &name)
{
    QString dir = remoteDir.isEmpty() ? "/" : remoteDir;
    if (!dir.endsWith('/')) dir += '/';
    const QString path = dir + name;

    QUrl url("https://pan.baidu.com/rest/2.0/xpan/file");
    QUrlQuery q;
    q.addQueryItem("method", "create");
    q.addQueryItem("access_token", m_token);
    url.setQuery(q);

    QUrlQuery form;
    form.addQueryItem("path", path);
    form.addQueryItem("isdir", "1");
    form.addQueryItem("rtype", "0");

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  "application/x-www-form-urlencoded");
    QNetworkReply *reply = m_net->post(req, form.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) emit errorOccurred(reply->errorString());
        else emit operationFinished();
        reply->deleteLater();
    });
}

void BaiduPanProvider::remove(const QString &remotePath)
{
    QUrl url("https://pan.baidu.com/rest/2.0/xpan/file");
    QUrlQuery q;
    q.addQueryItem("method", "filemanager");
    q.addQueryItem("access_token", m_token);
    q.addQueryItem("opera", "delete");
    url.setQuery(q);

    QUrlQuery form;
    form.addQueryItem("async", "0");
    form.addQueryItem("filelist", QString("[\"%1\"]").arg(remotePath));

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QNetworkReply *reply = m_net->post(req, form.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) emit errorOccurred(reply->errorString());
        else emit operationFinished();
        reply->deleteLater();
    });
}

void BaiduPanProvider::rename(const QString &remotePath, const QString &newName)
{
    QUrl url("https://pan.baidu.com/rest/2.0/xpan/file");
    QUrlQuery q;
    q.addQueryItem("method", "filemanager");
    q.addQueryItem("access_token", m_token);
    q.addQueryItem("opera", "rename");
    url.setQuery(q);

    QJsonObject item;
    item["path"] = remotePath;
    item["newname"] = newName;
    QJsonArray arr; arr.append(item);

    QUrlQuery form;
    form.addQueryItem("async", "0");
    form.addQueryItem("filelist",
        QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QNetworkReply *reply = m_net->post(req, form.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) emit errorOccurred(reply->errorString());
        else emit operationFinished();
        reply->deleteLater();
    });
}

// ===========================================================================
//  WebDAV — PROPFIND for listing, GET for download
// ===========================================================================
QByteArray WebDavProvider::basicAuthHeader() const
{
    QByteArray cred = (m_user + ":" + m_pass).toUtf8().toBase64();
    return "Basic " + cred;
}

QString WebDavProvider::absUrl(const QString &path) const
{
    QString base = m_base;
    if (base.endsWith('/')) base.chop(1);
    QString p = path;
    if (!p.startsWith('/')) p = "/" + p;
    return base + p;
}

void WebDavProvider::list(const QString &path)
{
    QNetworkRequest req((QUrl(absUrl(path.isEmpty() ? "/" : path))));
    req.setRawHeader("Authorization", basicAuthHeader());
    req.setRawHeader("Depth", "1");
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/xml");

    const QByteArray body =
        "<?xml version=\"1.0\"?><d:propfind xmlns:d=\"DAV:\"><d:prop>"
        "<d:displayname/><d:getcontentlength/><d:getlastmodified/>"
        "<d:resourcetype/></d:prop></d:propfind>";
    QNetworkReply *reply = m_net->sendCustomRequest(req, "PROPFIND", body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, path]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
            reply->deleteLater();
            return;
        }
        QVector<CloudEntry> out;
        QXmlStreamReader xml(reply->readAll());
        CloudEntry cur; bool inResp = false; QString href;
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement()) {
                const QString n = xml.name().toString();
                if (n == "response") { inResp = true; cur = CloudEntry(); href.clear(); }
                else if (inResp && n == "href") href = xml.readElementText();
                else if (inResp && n == "displayname") cur.name = xml.readElementText();
                else if (inResp && n == "getcontentlength") cur.size = xml.readElementText().toLongLong();
                else if (inResp && n == "getlastmodified")
                    cur.modified = QDateTime::fromString(xml.readElementText(), Qt::RFC2822Date);
                else if (inResp && n == "collection") cur.isDir = true;
            } else if (xml.isEndElement() && xml.name().toString() == "response") {
                // Derive path from href; skip the folder itself.
                QString decoded = QUrl::fromPercentEncoding(href.toUtf8());
                if (cur.name.isEmpty())
                    cur.name = decoded.section('/', -2, cur.isDir ? -2 : -1,
                                               QString::SectionSkipEmpty);
                QString rel = decoded;
                int davIdx = rel.indexOf(QUrl(m_base).path());
                if (davIdx >= 0) rel = rel.mid(davIdx + QUrl(m_base).path().length());
                cur.path = rel.isEmpty() ? "/" : rel;
                // Skip the container entry (same path as requested).
                QString reqPath = path.isEmpty() ? "/" : path;
                if (cur.path != reqPath && !cur.name.isEmpty())
                    out.push_back(cur);
                inResp = false;
            }
        }
        emit listed(path, out);
        reply->deleteLater();
    });
}

void WebDavProvider::download(const QString &remotePath, const QString &localPath)
{
    downloadUrl(absUrl(remotePath), remotePath, localPath,
                {{ "Authorization", basicAuthHeader() }});
}

void WebDavProvider::resolveStreamUrl(const QString &remotePath)
{
    // WebDAV GET with embedded credentials works for many players.
    QUrl u(absUrl(remotePath));
    u.setUserName(m_user);
    u.setPassword(m_pass);
    emit streamUrlReady(remotePath, u.toString(QUrl::FullyEncoded));
}

void WebDavProvider::upload(const QString &localPath, const QString &remoteDir)
{
    const QString name = QFileInfo(localPath).fileName();
    QString dir = remoteDir;
    if (dir.isEmpty()) dir = "/";
    if (!dir.endsWith('/')) dir += '/';
    uploadPut(absUrl(dir + name), localPath, remoteDir,
              {{ "Authorization", basicAuthHeader() }});
}

void WebDavProvider::makeDir(const QString &remoteDir, const QString &name)
{
    QString dir = remoteDir;
    if (dir.isEmpty()) dir = "/";
    if (!dir.endsWith('/')) dir += '/';
    QNetworkRequest req((QUrl(absUrl(dir + name))));
    req.setRawHeader("Authorization", basicAuthHeader());
    QNetworkReply *reply = m_net->sendCustomRequest(req, "MKCOL");
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) emit errorOccurred(reply->errorString());
        else emit operationFinished();
        reply->deleteLater();
    });
}

void WebDavProvider::remove(const QString &remotePath)
{
    QNetworkRequest req((QUrl(absUrl(remotePath))));
    req.setRawHeader("Authorization", basicAuthHeader());
    QNetworkReply *reply = m_net->sendCustomRequest(req, "DELETE");
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) emit errorOccurred(reply->errorString());
        else emit operationFinished();
        reply->deleteLater();
    });
}

void WebDavProvider::rename(const QString &remotePath, const QString &newName)
{
    // WebDAV rename = MOVE within the same parent folder.
    QString parent = remotePath;
    if (parent.endsWith('/')) parent.chop(1);
    int idx = parent.lastIndexOf('/');
    parent = (idx <= 0) ? "/" : parent.left(idx + 1);
    const QString destUrl = absUrl(parent + newName);

    QNetworkRequest req((QUrl(absUrl(remotePath))));
    req.setRawHeader("Authorization", basicAuthHeader());
    req.setRawHeader("Destination", destUrl.toUtf8());
    req.setRawHeader("Overwrite", "F");
    QNetworkReply *reply = m_net->sendCustomRequest(req, "MOVE");
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) emit errorOccurred(reply->errorString());
        else emit operationFinished();
        reply->deleteLater();
    });
}
