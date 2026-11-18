#ifndef TRANSFERMANAGER_H
#define TRANSFERMANAGER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QDateTime>
#include <QPointer>

class CloudProvider;
class QNetworkReply;

// One upload or download transfer tracked by the TransferManager. Everything
// the transfer panel needs to show (name, direction, size, progress, speed,
// state) lives here so the UI is a pure view over this model.
struct TransferItem
{
    enum Direction { Download, Upload };
    enum State { Queued, Running, Paused, Completed, Failed, Canceled };

    int       id = 0;
    Direction direction = Download;
    State     state = Queued;

    QString   driveLabel;    // which cloud drive, e.g. "BaiduPan-work"
    QString   name;          // file display name
    QString   remotePath;    // path/id on the cloud
    QString   localPath;     // local file path

    qint64    bytesDone = 0;
    qint64    bytesTotal = 0;
    double    speedBps = 0;      // bytes/second (smoothed)
    QString   error;             // failure message when state == Failed

    QDateTime started;
    QDateTime finished;

    // The provider that owns this transfer (not owned here). Kept so retry can
    // re-issue the operation and so cancel can abort the right reply.
    QPointer<CloudProvider> provider;
    // The in-flight network reply (if any) so cancel() can abort it.
    QPointer<QNetworkReply> reply;
    // Smoothing helpers for the speed readout.
    QDateTime lastTick;
    qint64    lastBytes = 0;

    double percent() const {
        return bytesTotal > 0 ? (100.0 * double(bytesDone) / double(bytesTotal)) : 0.0;
    }
    bool isActive() const { return state == Queued || state == Running || state == Paused; }
    QString stateText() const;
    QString directionText() const;
};

// Application-wide registry of cloud transfers. Providers report progress here;
// the TransferPanel renders the list and issues cancel/remove/retry commands.
//
// The manager is UI-agnostic: it only holds state and emits change signals. The
// actual byte movement is performed by the providers, which call the begin /
// progress / finish / fail hooks below.
class TransferManager : public QObject
{
    Q_OBJECT
public:
    static TransferManager *instance();

    const QVector<TransferItem> &items() const { return m_items; }
    TransferItem *find(int id);

    // --- hooks called by providers / the browser tab ---
    // Register a new transfer and return its id. The caller fills remote/local
    // paths and direction; state starts Queued.
    int begin(TransferItem::Direction dir, const QString &driveLabel,
              const QString &name, const QString &remotePath,
              const QString &localPath, CloudProvider *provider,
              qint64 totalHint = 0);
    void markRunning(int id);
    void updateProgress(int id, qint64 done, qint64 total);
    void markCompleted(int id);
    void markFailed(int id, const QString &error);
    void markCanceled(int id);

    // Associate a running network reply so cancel() can abort it.
    void attachReply(int id, QNetworkReply *reply);

    // --- commands issued by the UI ---
    void cancel(int id);          // abort an active transfer
    void remove(int id);          // drop a finished/canceled entry from the list
    void retry(int id);           // re-issue a failed/canceled transfer
    void clearFinished();         // remove all completed/failed/canceled entries

signals:
    void itemAdded(int id);
    void itemChanged(int id);
    void itemRemoved(int id);

private:
    explicit TransferManager(QObject *parent = nullptr);
    int indexOf(int id) const;

    QVector<TransferItem> m_items;
    int m_nextId = 1;
};

#endif // TRANSFERMANAGER_H