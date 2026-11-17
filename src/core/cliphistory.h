#ifndef CLIPHISTORY_H
#define CLIPHISTORY_H

#include <QObject>
#include <QStringList>

// Tracks the system clipboard and keeps a bounded history of distinct text
// entries. Newest entries are at the front. The history is persisted via
// QSettings so it survives application restarts.
class ClipHistory : public QObject
{
    Q_OBJECT
public:
    static ClipHistory *instance();

    // Start watching the clipboard (call once after QApplication exists).
    void initialize();

    const QStringList &entries() const { return m_entries; }
    int maxEntries() const { return m_max; }
    void setMaxEntries(int n);

    void clear();

signals:
    void historyChanged();

private slots:
    void onClipboardChanged();

private:
    explicit ClipHistory(QObject *parent = nullptr);
    void load();
    void save() const;

    QStringList m_entries;
    int m_max = 50;
    bool m_initialized = false;
};

#endif // CLIPHISTORY_H