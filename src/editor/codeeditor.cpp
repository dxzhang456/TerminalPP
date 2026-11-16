#include "codeeditor.h"

#include <QPainter>
#include <QTextBlock>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QFontMetrics>

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