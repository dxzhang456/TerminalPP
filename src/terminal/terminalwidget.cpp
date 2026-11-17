#include "terminalwidget.h"
#include "ansiparser.h"

#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QScrollBar>
#include <QApplication>
#include <QClipboard>
#include <QFontMetricsF>

TerminalWidget::TerminalWidget(QWidget *parent)
    : QAbstractScrollArea(parent)
{
    m_parser = new AnsiParser(this);
    connect(m_parser, &AnsiParser::screenUpdated,
            this, &TerminalWidget::onScreenUpdated);

    m_font = QFont("Consolas", 11);
    m_font.setStyleHint(QFont::Monospace);
    setTerminalFont(m_font);

    setFocusPolicy(Qt::StrongFocus);
    viewport()->setCursor(Qt::IBeamCursor);
    setAttribute(Qt::WA_InputMethodEnabled, true);
    // Keep keyboard focus on the scroll area itself (not the viewport) so our
    // keyPressEvent/inputMethodEvent overrides receive all input.
    viewport()->setFocusPolicy(Qt::NoFocus);
    viewport()->setAttribute(Qt::WA_InputMethodEnabled, true);

    QPalette pal = viewport()->palette();
    pal.setColor(QPalette::Window, m_defaultBg);
    viewport()->setPalette(pal);
    viewport()->setAutoFillBackground(true);

    verticalScrollBar()->setRange(0, 0);
}

void TerminalWidget::setColors(const QColor &fg, const QColor &bg)
{
    m_defaultFg = fg;
    m_defaultBg = bg;
    QPalette pal = viewport()->palette();
    pal.setColor(QPalette::Window, m_defaultBg);
    viewport()->setPalette(pal);
    viewport()->update();
}

void TerminalWidget::setTerminalFont(const QFont &f)
{
    m_font = f;
    QFontMetricsF fm(m_font);
    m_charWidth = qMax(1, qRound(fm.horizontalAdvance(QLatin1Char('M'))));
    m_charHeight = qMax(1, qRound(fm.height()));
    recalcGrid();
    viewport()->update();
}

int TerminalWidget::termRows() const { return m_rows; }
int TerminalWidget::termCols() const { return m_cols; }

void TerminalWidget::clearScreen()
{
    m_parser->reset();
    viewport()->update();
}

void TerminalWidget::recalcGrid()
{
    int w = viewport()->width();
    int h = viewport()->height();
    int cols = qMax(1, w / m_charWidth);
    int rows = qMax(1, h / m_charHeight);
    if (cols != m_cols || rows != m_rows) {
        m_cols = cols;
        m_rows = rows;
        m_parser->setSize(m_rows, m_cols);
        emit sizeChanged(m_rows, m_cols);
    }
}

void TerminalWidget::updateScrollBar()
{
    int sb = m_parser->scrollbackSize();
    verticalScrollBar()->setRange(0, sb);
    verticalScrollBar()->setPageStep(m_rows);
    if (m_scrollOffset == 0)
        verticalScrollBar()->setValue(sb);
}

void TerminalWidget::onScreenUpdated()
{
    updateScrollBar();
    viewport()->update();
}

void TerminalWidget::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    recalcGrid();
    updateScrollBar();
}

void TerminalWidget::paintEvent(QPaintEvent *)
{
    QPainter p(viewport());
    p.setFont(m_font);
    p.fillRect(viewport()->rect(), m_defaultBg);

    const QVector<QVector<TermCell>> &screen = m_parser->screen();
    const QVector<QVector<TermCell>> &sb = m_parser->scrollback();

    // Determine top line based on scrollbar.
    int sbSize = sb.size();
    int sbValue = verticalScrollBar()->value();
    int linesFromScrollback = sbSize - sbValue; // how many scrollback lines above screen
    m_scrollOffset = sbSize - sbValue;

    // Build the list of rows to draw (scrollback + screen).
    for (int viewRow = 0; viewRow < m_rows; ++viewRow) {
        int docRow = sbValue + viewRow; // index into virtual (scrollback+screen)
        const QVector<TermCell> *rowPtr = nullptr;

        if (docRow < sbSize) {
            rowPtr = &sb[docRow];
        } else {
            int sRow = docRow - sbSize;
            if (sRow >= 0 && sRow < screen.size())
                rowPtr = &screen[sRow];
        }
        if (!rowPtr)
            continue;

        const QVector<TermCell> &row = *rowPtr;
        int y = viewRow * m_charHeight;

        for (int col = 0; col < row.size() && col < m_cols; ++col) {
            const TermCell &cell = row[col];
            int x = col * m_charWidth;

            QColor fg = cell.hasFg ? cell.fg : m_defaultFg;
            QColor bg = cell.hasBg ? cell.bg : m_defaultBg;
            if (cell.reverse)
                std::swap(fg, bg);

            if (bg != m_defaultBg)
                p.fillRect(x, y, m_charWidth, m_charHeight, bg);

            if (cell.ch != QChar(' ') || cell.hasBg) {
                QFont cf = m_font;
                cf.setBold(cell.bold);
                cf.setItalic(cell.italic);
                cf.setUnderline(cell.underline);
                p.setFont(cf);
                p.setPen(fg);
                p.drawText(x, y + m_charHeight - qRound(QFontMetricsF(cf).descent()),
                           QString(cell.ch));
            }
        }
    }

    // Draw cursor (only when at the bottom / live screen).
    if (m_parser->cursorVisible() && hasFocus()) {
        int curDocRow = sbSize + m_parser->cursorRow();
        int viewRow = curDocRow - sbValue;
        if (viewRow >= 0 && viewRow < m_rows) {
            int x = m_parser->cursorCol() * m_charWidth;
            int y = viewRow * m_charHeight;
            p.fillRect(x, y, m_charWidth, m_charHeight,
                       QColor(m_defaultFg.red(), m_defaultFg.green(),
                              m_defaultFg.blue(), 160));
        }
    }

    Q_UNUSED(linesFromScrollback);
}

QByteArray TerminalWidget::keyToBytes(QKeyEvent *event)
{
    const int key = event->key();
    const Qt::KeyboardModifiers mod = event->modifiers();

    // Ctrl + letter -> control code.
    if (mod.testFlag(Qt::ControlModifier) && key >= Qt::Key_A && key <= Qt::Key_Z) {
        char c = char(key - Qt::Key_A + 1);
        return QByteArray(1, c);
    }

    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter:    return "\r";
    case Qt::Key_Backspace:return "\x7f";
    case Qt::Key_Tab:      return "\t";
    case Qt::Key_Escape:   return "\x1b";
    case Qt::Key_Up:       return "\x1b[A";
    case Qt::Key_Down:     return "\x1b[B";
    case Qt::Key_Right:    return "\x1b[C";
    case Qt::Key_Left:     return "\x1b[D";
    case Qt::Key_Home:     return "\x1b[H";
    case Qt::Key_End:      return "\x1b[F";
    case Qt::Key_PageUp:   return "\x1b[5~";
    case Qt::Key_PageDown: return "\x1b[6~";
    case Qt::Key_Insert:   return "\x1b[2~";
    case Qt::Key_Delete:   return "\x1b[3~";
    case Qt::Key_F1:       return "\x1bOP";
    case Qt::Key_F2:       return "\x1bOQ";
    case Qt::Key_F3:       return "\x1bOR";
    case Qt::Key_F4:       return "\x1bOS";
    case Qt::Key_F5:       return "\x1b[15~";
    case Qt::Key_F6:       return "\x1b[17~";
    case Qt::Key_F7:       return "\x1b[18~";
    case Qt::Key_F8:       return "\x1b[19~";
    case Qt::Key_F9:       return "\x1b[20~";
    case Qt::Key_F10:      return "\x1b[21~";
    case Qt::Key_F11:      return "\x1b[23~";
    case Qt::Key_F12:      return "\x1b[24~";
    default: break;
    }

    if (!event->text().isEmpty())
        return event->text().toUtf8();

    return QByteArray();
}

void TerminalWidget::showEvent(QShowEvent *e)
{
    QAbstractScrollArea::showEvent(e);
    setFocus();
}

bool TerminalWidget::event(QEvent *e)
{
    // Intercept key presses (including Tab/Backtab which Qt would otherwise
    // consume for focus navigation) so every keystroke reaches the terminal.
    if (e->type() == QEvent::KeyPress) {
        keyPressEvent(static_cast<QKeyEvent *>(e));
        return true;
    }
    return QAbstractScrollArea::event(e);
}

void TerminalWidget::keyPressEvent(QKeyEvent *event)
{
    // Copy / paste shortcuts.
    if (event->modifiers().testFlag(Qt::ControlModifier) &&
        event->modifiers().testFlag(Qt::ShiftModifier)) {
        if (event->key() == Qt::Key_C) { copySelection(); return; }
        if (event->key() == Qt::Key_V) {
            const QString clip = QApplication::clipboard()->text();
            emit dataEntered(clip.toUtf8());
            return;
        }
    }

    QByteArray bytes = keyToBytes(event);
    if (!bytes.isEmpty()) {
        // Scroll to bottom on input.
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
        emit dataEntered(bytes);
        event->accept();
        return;
    }
    QAbstractScrollArea::keyPressEvent(event);
}

void TerminalWidget::inputMethodEvent(QInputMethodEvent *event)
{
    const QString commit = event->commitString();
    if (!commit.isEmpty()) {
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
        emit dataEntered(commit.toUtf8());
    }
    event->accept();
}

QVariant TerminalWidget::inputMethodQuery(Qt::InputMethodQuery query) const
{
    if (query == Qt::ImCursorRectangle) {
        // Position the IME candidate window near the cursor.
        int x = m_parser->cursorCol() * m_charWidth;
        int sbSize = m_parser->scrollbackSize();
        int sbValue = verticalScrollBar()->value();
        int viewRow = (sbSize + m_parser->cursorRow()) - sbValue;
        int y = viewRow * m_charHeight;
        return QRect(x, y, m_charWidth, m_charHeight);
    }
    return QAbstractScrollArea::inputMethodQuery(query);
}

void TerminalWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_selecting = true;
        int col = event->position().x() / m_charWidth;
        int row = event->position().y() / m_charHeight;
        int sbValue = verticalScrollBar()->value();
        m_selStart = QPoint(col, sbValue + row);
        m_selEnd = m_selStart;
    } else if (event->button() == Qt::MiddleButton) {
        // Middle-click paste (X11 style).
        const QString clip = QApplication::clipboard()->text();
        emit dataEntered(clip.toUtf8());
    }
    setFocus();
}

void TerminalWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_selecting) {
        int col = event->position().x() / m_charWidth;
        int row = event->position().y() / m_charHeight;
        int sbValue = verticalScrollBar()->value();
        m_selEnd = QPoint(col, sbValue + row);
        viewport()->update();
    }
}

void TerminalWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_selecting = false;
        copySelection();
    }
}

void TerminalWidget::copySelection()
{
    // Minimal selection copy: copy the visible screen cell text between points.
    // (Selection rendering is simplified; copies by row range.)
    if (m_selStart == m_selEnd)
        return;

    const QVector<QVector<TermCell>> &screen = m_parser->screen();
    const QVector<QVector<TermCell>> &sb = m_parser->scrollback();
    int sbSize = sb.size();

    QPoint a = m_selStart;
    QPoint b = m_selEnd;
    if (a.y() > b.y() || (a.y() == b.y() && a.x() > b.x()))
        std::swap(a, b);

    QString out;
    for (int docRow = a.y(); docRow <= b.y(); ++docRow) {
        const QVector<TermCell> *rowPtr = nullptr;
        if (docRow < sbSize)
            rowPtr = &sb[docRow];
        else {
            int sRow = docRow - sbSize;
            if (sRow >= 0 && sRow < screen.size())
                rowPtr = &screen[sRow];
        }
        if (!rowPtr) continue;

        int startCol = (docRow == a.y()) ? a.x() : 0;
        int endCol = (docRow == b.y()) ? b.x() : rowPtr->size();
        QString line;
        for (int c = startCol; c < endCol && c < rowPtr->size(); ++c)
            line.append((*rowPtr)[c].ch);
        out += line.trimmed();
        if (docRow != b.y())
            out += "\n";
    }

    if (!out.isEmpty())
        QApplication::clipboard()->setText(out);
}

void TerminalWidget::wheelEvent(QWheelEvent *event)
{
    int delta = event->angleDelta().y();
    int step = (delta > 0 ? -3 : 3);
    verticalScrollBar()->setValue(verticalScrollBar()->value() + step);
    viewport()->update();
    event->accept();
}