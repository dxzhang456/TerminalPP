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

// Beyond Compare-style side-by-side text comparison tab.
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

private:
    void rebuildFromRows();
    void updateStats();

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