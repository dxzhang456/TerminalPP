#include "hexeditortab.h"
#include "core/i18n.h"

#include <QAbstractScrollArea>
#include <QScrollBar>
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QFontMetricsF>
#include <QVBoxLayout>
#include <QLabel>
#include <QFile>
#include <QFileInfo>

static QString TRc(const char *s) { return I18n::instance()->tr(QString::fromUtf8(s)); }

// ===========================================================================
//  HexView — an editable offset/hex/ASCII grid painted on a scroll area.
//  16 bytes per row. The caret can sit in the hex column (two nibbles per
//  byte) or the ASCII column. Typing overwrites the byte under the caret.
// ===========================================================================
class HexView : public QAbstractScrollArea
{
    Q_OBJECT
public:
    explicit HexView(QWidget *parent = nullptr) : QAbstractScrollArea(parent)
    {
        QFont f("Consolas", 10);
        f.setStyleHint(QFont::Monospace);
        setFont(f);
        recalc();
        viewport()->setFocusPolicy(Qt::StrongFocus);
        setFocusPolicy(Qt::StrongFocus);
    }

    void setData(const QByteArray &d)
    {
        m_data = d;
        m_modified = false;
        m_caret = 0;
        m_lowNibble = false;
        updateScroll();
        viewport()->update();
    }
    const QByteArray &data() const { return m_data; }
    bool modified() const { return m_modified; }
    void clearModified() { m_modified = false; }

    void setColors(const QColor &bg, const QColor &fg, const QColor &accent,
                   const QColor &sel)
    { m_bg = bg; m_fg = fg; m_accent = accent; m_sel = sel; viewport()->update(); }

signals:
    void modifiedChanged(bool);
    void caretMoved(int offset);

protected:
    void resizeEvent(QResizeEvent *e) override { QAbstractScrollArea::resizeEvent(e); updateScroll(); }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(viewport());
        p.fillRect(viewport()->rect(), m_bg);
        p.setFont(font());

        const int top = verticalScrollBar()->value();
        const int rows = viewport()->height() / m_rowH + 2;
        const int totalRows = (m_data.size() + 15) / 16;

        for (int r = 0; r < rows; ++r) {
            int row = top + r;
            if (row >= totalRows) break;
            int y = r * m_rowH;
            int base = row * 16;

            // Offset column.
            p.setPen(m_accent);
            p.drawText(QPointF(m_pad, y + m_ascent),
                       QString("%1").arg(base, 8, 16, QLatin1Char('0')).toUpper());

            // Hex + ASCII columns.
            for (int c = 0; c < 16; ++c) {
                int idx = base + c;
                if (idx >= m_data.size()) break;
                unsigned char b = static_cast<unsigned char>(m_data[idx]);

                qreal hx = hexX(c);
                qreal ax = asciiX(c);

                // Caret highlight.
                if (idx == m_caret) {
                    p.fillRect(QRectF(hx - 1, y, m_charW * 2 + 2, m_rowH), m_sel);
                    p.fillRect(QRectF(ax - 1, y, m_charW + 2, m_rowH), m_sel.lighter(120));
                }
                p.setPen(m_fg);
                p.drawText(QPointF(hx, y + m_ascent),
                           QString("%1").arg(b, 2, 16, QLatin1Char('0')).toUpper());
                QChar ch = (b >= 0x20 && b < 0x7f) ? QChar(b) : QChar('.');
                p.drawText(QPointF(ax, y + m_ascent), ch);
            }
        }
    }

    void mousePressEvent(QMouseEvent *e) override
    {
        const int top = verticalScrollBar()->value();
        int row = top + int(e->position().y()) / m_rowH;
        qreal x = e->position().x();
        int col = -1;
        bool ascii = false;
        for (int c = 0; c < 16; ++c) {
            if (x >= hexX(c) - 1 && x < hexX(c) + m_charW * 2 + 2) { col = c; break; }
            if (x >= asciiX(c) - 1 && x < asciiX(c) + m_charW + 2) { col = c; ascii = true; break; }
        }
        if (col >= 0) {
            int idx = row * 16 + col;
            if (idx < m_data.size()) {
                m_caret = idx;
                m_asciiMode = ascii;
                m_lowNibble = false;
                emit caretMoved(m_caret);
                viewport()->update();
            }
        }
        setFocus();
    }

    void keyPressEvent(QKeyEvent *e) override
    {
        if (m_data.isEmpty()) return;
        switch (e->key()) {
        case Qt::Key_Left:  moveCaret(-1); return;
        case Qt::Key_Right: moveCaret(+1); return;
        case Qt::Key_Up:    moveCaret(-16); return;
        case Qt::Key_Down:  moveCaret(+16); return;
        case Qt::Key_Home:  m_caret = 0; m_lowNibble = false; after(); return;
        case Qt::Key_End:   m_caret = m_data.size() - 1; m_lowNibble = false; after(); return;
        case Qt::Key_Tab:   m_asciiMode = !m_asciiMode; m_lowNibble = false; after(); return;
        default: break;
        }

        const QString t = e->text();
        if (t.isEmpty()) { QAbstractScrollArea::keyPressEvent(e); return; }

        if (m_asciiMode) {
            // Overwrite the byte with the typed character.
            QChar ch = t.at(0);
            if (ch.unicode() >= 0x20 && ch.unicode() < 0x7f) {
                setByte(m_caret, static_cast<char>(ch.toLatin1()));
                moveCaret(+1);
            }
        } else {
            // Hex entry: accept 0-9 a-f, two nibbles per byte.
            QChar ch = t.at(0).toLower();
            int v = -1;
            if (ch >= '0' && ch <= '9') v = ch.unicode() - '0';
            else if (ch >= 'a' && ch <= 'f') v = 10 + ch.unicode() - 'a';
            if (v < 0) return;
            unsigned char b = static_cast<unsigned char>(m_data[m_caret]);
            if (!m_lowNibble) {
                b = (b & 0x0f) | (v << 4);
                setByte(m_caret, static_cast<char>(b));
                m_lowNibble = true;
            } else {
                b = (b & 0xf0) | v;
                setByte(m_caret, static_cast<char>(b));
                m_lowNibble = false;
                moveCaret(+1);
            }
        }
        ensureCaretVisible();
        viewport()->update();
    }

private:
    void recalc()
    {
        QFontMetricsF fm(font());
        m_charW = fm.horizontalAdvance(QLatin1Char('0'));
        m_rowH = qMax(1, qRound(fm.height()) + 2);
        m_ascent = fm.ascent() + 1;
    }
    qreal hexX(int c) const
    { return m_pad + m_offsetW + c * (m_charW * 2 + m_charW * 0.5)
             + (c >= 8 ? m_charW : 0); }
    qreal asciiX(int c) const
    { return m_pad + m_offsetW + 16 * (m_charW * 2 + m_charW * 0.5) + m_charW * 2
             + c * m_charW; }

    void setByte(int idx, char v)
    {
        if (idx < 0 || idx >= m_data.size()) return;
        if (m_data[idx] == v) return;
        m_data[idx] = v;
        if (!m_modified) { m_modified = true; emit modifiedChanged(true); }
    }
    void moveCaret(int d)
    {
        int n = qBound(0, m_caret + d, m_data.size() - 1);
        m_caret = n; m_lowNibble = false; emit caretMoved(m_caret);
        ensureCaretVisible(); viewport()->update();
    }
    void after() { emit caretMoved(m_caret); ensureCaretVisible(); viewport()->update(); }
    void ensureCaretVisible()
    {
        int row = m_caret / 16;
        int top = verticalScrollBar()->value();
        int visible = viewport()->height() / m_rowH;
        if (row < top) verticalScrollBar()->setValue(row);
        else if (row >= top + visible) verticalScrollBar()->setValue(row - visible + 1);
    }
    void updateScroll()
    {
        int totalRows = (m_data.size() + 15) / 16;
        int visible = viewport()->height() / qMax(1, m_rowH);
        verticalScrollBar()->setRange(0, qMax(0, totalRows - visible));
        verticalScrollBar()->setPageStep(visible);
    }

    QByteArray m_data;
    int m_caret = 0;
    bool m_lowNibble = false;
    bool m_asciiMode = false;
    bool m_modified = false;

    qreal m_charW = 8, m_ascent = 12;
    int m_rowH = 16;
    const int m_pad = 6;
    const int m_offsetW = 80;   // room for the 8-digit offset + gap

    QColor m_bg = QColor(30, 30, 30);
    QColor m_fg = QColor(212, 212, 212);
    QColor m_accent = QColor(100, 160, 220);
    QColor m_sel = QColor(70, 90, 140);
};

// ===========================================================================
//  HexEditorTab
// ===========================================================================
HexEditorTab::HexEditorTab(QWidget *parent) : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    m_view = new HexView(this);
    outer->addWidget(m_view, 1);

    m_info = new QLabel(this);
    m_info->setContentsMargins(6, 2, 6, 2);
    outer->addWidget(m_info);

    connect(m_view, &HexView::modifiedChanged, this, [this](bool m) {
        emit modificationChanged(m);
    });
    connect(m_view, &HexView::caretMoved, this, [this](int off) {
        unsigned char b = off < m_view->data().size()
            ? static_cast<unsigned char>(m_view->data()[off]) : 0;
        m_info->setText(QString("Offset: 0x%1 (%2)   Byte: 0x%3   Size: %4 bytes")
            .arg(off, 0, 16).arg(off)
            .arg(b, 2, 16, QLatin1Char('0'))
            .arg(m_view->data().size()));
    });
}

bool HexEditorTab::loadFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    m_view->setData(f.readAll());
    f.close();
    m_path = path;
    m_info->setText(TRc("%1 bytes").arg(m_view->data().size()));
    return true;
}

bool HexEditorTab::save()
{
    if (m_path.isEmpty())
        return false;
    return saveAs(m_path);
}

bool HexEditorTab::saveAs(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    f.write(m_view->data());
    f.close();
    m_path = path;
    m_view->clearModified();
    emit modificationChanged(false);
    emit statusMessage(TRc("Saved %1").arg(path));
    return true;
}

bool HexEditorTab::isModified() const { return m_view->modified(); }

void HexEditorTab::applyTheme(const EditorTheme &t)
{
    m_view->setColors(t.background, t.foreground, t.lineNumberFg, t.selectionBg);
}

#include "hexeditortab.moc"