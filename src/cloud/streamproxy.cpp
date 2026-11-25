#include "streamproxy.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QPointer>
#include <QRandomGenerator>
#include <QStringList>
#include <QDebug>

StreamProxy *StreamProxy::instance()
{
    static StreamProxy inst;
    return &inst;
}

StreamProxy::StreamProxy(QObject *parent)
    : QObject(parent)
{
    m_net = new QNetworkAccessManager(this);
}

bool StreamProxy::ensureListening()
{
    if (m_server && m_server->isListening())
        return true;
    if (!m_server) {
        m_server = new QTcpServer(this);
        connect(m_server, &QTcpServer::newConnection,
                this, &StreamProxy::onNewConnection);
    }
    // Bind to loopback only (never expose the relay to the network) on a
    // free ephemeral port.
    return m_server->listen(QHostAddress::LocalHost, 0);
}

QString StreamProxy::publish(const QString &upstreamUrl,
                             const QList<QPair<QByteArray, QByteArray>> &headers,
                             const QString &fileName)
{
    if (upstreamUrl.isEmpty() || !ensureListening())
        return QString();

    const QString token = QString::number(++m_counter) + "-"
                          + QString::number(QRandomGenerator::global()->generate(), 16);
    Entry e;
    e.url = upstreamUrl;
    e.headers = headers;
    m_entries.insert(token, e);

    // Keep the original file name in the path so players pick the right
    // demuxer from the extension.
    QString name = fileName.isEmpty() ? QStringLiteral("stream") : fileName;
    name = QString::fromUtf8(QUrl::toPercentEncoding(name));

    return QStringLiteral("http://127.0.0.1:%1/%2/%3")
        .arg(m_server->serverPort()).arg(token, name);
}

void StreamProxy::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *sock = m_server->nextPendingConnection();
        auto *buf = new QByteArray();
        connect(sock, &QTcpSocket::readyRead, this, [this, sock, buf]() {
            buf->append(sock->readAll());
            // Wait for the end of the request headers.
            if (!buf->contains("\r\n\r\n"))
                return;
            const QByteArray req = *buf;
            buf->clear();
            handleRequest(sock, req);
        });
        connect(sock, &QTcpSocket::disconnected, this, [sock, buf]() {
            delete buf;
            sock->deleteLater();
        });
    }
}

void StreamProxy::handleRequest(QTcpSocket *sock, const QByteArray &requestData)
{
    // --- Parse the request line and the headers we care about ---
    const QList<QByteArray> lines = requestData.split('\n');
    if (lines.isEmpty()) {
        sock->disconnectFromHost();
        return;
    }
    const QList<QByteArray> reqLine = lines.first().trimmed().split(' ');
    if (reqLine.size() < 2) {
        sock->disconnectFromHost();
        return;
    }
    const QByteArray method = reqLine.at(0);
    const QByteArray path = reqLine.at(1);

    QByteArray clientRange;
    for (int i = 1; i < lines.size(); ++i) {
        const QByteArray l = lines.at(i).trimmed();
        if (l.isEmpty())
            break;
        if (l.toLower().startsWith("range:"))
            clientRange = l.mid(6).trimmed();
    }

    // Path format: /<token>/<name>
    const QString p = QString::fromUtf8(path);
    const QStringList parts = p.split('/', Qt::SkipEmptyParts);
    const QString token = parts.isEmpty() ? QString() : parts.first();
    if (!m_entries.contains(token)) {
        sock->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n"
                    "Connection: close\r\n\r\n");
        sock->disconnectFromHost();
        return;
    }
    const Entry entry = m_entries.value(token);

    // --- Issue the upstream request with the required headers injected ---
    QNetworkRequest req((QUrl(entry.url)));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    for (const auto &h : entry.headers)
        req.setRawHeader(h.first, h.second);
    // Forward the player's Range so seeking / partial playback works.
    if (!clientRange.isEmpty())
        req.setRawHeader("Range", clientRange);

    QNetworkReply *reply = (method == "HEAD") ? m_net->head(req) : m_net->get(req);

    QPointer<QTcpSocket> client(sock);
    auto *headerSent = new bool(false);

    qDebug() << "[StreamProxy]" << method << "/" << token
             << "->" << entry.url
             << "range=" << (clientRange.isEmpty() ? "(none)" : clientRange);

    // Relay the upstream status line + the headers a player needs, then stream.
    auto sendHeaders = [reply, client, headerSent]() {
        if (*headerSent || !client)
            return;
        *headerSent = true;
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status <= 0) status = 200;
        // Use the correct reason phrase so strict HTTP clients (Chrome's video
        // pipeline in particular) accept the response — "HTTP/1.1 206 OK" is
        // invalid and triggers ERR_INVALID_RESPONSE.
        const QByteArray reason =
            reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute)
                .toByteArray();
        QByteArray reasonPhrase = reason;
        if (reasonPhrase.isEmpty()) {
            switch (status) {
            case 200: reasonPhrase = "OK"; break;
            case 206: reasonPhrase = "Partial Content"; break;
            case 301: reasonPhrase = "Moved Permanently"; break;
            case 302: reasonPhrase = "Found"; break;
            case 304: reasonPhrase = "Not Modified"; break;
            case 404: reasonPhrase = "Not Found"; break;
            case 416: reasonPhrase = "Range Not Satisfiable"; break;
            case 500: reasonPhrase = "Internal Server Error"; break;
            default:  reasonPhrase = "OK"; break;
            }
        }
        QByteArray out = "HTTP/1.1 " + QByteArray::number(status) + ' '
                       + reasonPhrase + "\r\n";
        // Pass through the headers that matter for media playback/seeking.
        static const char *pass[] = { "Content-Type", "Content-Length",
                                      "Content-Range", "Accept-Ranges",
                                      "Last-Modified", "ETag", nullptr };
        bool haveType = false;
        for (int i = 0; pass[i]; ++i) {
            const QByteArray key(pass[i]);
            if (reply->hasRawHeader(key)) {
                out += key + ": " + reply->rawHeader(key) + "\r\n";
                if (key == "Content-Type") haveType = true;
            }
        }
        // Chrome refuses to play video without a Content-Type; synthesize one
        // from the URL when upstream omits it.
        if (!haveType) {
            const QString u = reply->url().toString();
            QByteArray ct = "application/octet-stream";
            if (u.endsWith(".mp4", Qt::CaseInsensitive)) ct = "video/mp4";
            else if (u.endsWith(".webm", Qt::CaseInsensitive)) ct = "video/webm";
            else if (u.endsWith(".mkv", Qt::CaseInsensitive)) ct = "video/x-matroska";
            else if (u.endsWith(".mov", Qt::CaseInsensitive)) ct = "video/quicktime";
            else if (u.endsWith(".mp3", Qt::CaseInsensitive)) ct = "audio/mpeg";
            else if (u.endsWith(".flac", Qt::CaseInsensitive)) ct = "audio/flac";
            else if (u.endsWith(".ogg", Qt::CaseInsensitive)) ct = "audio/ogg";
            else if (u.endsWith(".wav", Qt::CaseInsensitive)) ct = "audio/wav";
            else if (u.endsWith(".m4a", Qt::CaseInsensitive)) ct = "audio/mp4";
            out += "Content-Type: " + ct + "\r\n";
        }
        if (!reply->hasRawHeader("Accept-Ranges"))
            out += "Accept-Ranges: bytes\r\n";
        out += "Connection: close\r\n\r\n";
        client->write(out);
    };

    connect(reply, &QNetworkReply::metaDataChanged, this, sendHeaders);
    connect(reply, &QNetworkReply::readyRead, this, [reply, client, sendHeaders]() {
        if (!client) {
            reply->abort();
            return;
        }
        sendHeaders();
        client->write(reply->readAll());
        // Let the socket drain so we don't buffer the whole file in memory.
        client->flush();
    });
    connect(reply, &QNetworkReply::errorOccurred, this,
            [reply, client, headerSent](QNetworkReply::NetworkError err) {
        qWarning() << "[StreamProxy] upstream error:" << err
                   << reply->errorString() << "status="
                   << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // If headers never went out, send a 502 so the client sees SOMETHING
        // instead of an empty response (which Chrome reports as
        // ERR_INVALID_RESPONSE / ERR_EMPTY_RESPONSE).
        if (client && !*headerSent) {
            *headerSent = true;
            const QByteArray body = reply->errorString().toUtf8();
            QByteArray out;
            out += "HTTP/1.1 502 Bad Gateway\r\n";
            out += "Content-Type: text/plain; charset=utf-8\r\n";
            out += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
            out += "Connection: close\r\n\r\n";
            out += body;
            client->write(out);
            client->flush();
        }
    });
    connect(reply, &QNetworkReply::finished, this,
            [reply, client, sendHeaders, headerSent]() {
        qDebug() << "[StreamProxy] upstream finished status="
                 << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
                 << "bytesAvailable=" << reply->bytesAvailable();
        if (client) {
            // Only synthesize a 200 header if nothing was sent yet AND the
            // upstream didn't error out (errorOccurred already wrote a 502).
            if (!*headerSent && reply->error() == QNetworkReply::NoError)
                sendHeaders();
            if (reply->bytesAvailable() > 0)
                client->write(reply->readAll());
            client->flush();
            client->disconnectFromHost();
        }
        delete headerSent;
        reply->deleteLater();
    });
    // If the player closes the connection (stop/seek), cancel the upstream
    // transfer instead of continuing to download.
    connect(sock, &QTcpSocket::disconnected, reply, [reply]() {
        if (reply->isRunning())
            reply->abort();
    });
}