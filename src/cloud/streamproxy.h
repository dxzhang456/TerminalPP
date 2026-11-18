#ifndef STREAMPROXY_H
#define STREAMPROXY_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QPair>
#include <QByteArray>
#include <QString>

class QTcpServer;
class QTcpSocket;
class QNetworkAccessManager;

// A minimal localhost HTTP relay used for "play while downloading" (边下边播).
//
// Why this exists: some providers only serve their temporary download link when
// the request carries specific headers. Baidu Pan's dlink, for example, rejects
// any request whose User-Agent is not "pan.baidu.com" and answers
//     {"error_code":31326,"error_msg":"user is not authorized, hitcode:119"}
// An external media player (or a browser) always sends its own User-Agent and
// cannot be told to send a custom one, so handing it the raw dlink always fails.
//
// StreamProxy solves this by listening on 127.0.0.1 and exposing a plain,
// header-free URL. When the player connects, the proxy re-issues the request
// upstream with the required headers injected, follows redirects, forwards the
// client's Range header (so seeking works) and streams the bytes straight back.
//
// Usage:
//     QString local = StreamProxy::instance()->publish(dlink, headers, "a.mp4");
//     QDesktopServices::openUrl(QUrl(local));   // player sees a normal URL
class StreamProxy : public QObject
{
    Q_OBJECT
public:
    static StreamProxy *instance();

    // Register an upstream URL plus the headers that must be sent to it, and
    // return a local URL ("http://127.0.0.1:<port>/<token>/<name>") that any
    // player can fetch. Returns an empty string if the local server cannot be
    // started, so callers can fall back to download-then-play.
    QString publish(const QString &upstreamUrl,
                    const QList<QPair<QByteArray, QByteArray>> &headers,
                    const QString &fileName);

private slots:
    void onNewConnection();

private:
    explicit StreamProxy(QObject *parent = nullptr);
    bool ensureListening();
    void handleRequest(QTcpSocket *sock, const QByteArray &requestData);

    struct Entry {
        QString url;
        QList<QPair<QByteArray, QByteArray>> headers;
    };

    QTcpServer *m_server = nullptr;
    QNetworkAccessManager *m_net = nullptr;
    QHash<QString, Entry> m_entries;   // token -> upstream target
    quint64 m_counter = 0;
};

#endif // STREAMPROXY_H