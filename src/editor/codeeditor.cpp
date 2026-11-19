#include "codeeditor.h"

#include <QPainter>
#include <QTextBlock>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QFontMetrics>
#include <QApplication>
#include <QClipboard>
#include <QScrollBar>
#include <QAbstractTextDocumentLayout>
#include <QtGlobal>

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    m_lineNumberArea = new LineNumberArea(this);

    connect(this, &CodeEditor::blockCountChanged,
            this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &CodeEditor::updateRequest,
            this, &CodeEditor::updateLineNumberArea);
    connect(this, &CodeEditor::cursorPositionChanged,
            this, &CodeEditor::highlightCurrentLine);

    QFont font("Consolas", 11);
    font.setStyleHint(QFont::Monospace);
    setFont(font);
    setLineWrapMode(QPlainTextEdit::NoWrap);

    setTabWidthChars(m_tabWidth);
    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

int CodeEditor::lineNumberAreaWidth()
{
    if (!m_showLineNumbers)
        return 0;

    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    int space = 12 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void CodeEditor::setShowLineNumbers(bool show)
{
    m_showLineNumbers = show;
    m_lineNumberArea->setVisible(show);
    updateLineNumberAreaWidth(0);
}

void CodeEditor::setTabWidthChars(int chars)
{
    m_tabWidth = qMax(1, chars);
    QFontMetricsF fm(font());
    setTabStopDistance(fm.horizontalAdvance(QLatin1Char(' ')) * m_tabWidth);
}

void CodeEditor::setCurrentLineColor(const QColor &c)
{
    m_currentLineColor = c;
    highlightCurrentLine();
}

void CodeEditor::setLineNumberColors(const QColor &bg, const QColor &fg)
{
    m_lnBg = bg;
    m_lnFg = fg;
    m_lineNumberArea->update();
}

void CodeEditor::updateLineNumberAreaWidth(int)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        m_lineNumberArea->scroll(0, dy);
    else
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);
    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(
        QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(m_currentLineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }
    setExtraSelections(extraSelections);
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    if (!m_showLineNumbers)
        return;

    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), m_lnBg);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    painter.setPen(m_lnFg);
    const int w = m_lineNumberArea->width();
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.drawText(0, top, w - 6, fontMetrics().height(),
                             Qt::AlignRight, number);
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

QString CodeEditor::currentIndent() const
{
    QTextBlock block = textCursor().block();
    QString text = block.text();
    QString indent;
    for (QChar c : text) {
        if (c == QLatin1Char(' ') || c == QLatin1Char('\t'))
            indent.append(c);
        else
            break;
    }
    return indent;
}

void CodeEditor::keyPressEvent(QKeyEvent *event)
{
    const int key = event->key();
    const Qt::KeyboardModifiers mods = event->modifiers();

    // ---- VS Code style keyboard column selection --------------------------
    // Ctrl+Shift+Up / Ctrl+Shift+Down starts (or extends) a column (box)
    // selection one line at a time, independent of whether Alt+drag column
    // mode is already active. Checked first so it always takes priority over
    // the "any other key cancels column mode" fallthrough below.
    if (mods == (Qt::ControlModifier | Qt::ShiftModifier)
        && (key == Qt::Key_Up || key == Qt::Key_Down)) {
        extendColumnSelectionVertically(key == Qt::Key_Up ? -1 : +1);
        event->accept();
        return;
    }

    // ---- Column (block) selection mode handling --------------------------
    if (m_columnMode) {
        // Escape leaves column mode.
        if (key == Qt::Key_Escape) {
            clearColumnSelection();
            return;
        }
        // Copy / Cut the rectangular selection.
        if (mods.testFlag(Qt::ControlModifier) && key == Qt::Key_C) {
            QApplication::clipboard()->setText(columnSelectionText());
            return;
        }
        if (mods.testFlag(Qt::ControlModifier) && key == Qt::Key_X) {
            QApplication::clipboard()->setText(columnSelectionText());
            deleteColumnSelection();
            clearColumnSelection();
            return;
        }
        // Paste: distribute clipboard lines across the block rows.
        if (mods.testFlag(Qt::ControlModifier) && key == Qt::Key_V) {
            pasteColumn(QApplication::clipboard()->text());
            clearColumnSelection();
            return;
        }
        // Backspace / Delete remove the rectangle (or one column if empty).
        if (key == Qt::Key_Backspace || key == Qt::Key_Delete) {
            if (m_colValid && m_colAnchorCol != m_colCaretCol) {
                deleteColumnSelection();
            }
            clearColumnSelection();
            return;
        }
        // Printable text: insert it at the left edge of the rectangle on
        // every spanned line (replacing the rectangle first if non-empty).
        if (!event->text().isEmpty() && event->text().at(0).isPrint()) {
            if (m_colValid && m_colAnchorCol != m_colCaretCol)
                deleteColumnSelection();
            insertIntoColumn(event->text());
            return;
        }
        // Any other navigation key cancels column mode and falls through.
        clearColumnSelection();
    }

    // Tab -> spaces, Shift+Tab -> unindent, block indent on selection.
    if (event->key() == Qt::Key_Tab && !event->modifiers().testFlag(Qt::ControlModifier)) {
        QTextCursor cur = textCursor();
        if (cur.hasSelection()) {
            // Indent each selected line.
            int start = cur.selectionStart();
            int end = cur.selectionEnd();
            cur.setPosition(start);
            int firstBlock = cur.blockNumber();
            cur.setPosition(end);
            int lastBlock = cur.blockNumber();
            cur.beginEditBlock();
            QTextBlock b = document()->findBlockByNumber(firstBlock);
            while (b.isValid() && b.blockNumber() <= lastBlock) {
                QTextCursor c(b);
                c.movePosition(QTextCursor::StartOfBlock);
                c.insertText(m_useSpaces ? QString(m_tabWidth, ' ') : QStringLiteral("\t"));
                b = b.next();
            }
            cur.endEditBlock();
            return;
        }
        if (m_useSpaces) {
            insertPlainText(QString(m_tabWidth, ' '));
            return;
        }
    } else if (event->key() == Qt::Key_Backtab) {
        QTextCursor cur = textCursor();
        int start = cur.selectionStart();
        int end = cur.selectionEnd();
        cur.setPosition(start);
        int firstBlock = cur.blockNumber();
        cur.setPosition(end);
        int lastBlock = cur.blockNumber();
        cur.beginEditBlock();
        QTextBlock b = document()->findBlockByNumber(firstBlock);
        while (b.isValid() && b.blockNumber() <= lastBlock) {
            QTextCursor c(b);
            c.movePosition(QTextCursor::StartOfBlock);
            QString text = b.text();
            int removed = 0;
            while (removed < m_tabWidth && removed < text.size()) {
                if (text.at(removed) == QLatin1Char(' '))
                    removed++;
                else if (text.at(removed) == QLatin1Char('\t')) {
                    removed = 1;
                    break;
                } else
                    break;
            }
            for (int i = 0; i < removed; ++i)
                c.deleteChar();
            b = b.next();
        }
        cur.endEditBlock();
        return;
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        // Auto-indent: preserve current line indentation.
        QString indent = currentIndent();
        QPlainTextEdit::keyPressEvent(event);
        insertPlainText(indent);
        return;
    }

    QPlainTextEdit::keyPressEvent(event);
}

void CodeEditor::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        if (event->angleDelta().y() > 0)
            zoomIn(1);
        else
            zoomOut(1);
        updateLineNumberAreaWidth(0);
        event->accept();
        return;
    }
    QPlainTextEdit::wheelEvent(event);
}

void CodeEditor::matchBrackets()
{
    // Reserved for future bracket matching extension.
}

// ============================================================================
//  Column (block / rectangular) editing — Alt + drag, Notepad++ style.
// ============================================================================

// Convert a viewport point into a (block index, column-in-chars) pair.
// The column may extend past the end of a short line (virtual space), which is
// exactly what a rectangular selection needs.
void CodeEditor::pointToBlockColumn(const QPoint &vpPos, int &block, int &column) const
{
    QTextCursor cur = cursorForPosition(vpPos);
    block = cur.blockNumber();

    // Horizontal: measure from the block's left edge in character widths.
    const qreal charW = QFontMetricsF(font()).horizontalAdvance(QLatin1Char(' '));
    QTextBlock blk = document()->findBlockByNumber(block);
    QRectF blockRect = blockBoundingGeometry(blk).translated(contentOffset());
    qreal xInBlock = vpPos.x() - blockRect.left();
    if (charW > 0.0)
        column = qMax(0, qRound(xInBlock / charW));
    else
        column = 0;
}

// Compute the viewport rectangle of character cell (block, column), allowing
// the column to lie beyond the physical end of the line (virtual space).
QRect CodeEditor::cellRect(int block, int column) const
{
    QTextBlock blk = document()->findBlockByNumber(block);
    if (!blk.isValid())
        return QRect();

    QRectF geo = blockBoundingGeometry(blk).translated(contentOffset());
    const qreal charW = QFontMetricsF(font()).horizontalAdvance(QLatin1Char(' '));
    int x = qRound(geo.left() + column * charW);
    int y = qRound(geo.top());
    int h = qRound(geo.height());
    return QRect(x, y, qMax(1, qRound(charW)), h);
}

void CodeEditor::clearColumnSelection()
{
    bool was = m_columnMode;
    m_columnMode = false;
    m_colValid = false;
    m_colDragging = false;
    viewport()->update();
    if (was)
        emit columnModeChanged(false);
}

// Build the rectangular text: each spanned line contributes the substring
// between the left and right columns (clamped to the line length); rows joined
// with '\n'.
QString CodeEditor::columnSelectionText() const
{
    if (!m_colValid)
        return QString();

    int b0 = qMin(m_colAnchorBlk, m_colCaretBlk);
    int b1 = qMax(m_colAnchorBlk, m_colCaretBlk);
    int c0 = qMin(m_colAnchorCol, m_colCaretCol);
    int c1 = qMax(m_colAnchorCol, m_colCaretCol);

    QStringList lines;
    for (int b = b0; b <= b1; ++b) {
        QTextBlock blk = document()->findBlockByNumber(b);
        if (!blk.isValid()) {
            lines << QString();
            continue;
        }
        const QString text = blk.text();
        int from = qMin(c0, text.size());
        int to = qMin(c1, text.size());
        lines << text.mid(from, to - from);
    }
    return lines.join(QLatin1Char('\n'));
}

// Remove the rectangle on every spanned line.
bool CodeEditor::deleteColumnSelection()
{
    if (!m_colValid)
        return false;

    int b0 = qMin(m_colAnchorBlk, m_colCaretBlk);
    int b1 = qMax(m_colAnchorBlk, m_colCaretBlk);
    int c0 = qMin(m_colAnchorCol, m_colCaretCol);
    int c1 = qMax(m_colAnchorCol, m_colCaretCol);
    if (c0 == c1)
        return false;

    QTextCursor edit(document());
    edit.beginEditBlock();
    for (int b = b0; b <= b1; ++b) {
        QTextBlock blk = document()->findBlockByNumber(b);
        if (!blk.isValid())
            continue;
        const int len = blk.text().size();
        int from = qMin(c0, len);
        int to = qMin(c1, len);
        if (to <= from)
            continue;
        QTextCursor c(blk);
        c.setPosition(blk.position() + from);
        c.setPosition(blk.position() + to, QTextCursor::KeepAnchor);
        c.removeSelectedText();
    }
    edit.endEditBlock();

    // Collapse the rectangle to its left edge.
    m_colAnchorCol = m_colCaretCol = c0;
    return true;
}

// Insert the same text on every spanned line at the rectangle's left column.
void CodeEditor::insertIntoColumn(const QString &text)
{
    if (text.isEmpty())
        return;

    int b0 = qMin(m_colAnchorBlk, m_colCaretBlk);
    int b1 = qMax(m_colAnchorBlk, m_colCaretBlk);
    int col = qMin(m_colAnchorCol, m_colCaretCol);

    QTextCursor edit(document());
    edit.beginEditBlock();
    for (int b = b0; b <= b1; ++b) {
        QTextBlock blk = document()->findBlockByNumber(b);
        if (!blk.isValid())
            continue;
        const int len = blk.text().size();
        QTextCursor c(blk);
        if (col > len) {
            // Pad short lines with spaces so the column aligns (virtual space).
            c.setPosition(blk.position() + len);
            c.insertText(QString(col - len, QLatin1Char(' ')) + text);
        } else {
            c.setPosition(blk.position() + col);
            c.insertText(text);
        }
    }
    edit.endEditBlock();

    // Advance the caret column by the inserted length and keep the block.
    int newCol = col + text.size();
    m_colAnchorCol = m_colCaretCol = newCol;
    m_colValid = true;
    viewport()->update();
}

// VS Code style keyboard column selection: Ctrl+Shift+Up/Down starts (if not
// already active) or extends the column (box) selection by one line, keeping
// the same character column as the anchor/caret. This mirrors VS Code's
// "Column Selection" (Ctrl+Shift+Up/Down) so users familiar with that editor
// get the same muscle-memory shortcut instead of having to Alt+drag.
void CodeEditor::extendColumnSelectionVertically(int dy)
{
    if (!m_columnMode) {
        // Start a fresh column selection anchored at the current text cursor.
        QTextCursor cur = textCursor();
        m_colAnchorBlk = m_colCaretBlk = cur.blockNumber();
        m_colAnchorCol = m_colCaretCol = cur.positionInBlock();
        m_columnMode = true;
        m_colValid = false;
        emit columnModeChanged(true);
    }

    const int maxBlk = document()->blockCount() - 1;
    m_colCaretBlk = qBound(0, m_colCaretBlk + dy, maxBlk);
    m_colValid = (m_colAnchorBlk != m_colCaretBlk) || (m_colAnchorCol != m_colCaretCol);

    // Move the real text cursor to follow the caret row/column so scrolling
    // (ensureCursorVisible) and the status bar position indicator stay in
    // sync with the keyboard-driven selection.
    QTextBlock blk = document()->findBlockByNumber(m_colCaretBlk);
    if (blk.isValid()) {
        QTextCursor c(blk);
        const int pos = qMin(m_colCaretCol, blk.length() - 1);
        c.setPosition(blk.position() + qMax(0, pos));
        setTextCursor(c);
        ensureCursorVisible();
    }
    viewport()->update();
}

// IME-committed text (Chinese/Japanese/Korean input via the system input
// method) does not arrive through keyPressEvent() — Qt delivers it here
// instead. Without this override, typing Chinese while a column (box)
// selection is active would insert the composed text only at the single
// QPlainTextEdit cursor position, ignoring every other selected row. Route
// it through the same insertIntoColumn()/deleteColumnSelection() path used
// for regular ASCII keystrokes so CJK input (and IME-driven deletion via an
// empty commit string with a preceding backspace) is distributed correctly
// across the whole column selection.
void CodeEditor::inputMethodEvent(QInputMethodEvent *event)
{
    if (m_columnMode && !event->commitString().isEmpty()) {
        if (m_colValid && m_colAnchorCol != m_colCaretCol)
            deleteColumnSelection();
        insertIntoColumn(event->commitString());
        event->accept();
        return;
    }
    QPlainTextEdit::inputMethodEvent(event);
}

// Paste clipboard content line-by-line down the block rows.
void CodeEditor::pasteColumn(const QString &text)
{
    if (text.isEmpty())
        return;

    // Replace any existing non-empty rectangle first.
    if (m_colValid && m_colAnchorCol != m_colCaretCol)
        deleteColumnSelection();

    const QStringList parts = text.split(QLatin1Char('\n'));
    int startBlk = qMin(m_colAnchorBlk, m_colCaretBlk);
    int col = qMin(m_colAnchorCol, m_colCaretCol);

    QTextCursor edit(document());
    edit.beginEditBlock();
    for (int i = 0; i < parts.size(); ++i) {
        QString piece = parts.at(i);
        if (piece.endsWith(QLatin1Char('\r')))
            piece.chop(1);

        int blkIdx = startBlk + i;
        // Create extra lines at the end of the document if the clipboard has
        // more rows than the document currently holds.
        while (blkIdx >= document()->blockCount()) {
            QTextCursor tail(document());
            tail.movePosition(QTextCursor::End);
            tail.insertBlock();
        }
        QTextBlock blk = document()->findBlockByNumber(blkIdx);
        if (!blk.isValid())
            continue;
        const int len = blk.text().size();
        QTextCursor c(blk);
        if (col > len) {
            c.setPosition(blk.position() + len);
            c.insertText(QString(col - len, QLatin1Char(' ')) + piece);
        } else {
            c.setPosition(blk.position() + col);
            c.insertText(piece);
        }
    }
    edit.endEditBlock();
    viewport()->update();
}

void CodeEditor::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton &&
        event->modifiers().testFlag(Qt::AltModifier)) {
        // Begin an Alt+drag rectangular selection.
        m_columnMode = true;
        m_colDragging = true;
        m_colValid = false;
        pointToBlockColumn(event->pos(), m_colAnchorBlk, m_colAnchorCol);
        m_colCaretBlk = m_colAnchorBlk;
        m_colCaretCol = m_colAnchorCol;

        // Place the real text cursor there too (so typing origin is sane).
        setTextCursor(cursorForPosition(event->pos()));
        viewport()->update();
        emit columnModeChanged(true);
        event->accept();
        return;
    }

    // A normal click cancels any active column selection.
    if (m_columnMode)
        clearColumnSelection();

    QPlainTextEdit::mousePressEvent(event);
}

void CodeEditor::mouseMoveEvent(QMouseEvent *event)
{
    if (m_colDragging) {
        pointToBlockColumn(event->pos(), m_colCaretBlk, m_colCaretCol);
        m_colValid = true;
        viewport()->update();
        event->accept();
        return;
    }
    QPlainTextEdit::mouseMoveEvent(event);
}

void CodeEditor::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_colDragging) {
        m_colDragging = false;
        pointToBlockColumn(event->pos(), m_colCaretBlk, m_colCaretCol);
        m_colValid = (m_colAnchorBlk != m_colCaretBlk) ||
                     (m_colAnchorCol != m_colCaretCol);
        // Even a zero-width rectangle keeps column mode active so the user can
        // immediately type a multi-line insertion at that column.
        viewport()->update();
        event->accept();
        return;
    }
    QPlainTextEdit::mouseReleaseEvent(event);
}

void CodeEditor::paintEvent(QPaintEvent *event)
{
    QPlainTextEdit::paintEvent(event);

    if (!m_columnMode)
        return;

    QPainter p(viewport());
    const QColor fill(80, 140, 230, 70);
    const QColor border(60, 120, 210, 180);
    p.setPen(border);
    p.setBrush(fill);

    int b0 = qMin(m_colAnchorBlk, m_colCaretBlk);
    int b1 = qMax(m_colAnchorBlk, m_colCaretBlk);
    int c0 = qMin(m_colAnchorCol, m_colCaretCol);
    int c1 = qMax(m_colAnchorCol, m_colCaretCol);

    for (int b = b0; b <= b1; ++b) {
        QRect left = cellRect(b, c0);
        QRect right = cellRect(b, c1);
        if (left.isNull() || right.isNull())
            continue;
        if (c0 == c1) {
            // Zero-width caret rectangle: draw a thin vertical bar.
            p.fillRect(QRect(left.left(), left.top(), 2, left.height()), border);
        } else {
            QRect row(left.left(), left.top(),
                      right.left() - left.left(), left.height());
            p.drawRect(row);
        }
    }
}
