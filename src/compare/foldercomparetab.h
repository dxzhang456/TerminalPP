#ifndef FOLDERCOMPARETAB_H
#define FOLDERCOMPARETAB_H

#include <QWidget>
#include <QString>
#include "editor/thememanager.h"

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QToolBar;
class QLineEdit;

// side-by-side folder comparison tab.
//  * recursive side-by-side tree of two directories
//  * per-entry status: Same / Left-newer / Right-newer / Left-only / Right-only
//    / Different-size
//  * color coding of the status column
//  * name filter (glob), show-only-differences toggle
//  * double-click a differing file pair -> emits compareFiles() so the host can
//    open a Text/Hex compare tab
//  * copy left->right / right->left of the selected file, refresh
class FolderCompareTab : public QWidget
{
    Q_OBJECT
public:
    explicit FolderCompareTab(QWidget *parent = nullptr);

    bool setFolders(const QString &leftDir, const QString &rightDir);
    void applyTheme(const EditorTheme &theme);

    QString leftDir() const { return m_leftDir; }
    QString rightDir() const { return m_rightDir; }

signals:
    void statusMessage(const QString &msg);
    // Request the host to open a file comparison for a differing pair.
    void compareFiles(const QString &leftFile, const QString &rightFile);

public slots:
    void refresh();
    void copyLeftToRight();
    void copyRightToLeft();
    void setShowOnlyDifferences(bool on);
    void setNameFilter(const QString &glob);

private:
    enum Status { Same, LeftNewer, RightNewer, LeftOnly, RightOnly, DiffSize, FolderNode };

    void scanInto(QTreeWidgetItem *parent,
                  const QString &leftDir, const QString &rightDir,
                  const QString &relPath);
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);
    void applyFilterVisibility();

    QTreeWidget *m_tree = nullptr;
    QLabel *m_leftLabel = nullptr;
    QLabel *m_rightLabel = nullptr;
    QLabel *m_statsLabel = nullptr;
    QLineEdit *m_filterEdit = nullptr;

    QString m_leftDir;
    QString m_rightDir;
    bool m_onlyDiffs = false;
    QString m_filterGlob;

    int m_cntSame = 0, m_cntDiff = 0, m_cntLeftOnly = 0, m_cntRightOnly = 0;
};

#endif // FOLDERCOMPARETAB_H