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
//  * column (block / rectangular) editing mode (Alt + drag, like Notepad++)
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

    // Column (rectangular / block) selection mode. When active the user can
    // Alt+drag to select a rectangle and type / delete / paste across lines.
    bool hasColumnSelection() const { return m_columnMode && m_colValid; }

signals:
    // Emitted when the column-mode state changes (for the status bar indicator).
    void columnModeChanged(bool active);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    // Routes IME-committed text (Chinese/Japanese/Korean input) into the
    // column insertion path while column (box) mode is active, so CJK input
    // is distributed across every selected row instead of landing at a
    // single cursor position.
    void inputMethodEvent(QInputMethodEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    void matchBrackets();
    QString currentIndent() const;

    // --- Column editing helpers ---
    // Convert a viewport point to (block, columnInChars).
    void pointToBlockColumn(const QPoint &vpPos, int &block, int &column) const;
    // Position of a (block, column) char cell, in viewport coordinates.
    QRect cellRect(int block, int column) const;
    // Clear the column selection and leave column mode.
    void clearColumnSelection();
    // Copy the rectangular selection text (lines joined with '\n').
    QString columnSelectionText() const;
    // Delete the rectangular selection (returns true if anything removed).
    bool deleteColumnSelection();
    // Insert text into every selected line at the block's left column.
    void insertIntoColumn(const QString &text);
    // Paste a (possibly multi-line) string line-by-line into the block rows.
    void pasteColumn(const QString &text);
    // VS Code style keyboard column selection: Ctrl+Shift+Up/Down starts (if
    // not already active) or extends the column (box) selection by one line,
    // keeping the same character column. dy is -1 (up) or +1 (down).
    void extendColumnSelectionVertically(int dy);

    LineNumberArea *m_lineNumberArea = nullptr;
    bool   m_showLineNumbers = true;
    int    m_tabWidth = 4;
    bool   m_useSpaces = true;
    QColor m_currentLineColor = QColor(232, 242, 254);
    QColor m_lnBg = QColor(240, 240, 240);
    QColor m_lnFg = QColor(130, 130, 130);

    // --- Column (block) selection state ---
    bool m_columnMode   = false; // currently dragging/holding a column selection
    bool m_colValid     = false; // the rectangle spans a real selection
    bool m_colDragging  = false; // mouse button held during Alt+drag
    int  m_colAnchorBlk = 0;     // anchor block (line) index
    int  m_colAnchorCol = 0;     // anchor column (in characters)
    int  m_colCaretBlk  = 0;     // moving-end block index
    int  m_colCaretCol  = 0;     // moving-end column (in characters)
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