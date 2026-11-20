#include "ftpprovider.h"

#include <QThread>
#include <QTcpSocket>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QRegularExpression>
#include <QStringList>

#include <atomic>

// ===========================================================================
//  FtpWorker — owns the control-channel QTcpSocket; lives on its own thread.
//  Uses simple blocking reads (waitForReadyRead/waitForConnected) since this
//  thread has no other purpose and FTP's control protocol is a plain
//  request/single- or multi-line-reply exchange.
// ===========================================================================
class FtpWorker : public QObject
{
    Q_OBJECT
public:
    FtpWorker(const QString &host, int port, const QString &user, const QString &pass)
        : m_host(host), m_port(port), m_user(user), m_pass(pass) {}

    ~FtpWorker() override { closeAll(); }

    // Set from the provider (GUI) thread when the user cancels; the blocking
    // download/upload loops poll it and bail out. Atomic => lock-free & safe.
    void requestCancel() { m_cancel.store(true); }

public slots:
    void doList(const QString &path)
    {
        if (!ensureConnected()) return;
        QTcpSocket *data = openPassiveData();
        if (!data) { emit error(tr("Cannot open FTP data connection.")); return; }
        sendCmd("LIST " + path);
        QByteArray listing;
        while (data->state() != QAbstractSocket::UnconnectedState) {
            if (!data->waitForReadyRead(5000)) {
                if (data->state() == QAbstractSocket::UnconnectedState) break;
                continue;
            }
            listing += data->readAll();
        }
        data->close();
        delete data;
        readReply(); // 226 Transfer complete

        QVector<CloudEntry> out;
        const QStringList lines = QString::fromUtf8(listing).split("\r\n", Qt::SkipEmptyParts);
        // Unix-style LIST line, e.g.:
        //   drwxr-xr-x  2 user group 4096 Jan 01 12:00 dirname
        //   -rw-r--r--  1 user group 1234 Jan 01 12:00 file.txt
        static const QRegularExpression re(
            "^([\\-dl])\\S{9}\\s+\\S+\\s+\\S+\\s+\\S+\\s+(\\d+)\\s+(\\S+\\s+\\S+\\s+\\S+)\\s+(.+)$");
        for (const QString &line : lines) {
            auto m = re.match(line);
            CloudEntry e;
            if (m.hasMatch()) {
                e.isDir = (m.captured(1) == "d");
                e.size = m.captured(2).toLongLong();
                e.name = m.captured(4).trimmed();
            } else {
                // Fallback: MS-DOS style LIST (some FTP servers), e.g.:
                //   01-01-24  12:00PM   <DIR>          dirname
                //   01-01-24  12:00PM         1234 file.txt
                static const QRegularExpression dosRe(
                    "^\\S+\\s+\\S+\\s+(<DIR>|\\d+)\\s+(.+)$");
                auto dm = dosRe.match(line);
                if (!dm.hasMatch())
                    continue;
                e.isDir = (dm.captured(1) == "<DIR>");
                e.size = e.isDir ? 0 : dm.captured(1).toLongLong();
                e.name = dm.captured(2).trimmed();
            }
            if (e.name.isEmpty() || e.name == "." || e.name == "..")
                continue;
            QString rel = path;
            if (!rel.endsWith('/')) rel += '/';
            rel += e.name;
            e.path = rel;
            out.push_back(e);
        }
        emit listed(path, out);
    }

    void doDownload(const QString &remotePath, const QString &localPath)
    {
        m_cancel.store(false);              // fresh operation
        if (!ensureConnected()) return;
        QTcpSocket *data = openPassiveData();
        if (!data) { emit error(tr("Cannot open FTP data connection.")); return; }
        sendCmd("TYPE I");
        readReply();
        // Re-open PASV after TYPE (some servers require it per-transfer); keep
        // it simple: issue PASV again right before RETR.
        delete data;
        data = openPassiveData();
        if (!data) { emit error(tr("Cannot open FTP data connection.")); return; }
        sendCmd("RETR " + remotePath);
        const QString firstLine = readReply();
        if (!firstLine.startsWith("150") && !firstLine.startsWith("125")) {
            data->close(); delete data;
            emit error(tr("FTP RETR failed: %1").arg(firstLine));
            return;
        }
        QFile dst(localPath);
        if (!dst.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            data->close(); delete data;
            emit error(tr("Cannot write local file: %1").arg(localPath));
            return;
        }
        qint64 done = 0;
        while (data->state() != QAbstractSocket::UnconnectedState || data->bytesAvailable() > 0) {
            if (m_cancel.load()) {          // user canceled -> stop immediately
                dst.close();
                data->abort(); delete data;
                dst.remove();               // drop the partial file
                return;                     // silent: manager already marked it
            }
            if (!data->waitForReadyRead(5000)) {
                if (data->state() == QAbstractSocket::UnconnectedState) break;
                continue;
            }
            QByteArray chunk = data->readAll();
            dst.write(chunk);
            done += chunk.size();
            emit downloadProgress(done, done); // FTP LIST doesn't give total size upfront
        }
        dst.close();
        data->close();
        delete data;
        readReply(); // 226 Transfer complete
        emit downloadFinished(remotePath, localPath);
    }

    void doUpload(const QString &localPath, const QString &remoteDir)
    {
        m_cancel.store(false);              // fresh operation
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

        sendCmd("TYPE I");
        readReply();
        // Delete remote file first if it exists, to ensure a clean overwrite across all FTP server implementations
        sendCmd("DELE " + rel);
        readReply(); // Ignore reply (e.g. 550 if file doesn't exist)

        QTcpSocket *data = openPassiveData();
        if (!data) { emit error(tr("Cannot open FTP data connection.")); src.close(); return; }
        sendCmd("STOR " + rel);
        const QString firstLine = readReply();
        if (!firstLine.startsWith("150") && !firstLine.startsWith("125")) {
            data->close(); delete data; src.close();
            emit error(tr("FTP STOR failed: %1").arg(firstLine));
            return;
        }
        const qint64 total = src.size();
        qint64 done = 0;
        while (!src.atEnd()) {
            if (m_cancel.load()) {          // user canceled -> stop immediately
                src.close();
                data->abort(); delete data;
                return;
            }
            QByteArray chunk = src.read(65536);
            data->write(chunk);
            data->waitForBytesWritten(5000);
            done += chunk.size();
            emit uploadProgress(localPath, done, total);
        }
        src.close();
        data->close();
        delete data;
        readReply(); // 226 Transfer complete
        emit uploadFinished(localPath, remoteDir);
    }

    void doMakeDir(const QString &remoteDir, const QString &name)
    {
        if (!ensureConnected()) return;
        QString rel = remoteDir;
        if (!rel.endsWith('/')) rel += '/';
        rel += name;
        sendCmd("MKD " + rel);
        const QString reply = readReply();
        if (reply.startsWith("257"))
            emit opFinished();
        else
            emit error(tr("Cannot create remote folder: %1 (%2)").arg(rel, reply));
    }

    void doRemove(const QString &remotePath)
    {
        if (!ensureConnected()) return;
        // Try file delete first; if that fails, attempt as a directory.
        sendCmd("DELE " + remotePath);
        QString reply = readReply();
        if (reply.startsWith("250")) { emit opFinished(); return; }

        sendCmd("RMD " + remotePath);
        reply = readReply();
        if (reply.startsWith("250")) { emit opFinished(); return; }

        emit error(tr("Cannot delete: %1 (%2)").arg(remotePath, reply));
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

        sendCmd("RNFR " + remotePath);
        QString r1 = readReply();
        if (!r1.startsWith("350")) {
            emit error(tr("Cannot rename (RNFR): %1").arg(r1));
            return;
        }
        sendCmd("RNTO " + dest);
        QString r2 = readReply();
        if (r2.startsWith("250"))
            emit opFinished();
        else
            emit error(tr("Cannot rename (RNTO): %1").arg(r2));
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
    bool ensureConnected()
    {
        if (m_ctrl && m_ctrl->state() == QAbstractSocket::ConnectedState)
            return true;

        delete m_ctrl;
        m_ctrl = new QTcpSocket();
        m_ctrl->connectToHost(m_host, m_port);
        if (!m_ctrl->waitForConnected(8000)) {
            emit error(tr("Cannot connect to FTP server %1:%2").arg(m_host).arg(m_port));
            return false;
        }
        readReply(); // 220 welcome banner

        sendCmd("USER " + m_user);
        QString r = readReply();
        if (r.startsWith("331")) {
            sendCmd("PASS " + m_pass);
            r = readReply();
        }
        if (!r.startsWith("230")) {
            emit error(tr("FTP login failed for user \"%1\": %2").arg(m_user, r));
            m_ctrl->close();
            return false;
        }
        // Binary mode by default for all transfers.
        sendCmd("TYPE I");
        readReply();
        return true;
    }

    void sendCmd(const QString &cmd)
    {
        if (!m_ctrl) return;
        m_ctrl->write((cmd + "\r\n").toUtf8());
        m_ctrl->waitForBytesWritten(3000);
    }

    // Reads one logical FTP reply (handles multi-line "250-...\r\n250 ...").
    QString readReply()
    {
        if (!m_ctrl) return QString();
        QByteArray buf;
        while (true) {
            if (!m_ctrl->waitForReadyRead(8000) && buf.isEmpty())
                break;
            buf += m_ctrl->readAll();
            // A reply is complete once we see "NNN " (space, not dash) at the
            // start of the last line.
            const QList<QByteArray> lines = buf.split('\n');
            if (!lines.isEmpty()) {
                QByteArray last = lines.last().isEmpty() && lines.size() > 1
                                      ? lines.at(lines.size() - 2) : lines.last();
                last = last.trimmed();
                if (last.size() >= 4 && last.at(3) == ' ')
                    break;
            }
            if (m_ctrl->bytesAvailable() == 0 && !buf.endsWith('\n'))
                continue;
        }
        return QString::fromUtf8(buf).trimmed();
    }

    // Issues PASV, parses the "227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)"
    // reply and opens a data connection to that host:port.
    QTcpSocket *openPassiveData()
    {
        sendCmd("PASV");
        const QString reply = readReply();
        static const QRegularExpression re(
            "\\((\\d+),(\\d+),(\\d+),(\\d+),(\\d+),(\\d+)\\)");
        auto m = re.match(reply);
        if (!m.hasMatch())
            return nullptr;
        const QString host = QString("%1.%2.%3.%4")
            .arg(m.captured(1), m.captured(2), m.captured(3), m.captured(4));
        const int port = m.captured(5).toInt() * 256 + m.captured(6).toInt();

        auto *data = new QTcpSocket();
        data->connectToHost(host, port);
        if (!data->waitForConnected(8000)) {
            delete data;
            return nullptr;
        }
        return data;
    }

    void closeAll()
    {
        if (m_ctrl) {
            if (m_ctrl->state() == QAbstractSocket::ConnectedState) {
                sendCmd("QUIT");
            }
            m_ctrl->close();
            delete m_ctrl;
            m_ctrl = nullptr;
        }
    }

    QString m_host, m_user, m_pass;
    int m_port;
    QTcpSocket *m_ctrl = nullptr;
    std::atomic_bool m_cancel { false };   // set by requestCancel() to abort I/O
};

// ===========================================================================
//  FtpProvider — GUI-thread facade mirroring SftpProvider's worker split.
// ===========================================================================
FtpProvider::FtpProvider(QObject *parent) : CloudProvider(parent) {}

FtpProvider::~FtpProvider()
{
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(2000);
    }
}

void FtpProvider::ensureWorker()
{
    if (m_worker)
        return;
    m_thread = new QThread(this);
    m_worker = new FtpWorker(m_host, m_port, m_user, m_pass);
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(m_worker, &FtpWorker::listed, this, &FtpProvider::onWorkerListed);
    connect(m_worker, &FtpWorker::error, this, &FtpProvider::onWorkerError);
    connect(m_worker, &FtpWorker::downloadProgress, this, &FtpProvider::onWorkerDownloadProgress);
    connect(m_worker, &FtpWorker::downloadFinished, this, &FtpProvider::onWorkerDownloadFinished);
    connect(m_worker, &FtpWorker::uploadProgress, this, &FtpProvider::onWorkerUploadProgress);
    connect(m_worker, &FtpWorker::uploadFinished, this, &FtpProvider::onWorkerUploadFinished);
    connect(m_worker, &FtpWorker::opFinished, this, &FtpProvider::onWorkerOpFinished);

    m_thread->start();
}

void FtpProvider::list(const QString &path)
{
    ensureWorker();
    QMetaObject::invokeMethod(m_worker, "doList", Qt::QueuedConnection, Q_ARG(QString, path));
}

void FtpProvider::download(const QString &remotePath, const QString &localPath)
{
    ensureWorker();
    QMetaObject::invokeMethod(m_worker, "doDownload", Qt::QueuedConnection,
                              Q_ARG(QString, remotePath), Q_ARG(QString, localPath));
}

void FtpProvider::resolveStreamUrl(const QString &remotePath)
{
    Q_UNUSED(remotePath);
    emit errorOccurred(tr("Streaming not supported over FTP; downloading instead."));
}

void FtpProvider::upload(const QString &localPath, const QString &remoteDir)
{
    ensureWorker();
    QMetaObject::invokeMethod(m_worker, "doUpload", Qt::QueuedConnection,
                              Q_ARG(QString, localPath), Q_ARG(QString, remoteDir));
}

void FtpProvider::makeDir(const QString &remoteDir, const QString &name)
{
    ensureWorker();
    QMetaObject::invokeMethod(m_worker, "doMakeDir", Qt::QueuedConnection,
                              Q_ARG(QString, remoteDir), Q_ARG(QString, name));
}

void FtpProvider::remove(const QString &remotePath)
{
    ensureWorker();
    QMetaObject::invokeMethod(m_worker, "doRemove", Qt::QueuedConnection,
                              Q_ARG(QString, remotePath));
}

void FtpProvider::rename(const QString &remotePath, const QString &newName)
{
    ensureWorker();
    QMetaObject::invokeMethod(m_worker, "doRename", Qt::QueuedConnection,
                              Q_ARG(QString, remotePath), Q_ARG(QString, newName));
}

void FtpProvider::cancelTransfers()
{
    // The worker runs a blocking read/write loop; setting the atomic cancel
    // flag (thread-safe) makes that loop bail out on its next iteration.
    if (m_worker)
        m_worker->requestCancel();
}

void FtpProvider::onWorkerListed(const QString &path, const QVector<CloudEntry> &entries)
{ emit listed(path, entries); }

void FtpProvider::onWorkerError(const QString &msg)
{ emit errorOccurred(msg); }

void FtpProvider::onWorkerDownloadProgress(qint64 done, qint64 total)
{ emit downloadProgress(done, total); }

void FtpProvider::onWorkerDownloadFinished(const QString &remotePath, const QString &localPath)
{ emit downloadFinished(remotePath, localPath); }

void FtpProvider::onWorkerUploadProgress(const QString &localPath, qint64 done, qint64 total)
{ emit uploadProgress(localPath, done, total); }

void FtpProvider::onWorkerUploadFinished(const QString &localPath, const QString &remoteDir)
{ emit uploadFinished(localPath, remoteDir); }

void FtpProvider::onWorkerOpFinished()
{ emit operationFinished(); }

#include "ftpprovider.moc"