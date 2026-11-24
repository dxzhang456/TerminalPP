#include "ansiparser.h"
#include <QStringDecoder>

AnsiParser::AnsiParser(QObject *parent)
    : QObject(parent)
{
    setSize(24, 80);
}

TermCell AnsiParser::blankCell() const
{
    TermCell c;
    c.ch = QChar(' ');
    return c;
}

void AnsiParser::setSize(int rows, int cols)
{
    m_rows = qMax(1, rows);
    m_cols = qMax(1, cols);

    m_screen.resize(m_rows);
    for (auto &row : m_screen) {
        int old = row.size();
        row.resize(m_cols);
        for (int i = old; i < m_cols; ++i)
            row[i] = blankCell();
    }
    m_scrollTop = 0;
    m_scrollBottom = m_rows - 1;
    clampCursor();
    emit screenUpdated();
}

void AnsiParser::reset()
{
    m_curRow = m_curCol = 0;
    m_savedRow = m_savedCol = 0;
    m_cursorVisible = true;
    m_wrapPending = false;
    m_currentAttr = TermCell();
    m_state = Normal;
    m_collect.clear();
    for (auto &row : m_screen)
        for (auto &cell : row)
            cell = blankCell();
    emit screenUpdated();
}

void AnsiParser::clampCursor()
{
    m_curRow = qBound(0, m_curRow, m_rows - 1);
    m_curCol = qBound(0, m_curCol, m_cols - 1);
}

void AnsiParser::feed(const QByteArray &data)
{
    // Decode incoming bytes as UTF-8, handling partial sequences.
    m_utf8Pending.append(data);
    QStringDecoder dec(QStringConverter::Utf8);
    QString text = dec.decode(m_utf8Pending);
    m_utf8Pending.clear();

    for (int i = 0; i < text.size(); ++i) {
        QChar c = text.at(i);
        ushort u = c.unicode();

        switch (m_state) {
        case Normal:
            if (u == 0x1B) {               // ESC
                m_state = Escape;
                m_collect.clear();
            } else if (u == '\n') {
                newline();
            } else if (u == '\r') {
                carriageReturn();
            } else if (u == '\b') {
                backspace();
            } else if (u == '\t') {
                tab();
            } else if (u == 0x07) {        // BEL
                emit bell();
            } else if (u >= 0x20) {
                putChar(c);
            }
            break;

        case Escape:
            if (u == '[') {
                m_state = Csi;
                m_collect.clear();
            } else if (u == ']') {
                m_state = Osc;
                m_collect.clear();
            } else if (u == '(' || u == ')') {
                // Character set select - consume next char.
                if (i + 1 < text.size())
                    ++i;
                m_state = Normal;
            } else if (u == '7') {         // Save cursor
                m_savedRow = m_curRow;
                m_savedCol = m_curCol;
                m_state = Normal;
            } else if (u == '8') {         // Restore cursor
                m_curRow = m_savedRow;
                m_curCol = m_savedCol;
                clampCursor();
                m_state = Normal;
            } else if (u == 'M') {         // Reverse line feed
                if (m_curRow == m_scrollTop) {
                    // scroll down - simplistic
                } else {
                    m_curRow--;
                }
                m_state = Normal;
            } else if (u == 'c') {         // Full reset
                reset();
                m_state = Normal;
            } else {
                m_state = Normal;
            }
            break;

        case Csi:
            if ((u >= '0' && u <= '9') || u == ';' || u == '?' || u == '>' ||
                u == '=' || u == '!' || u == ' ') {
                m_collect.append(c);
            } else {
                processCsi(m_collect, c);
                m_state = Normal;
            }
            break;

        case Osc:
            if (u == 0x07) {               // BEL terminates OSC
                processOsc(m_collect);
                m_state = Normal;
            } else if (u == 0x1B) {
                // ST (ESC \) - peek next
                if (i + 1 < text.size() && text.at(i + 1).unicode() == '\\') {
                    ++i;
                    processOsc(m_collect);
                    m_state = Normal;
                }
            } else {
                m_collect.append(c);
            }
            break;
        }
    }

    emit screenUpdated();
}

// East-Asian width table (the ranges that matter in a terminal). Keeping this
// in one place means the parser and the renderer always agree on how many
// columns a glyph owns.
bool termCharIsWide(uint c)
{
    return (c >= 0x1100 && c <= 0x115F)     // Hangul Jamo initial consonants
        || (c >= 0x2E80 && c <= 0x303E)     // CJK radicals, Kangxi, punctuation
        || (c >= 0x3041 && c <= 0x33FF)     // Hiragana..CJK compatibility
        || (c >= 0x3400 && c <= 0x4DBF)     // CJK ext A
        || (c >= 0x4E00 && c <= 0x9FFF)     // CJK unified ideographs
        || (c >= 0xA000 && c <= 0xA4CF)     // Yi
        || (c >= 0xAC00 && c <= 0xD7A3)     // Hangul syllables
        || (c >= 0xF900 && c <= 0xFAFF)     // CJK compatibility ideographs
        || (c >= 0xFE10 && c <= 0xFE19)     // vertical forms
        || (c >= 0xFE30 && c <= 0xFE6F)     // CJK compatibility forms
        || (c >= 0xFF00 && c <= 0xFF60)     // fullwidth forms
        || (c >= 0xFFE0 && c <= 0xFFE6)     // fullwidth signs
        || (c >= 0x1F300 && c <= 0x1F64F)   // emoji / pictographs
        || (c >= 0x1F900 && c <= 0x1F9FF)
        || (c >= 0x20000 && c <= 0x3FFFD);  // CJK ext B..
}

void AnsiParser::putChar(QChar c)
{
    if (m_wrapPending) {
        m_curCol = 0;
        newline();
        m_wrapPending = false;
    }
    if (m_curRow < 0 || m_curRow >= m_rows)
        clampCursor();

    // A double-width glyph needs two columns. When only one column is left on
    // the line, wrap first so the glyph is never split across the edge.
    const bool wide = termCharIsWide(c.unicode());
    if (wide && m_curCol >= m_cols - 1) {
        m_screen[m_curRow][m_curCol] = blankCell();
        m_curCol = 0;
        newline();
        if (m_curRow < 0 || m_curRow >= m_rows)
            clampCursor();
    }

    TermCell cell = m_currentAttr;
    cell.ch = c;
    cell.wide = wide;
    cell.continuation = false;
    m_screen[m_curRow][m_curCol] = cell;

    if (wide) {
        // Reserve the right half: same attributes, no glyph of its own.
        if (m_curCol + 1 < m_cols) {
            TermCell cont = m_currentAttr;
            cont.ch = QChar(' ');
            cont.wide = false;
            cont.continuation = true;
            m_screen[m_curRow][m_curCol + 1] = cont;
        }
        if (m_curCol + 2 > m_cols - 1) {
            m_curCol = m_cols - 1;
            m_wrapPending = true;
        } else {
            m_curCol += 2;
        }
        return;
    }

    if (m_curCol >= m_cols - 1) {
        m_wrapPending = true;
    } else {
        m_curCol++;
    }
}

void AnsiParser::newline()
{
    m_wrapPending = false;
    if (m_curRow >= m_scrollBottom) {
        scrollUp(1);
    } else {
        m_curRow++;
    }
}

void AnsiParser::carriageReturn()
{
    m_curCol = 0;
    m_wrapPending = false;
}

void AnsiParser::backspace()
{
    if (m_curCol > 0)
        m_curCol--;
    m_wrapPending = false;
}

void AnsiParser::tab()
{
    int next = ((m_curCol / 8) + 1) * 8;
    m_curCol = qMin(next, m_cols - 1);
}

void AnsiParser::scrollUp(int n)
{
    for (int k = 0; k < n; ++k) {
        // Push top scrolling-region line into scrollback (if whole screen).
        if (m_scrollTop == 0) {
            m_scrollback.append(m_screen[m_scrollTop]);
            if (m_scrollback.size() > m_maxScrollback)
                m_scrollback.remove(0, m_scrollback.size() - m_maxScrollback);
        }
        for (int r = m_scrollTop; r < m_scrollBottom; ++r)
            m_screen[r] = m_screen[r + 1];
        QVector<TermCell> blank(m_cols, blankCell());
        m_screen[m_scrollBottom] = blank;
    }
}

void AnsiParser::processCsi(const QString &seq, QChar finalChar)
{
    // Parse numeric params.
    QString params = seq;
    bool priv = false;
    if (params.startsWith('?')) { priv = true; params.remove(0, 1); }
    if (params.startsWith('>') || params.startsWith('=')) params.remove(0, 1);

    QList<int> args;
    const QStringList parts = params.split(';');
    for (const QString &p : parts) {
        if (p.isEmpty())
            args.append(-1);
        else
            args.append(p.toInt());
    }
    auto arg = [&](int idx, int def) {
        if (idx < args.size() && args[idx] >= 0) return args[idx];
        return def;
    };

    ushort f = finalChar.unicode();
    switch (f) {
    case 'A': m_curRow -= arg(0, 1); clampCursor(); break;             // up
    case 'B': m_curRow += arg(0, 1); clampCursor(); break;             // down
    case 'C': m_curCol += arg(0, 1); clampCursor(); break;             // right
    case 'D': m_curCol -= arg(0, 1); clampCursor(); break;             // left
    case 'E': m_curRow += arg(0, 1); m_curCol = 0; clampCursor(); break;
    case 'F': m_curRow -= arg(0, 1); m_curCol = 0; clampCursor(); break;
    case 'G': m_curCol = arg(0, 1) - 1; clampCursor(); break;          // col
    case 'd': m_curRow = arg(0, 1) - 1; clampCursor(); break;          // row
    case 'H':
    case 'f':
        m_curRow = arg(0, 1) - 1;
        m_curCol = arg(1, 1) - 1;
        m_wrapPending = false;
        clampCursor();
        break;
    case 'J': eraseInDisplay(arg(0, 0)); break;
    case 'K': eraseInLine(arg(0, 0)); break;
    case 'S': scrollUp(arg(0, 1)); break;
    case 'L': { // insert lines
        int n = arg(0, 1);
        for (int k = 0; k < n; ++k) {
            for (int r = m_scrollBottom; r > m_curRow; --r)
                m_screen[r] = m_screen[r - 1];
            m_screen[m_curRow] = QVector<TermCell>(m_cols, blankCell());
        }
        break;
    }
    case 'M': { // delete lines
        int n = arg(0, 1);
        for (int k = 0; k < n; ++k) {
            for (int r = m_curRow; r < m_scrollBottom; ++r)
                m_screen[r] = m_screen[r + 1];
            m_screen[m_scrollBottom] = QVector<TermCell>(m_cols, blankCell());
        }
        break;
    }
    case 'P': { // delete chars
        int n = arg(0, 1);
        for (int k = 0; k < n; ++k) {
            for (int col = m_curCol; col < m_cols - 1; ++col)
                m_screen[m_curRow][col] = m_screen[m_curRow][col + 1];
            m_screen[m_curRow][m_cols - 1] = blankCell();
        }
        break;
    }
    case '@': { // insert blank chars
        int n = arg(0, 1);
        for (int k = 0; k < n; ++k) {
            for (int col = m_cols - 1; col > m_curCol; --col)
                m_screen[m_curRow][col] = m_screen[m_curRow][col - 1];
            m_screen[m_curRow][m_curCol] = blankCell();
        }
        break;
    }
    case 'X': { // erase chars
        int n = arg(0, 1);
        for (int k = 0; k < n && (m_curCol + k) < m_cols; ++k)
            m_screen[m_curRow][m_curCol + k] = blankCell();
        break;
    }
    case 'm': {
        if (args.isEmpty() || (args.size() == 1 && args[0] < 0))
            args = {0};
        setGraphicsRendition(args);
        break;
    }
    case 'r': { // scrolling region
        m_scrollTop = arg(0, 1) - 1;
        m_scrollBottom = arg(1, m_rows) - 1;
        m_scrollTop = qBound(0, m_scrollTop, m_rows - 1);
        m_scrollBottom = qBound(0, m_scrollBottom, m_rows - 1);
        break;
    }
    case 'h':
        if (priv && arg(0, 0) == 25) m_cursorVisible = true;
        break;
    case 'l':
        if (priv && arg(0, 0) == 25) m_cursorVisible = false;
        break;
    case 's':
        m_savedRow = m_curRow; m_savedCol = m_curCol;
        break;
    case 'u':
        m_curRow = m_savedRow; m_curCol = m_savedCol; clampCursor();
        break;
    default:
        break;
    }
}

void AnsiParser::processOsc(const QString &seq)
{
    // OSC 0;title  or  OSC 2;title
    int sc = seq.indexOf(';');
    if (sc >= 0) {
        QString code = seq.left(sc);
        QString value = seq.mid(sc + 1);
        if (code == "0" || code == "2")
            emit titleChanged(value);
    }
}

void AnsiParser::setGraphicsRendition(const QList<int> &params)
{
    for (int i = 0; i < params.size(); ++i) {
        int p = params[i] < 0 ? 0 : params[i];
        if (p == 0) {
            m_currentAttr = TermCell();
        } else if (p == 1) {
            m_currentAttr.bold = true;
        } else if (p == 3) {
            m_currentAttr.italic = true;
        } else if (p == 4) {
            m_currentAttr.underline = true;
        } else if (p == 7) {
            m_currentAttr.reverse = true;
        } else if (p == 22) {
            m_currentAttr.bold = false;
        } else if (p == 23) {
            m_currentAttr.italic = false;
        } else if (p == 24) {
            m_currentAttr.underline = false;
        } else if (p == 27) {
            m_currentAttr.reverse = false;
        } else if (p >= 30 && p <= 37) {
            m_currentAttr.fg = xterm256(p - 30);
            m_currentAttr.hasFg = true;
        } else if (p == 39) {
            m_currentAttr.hasFg = false;
        } else if (p >= 40 && p <= 47) {
            m_currentAttr.bg = xterm256(p - 40);
            m_currentAttr.hasBg = true;
        } else if (p == 49) {
            m_currentAttr.hasBg = false;
        } else if (p >= 90 && p <= 97) {
            m_currentAttr.fg = xterm256(p - 90 + 8);
            m_currentAttr.hasFg = true;
        } else if (p >= 100 && p <= 107) {
            m_currentAttr.bg = xterm256(p - 100 + 8);
            m_currentAttr.hasBg = true;
        } else if (p == 38 || p == 48) {
            // Extended color: 38;5;n (256) or 38;2;r;g;b (truecolor)
            bool fg = (p == 38);
            if (i + 1 < params.size() && params[i + 1] == 5) {
                int idx = (i + 2 < params.size()) ? params[i + 2] : 0;
                QColor c = xterm256(idx);
                if (fg) { m_currentAttr.fg = c; m_currentAttr.hasFg = true; }
                else    { m_currentAttr.bg = c; m_currentAttr.hasBg = true; }
                i += 2;
            } else if (i + 1 < params.size() && params[i + 1] == 2) {
                int r = (i + 2 < params.size()) ? params[i + 2] : 0;
                int g = (i + 3 < params.size()) ? params[i + 3] : 0;
                int b = (i + 4 < params.size()) ? params[i + 4] : 0;
                QColor c(r, g, b);
                if (fg) { m_currentAttr.fg = c; m_currentAttr.hasFg = true; }
                else    { m_currentAttr.bg = c; m_currentAttr.hasBg = true; }
                i += 4;
            }
        }
    }
}

void AnsiParser::eraseInDisplay(int mode)
{
    if (mode == 0) {
        // cursor to end
        for (int col = m_curCol; col < m_cols; ++col)
            m_screen[m_curRow][col] = blankCell();
        for (int r = m_curRow + 1; r < m_rows; ++r)
            m_screen[r] = QVector<TermCell>(m_cols, blankCell());
    } else if (mode == 1) {
        // start to cursor
        for (int r = 0; r < m_curRow; ++r)
            m_screen[r] = QVector<TermCell>(m_cols, blankCell());
        for (int col = 0; col <= m_curCol && col < m_cols; ++col)
            m_screen[m_curRow][col] = blankCell();
    } else {
        // entire screen
        for (int r = 0; r < m_rows; ++r)
            m_screen[r] = QVector<TermCell>(m_cols, blankCell());
    }
}

void AnsiParser::eraseInLine(int mode)
{
    if (mode == 0) {
        for (int col = m_curCol; col < m_cols; ++col)
            m_screen[m_curRow][col] = blankCell();
    } else if (mode == 1) {
        for (int col = 0; col <= m_curCol && col < m_cols; ++col)
            m_screen[m_curRow][col] = blankCell();
    } else {
        m_screen[m_curRow] = QVector<TermCell>(m_cols, blankCell());
    }
}

QColor AnsiParser::xterm256(int index)
{
    static QColor base[16] = {
        QColor(0, 0, 0),       QColor(205, 0, 0),     QColor(0, 205, 0),
        QColor(205, 205, 0),   QColor(0, 0, 238),     QColor(205, 0, 205),
        QColor(0, 205, 205),   QColor(229, 229, 229), QColor(127, 127, 127),
        QColor(255, 0, 0),     QColor(0, 255, 0),     QColor(255, 255, 0),
        QColor(92, 92, 255),   QColor(255, 0, 255),   QColor(0, 255, 255),
        QColor(255, 255, 255)
    };
    if (index < 0) index = 0;
    if (index < 16)
        return base[index];
    if (index < 232) {
        int i = index - 16;
        int r = (i / 36) % 6;
        int g = (i / 6) % 6;
        int b = i % 6;
        auto conv = [](int v) { return v == 0 ? 0 : 55 + v * 40; };
        return QColor(conv(r), conv(g), conv(b));
    }
    if (index < 256) {
        int v = 8 + (index - 232) * 10;
        return QColor(v, v, v);
    }
    return QColor(255, 255, 255);
}