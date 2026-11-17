#ifndef HEXCOMPARETAB_H
#define HEXCOMPARETAB_H

#include <QWidget>
#include <QByteArray>
#include "editor/thememanager.h"

class QPlainTextEdit;
class QLabel;
class HexDiffHighlighter;

// Beyond Compare-style hex (binary) comparison tab.
//  * two side-by-side hex dumps (offset | hex bytes | ASCII)
//  * differing bytes highlighted on both sides
//  * synchronized scrolling
//  * stats: equal / different byte counts, size difference
class HexCompareTab : public QWidget
{
    Q_OBJECT
public:
    explicit HexCompareTab(QWidget *parent = nullptr);

    bool loadFiles(const QString &leftPath, const QString &rightPath);
    void applyTheme(const EditorTheme &theme);

    QString leftPath() const { return m_leftPath; }
    QString rightPath() const { return m_rightPath; }

signals:
    void statusMessage(const QString &msg);

public slots:
    void reload();

private:
    void rebuild();

    QPlainTextEdit *m_left = nullptr;
    QPlainTextEdit *m_right = nullptr;
    QLabel *m_leftLabel = nullptr;
    QLabel *m_rightLabel = nullptr;
    QLabel *m_statsLabel = nullptr;

    QByteArray m_leftData;
    QByteArray m_rightData;
    QString m_leftPath;
    QString m_rightPath;

    HexDiffHighlighter *m_leftHl = nullptr;
    HexDiffHighlighter *m_rightHl = nullptr;

    QColor m_diffColor = QColor(180, 60, 60);
};

#endif // HEXCOMPARETAB_H