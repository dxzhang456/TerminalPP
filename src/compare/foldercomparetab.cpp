#include "foldercomparetab.h"
#include "core/i18n.h"
static inline QString TRc(const char *s){ return I18n::instance()->tr(QString::fromUtf8(s)); }

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolBar>
#include "core/iconfactory.h"
#include <QAction>
#include <QLineEdit>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QCryptographicHash>
#include <QHeaderView>
#include <QLocale>
#include <QSet>
#include <QBrush>
#include <QDateTime>
#include <QRegularExpression>
#include <functional>

namespace {

enum Column { ColName = 0, ColLeftSize, ColLeftDate, ColStatus,
              ColRightSize, ColRightDate, ColCount };

// The status is stored in the item's data so filtering/coloring can use it.
const int RoleStatus   = Qt::UserRole + 1;
const int RoleLeftPath = Qt::UserRole + 2;
const int RoleRightPath= Qt::UserRole + 3;
const int RoleIsDir    = Qt::UserRole + 4;

QString humanSize(qint64 bytes)
{
    if (bytes < 0) return QString();
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

// Quick file equality: size first, then MD5 only if sizes match.
bool filesEqual(const QString &a, const QString &b, bool &sameSize)
{
    QFileInfo fa(a), fb(b);
    sameSize = (fa.size() == fb.size());
    if (!sameSize)
        return false;
    QFile f1(a), f2(b);
    if (!f1.open(QIODevice::ReadOnly) || !f2.open(QIODevice::ReadOnly))
        return false;
    QCryptographicHash h1(QCryptographicHash::Md5), h2(QCryptographicHash::Md5);
    h1.addData(&f1);
    h2.addData(&f2);
    return h1.result() == h2.result();
}

} // namespace

FolderCompareTab::FolderCompareTab(QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *tb = new QToolBar(this);
    tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
    tb->setIconSize(QSize(20, 20));
    auto addIconAction = [&](const char *src, auto slot) -> QAction * {
        QIcon icon = IconFactory::makeToolIcon(QString::fromUtf8(src));
        QAction *a = icon.isNull() ? tb->addAction(TRc(src), this, slot)
                                   : tb->addAction(icon, TRc(src), this, slot);
        a->setToolTip(TRc(src));
        return a;
    };
    addIconAction("Refresh", &FolderCompareTab::refresh);
    addIconAction("Copy →", &FolderCompareTab::copyLeftToRight);
    addIconAction("← Copy", &FolderCompareTab::copyRightToLeft);
    tb->addSeparator();
    QAction *diffOnly = tb->addAction(IconFactory::makeToolIcon(QStringLiteral("Differences Only")),
                                      TRc("Differences Only"));
    diffOnly->setToolTip(TRc("Differences Only"));
    diffOnly->setCheckable(true);
    connect(diffOnly, &QAction::toggled, this, &FolderCompareTab::setShowOnlyDifferences);
    tb->addSeparator();
    tb->addWidget(new QLabel(TRc(" Filter: "), this));
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(TRc("*.cpp;*.h   (glob, ; separated)"));
    m_filterEdit->setMaximumWidth(220);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &FolderCompareTab::setNameFilter);
    tb->addWidget(m_filterEdit);
    outer->addWidget(tb);

    auto *hdr = new QHBoxLayout();
    hdr->setContentsMargins(4, 2, 4, 2);
    m_leftLabel = new QLabel(TRc("(left folder)"), this);
    m_rightLabel = new QLabel(TRc("(right folder)"), this);
    m_leftLabel->setStyleSheet("font-weight:bold;");
    m_rightLabel->setStyleSheet("font-weight:bold;");
    hdr->addWidget(m_leftLabel, 1);
    hdr->addWidget(m_rightLabel, 1);
    outer->addLayout(hdr);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(ColCount);
    m_tree->setHeaderLabels({TRc("Name"), TRc("Size"), TRc("Modified"),
                             TRc("Status"), TRc("Size"), TRc("Modified")});
    m_tree->setAlternatingRowColors(true);
    m_tree->setUniformRowHeights(true);
    m_tree->header()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, &FolderCompareTab::onItemDoubleClicked);
    outer->addWidget(m_tree, 1);

    m_statsLabel = new QLabel(this);
    m_statsLabel->setContentsMargins(6, 2, 6, 2);
    outer->addWidget(m_statsLabel);
}

bool FolderCompareTab::setFolders(const QString &leftDir, const QString &rightDir)
{
    m_leftDir = leftDir;
    m_rightDir = rightDir;
    m_leftLabel->setText(leftDir);
    m_rightLabel->setText(rightDir);
    refresh();
    return QFileInfo(leftDir).isDir() && QFileInfo(rightDir).isDir();
}

void FolderCompareTab::refresh()
{
    m_tree->clear();
    m_cntSame = m_cntDiff = m_cntLeftOnly = m_cntRightOnly = 0;
    if (m_leftDir.isEmpty() || m_rightDir.isEmpty())
        return;

    QTreeWidgetItem *root = m_tree->invisibleRootItem();
    scanInto(root, m_leftDir, m_rightDir, QString());
    m_tree->expandToDepth(1);
    applyFilterVisibility();

    const QString msg = TRc("Same: %1   Different: %2   Left only: %3   Right only: %4")
                            .arg(m_cntSame).arg(m_cntDiff)
                            .arg(m_cntLeftOnly).arg(m_cntRightOnly);
    m_statsLabel->setText(msg);
    emit statusMessage(msg);
}

void FolderCompareTab::scanInto(QTreeWidgetItem *parent,
                                const QString &leftDir, const QString &rightDir,
                                const QString &relPath)
{
    // Collect the union of entry names on both sides. Each side may be empty
    // (directory missing on that side), in which case it contributes nothing.
    QSet<QString> names;
    if (!leftDir.isEmpty()) {
        QDir ld(leftDir);
        for (const QString &n : ld.entryList(QDir::AllEntries | QDir::NoDotAndDotDot))
            names.insert(n);
    }
    if (!rightDir.isEmpty()) {
        QDir rd(rightDir);
        for (const QString &n : rd.entryList(QDir::AllEntries | QDir::NoDotAndDotDot))
            names.insert(n);
    }

    QStringList sorted = names.values();
    sorted.sort(Qt::CaseInsensitive);

    for (const QString &name : std::as_const(sorted)) {
        const QString lPath = leftDir.isEmpty()  ? QString() : QDir(leftDir).filePath(name);
        const QString rPath = rightDir.isEmpty() ? QString() : QDir(rightDir).filePath(name);
        QFileInfo lfi(lPath), rfi(rPath);
        const bool lExists = !lPath.isEmpty() && lfi.exists();
        const bool rExists = !rPath.isEmpty() && rfi.exists();
        const bool lDir = lExists && lfi.isDir();
        const bool rDir = rExists && rfi.isDir();

        auto *item = new QTreeWidgetItem(parent);
        item->setText(ColName, name);
        item->setData(ColName, RoleLeftPath, lExists ? lPath : QString());
        item->setData(ColName, RoleRightPath, rExists ? rPath : QString());
        item->setData(ColName, RoleIsDir, (lDir || rDir));

        if ((lExists && lDir) || (rExists && rDir)) {
            // Directory node: recurse. Status derived from children.
            item->setData(ColName, RoleStatus, int(FolderNode));
            if (lExists) {
                item->setText(ColLeftSize, TRc("<dir>"));
            }
            if (rExists) {
                item->setText(ColRightSize, TRc("<dir>"));
            }
            // Recurse into whichever side(s) exist.
            scanInto(item,
                     lDir ? lPath : QString(),
                     rDir ? rPath : QString(),
                     relPath.isEmpty() ? name : relPath + "/" + name);

            if (!lExists) {
                item->setText(ColStatus, TRc("Right only"));
                item->setData(ColName, RoleStatus, int(RightOnly));
                ++m_cntRightOnly;
            } else if (!rExists) {
                item->setText(ColStatus, TRc("Left only"));
                item->setData(ColName, RoleStatus, int(LeftOnly));
                ++m_cntLeftOnly;
            } else {
                item->setText(ColStatus, TRc("Folder"));
            }
            continue;
        }

        // File node.
        if (lExists) {
            item->setText(ColLeftSize, humanSize(lfi.size()));
            item->setText(ColLeftDate, lfi.lastModified().toString("yyyy-MM-dd HH:mm"));
        }
        if (rExists) {
            item->setText(ColRightSize, humanSize(rfi.size()));
            item->setText(ColRightDate, rfi.lastModified().toString("yyyy-MM-dd HH:mm"));
        }

        Status st;
        if (lExists && !rExists) { st = LeftOnly; ++m_cntLeftOnly; }
        else if (!lExists && rExists) { st = RightOnly; ++m_cntRightOnly; }
        else {
            bool sameSize = false;
            bool eq = filesEqual(lPath, rPath, sameSize);
            if (eq) { st = Same; ++m_cntSame; }
            else {
                ++m_cntDiff;
                if (lfi.lastModified() > rfi.lastModified()) st = LeftNewer;
                else if (rfi.lastModified() > lfi.lastModified()) st = RightNewer;
                else st = DiffSize;
            }
        }
        item->setData(ColName, RoleStatus, int(st));

        switch (st) {
        case Same:       item->setText(ColStatus, TRc("Same")); break;
        case LeftNewer:  item->setText(ColStatus, TRc("Left newer")); break;
        case RightNewer: item->setText(ColStatus, TRc("Right newer")); break;
        case LeftOnly:   item->setText(ColStatus, TRc("Left only")); break;
        case RightOnly:  item->setText(ColStatus, TRc("Right only")); break;
        case DiffSize:   item->setText(ColStatus, TRc("Different")); break;
        default: break;
        }
    }
}

void FolderCompareTab::onItemDoubleClicked(QTreeWidgetItem *item, int)
{
    if (!item)
        return;
    const bool isDir = item->data(ColName, RoleIsDir).toBool();
    if (isDir)
        return;
    const QString l = item->data(ColName, RoleLeftPath).toString();
    const QString r = item->data(ColName, RoleRightPath).toString();
    if (!l.isEmpty() && !r.isEmpty())
        emit compareFiles(l, r);
}

// Recursively copy a file or an entire directory tree from src to dst,
// overwriting whatever is already at dst.
static void copyRecursive(const QString &src, const QString &dst)
{
    QFileInfo sfi(src);
    if (sfi.isDir()) {
        QDir().mkpath(dst);
        QDir sd(src);
        const auto entries = sd.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
        for (const QString &name : entries)
            copyRecursive(sd.filePath(name), QDir(dst).filePath(name));
    } else {
        QFile::remove(dst);
        QDir().mkpath(QFileInfo(dst).absolutePath());
        QFile::copy(src, dst);
    }
}

static void copyFileOverwrite(const QString &src, const QString &dst)
{
    copyRecursive(src, dst);
}

void FolderCompareTab::copyLeftToRight()
{
    QTreeWidgetItem *item = m_tree->currentItem();
    if (!item) return;
    const QString l = item->data(ColName, RoleLeftPath).toString();
    if (l.isEmpty()) return;
    // Compute destination path mirroring the structure.
    QString rel = QDir(m_leftDir).relativeFilePath(l);
    QString dst = QDir(m_rightDir).filePath(rel);
    copyFileOverwrite(l, dst);
    refresh();
    emit statusMessage(TRc("Copied to right: %1").arg(rel));
}

void FolderCompareTab::copyRightToLeft()
{
    QTreeWidgetItem *item = m_tree->currentItem();
    if (!item) return;
    const QString r = item->data(ColName, RoleRightPath).toString();
    if (r.isEmpty()) return;
    QString rel = QDir(m_rightDir).relativeFilePath(r);
    QString dst = QDir(m_leftDir).filePath(rel);
    copyFileOverwrite(r, dst);
    refresh();
    emit statusMessage(TRc("Copied to left: %1").arg(rel));
}

void FolderCompareTab::setShowOnlyDifferences(bool on)
{
    m_onlyDiffs = on;
    applyFilterVisibility();
}

void FolderCompareTab::setNameFilter(const QString &glob)
{
    m_filterGlob = glob.trimmed();
    applyFilterVisibility();
}

// Recursively apply "differences only" + name-glob filters, hiding items that
// don't match. A folder stays visible if any descendant is visible.
static bool applyFilterRec(QTreeWidgetItem *item, bool onlyDiffs,
                           const QStringList &globs)
{
    bool anyChildVisible = false;
    for (int i = 0; i < item->childCount(); ++i)
        anyChildVisible |= applyFilterRec(item->child(i), onlyDiffs, globs);

    const int st = item->data(0, RoleStatus).toInt();
    const bool isDir = item->data(0, RoleIsDir).toBool();

    bool selfVisible = true;

    if (onlyDiffs && !isDir && st == 0 /*Same*/)
        selfVisible = false;

    if (!globs.isEmpty() && !isDir) {
        const QString name = item->text(0);
        bool match = false;
        for (const QString &g : globs) {
            QRegularExpression re(QRegularExpression::wildcardToRegularExpression(g),
                                  QRegularExpression::CaseInsensitiveOption);
            if (re.match(name).hasMatch()) { match = true; break; }
        }
        if (!match)
            selfVisible = false;
    }

    const bool visible = isDir ? anyChildVisible : selfVisible;
    item->setHidden(!visible);
    return visible;
}

void FolderCompareTab::applyFilterVisibility()
{
    QStringList globs;
    if (!m_filterGlob.isEmpty())
        globs = m_filterGlob.split(QRegularExpression("[;,]"), Qt::SkipEmptyParts);

    QTreeWidgetItem *root = m_tree->invisibleRootItem();
    for (int i = 0; i < root->childCount(); ++i)
        applyFilterRec(root->child(i), m_onlyDiffs, globs);
}

void FolderCompareTab::applyTheme(const EditorTheme &t)
{
    // Color the status column per status, themed for dark/light.
    const bool dark = t.background.lightness() < 128;
    QColor cSame   = t.foreground;
    QColor cLeft   = dark ? QColor(120, 200, 255) : QColor(0, 90, 200);
    QColor cRight  = dark ? QColor(160, 230, 160) : QColor(0, 140, 0);
    QColor cOnly   = dark ? QColor(255, 180, 120) : QColor(200, 90, 0);
    QColor cDiff   = dark ? QColor(255, 150, 150) : QColor(200, 0, 0);

    std::function<void(QTreeWidgetItem *)> colorRec = [&](QTreeWidgetItem *it) {
        const int st = it->data(ColName, RoleStatus).toInt();
        QColor c = cSame;
        switch (st) {
        case LeftNewer:  c = cLeft;  break;
        case RightNewer: c = cRight; break;
        case LeftOnly:
        case RightOnly:  c = cOnly;  break;
        case DiffSize:   c = cDiff;  break;
        default: break;
        }
        it->setForeground(ColStatus, QBrush(c));
        for (int i = 0; i < it->childCount(); ++i)
            colorRec(it->child(i));
    };
    QTreeWidgetItem *root = m_tree->invisibleRootItem();
    for (int i = 0; i < root->childCount(); ++i)
        colorRec(root->child(i));
}