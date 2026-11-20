#ifndef TRANSFERPANEL_H
#define TRANSFERPANEL_H

#include <QWidget>
#include <QHash>

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QSpinBox;

// A dockable view over the TransferManager. Shows every upload/download with a
// direction icon, name, drive, size, progress bar, speed, ETA and state. The
// context menu / toolbar offer Cancel, Retry, Remove, Open, Open Folder and
// Clear Finished.
class TransferPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TransferPanel(QWidget *parent = nullptr);

signals:
    // Ask the host window to open a finished download locally (editor / viewer).
    void openLocalFile(const QString &localPath);

private slots:
    void onItemAdded(int id);
    void onItemChanged(int id);
    void onItemRemoved(int id);
    void onContextMenu(const QPoint &pos);

private:
    void rebuild();
    void refreshRow(int id);
    QTreeWidgetItem *rowFor(int id) const;
    int idForItem(QTreeWidgetItem *item) const;

    QTreeWidget *m_tree = nullptr;
    QLabel *m_summary = nullptr;
    QSpinBox *m_maxTasksSpin = nullptr;
    QLabel *m_maxTasksLabel = nullptr;
    QHash<int, QTreeWidgetItem *> m_rows;   // transfer id -> row
};

#endif // TRANSFERPANEL_H