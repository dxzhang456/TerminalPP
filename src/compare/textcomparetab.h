#ifndef TEXTCOMPARETAB_H
#define TEXTCOMPARETAB_H

#include <QWidget>
#include <QStringList>
#include <QVector>
#include "diffengine.h"
#include "editor/thememanager.h"

class QPlainTextEdit;
class QScrollBar;
class QLabel;
class QToolBar;
class DiffPane;   // internal painting pane (defined in .cpp)

// side-by-side side-by-side text comparison tab.
//  * two aligned panes with gaps for inserted/removed lines
//  * per-line coloring: changed / inserted / removed / same
//  * inline character-level highlighting of changes
//  * synchronized vertical scrolling
//  * next / previous difference navigation
//  * copy a difference section to the other side
//  * reload, swap sides, ignore case / whitespace toggles
class TextCompareTab : public QWidget
{
    Q_OBJECT
public:
    explicit TextCompareTab(QWidget *parent = nullptr);

    // Load the two files to compare.
    bool loadFiles(const QString &leftPath, const QString &rightPath);
    // Compare arbitrary in-memory text (used by "compare with clipboard" etc.)
    void setTexts(const QStringList &left, const QStringList &right,
                  const QString &leftTitle, const QString &rightTitle);

    void applyTheme(const EditorTheme &theme);

    QString leftPath() const { return m_leftPath; }
    QString rightPath() const { return m_rightPath; }

signals:
    void statusMessage(const QString &msg);
    // Request the host to reopen the current two files as another comparison
    // type (0=Text, 1=Folder, 2=Hex, 3=Image — matching CompareDialog::Type).
    void requestCompareAs(int type, const QString &left, const QString &right);

public slots:
    void recompute();
    void nextDiff();
    void prevDiff();
    void swapSides();
    void copyLeftToRight();
    void copyRightToLeft();
    void reload();
    void saveLeft();
    void saveRight();

private slots:
    // Row-level operations on the current selection (one line or many).
    // "Copy" overwrites the matching lines on the other side; "Move" copies
    // and then deletes them from the source side; "Delete" removes them from
    // the side they were selected in. Every operation re-runs the diff so the
    // comparison result updates immediately.
    void copySelectionToRight();
    void copySelectionToLeft();
    void moveSelectionToRight();
    void moveSelectionToLeft();
    void deleteSelection();
    // Edit the first selected line in place (context menu "Edit Line..." and
    // double-clicking a row both land here). The change is applied to the
    // in-memory line list and the diff is recomputed immediately; use
    // "Save L" / "Save R" to write it back to disk.
    void editSelectedLine();
    // Pop up the row action menu for the pane the request came from.
    void showRowMenu(const QPoint &globalPos, bool fromLeft);

private:
    void rebuildFromRows();
    void updateStats();

    // Shared implementation for the five row operations above.
    //   fromLeft : the side the selected rows belong to
    //   copy     : write the selected lines onto the other side
    //   erase    : delete the selected lines from their own side
    // (copy=true, erase=false) == Copy, (true,true) == Move,
    // (false,true) == Delete.
    void applyRowOperation(bool fromLeft, bool copy, bool erase);
    // Translate the pane's selected row range into this side's line indices.
    // Returns false when the selection covers only gap rows.
    bool selectionLineRange(bool fromLeft, int *firstLine, int *lastLine,
                            int *insertAtOther) const;

    // The pane the selection currently lives in and its row range, tracked so
    // the context menu knows what to act on.
    bool m_selFromLeft = true;
    int  m_selFirstRow = -1;
    int  m_selLastRow = -1;

    DiffPane *m_left = nullptr;
    DiffPane *m_right = nullptr;
    QLabel *m_leftLabel = nullptr;
    QLabel *m_rightLabel = nullptr;
    QLabel *m_statsLabel = nullptr;

    QStringList m_leftLines;
    QStringList m_rightLines;
    QString m_leftPath;
    QString m_rightPath;

    QVector<Diff::Row> m_rows;
    Diff::Options m_opts;

    int m_currentDiff = -1;   // index of the current difference block
};

#endif // TEXTCOMPARETAB_H