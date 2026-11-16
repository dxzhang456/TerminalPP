#ifndef SSHSESSION_H
#define SSHSESSION_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QByteArray>
#include <atomic>
#include "sessionmanager.h"

// Native SSH client built directly on libssh2 (no external ssh.exe / plink.exe).
// Runs the blocking libssh2 session loop in its own worker thread and exchanges
// data with the GUI thread via queued signals. Replicates MuxTerm's SSH worker
// including PTY allocation, tmux auto-attach, password & key authentication.
//
// This makes the SSH feature completely self-contained and cross-platform
// (Windows + Linux), with zero runtime dependency on a system SSH binary.
class SshSession : public QObject
{
    Q_OBJECT
public:
    explicit SshSession(QObject *parent = nullptr);
    ~SshSession() override;

    void setConfig(const SshSessionConfig &cfg) { m_config = cfg; }
    const SshSessionConfig &config() const { return m_config; }

    void start();
    void stop();
    bool isRunning() const;

public slots:
    void sendData(const QByteArray &data);
    void setWindowSize(int rows, int cols);

signals:
    void dataReceived(const QByteArray &data);
    void connected();
    void disconnected(int exitCode);
    void errorText(const QString &text);

    // Internal: forward outgoing bytes to the worker thread.
    void sendToWorker(const QByteArray &data);
    void resizeWorker(int rows, int cols);

private:
    SshSessionConfig m_config;
    QThread *m_thread = nullptr;
    class SshWorker *m_worker = nullptr;
    std::atomic<bool> m_running { false };
    int m_rows = 24;
    int m_cols = 80;
};

// -----------------------------------------------------------------------------
// The worker object lives in the SSH thread and owns the libssh2 session.
// -----------------------------------------------------------------------------
class SshWorker : public QObject
{
    Q_OBJECT
public:
    explicit SshWorker(const SshSessionConfig &cfg, int rows, int cols);
    ~SshWorker() override;

public slots:
    void run();                                   // entry point in worker thread
    void writeData(const QByteArray &data);       // queued from GUI thread
    void resize(int rows, int cols);              // queued from GUI thread
    void requestStop();

signals:
    void output(const QByteArray &data);
    void status(const QString &text);
    void finished(int exitCode);

private:
    bool connectAndAuth();
    void closeAll();

    SshSessionConfig m_config;
    int m_rows;
    int m_cols;

    // libssh2 handles (void* to avoid leaking the header here).
    void *m_session = nullptr;   // LIBSSH2_SESSION*
    void *m_channel = nullptr;   // LIBSSH2_CHANNEL*
    qintptr m_socket = -1;       // native socket descriptor

    QMutex m_writeMutex;
    QByteArray m_pendingWrite;
    std::atomic<bool> m_stop { false };
    std::atomic<bool> m_resizePending { false };
};

#endif // SSHSESSION_H