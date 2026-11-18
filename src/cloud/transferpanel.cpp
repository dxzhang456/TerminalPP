#include "transferpanel.h"
#include "transfermanager.h"
#include "core/i18n.h"

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QAbstractItemView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QLabel>
#include <QMenu>
#include <QHeaderView>
#include <QProgressBar>
#include <QLocale>
#include <QFileInfo>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QApplication>
#include <QClipboard>

static QString TRc(const char *s) { return I18n::instance()->tr(QString::fromUtf8(s)); }

namespace {
enum Col { ColDir = 0, ColName, ColDrive, ColSize, ColProgress, ColSpeed, ColState, ColCount };
const int RoleId = Qt::UserRole + 1;

QString humanBytes(qint64 n)
{
    if (n <= 0) return QStringLiteral("—");
    return QLocale().formattedDataSize(n, 1, QLocale::DataSizeTraditionalFormat);
}
QString humanSpeed(double bps)
{
    if (bps <= 0) return QString();
    return humanBytes(qint64(bps)) + QStringLiteral("/s");
}
} // namespace

TransferPanel::TransferPanel(QWidget *parent) : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *tb = new QToolBar(this);
    tb->addAction(TRc("Cancel"), this, [this]() {
        if (auto *it = m_tree->currentItem())
            TransferManager::instance()->cancel(idForItem(it));
    });
    tb->addAction(TRc("Retry"), this, [this]() {
        if (auto *it = m_tree->currentItem())
            TransferManager::instance()->retry(idForItem(it));
    });
    tb->addAction(TRc("Remove"), this, [this]() {
        if (auto *it = m_tree->currentItem())
            TransferManager::instance()->remove(idForItem(it));
    });
    tb->addSeparator();
    tb->addAction(TRc("Open"), this, [this]() {
        if (auto *it = m_tree->currentItem()) {
            if (TransferItem *t = TransferManager::instance()->find(idForItem(it)))
                if (t->direction == TransferItem::Download
                    && t->state == TransferItem::Completed)
                    emit openLocalFile(t->localPath);
        }
    });
    tb->addAction(TRc("Open Folder"), this, [this]() {
        if (auto *it = m_tree->currentItem()) {
            if (TransferItem *t = TransferManager::instance()->find(idForItem(it))) {
                const QString dir = QFileInfo(t->localPath).absolutePath();
                QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
            }
        }
    });
    tb->addSeparator();
    tb->addAction(TRc("Clear Finished"), this,
                  []() { TransferManager::instance()->clearFinished(); });
    outer->addWidget(tb);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(ColCount);
    m_tree->setHeaderLabels({ TRc("Type"), TRc("Name"), TRc("Cloud"),
                              TRc("Size"), TRc("Progress"), TRc("Speed"),
                              TRc("Status") });
    m_tree->setRootIsDecorated(false);
    m_tree->setAlternatingRowColors(true);
    m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->header()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &TransferPanel::onContextMenu);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem *it, int) {
        if (TransferItem *t = TransferManager::instance()->find(idForItem(it)))
            if (t->direction == TransferItem::Download
                && t->state == TransferItem::Completed)
                emit openLocalFile(t->localPath);
    });
    outer->addWidget(m_tree, 1);

    m_summary = new QLabel(this);
    m_summary->setContentsMargins(6, 2, 6, 2);
    outer->addWidget(m_summary);

    TransferManager *tm = TransferManager::instance();
    connect(tm, &TransferManager::itemAdded, this, &TransferPanel::onItemAdded);
    connect(tm, &TransferManager::itemChanged, this, &TransferPanel::onItemChanged);
    connect(tm, &TransferManager::itemRemoved, this, &TransferPanel::onItemRemoved);

    rebuild();
}

QTreeWidgetItem *TransferPanel::rowFor(int id) const
{
    return m_rows.value(id, nullptr);
}

int TransferPanel::idForItem(QTreeWidgetItem *item) const
{
    return item ? item->data(ColDir, RoleId).toInt() : -1;
}

void TransferPanel::rebuild()
{
    m_tree->clear();
    m_rows.clear();
    for (const TransferItem &t : TransferManager::instance()->items())
        onItemAdded(t.id);
}

void TransferPanel::onItemAdded(int id)
{
    if (m_rows.contains(id))
        return;
    auto *item = new QTreeWidgetItem(m_tree);
    item->setData(ColDir, RoleId, id);
    auto *bar = new QProgressBar(m_tree);
    bar->setRange(0, 100);
    bar->setTextVisible(true);
    m_tree->setItemWidget(item, ColProgress, bar);
    m_rows.insert(id, item);
    refreshRow(id);
}

void TransferPanel::onItemChanged(int id)
{
    refreshRow(id);
}

void TransferPanel::onItemRemoved(int id)
{
    if (auto *item = m_rows.take(id)) {
        delete item;   // also removes the embedded progress bar widget
    }
    // Update the summary line.
    refreshRow(-1);
}

void TransferPanel::refreshRow(int id)
{
    TransferManager *tm = TransferManager::instance();

    if (id >= 0) {
        QTreeWidgetItem *item = rowFor(id);
        TransferItem *t = tm->find(id);
        if (item && t) {
            item->setText(ColDir, t->directionText());
            item->setText(ColName, t->name);
            item->setText(ColDrive, t->driveLabel);
            item->setText(ColSize, humanBytes(t->bytesTotal));
            item->setText(ColSpeed, humanSpeed(t->speedBps));
            item->setText(ColState,
                t->state == TransferItem::Failed && !t->error.isEmpty()
                    ? (t->stateText() + ": " + t->error)
                    : t->stateText());
            if (auto *bar = qobject_cast<QProgressBar *>(
                    m_tree->itemWidget(item, ColProgress))) {
                if (t->bytesTotal > 0)
                    bar->setValue(int(t->percent()));
                else
                    bar->setRange(0, t->isActive() ? 0 : 100);   // busy indicator
                bar->setFormat(QString("%1 / %2")
                    .arg(humanBytes(t->bytesDone), humanBytes(t->bytesTotal)));
            }
        }
    }

    // Summary: counts of active / done / failed.
    int active = 0, done = 0, failed = 0;
    for (const TransferItem &t : tm->items()) {
        if (t.isActive()) ++active;
        else if (t.state == TransferItem::Completed) ++done;
        else if (t.state == TransferItem::Failed) ++failed;
    }
    m_summary->setText(TRc("Active: %1   Completed: %2   Failed: %3")
                           .arg(active).arg(done).arg(failed));
}

void TransferPanel::onContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    if (!item) return;
    const int id = idForItem(item);
    TransferItem *t = TransferManager::instance()->find(id);
    if (!t) return;

    QMenu menu(this);
    if (t->isActive())
        menu.addAction(TRc("Cancel"), this,
                       [id]() { TransferManager::instance()->cancel(id); });
    if (t->state == TransferItem::Failed || t->state == TransferItem::Canceled)
        menu.addAction(TRc("Retry"), this,
                       [id]() { TransferManager::instance()->retry(id); });
    if (t->direction == TransferItem::Download && t->state == TransferItem::Completed) {
        menu.addAction(TRc("Open"), this, [this, t]() { emit openLocalFile(t->localPath); });
    }
    menu.addAction(TRc("Open Folder"), this, [t]() {
        QDesktopServices::openUrl(
            QUrl::fromLocalFile(QFileInfo(t->localPath).absolutePath()));
    });
    menu.addSeparator();
    // Copy submenu: file name, local path, or remote path to the clipboard.
    QMenu *copyMenu = menu.addMenu(TRc("Copy"));
    copyMenu->addAction(TRc("Copy Name"), this, [t]() {
        QApplication::clipboard()->setText(t->name);
    });
    copyMenu->addAction(TRc("Copy Local Path"), this, [t]() {
        QApplication::clipboard()->setText(t->localPath);
    });
    copyMenu->addAction(TRc("Copy Remote Path"), this, [t]() {
        QApplication::clipboard()->setText(t->remotePath);
    });
    menu.addSeparator();
    menu.addAction(TRc("Remove"), this,
                   [id]() { TransferManager::instance()->remove(id); });
    menu.addAction(TRc("Clear Finished"), this,
                   []() { TransferManager::instance()->clearFinished(); });
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}