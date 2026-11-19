#include "transfermanager.h"
#include "cloudprovider.h"
#include "core/i18n.h"

#include <QNetworkReply>

static QString TRc(const char *s) { return I18n::instance()->tr(QString::fromUtf8(s)); }

QString TransferItem::stateText() const
{
    switch (state) {
    case Queued:    return TRc("Queued");
    case Running:   return TRc("Running");
    case Paused:    return TRc("Paused");
    case Completed: return TRc("Completed");
    case Failed:    return TRc("Failed");
    case Canceled:  return TRc("Canceled");
    }
    return QString();
}

QString TransferItem::directionText() const
{
    return direction == Upload ? TRc("Upload") : TRc("Download");
}

// ---------------------------------------------------------------------------
TransferManager *TransferManager::instance()
{
    static TransferManager inst;
    return &inst;
}

TransferManager::TransferManager(QObject *parent) : QObject(parent) {}

int TransferManager::indexOf(int id) const
{
    for (int i = 0; i < m_items.size(); ++i)
        if (m_items[i].id == id)
            return i;
    return -1;
}

TransferItem *TransferManager::find(int id)
{
    int i = indexOf(id);
    return i >= 0 ? &m_items[i] : nullptr;
}

int TransferManager::begin(TransferItem::Direction dir, const QString &driveLabel,
                           const QString &name, const QString &remotePath,
                           const QString &localPath, CloudProvider *provider,
                           qint64 totalHint)
{
    TransferItem t;
    t.id         = m_nextId++;
    t.direction  = dir;
    t.state      = TransferItem::Queued;
    t.driveLabel = driveLabel;
    t.name       = name;
    t.remotePath = remotePath;
    t.localPath  = localPath;
    t.provider   = provider;
    t.bytesTotal = totalHint;
    t.started    = QDateTime::currentDateTime();
    t.lastTick   = t.started;
    m_items.push_back(t);
    emit itemAdded(t.id);
    return t.id;
}

void TransferManager::markRunning(int id)
{
    int i = indexOf(id);
    if (i < 0) return;
    m_items[i].state = TransferItem::Running;
    emit itemChanged(id);
}

void TransferManager::updateProgress(int id, qint64 done, qint64 total)
{
    int i = indexOf(id);
    if (i < 0) return;
    TransferItem &t = m_items[i];
    t.bytesDone  = done;
    if (total > 0)
        t.bytesTotal = total;
    if (t.state == TransferItem::Queued)
        t.state = TransferItem::Running;

    // Smooth the speed over ~500ms windows.
    const QDateTime now = QDateTime::currentDateTime();
    const qint64 ms = t.lastTick.msecsTo(now);
    if (ms >= 400) {
        const double inst = double(done - t.lastBytes) * 1000.0 / double(ms);
        t.speedBps = (t.speedBps <= 0) ? inst : (t.speedBps * 0.6 + inst * 0.4);
        t.lastTick = now;
        t.lastBytes = done;
    }
    emit itemChanged(id);
}

void TransferManager::markCompleted(int id)
{
    int i = indexOf(id);
    if (i < 0) return;
    TransferItem &t = m_items[i];
    t.state = TransferItem::Completed;
    if (t.bytesTotal <= 0) t.bytesTotal = t.bytesDone;
    t.bytesDone = t.bytesTotal;
    t.speedBps = 0;
    t.finished = QDateTime::currentDateTime();
    t.reply = nullptr;
    emit itemChanged(id);
}

void TransferManager::markFailed(int id, const QString &error)
{
    int i = indexOf(id);
    if (i < 0) return;
    TransferItem &t = m_items[i];
    // A user-initiated cancel also routes an error through the reply; don't
    // overwrite the Canceled state with Failed.
    if (t.state == TransferItem::Canceled)
        return;
    t.state = TransferItem::Failed;
    t.error = error;
    t.speedBps = 0;
    t.finished = QDateTime::currentDateTime();
    t.reply = nullptr;
    emit itemChanged(id);
}

void TransferManager::markCanceled(int id)
{
    int i = indexOf(id);
    if (i < 0) return;
    TransferItem &t = m_items[i];
    t.state = TransferItem::Canceled;
    t.speedBps = 0;
    t.finished = QDateTime::currentDateTime();
    t.reply = nullptr;
    emit itemChanged(id);
}

void TransferManager::attachReply(int id, QNetworkReply *reply)
{
    int i = indexOf(id);
    if (i < 0) return;
    m_items[i].reply = reply;
}

void TransferManager::cancel(int id)
{
    int i = indexOf(id);
    if (i < 0) return;
    TransferItem &t = m_items[i];
    if (!t.isActive())
        return;
    // Mark canceled first so the reply's error handler won't flip it to Failed.
    t.state = TransferItem::Canceled;
    t.speedBps = 0;
    t.finished = QDateTime::currentDateTime();
    // REST providers: abort the attached network reply. Worker-thread/blocking
    // providers (SFTP/FTP/SMB) have no QNetworkReply, so also ask the provider
    // to abort — this stops the transfer immediately instead of letting the
    // background copy run to completion.
    if (t.reply)
        t.reply->abort();
    if (t.provider)
        t.provider->cancelTransfers();
    t.reply = nullptr;
    emit itemChanged(id);
}

void TransferManager::remove(int id)
{
    int i = indexOf(id);
    if (i < 0) return;
    // Abort first if somehow still active (network reply AND provider-level).
    if (m_items[i].isActive()) {
        if (m_items[i].reply)
            m_items[i].reply->abort();
        if (m_items[i].provider)
            m_items[i].provider->cancelTransfers();
    }
    m_items.remove(i);
    emit itemRemoved(id);
}

void TransferManager::retry(int id)
{
    int i = indexOf(id);
    if (i < 0) return;
    TransferItem t = m_items[i];           // copy the descriptor
    if (!t.provider)
        return;
    if (t.state == TransferItem::Running || t.state == TransferItem::Queued)
        return;
    // Reset counters and re-issue through the provider. The provider will call
    // begin() again for a fresh transfer; drop this stale entry.
    CloudProvider *p = t.provider;
    const TransferItem::Direction dir = t.direction;
    const QString remote = t.remotePath;
    const QString local  = t.localPath;
    remove(id);
    if (dir == TransferItem::Download)
        p->download(remote, local);
    else
        p->upload(local, remote);
}

void TransferManager::clearFinished()
{
    for (int i = m_items.size() - 1; i >= 0; --i) {
        if (!m_items[i].isActive()) {
            const int id = m_items[i].id;
            m_items.remove(i);
            emit itemRemoved(id);
        }
    }
}