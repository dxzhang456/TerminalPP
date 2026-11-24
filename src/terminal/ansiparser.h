#ifndef ANSIPARSER_H
#define ANSIPARSER_H

#include <QObject>
#include <QVector>
#include <QColor>
#include <QString>

// A terminal cell: one character + attributes.
//
// East-Asian width: a CJK ideograph, a full-width form or an emoji occupies
// TWO terminal columns, exactly as it does in PuTTY / xterm. Such a glyph is
// stored once with `wide = true` and is followed by a placeholder cell with
// `continuation = true`, so the renderer can give it a two-cell box instead of
// squeezing it into one (which is what made Chinese text overlap).
struct TermCell {
    QChar ch = QChar(' ');
    QColor fg;
    QColor bg;
    bool bold = false;
    bool underline = false;
    bool italic = false;
    bool reverse = false;
    bool hasFg = false;
    bool hasBg = false;
    bool wide = false;          // this cell carries a double-width glyph
    bool continuation = false;  // right half of the preceding wide glyph
};

// True when `c` is rendered two terminal columns wide (East Asian Wide /
// Fullwidth, plus the common emoji and box ranges that behave the same way).
bool termCharIsWide(uint c);

// A VT100 / xterm-256color screen buffer + ANSI escape sequence interpreter.
// Shared by the SSH and Serial terminals, replicating MuxTerm's VT100 engine
// and PuTTY's terminal emulation.
class AnsiParser : public QObject
{
    Q_OBJECT
public:
    explicit AnsiParser(QObject *parent = nullptr);

    void setSize(int rows, int cols);
    int rows() const { return m_rows; }
    int cols() const { return m_cols; }

    // Feed raw bytes received from the remote side.
    void feed(const QByteArray &data);

    // Access the visible screen.
    const QVector<QVector<TermCell>> &screen() const { return m_screen; }
    int cursorRow() const { return m_curRow; }
    int cursorCol() const { return m_curCol; }
    bool cursorVisible() const { return m_cursorVisible; }

    // Scrollback.
    const QVector<QVector<TermCell>> &scrollback() const { return m_scrollback; }
    int scrollbackSize() const { return m_scrollback.size(); }

    void reset();

    // 256-color palette lookup.
    static QColor xterm256(int index);

signals:
    void screenUpdated();
    void bell();
    void titleChanged(const QString &title);

private:
    void putChar(QChar c);
    void newline();
    void carriageReturn();
    void backspace();
    void tab();
    void processCsi(const QString &seq, QChar final);
    void processOsc(const QString &seq);
    void setGraphicsRendition(const QList<int> &params);
    void eraseInDisplay(int mode);
    void eraseInLine(int mode);
    void scrollUp(int n);
    void insertBlankRowAtScroll();
    void clampCursor();
    TermCell blankCell() const;

    int m_rows = 24;
    int m_cols = 80;
    int m_curRow = 0;
    int m_curCol = 0;
    int m_savedRow = 0;
    int m_savedCol = 0;
    bool m_cursorVisible = true;
    bool m_wrapPending = false;

    // scrolling region
    int m_scrollTop = 0;
    int m_scrollBottom = 23;

    TermCell m_currentAttr;

    QVector<QVector<TermCell>> m_screen;
    QVector<QVector<TermCell>> m_scrollback;
    int m_maxScrollback = 5000;

    // Escape parsing state machine.
    enum State { Normal, Escape, Csi, Osc } m_state = Normal;
    QString m_collect;
    QByteArray m_utf8Pending;
};

#endif // ANSIPARSER_H