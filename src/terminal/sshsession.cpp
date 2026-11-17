#include "sshsession.h"

#include <QFileInfo>
#include <QThread>
#include <QHostInfo>
#include <QHostAddress>

#include <libssh2.h>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
   typedef SOCKET sock_t;
#  define CLOSESOCK closesocket
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <netdb.h>
#  include <unistd.h>
#  include <arpa/inet.h>
#  include <fcntl.h>
   typedef int sock_t;
#  define INVALID_SOCKET (-1)
#  define CLOSESOCK ::close
#endif

#include <cstring>

// ====================================================================
//  SshSession (GUI-thread facade)
// ====================================================================
SshSession::SshSession(QObject *parent)
    : QObject(parent)
{
}

SshSession::~SshSession()
{
    stop();
}

bool SshSession::isRunning() const
{
    return m_running.load();
}

void SshSession::start()
{
    if (m_running.load())
        return;

    m_thread = new QThread();
    m_worker = new SshWorker(m_config, m_rows, m_cols);
    m_worker->moveToThread(m_thread);

    // Worker -> GUI.
    connect(m_worker, &SshWorker::output, this, &SshSession::dataReceived);
    connect(m_worker, &SshWorker::status, this, &SshSession::errorText);
    connect(m_worker, &SshWorker::finished, this, [this](int code) {
        m_running.store(false);
        emit disconnected(code);
    });

    // NOTE: We intentionally do NOT use queued signals for GUI->worker here.
    // run() blocks the worker thread in a tight loop and never returns to its
    // event loop, so queued slot invocations would never execute. Instead
    // sendData()/setWindowSize() call the worker's thread-safe (mutex-guarded)
    // methods directly from the GUI thread.

    // Thread lifecycle.
    connect(m_thread, &QThread::started, m_worker, &SshWorker::run);
    connect(m_worker, &SshWorker::finished, m_thread, &QThread::quit);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, this, [this]() {
        m_worker = nullptr;
        m_thread = nullptr;
    });

    m_running.store(true);
    m_thread->start();
    emit connected();
}

void SshSession::stop()
{
    if (m_worker)
        m_worker->requestStop();
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(3000);
    }
    m_running.store(false);
}

void SshSession::sendData(const QByteArray &data)
{
    // writeData() is mutex-guarded, safe to call across threads directly.
    if (m_running.load() && m_worker)
        m_worker->writeData(data);
}

void SshSession::setWindowSize(int rows, int cols)
{
    m_rows = rows;
    m_cols = cols;
    if (m_running.load() && m_worker)
        m_worker->resize(rows, cols);
}

// ====================================================================
//  SshWorker (SSH-thread; owns libssh2)
// ====================================================================
SshWorker::SshWorker(const SshSessionConfig &cfg, int rows, int cols)
    : m_config(cfg), m_rows(rows), m_cols(cols)
{
}

SshWorker::~SshWorker()
{
    closeAll();
}

void SshWorker::requestStop()
{
    m_stop.store(true);
}

void SshWorker::writeData(const QByteArray &data)
{
    QMutexLocker lock(&m_writeMutex);
    m_pendingWrite.append(data);
}

void SshWorker::resize(int rows, int cols)
{
    m_rows = rows;
    m_cols = cols;
    m_resizePending.store(true);
}

static sock_t makeConnection(const QString &host, int port, QString &err)
{
#ifdef _WIN32
    WSADATA wsa;
    static bool wsaInit = false;
    if (!wsaInit) {
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            err = "WSAStartup failed";
            return INVALID_SOCKET;
        }
        wsaInit = true;
    }
#endif

    QHostInfo info = QHostInfo::fromName(host);
    if (info.addresses().isEmpty()) {
        err = QString("Cannot resolve host %1").arg(host);
        return INVALID_SOCKET;
    }
    QHostAddress addr = info.addresses().first();

    sock_t sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        err = "socket() failed";
        return INVALID_SOCKET;
    }

    struct sockaddr_in sin;
    std::memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(static_cast<quint16>(port));
    sin.sin_addr.s_addr = htonl(addr.toIPv4Address());

    if (::connect(sock, reinterpret_cast<struct sockaddr *>(&sin), sizeof(sin)) != 0) {
        err = QString("Cannot connect to %1:%2").arg(host).arg(port);
        CLOSESOCK(sock);
        return INVALID_SOCKET;
    }
    return sock;
}

bool SshWorker::connectAndAuth()
{
    QString err;
    sock_t sock = makeConnection(m_config.host, m_config.port, err);
    if (sock == INVALID_SOCKET) {
        emit status("\r\n[" + err + "]\r\n");
        return false;
    }
    m_socket = static_cast<qintptr>(sock);

    if (libssh2_init(0) != 0) {
        emit status("\r\n[libssh2_init failed]\r\n");
        return false;
    }

    LIBSSH2_SESSION *session = libssh2_session_init();
    if (!session) {
        emit status("\r\n[libssh2_session_init failed]\r\n");
        return false;
    }
    m_session = session;

    libssh2_session_set_blocking(session, 1);

    if (libssh2_session_handshake(session, static_cast<int>(sock)) != 0) {
        emit status("\r\n[SSH handshake failed]\r\n");
        return false;
    }

    // --- Authentication ---
    const QByteArray user = m_config.username.toUtf8();
    bool authed = false;

    if (!m_config.keyFile.isEmpty()) {
        QByteArray key = m_config.keyFile.toUtf8();
        QByteArray pub = (m_config.keyFile + ".pub").toUtf8();
        QByteArray pass = m_config.password.toUtf8();
        if (libssh2_userauth_publickey_fromfile(
                session, user.constData(),
                pub.constData(), key.constData(),
                pass.isEmpty() ? nullptr : pass.constData()) == 0) {
            authed = true;
        } else {
            emit status("\r\n[Public key auth failed, trying password...]\r\n");
        }
    }

    if (!authed && !m_config.password.isEmpty()) {
        QByteArray pass = m_config.password.toUtf8();
        if (libssh2_userauth_password(session, user.constData(), pass.constData()) == 0)
            authed = true;
    }

    if (!authed) {
        emit status("\r\n[Authentication failed. Check username/password/key.]\r\n");
        return false;
    }

    // --- Open channel + PTY + shell ---
    LIBSSH2_CHANNEL *channel = libssh2_channel_open_session(session);
    if (!channel) {
        emit status("\r\n[Failed to open SSH channel]\r\n");
        return false;
    }
    m_channel = channel;

    libssh2_channel_request_pty_ex(channel, "xterm-256color", 14,
                                   nullptr, 0,
                                   m_cols, m_rows, 0, 0);

    if (m_config.useTmux) {
        QString tmuxName = m_config.tmuxName.isEmpty()
            ? QStringLiteral("muxterm") : m_config.tmuxName;
        QByteArray cmd = QString("tmux new-session -A -s %1").arg(tmuxName).toUtf8();
        if (libssh2_channel_exec(channel, cmd.constData()) != 0) {
            emit status("\r\n[tmux exec failed, falling back to shell]\r\n");
            libssh2_channel_shell(channel);
        }
    } else {
        if (libssh2_channel_shell(channel) != 0) {
            emit status("\r\n[Failed to start shell]\r\n");
            return false;
        }
    }

    // Non-blocking for the IO loop.
    libssh2_session_set_blocking(session, 0);
    emit status(QString("\r\n[Connected to %1@%2:%3]\r\n")
                    .arg(m_config.username, m_config.host)
                    .arg(m_config.port));
    return true;
}

void SshWorker::run()
{
    if (!connectAndAuth()) {
        closeAll();
        emit finished(1);
        return;
    }

    LIBSSH2_SESSION *session = static_cast<LIBSSH2_SESSION *>(m_session);
    LIBSSH2_CHANNEL *channel = static_cast<LIBSSH2_CHANNEL *>(m_channel);

    char buf[32768];
    while (!m_stop.load()) {
        // Apply pending resize.
        if (m_resizePending.exchange(false)) {
            libssh2_channel_request_pty_size(channel, m_cols, m_rows);
        }

        // Write pending user input.
        {
            QMutexLocker lock(&m_writeMutex);
            while (!m_pendingWrite.isEmpty()) {
                ssize_t n = libssh2_channel_write(channel,
                                                   m_pendingWrite.constData(),
                                                   m_pendingWrite.size());
                if (n == LIBSSH2_ERROR_EAGAIN)
                    break;
                if (n < 0) {
                    m_pendingWrite.clear();
                    break;
                }
                m_pendingWrite.remove(0, static_cast<int>(n));
            }
        }

        // Read stdout.
        ssize_t rc = libssh2_channel_read(channel, buf, sizeof(buf));
        if (rc > 0) {
            emit output(QByteArray(buf, static_cast<int>(rc)));
            continue; // drain fast
        } else if (rc == LIBSSH2_ERROR_EAGAIN) {
            // nothing now
        } else if (rc < 0) {
            break;
        }

        // Read stderr.
        ssize_t re = libssh2_channel_read_stderr(channel, buf, sizeof(buf));
        if (re > 0) {
            emit output(QByteArray(buf, static_cast<int>(re)));
            continue;
        }

        if (libssh2_channel_eof(channel))
            break;

        QThread::msleep(10);
    }

    int exitCode = 0;
    if (channel) {
        libssh2_channel_close(channel);
        exitCode = libssh2_channel_get_exit_status(channel);
    }
    closeAll();
    emit finished(exitCode);
}

void SshWorker::closeAll()
{
    if (m_channel) {
        libssh2_channel_free(static_cast<LIBSSH2_CHANNEL *>(m_channel));
        m_channel = nullptr;
    }
    if (m_session) {
        libssh2_session_disconnect(static_cast<LIBSSH2_SESSION *>(m_session),
                                   "Client disconnecting");
        libssh2_session_free(static_cast<LIBSSH2_SESSION *>(m_session));
        m_session = nullptr;
    }
    if (m_socket >= 0) {
        CLOSESOCK(static_cast<sock_t>(m_socket));
        m_socket = -1;
    }
}