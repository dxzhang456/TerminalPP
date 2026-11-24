#ifndef TERMINALWIDGET_H
#define TERMINALWIDGET_H

#include <QAbstractScrollArea>
#include <QColor>
#include <QFont>

class AnsiParser;

// A VT100 terminal display widget, replicating MuxTerm's terminal renderer
// and PuTTY's terminal window: paints the AnsiParser screen buffer, handles
// keyboard input, selection/copy, scrollback, and Chinese IME positioning.
class TerminalWidget : public QAbstractScrollArea
{
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget *parent = nullptr);

    AnsiParser *parser() const { return m_parser; }

    void setColors(const QColor &fg, const QColor &bg);
    void setTerminalFont(const QFont &f);
    QFont terminalFont() const { return m_font; }

    int termRows() const;
    int termCols() const;

    void clearScreen();

    // --- Clipboard ---------------------------------------------------------
    // Exposed publicly because the main window's Edit menu owns the Ctrl+C /
    // Ctrl+V / Ctrl+A accelerators: a QAction shortcut always wins over a
    // widget key press, so without routing Edit > Paste here Ctrl+V would do
    // nothing at all in a terminal tab.
    void pasteClipboard();        // clipboard -> remote side
    void copySelectionToClipboard();
    void selectAllVisible();      // select the whole scrollback + screen

signals:
    // Bytes the user typed, to be sent to the remote side.
    void dataEntered(const QByteArray &data);
    // Emitted when the widget is resized to a new terminal grid.
    void sizeChanged(int rows, int cols);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    void wheelEvent(QWheelEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool event(QEvent *event) override;

private slots:
    void onScreenUpdated();

private:
    void recalcGrid();
    void updateScrollBar();
    QByteArray keyToBytes(QKeyEvent *event);
    void copySelection();

    AnsiParser *m_parser = nullptr;
    QFont   m_font;
    int     m_charWidth = 8;
    int     m_charHeight = 16;
    int     m_rows = 24;
    int     m_cols = 80;

    QColor  m_defaultFg = QColor(220, 220, 220);
    QColor  m_defaultBg = QColor(0, 0, 0);

    bool    m_selecting = false;
    QPoint  m_selStart;    // (col,row) in document space
    QPoint  m_selEnd;

    int     m_scrollOffset = 0; // 0 = at bottom
};

#endif // TERMINALWIDGET_H