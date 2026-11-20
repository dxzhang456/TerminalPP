#include "sftpprovider.h"

#include <QThread>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>

#include <atomic>

#include <libssh2.h>
#include <libssh2_sftp.h>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
   typedef SOCKET sock_t;
#  define CLOSESOCK closesocket
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <netdb.h>
#  include <unistd.h>
#  include <arpa/inet.h>
   typedef int sock_t;
#  define INVALID_SOCKET (-1)
#  define CLOSESOCK ::close
#endif

#include <cstring>

// ===========================================================================
//  SftpWorker — owns the libssh2 SFTP session; lives on its own QThread.
//  All methods below run IN THE WORKER THREAD (invoked via QMetaObject
//  invokeMethod with Qt::QueuedConnection from the provider's thread).
// ===========================================================================
class SftpWorker : public QObject
{
    Q_OBJECT
public:
    SftpWorker(const QString &host, int port, const QString &user,
               const QString &pass, const QString &keyPath)
        : m_host(host), m_port(port), m_user(user), m_pass(pass), m_keyPath(keyPath) {}

    ~SftpWorker() override { closeAll(); }

    // Set from the provider (GUI) thread when the user cancels; the blocking
    // download/upload loops below poll it and bail out. std::atomic makes the
    // cross-thread write safe without a lock.
    void requestCancel() { m_cancel.store(true); }

public slots:
    void doList(const QString &path)
    {
        if (!ensureConnected()) return;
        LIBSSH2_SFTP_HANDLE *dh = libssh2_sftp_opendir(m_sftp, path.toUtf8().constData());
        if (!dh) {
            emit error(tr("Cannot open remote directory: %1").arg(path));
            return;
        }
        QVector<CloudEntry> out;
        char nameBuf[1024];
        char longEntry[1024];
        LIBSSH2_SFTP_ATTRIBUTES attrs;
        int rc;
        while ((rc = libssh2_sftp_readdir_ex(dh, nameBuf, sizeof(nameBuf),
                                             longEntry, sizeof(longEntry), &attrs)) > 0) {
            QString name = QString::fromUtf8(nameBuf, rc);
            if (name == "." || name == "..")
                continue;
            CloudEntry e;
            e.name = name;
            QString rel = path;
            if (!rel.endsWith('/')) rel += '/';
            rel += name;
            e.path = rel;
            e.isDir = LIBSSH2_SFTP_S_ISDIR(attrs.permissions);
            e.size = (attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) ? (qint64)attrs.filesize : 0;
            if (attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME)
                e.modified = QDateTime::fromSecsSinceEpoch((qint64)attrs.mtime);
            out.push_back(e);
        }
        libssh2_sftp_closedir(dh);
        emit listed(path, out);
    }

    void doDownload(const QString &remotePath, const QString &localPath)
    {
        m_cancel.store(false);           // fresh operation
        if (!ensureConnected()) return;
        LIBSSH2_SFTP_HANDLE *fh = libssh2_sftp_open(
            m_sftp, remotePath.toUtf8().constData(), LIBSSH2_FXF_READ, 0);
        if (!fh) {
            emit error(tr("Cannot open remote file: %1").arg(remotePath));
            return;
        }
        LIBSSH2_SFTP_ATTRIBUTES attrs;
        qint64 total = 0;
        if (libssh2_sftp_fstat(fh, &attrs) == 0 && (attrs.flags & LIBSSH2_SFTP_ATTR_SIZE))
            total = (qint64)attrs.filesize;

        QFile dst(localPath);
        if (!dst.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            libssh2_sftp_close(fh);
            emit error(tr("Cannot write local file: %1").arg(localPath));
            return;
        }
        char buf[65536];
        qint64 done = 0;
        ssize_t n;
        while ((n = libssh2_sftp_read(fh, buf, sizeof(buf))) > 0) {
            if (m_cancel.load()) {        // user canceled -> stop immediately
                dst.close();
                libssh2_sftp_close(fh);
                dst.remove();             // drop the partial file
                return;                   // silent: manager already marked it
            }
            dst.write(buf, n);
            done += n;
            emit downloadProgress(done, total);
        }
        dst.close();
        libssh2_sftp_close(fh);
        if (n < 0) {
            emit error(tr("Read error while downloading: %1").arg(remotePath));
            return;
        }
        emit downloadFinished(remotePath, localPath);
    }

    void doUpload(const QString &localPath, const QString &remoteDir)
    {
        m_cancel.store(false);           // fresh operation
        if (!ensureConnected()) return;
        QFile src(localPath);
        if (!src.open(QIODevice::ReadOnly)) {
            emit error(tr("Cannot read local file: %1").arg(localPath));
            return;
        }
        const QString name = QFileInfo(localPath).fileName();
        QString rel = remoteDir;
        if (!rel.endsWith('/')) rel += '/';
        rel += name;

        // If remote file exists, unlink first to guarantee a clean overwrite across all SFTP server permissions
        libssh2_sftp_unlink(m_sftp, rel.toUtf8().constData());

        LIBSSH2_SFTP_HANDLE *fh = libssh2_sftp_open(
            m_sftp, rel.toUtf8().constData(),
            LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC,
            LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR |
            LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IROTH);
        if (!fh) {
            emit error(tr("Cannot create remote file: %1").arg(rel));
            src.close();
            return;
        }
        const qint64 total = src.size();
        qint64 done = 0;
        while (!src.atEnd()) {
            if (m_cancel.load()) {        // user canceled -> stop immediately
                libssh2_sftp_close(fh);
                src.close();
                return;
            }
            QByteArray chunk = src.read(65536);
            const char *p = chunk.constData();
            qint64 remaining = chunk.size();
            while (remaining > 0) {
                ssize_t n = libssh2_sftp_write(fh, p, remaining);
                if (n < 0) {
                    libssh2_sftp_close(fh);
                    src.close();
                    emit error(tr("Write error while uploading to: %1").arg(rel));
                    return;
                }
                p += n; remaining -= n; done += n;
            }
            emit uploadProgress(localPath, done, total);
        }
        src.close();
        libssh2_sftp_close(fh);
        emit uploadFinished(localPath, remoteDir);
    }

    void doMakeDir(const QString &remoteDir, const QString &name)
    {
        if (!ensureConnected()) return;
        QString rel = remoteDir;
        if (!rel.endsWith('/')) rel += '/';
        rel += name;
        int rc = libssh2_sftp_mkdir(m_sftp, rel.toUtf8().constData(),
            LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP |
            LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH);
        if (rc == 0) emit opFinished();
        else emit error(tr("Cannot create remote folder: %1").arg(rel));
    }

    void doRemove(const QString &remotePath)
    {
        if (!ensureConnected()) return;
        LIBSSH2_SFTP_ATTRIBUTES attrs;
        bool isDir = false;
        if (libssh2_sftp_stat(m_sftp, remotePath.toUtf8().constData(), &attrs) == 0)
            isDir = LIBSSH2_SFTP_S_ISDIR(attrs.permissions);

        bool ok;
        if (isDir)
            ok = removeDirRecursive(remotePath);
        else
            ok = (libssh2_sftp_unlink(m_sftp, remotePath.toUtf8().constData()) == 0);

        if (ok) emit opFinished();
        else emit error(tr("Cannot delete: %1").arg(remotePath));
    }

    void doRename(const QString &remotePath, const QString &newName)
    {
        if (!ensureConnected()) return;
        QString parent = remotePath;
        if (parent.endsWith('/')) parent.chop(1);
        int idx = parent.lastIndexOf('/');
        parent = (idx <= 0) ? "/" : parent.left(idx);
        QString dest = parent;
        if (!dest.endsWith('/')) dest += '/';
        dest += newName;

        int rc = libssh2_sftp_rename(m_sftp, remotePath.toUtf8().constData(),
                                     dest.toUtf8().constData());
        if (rc == 0) emit opFinished();
        else emit error(tr("Cannot rename: %1").arg(remotePath));
    }

signals:
    void listed(const QString &path, const QVector<CloudEntry> &entries);
    void error(const QString &msg);
    void downloadProgress(qint64 done, qint64 total);
    void downloadFinished(const QString &remotePath, const QString &localPath);
    void uploadProgress(const QString &localPath, qint64 done, qint64 total);
    void uploadFinished(const QString &localPath, const QString &remoteDir);
    void opFinished();

private:
    bool removeDirRecursive(const QString &path)
    {
        LIBSSH2_SFTP_HANDLE *dh = libssh2_sftp_opendir(m_sftp, path.toUtf8().constData());
        if (!dh) return false;
        char nameBuf[1024];
        LIBSSH2_SFTP_ATTRIBUTES attrs;
        int rc;
        bool ok = true;
        while ((rc = libssh2_sftp_readdir(dh, nameBuf, sizeof(nameBuf), &attrs)) > 0) {
            QString name = QString::fromUtf8(nameBuf, rc);
            if (name == "." || name == "..") continue;
            QString child = path;
            if (!child.endsWith('/')) child += '/';
            child += name;
            if (LIBSSH2_SFTP_S_ISDIR(attrs.permissions))
                ok = removeDirRecursive(child) && ok;
            else
                ok = (libssh2_sftp_unlink(m_sftp, child.toUtf8().constData()) == 0) && ok;
        }
        libssh2_sftp_closedir(dh);
        ok = (libssh2_sftp_rmdir(m_sftp, path.toUtf8().constData()) == 0) && ok;
        return ok;
    }

    bool ensureConnected()
    {
        if (m_sftp)
            return true;

#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
        struct addrinfo hints; memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        struct addrinfo *res = nullptr;
        const QByteArray hostUtf8 = m_host.toUtf8();
        const QByteArray portStr = QByteArray::number(m_port);
        if (getaddrinfo(hostUtf8.constData(), portStr.constData(), &hints, &res) != 0) {
            emit error(tr("Cannot resolve host: %1").arg(m_host));
            return false;
        }
        sock_t sock = INVALID_SOCKET;
        for (struct addrinfo *rp = res; rp; rp = rp->ai_next) {
            sock = (sock_t)::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (sock == INVALID_SOCKET) continue;
            if (::connect(sock, rp->ai_addr, (int)rp->ai_addrlen) == 0)
                break;
            CLOSESOCK(sock);
            sock = INVALID_SOCKET;
        }
        freeaddrinfo(res);
        if (sock == INVALID_SOCKET) {
            emit error(tr("Cannot connect to %1:%2").arg(m_host).arg(m_port));
            return false;
        }

        libssh2_init(0);
        m_session = libssh2_session_init();
        if (!m_session) {
            CLOSESOCK(sock);
            emit error(tr("Failed to init SSH session."));
            return false;
        }
        libssh2_session_set_blocking(m_session, 1);
        if (libssh2_session_handshake(m_session, (libssh2_socket_t)sock) != 0) {
            emit error(tr("SSH handshake failed with %1:%2").arg(m_host).arg(m_port));
            libssh2_session_free(m_session); m_session = nullptr;
            CLOSESOCK(sock);
            return false;
        }

        int authRc = -1;
        if (!m_keyPath.isEmpty()) {
            const QByteArray keyUtf8 = m_keyPath.toUtf8();
            authRc = libssh2_userauth_publickey_fromfile(
                m_session, m_user.toUtf8().constData(), nullptr,
                keyUtf8.constData(), m_pass.toUtf8().constData());
        } else {
            authRc = libssh2_userauth_password(
                m_session, m_user.toUtf8().constData(), m_pass.toUtf8().constData());
        }
        if (authRc != 0) {
            emit error(tr("SFTP authentication failed for user \"%1\".").arg(m_user));
            libssh2_session_disconnect(m_session, "auth failed");
            libssh2_session_free(m_session); m_session = nullptr;
            CLOSESOCK(sock);
            return false;
        }

        m_sftp = libssh2_sftp_init(m_session);
        if (!m_sftp) {
            emit error(tr("Failed to initialize SFTP subsystem."));
            libssh2_session_disconnect(m_session, "sftp init failed");
            libssh2_session_free(m_session); m_session = nullptr;
            CLOSESOCK(sock);
            return false;
        }

        m_socket = (qintptr)sock;
        return true;
    }

    void closeAll()
    {
        if (m_sftp) { libssh2_sftp_shutdown(m_sftp); m_sftp = nullptr; }
        if (m_session) {
            libssh2_session_disconnect(m_session, "bye");
            libssh2_session_free(m_session);
            m_session = nullptr;
        }
        if (m_socket >= 0) {
            CLOSESOCK((sock_t)m_socket);
            m_socket = -1;
        }
    }

    QString m_host, m_user, m_pass, m_keyPath;
    int m_port;

    LIBSSH2_SESSION *m_session = nullptr;
    LIBSSH2_SFTP *m_sftp = nullptr;
    qintptr m_socket = -1;
    std::atomic_bool m_cancel { false };   // set by requestCancel() to abort I/O
};

// ===========================================================================
//  SftpProvider — GUI-thread facade. Forwards calls to the worker thread and
//  re-emits its result signals as the normal CloudProvider signals.
// ===========================================================================
SftpProvider::SftpProvider(QObject *parent) : CloudProvider(parent) {}

SftpProvider::~SftpProvider()
{
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(2000);
    }
}

void SftpProvider::ensureWorker()
{
    if (m_worker)
        return;
    m_thread = new QThread(this);
    m_worker = new SftpWorker(m_host, m_port, m_user, m_pass, m_keyPath);
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(m_worker, &SftpWorker::listed, this, &SftpProvider::onWorkerListed);
    connect(m_worker, &SftpWorker::error, this, &SftpProvider::onWorkerError);
    connect(m_worker, &SftpWorker::downloadProgress, this, &SftpProvider::onWorkerDownloadProgress);
    connect(m_worker, &SftpWorker::downloadFinished, this, &SftpProvider::onWorkerDownloadFinished);
    connect(m_worker, &SftpWorker::uploadProgress, this, &SftpProvider::onWorkerUploadProgress);
    connect(m_worker, &SftpWorker::uploadFinished, this, &SftpProvider::onWorkerUploadFinished);
    connect(m_worker, &SftpWorker::opFinished, this, &SftpProvider::onWorkerOpFinished);

    m_thread->start();
}

void SftpProvider::list(const QString &path)
{
    ensureWorker();
    QMetaObject::invokeMethod(m_worker, "doList", Qt::QueuedConnection, Q_ARG(QString, path));
}

void SftpProvider::download(const QString &remotePath, const QString &localPath)
{
    ensureWorker();
    QMetaObject::invokeMethod(m_worker, "doDownload", Qt::QueuedConnection,
                              Q_ARG(QString, remotePath), Q_ARG(QString, localPath));
}

void SftpProvider::resolveStreamUrl(const QString &remotePath)
{
    // SFTP has no "temporary direct URL" concept; stream by downloading to a
    // temp file first (the browser tab already falls back to this when
    // errorOccurred() is emitted for unsupported streaming).
    Q_UNUSED(remotePath);
    emit errorOccurred(tr("Streaming not supported over SFTP; downloading instead."));
}

void SftpProvider::upload(const QString &localPath, const QString &remoteDir)
{
    ensureWorker();
    QMetaObject::invokeMethod(m_worker, "doUpload", Qt::QueuedConnection,
                              Q_ARG(QString, localPath), Q_ARG(QString, remoteDir));
}

void SftpProvider::makeDir(const QString &remoteDir, const QString &name)
{
    ensureWorker();
    QMetaObject::invokeMethod(m_worker, "doMakeDir", Qt::QueuedConnection,
                              Q_ARG(QString, remoteDir), Q_ARG(QString, name));
}

void SftpProvider::remove(const QString &remotePath)
{
    ensureWorker();
    QMetaObject::invokeMethod(m_worker, "doRemove", Qt::QueuedConnection,
                              Q_ARG(QString, remotePath));
}

void SftpProvider::rename(const QString &remotePath, const QString &newName)
{
    ensureWorker();
    QMetaObject::invokeMethod(m_worker, "doRename", Qt::QueuedConnection,
                              Q_ARG(QString, remotePath), Q_ARG(QString, newName));
}

void SftpProvider::cancelTransfers()
{
    // The worker runs a blocking read/write loop; setting the atomic cancel
    // flag (thread-safe) makes that loop bail out on its next iteration. Safe
    // to call even if no worker exists yet.
    if (m_worker)
        m_worker->requestCancel();
}

void SftpProvider::onWorkerListed(const QString &path, const QVector<CloudEntry> &entries)
{ emit listed(path, entries); }

void SftpProvider::onWorkerError(const QString &msg)
{ emit errorOccurred(msg); }

void SftpProvider::onWorkerDownloadProgress(qint64 done, qint64 total)
{ emit downloadProgress(done, total); }

void SftpProvider::onWorkerDownloadFinished(const QString &remotePath, const QString &localPath)
{ emit downloadFinished(remotePath, localPath); }

void SftpProvider::onWorkerUploadProgress(const QString &localPath, qint64 done, qint64 total)
{ emit uploadProgress(localPath, done, total); }

void SftpProvider::onWorkerUploadFinished(const QString &localPath, const QString &remoteDir)
{ emit uploadFinished(localPath, remoteDir); }

void SftpProvider::onWorkerOpFinished()
{ emit operationFinished(); }

#include "sftpprovider.moc"