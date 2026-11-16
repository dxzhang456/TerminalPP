#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QPlainTextEdit>
#include <QObject>

class LineNumberArea;

// A capable code editor widget replicating Notepad--'s core editing pane:
//  * line number gutter
//  * current line highlighting
//  * tab/space handling, auto-indent
//  * bracket matching
//  * zoom in/out
class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit CodeEditor(QWidget *parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int  lineNumberAreaWidth();

    void setShowLineNumbers(bool show);
    bool showLineNumbers() const { return m_showLineNumbers; }

    void setTabWidthChars(int chars);
    int  tabWidthChars() const { return m_tabWidth; }

    void setUseSpacesForTab(bool use) { m_useSpaces = use; }
    bool useSpacesForTab() const { return m_useSpaces; }

    void setCurrentLineColor(const QColor &c);
    void setLineNumberColors(const QColor &bg, const QColor &fg);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    void matchBrackets();
    QString currentIndent() const;

    LineNumberArea *m_lineNumberArea = nullptr;
    bool   m_showLineNumbers = true;
    int    m_tabWidth = 4;
    bool   m_useSpaces = true;
    QColor m_currentLineColor = QColor(232, 242, 254);
    QColor m_lnBg = QColor(240, 240, 240);
    QColor m_lnFg = QColor(130, 130, 130);
};

// The gutter widget painted alongside the editor.
class LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(CodeEditor *editor)
        : QWidget(editor), m_editor(editor) {}

    QSize sizeHint() const override
    {
        return QSize(m_editor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        m_editor->lineNumberAreaPaintEvent(event);
    }

private:
    CodeEditor *m_editor;
};

#endif // CODEEDITOR_H