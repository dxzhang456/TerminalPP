#include "textcomparetab.h"
#include "diffengine.h"
#include "core/i18n.h"
#include "compare/comparedialog.h"

// Runtime UI translator shorthand.
static inline QString TRc(const char *s) { return I18n::instance()->tr(QString::fromUtf8(s)); }

#include <QAbstractScrollArea>
#include <QPainter>
#include <QScrollBar>
#include <QFontMetricsF>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolBar>
#include <QAction>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QMouseEvent>
#include <QKeyEvent>

// ===========================================================================
//  DiffPane — a custom scroll area that paints one side of the comparison.
//  It holds the aligned rows (shared model) and knows which side it renders.
// ===========================================================================
class DiffPane : public QAbstractScrollArea
{
    Q_OBJECT
public:
    enum Side { Left, Right };

    explicit DiffPane(Side side, QWidget *parent = nullptr)
        : QAbstractScrollArea(parent), m_side(side)
    {
        QFont f("Consolas", 10);
        f.setStyleHint(QFont::Monospace);
        setFont(f);
        recalcMetrics();
        setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        viewport()->setMouseTracking(true);
    }

    void setModel(const QStringList *lines,
                  const QVector<Diff::Row> *rows,
                  const Diff::Options *opts)
    {
        m_lines = lines;
        m_rows = rows;
        m_opts = opts;
        updateScrollRange();
        viewport()->update();
    }

    void setColors(const QColor &bg, const QColor &fg,
                   const QColor &changed, const QColor &inserted,
                   const QColor &removed, const QColor &inlineHi,
                   const QColor &gutterBg, const QColor &gutterFg)
    {
        m_bg = bg; m_fg = fg; m_changed = changed; m_inserted = inserted;
        m_removed = removed; m_inlineHi = inlineHi;
        m_gutterBg = gutterBg; m_gutterFg = gutterFg;
        QPalette pal = viewport()->palette();
        pal.setColor(QPalette::Window, m_bg);
        viewport()->setPalette(pal);
        viewport()->setAutoFillBackground(true);
        viewport()->update();
    }

    int rowHeight() const { return m_rowH; }

    // Scroll both panes together: expose/set the top row.
    int topRow() const { return verticalScrollBar()->value(); }
    void setTopRow(int r) { verticalScrollBar()->setValue(r); }

    void recalcMetrics()
    {
        QFontMetricsF fm(font());
        m_rowH = qMax(1, qRound(fm.height()) + 2);
        m_charW = fm.horizontalAdvance(QLatin1Char('M'));
        m_gutterW = qRound(m_charW * 6) + 10;
    }

    void updateScrollRange()
    {
        int rows = m_rows ? m_rows->size() : 0;
        int visible = viewport()->height() / qMax(1, m_rowH);
        verticalScrollBar()->setRange(0, qMax(0, rows - visible));
        verticalScrollBar()->setPageStep(visible);

        // Horizontal: widest line.
        int maxLen = 0;
        if (m_lines) {
            for (const QString &s : *m_lines)
                maxLen = qMax(maxLen, s.size());
        }
        int w = qRound(maxLen * m_charW) + m_gutterW + 20;
        horizontalScrollBar()->setRange(0, qMax(0, w - viewport()->width()));
        horizontalScrollBar()->setPageStep(viewport()->width());
    }

signals:
    void scrolledVertically(int topRow);

protected:
    void resizeEvent(QResizeEvent *e) override
    {
        QAbstractScrollArea::resizeEvent(e);
        updateScrollRange();
    }

    void scrollContentsBy(int dx, int dy) override
    {
        QAbstractScrollArea::scrollContentsBy(dx, dy);
        if (dy != 0)
            emit scrolledVertically(verticalScrollBar()->value());
        viewport()->update();
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(viewport());
        p.fillRect(viewport()->rect(), m_bg);
        if (!m_rows || !m_lines)
            return;

        p.setFont(font());
        const int top = verticalScrollBar()->value();
        const int hoff = horizontalScrollBar()->value();
        const int visible = viewport()->height() / m_rowH + 2;
        QFontMetricsF fm(font());
        const qreal ascent = fm.ascent();

        for (int vr = 0; vr < visible; ++vr) {
            int rowIdx = top + vr;
            if (rowIdx >= m_rows->size())
                break;
            const Diff::Row &row = m_rows->at(rowIdx);
            int y = vr * m_rowH;

            int lineIdx = (m_side == Left) ? row.left : row.right;
            bool present = (lineIdx >= 0);

            // Row background by kind.
            QColor rowBg = m_bg;
            bool thisSideChanged = false;
            switch (row.kind) {
            case Diff::Equal: rowBg = m_bg; break;
            case Diff::Changed: rowBg = m_changed; thisSideChanged = true; break;
            case Diff::Inserted:
                rowBg = (m_side == Right) ? m_inserted : gapColor();
                break;
            case Diff::Removed:
                rowBg = (m_side == Left) ? m_removed : gapColor();
                break;
            }
            p.fillRect(0, y, viewport()->width(), m_rowH, rowBg);

            // Gutter (line number).
            p.fillRect(0, y, m_gutterW, m_rowH, m_gutterBg);
            if (present) {
                p.setPen(m_gutterFg);
                p.drawText(0, y, m_gutterW - 6, m_rowH, Qt::AlignRight | Qt::AlignVCenter,
                           QString::number(lineIdx + 1));
            }

            if (!present)
                continue;

            const QString &text = m_lines->at(lineIdx);
            int tx = m_gutterW + 4 - hoff;

            // Inline highlight for changed rows.
            if (thisSideChanged) {
                QVector<Diff::InlineSpan> ls, rs;
                Diff::inlineDiff(m_lines->at(lineIdx),
                                 otherText(row),
                                 ls, rs, *m_opts);
                const QVector<Diff::InlineSpan> &spans = (m_side == Left) ? ls : rs;
                for (const Diff::InlineSpan &sp : spans) {
                    qreal sx = tx + fm.horizontalAdvance(text.left(sp.start));
                    qreal sw = fm.horizontalAdvance(text.mid(sp.start, sp.length));
                    p.fillRect(QRectF(sx, y, sw, m_rowH), m_inlineHi);
                }
            }

            p.setPen(m_fg);
            p.drawText(QPointF(tx, y + ascent + 1), text);
        }
    }

    void wheelEvent(QWheelEvent *e) override
    {
        int delta = e->angleDelta().y();
        int step = (delta > 0 ? -3 : 3);
        verticalScrollBar()->setValue(verticalScrollBar()->value() + step);
        e->accept();
    }

private:
    QColor gapColor() const
    {
        // Hatched-ish gap: a slightly darker/greyer shade of the background.
        return QColor::fromRgb(
            (m_bg.red() + m_gutterBg.red()) / 2,
            (m_bg.green() + m_gutterBg.green()) / 2,
            (m_bg.blue() + m_gutterBg.blue()) / 2);
    }

    QString otherText(const Diff::Row &row) const
    {
        // Needs the other side's lines; the owning tab keeps both, but each
        // pane only has its own. For inline diff we fetch from the sibling via
        // a stored pointer set by the tab.
        if (!m_otherLines)
            return QString();
        int idx = (m_side == Left) ? row.right : row.left;
        if (idx < 0 || idx >= m_otherLines->size())
            return QString();
        return m_otherLines->at(idx);
    }

public:
    void setOtherLines(const QStringList *other) { m_otherLines = other; }

private:
    Side m_side;
    const QStringList *m_lines = nullptr;
    const QStringList *m_otherLines = nullptr;
    const QVector<Diff::Row> *m_rows = nullptr;
    const Diff::Options *m_opts = nullptr;

    int m_rowH = 16;
    qreal m_charW = 8;
    int m_gutterW = 50;

    QColor m_bg = QColor(30, 30, 30);
    QColor m_fg = QColor(212, 212, 212);
    QColor m_changed = QColor(60, 60, 20);
    QColor m_inserted = QColor(20, 70, 30);
    QColor m_removed = QColor(80, 30, 30);
    QColor m_inlineHi = QColor(120, 100, 40);
    QColor m_gutterBg = QColor(45, 45, 45);
    QColor m_gutterFg = QColor(140, 140, 140);
};

// ===========================================================================
//  TextCompareTab
// ===========================================================================
TextCompareTab::TextCompareTab(QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // Toolbar with side-by-side actions.
    auto *tb = new QToolBar(this);
    tb->addAction(TRc("◀ Prev Diff"), this, &TextCompareTab::prevDiff);
    tb->addAction(TRc("Next Diff ▶"), this, &TextCompareTab::nextDiff);
    tb->addSeparator();
    tb->addAction(TRc("Copy →"), this, &TextCompareTab::copyLeftToRight);
    tb->addAction(TRc("← Copy"), this, &TextCompareTab::copyRightToLeft);
    tb->addSeparator();
    tb->addAction(TRc("Swap"), this, &TextCompareTab::swapSides);
    tb->addAction(TRc("Reload"), this, &TextCompareTab::reload);
    // Switch the current two files to another comparison type.
    tb->addAction(TRc("As Hex"), this, [this]() {
        if (!m_leftPath.isEmpty() && !m_rightPath.isEmpty())
            emit requestCompareAs(2, m_leftPath, m_rightPath);
    });
    tb->addAction(TRc("As Picture"), this, [this]() {
        if (!m_leftPath.isEmpty() && !m_rightPath.isEmpty())
            emit requestCompareAs(3, m_leftPath, m_rightPath);
    });
    tb->addAction(TRc("Save L"), this, &TextCompareTab::saveLeft);
    tb->addAction(TRc("Save R"), this, &TextCompareTab::saveRight);
    tb->addSeparator();

    QAction *icAct = tb->addAction(TRc("Ignore Case"));
    icAct->setCheckable(true);
    connect(icAct, &QAction::toggled, this, [this](bool on) {
        m_opts.ignoreCase = on; recompute();
    });
    QAction *iwAct = tb->addAction(TRc("Ignore Whitespace"));
    iwAct->setCheckable(true);
    connect(iwAct, &QAction::toggled, this, [this](bool on) {
        m_opts.ignoreWhitespace = on; recompute();
    });
    outer->addWidget(tb);

    // Header labels (file paths).
    auto *hdr = new QHBoxLayout();
    hdr->setContentsMargins(4, 2, 4, 2);
    m_leftLabel = new QLabel(TRc("(left)"), this);
    m_rightLabel = new QLabel(TRc("(right)"), this);
    m_leftLabel->setStyleSheet("font-weight:bold;");
    m_rightLabel->setStyleSheet("font-weight:bold;");
    hdr->addWidget(m_leftLabel, 1);
    hdr->addWidget(m_rightLabel, 1);
    outer->addLayout(hdr);

    // The two panes side by side.
    auto *panes = new QHBoxLayout();
    panes->setContentsMargins(0, 0, 0, 0);
    panes->setSpacing(1);
    m_left = new DiffPane(DiffPane::Left, this);
    m_right = new DiffPane(DiffPane::Right, this);
    panes->addWidget(m_left, 1);
    panes->addWidget(m_right, 1);
    outer->addLayout(panes, 1);

    m_statsLabel = new QLabel(this);
    m_statsLabel->setContentsMargins(6, 2, 6, 2);
    outer->addWidget(m_statsLabel);

    // Synchronized vertical scrolling.
    connect(m_left, &DiffPane::scrolledVertically, this, [this](int r) {
        if (m_right->topRow() != r) m_right->setTopRow(r);
    });
    connect(m_right, &DiffPane::scrolledVertically, this, [this](int r) {
        if (m_left->topRow() != r) m_left->setTopRow(r);
    });

    m_left->setOtherLines(&m_rightLines);
    m_right->setOtherLines(&m_leftLines);
}

static QStringList readLines(const QString &path, bool &ok)
{
    QStringList lines;
    QFile f(path);
    ok = f.open(QIODevice::ReadOnly | QIODevice::Text);
    if (!ok)
        return lines;
    QTextStream ts(&f);
    while (!ts.atEnd())
        lines << ts.readLine();
    f.close();
    return lines;
}

bool TextCompareTab::loadFiles(const QString &leftPath, const QString &rightPath)
{
    bool okL = false, okR = false;
    m_leftLines = readLines(leftPath, okL);
    m_rightLines = readLines(rightPath, okR);
    m_leftPath = leftPath;
    m_rightPath = rightPath;
    m_leftLabel->setText(QFileInfo(leftPath).fileName() + "  —  " + leftPath);
    m_rightLabel->setText(QFileInfo(rightPath).fileName() + "  —  " + rightPath);
    recompute();
    return okL && okR;
}

void TextCompareTab::setTexts(const QStringList &left, const QStringList &right,
                              const QString &leftTitle, const QString &rightTitle)
{
    m_leftLines = left;
    m_rightLines = right;
    m_leftPath.clear();
    m_rightPath.clear();
    m_leftLabel->setText(leftTitle);
    m_rightLabel->setText(rightTitle);
    recompute();
}

void TextCompareTab::recompute()
{
    m_rows = Diff::compute(m_leftLines, m_rightLines, m_opts);
    rebuildFromRows();
    m_currentDiff = -1;
    updateStats();
}

void TextCompareTab::rebuildFromRows()
{
    m_left->setModel(&m_leftLines, &m_rows, &m_opts);
    m_right->setModel(&m_rightLines, &m_rows, &m_opts);
    m_left->setOtherLines(&m_rightLines);
    m_right->setOtherLines(&m_leftLines);
}

void TextCompareTab::updateStats()
{
    int changed = 0, inserted = 0, removed = 0, same = 0;
    for (const Diff::Row &r : m_rows) {
        switch (r.kind) {
        case Diff::Equal: ++same; break;
        case Diff::Changed: ++changed; break;
        case Diff::Inserted: ++inserted; break;
        case Diff::Removed: ++removed; break;
        }
    }
    const QString msg = TRc("Same: %1   Changed: %2   Inserted: %3   Removed: %4")
                            .arg(same).arg(changed).arg(inserted).arg(removed);
    m_statsLabel->setText(msg);
    emit statusMessage(msg);
}

// Find the start row index of the next/previous difference block.
static bool isDiffRow(const Diff::Row &r) { return r.kind != Diff::Equal; }

void TextCompareTab::nextDiff()
{
    if (m_rows.isEmpty())
        return;
    int start = m_currentDiff < 0 ? 0 : m_currentDiff + 1;
    // Skip to the next diff block boundary.
    int i = start;
    // If we're inside a diff block, skip past it first.
    while (i < m_rows.size() && isDiffRow(m_rows[i]))
        ++i;
    while (i < m_rows.size() && !isDiffRow(m_rows[i]))
        ++i;
    if (i < m_rows.size()) {
        m_currentDiff = i;
        m_left->setTopRow(qMax(0, i - 2));
        m_right->setTopRow(qMax(0, i - 2));
    }
}

void TextCompareTab::prevDiff()
{
    if (m_rows.isEmpty())
        return;
    int i = (m_currentDiff < 0 ? m_rows.size() : m_currentDiff) - 1;
    while (i >= 0 && isDiffRow(m_rows[i]))
        --i;
    while (i >= 0 && !isDiffRow(m_rows[i]))
        --i;
    // i now at last row of previous equal run; step to the block start.
    int blockEnd = i;
    while (blockEnd >= 0 && isDiffRow(m_rows[blockEnd]))
        --blockEnd;
    int start = blockEnd + 1;
    if (i >= 0 && isDiffRow(m_rows[i])) {
        m_currentDiff = start;
        m_left->setTopRow(qMax(0, start - 2));
        m_right->setTopRow(qMax(0, start - 2));
    }
}

void TextCompareTab::swapSides()
{
    std::swap(m_leftLines, m_rightLines);
    std::swap(m_leftPath, m_rightPath);
    const QString lt = m_leftLabel->text();
    m_leftLabel->setText(m_rightLabel->text());
    m_rightLabel->setText(lt);
    recompute();
}

// Copy the current difference block from left to right (replace right's lines
// in that block with left's). Simplified block-level copy.
void TextCompareTab::copyLeftToRight()
{
    if (m_currentDiff < 0 || m_currentDiff >= m_rows.size())
        return;
    // Determine block extent.
    int s = m_currentDiff;
    while (s > 0 && isDiffRow(m_rows[s - 1])) --s;
    int e = m_currentDiff;
    while (e + 1 < m_rows.size() && isDiffRow(m_rows[e + 1])) ++e;

    QStringList leftBlock;
    int rightStart = -1, rightCount = 0;
    for (int i = s; i <= e; ++i) {
        const Diff::Row &r = m_rows[i];
        if (r.left >= 0) leftBlock << m_leftLines[r.left];
        if (r.right >= 0) {
            if (rightStart < 0) rightStart = r.right;
            ++rightCount;
        }
    }
    if (rightStart < 0) {
        // Pure insertion on left side: figure insertion point on right.
        rightStart = 0;
        for (int i = s - 1; i >= 0; --i) {
            if (m_rows[i].right >= 0) { rightStart = m_rows[i].right + 1; break; }
        }
    }
    for (int i = 0; i < rightCount; ++i)
        m_rightLines.removeAt(rightStart);
    for (int i = 0; i < leftBlock.size(); ++i)
        m_rightLines.insert(rightStart + i, leftBlock[i]);

    recompute();
    emit statusMessage(TRc("Copied block to the right side."));
}

void TextCompareTab::copyRightToLeft()
{
    if (m_currentDiff < 0 || m_currentDiff >= m_rows.size())
        return;
    int s = m_currentDiff;
    while (s > 0 && isDiffRow(m_rows[s - 1])) --s;
    int e = m_currentDiff;
    while (e + 1 < m_rows.size() && isDiffRow(m_rows[e + 1])) ++e;

    QStringList rightBlock;
    int leftStart = -1, leftCount = 0;
    for (int i = s; i <= e; ++i) {
        const Diff::Row &r = m_rows[i];
        if (r.right >= 0) rightBlock << m_rightLines[r.right];
        if (r.left >= 0) {
            if (leftStart < 0) leftStart = r.left;
            ++leftCount;
        }
    }
    if (leftStart < 0) {
        leftStart = 0;
        for (int i = s - 1; i >= 0; --i) {
            if (m_rows[i].left >= 0) { leftStart = m_rows[i].left + 1; break; }
        }
    }
    for (int i = 0; i < leftCount; ++i)
        m_leftLines.removeAt(leftStart);
    for (int i = 0; i < rightBlock.size(); ++i)
        m_leftLines.insert(leftStart + i, rightBlock[i]);

    recompute();
    emit statusMessage(TRc("Copied block to the left side."));
}

void TextCompareTab::reload()
{
    if (!m_leftPath.isEmpty() && !m_rightPath.isEmpty())
        loadFiles(m_leftPath, m_rightPath);
}

static bool writeLines(const QString &path, const QStringList &lines)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;
    QTextStream ts(&f);
    for (int i = 0; i < lines.size(); ++i) {
        ts << lines[i];
        if (i + 1 < lines.size())
            ts << '\n';
    }
    f.close();
    return true;
}

void TextCompareTab::saveLeft()
{
    if (m_leftPath.isEmpty())
        return;
    if (writeLines(m_leftPath, m_leftLines))
        emit statusMessage(TRc("Saved left file: %1").arg(m_leftPath));
}

void TextCompareTab::saveRight()
{
    if (m_rightPath.isEmpty())
        return;
    if (writeLines(m_rightPath, m_rightLines))
        emit statusMessage(TRc("Saved right file: %1").arg(m_rightPath));
}

void TextCompareTab::applyTheme(const EditorTheme &t)
{
    const bool dark = t.background.lightness() < 128;
    QColor changed  = dark ? QColor(60, 58, 20)  : QColor(255, 250, 205);
    QColor inserted = dark ? QColor(22, 70, 34)  : QColor(210, 245, 215);
    QColor removed  = dark ? QColor(80, 30, 30)  : QColor(255, 220, 220);
    QColor inlineHi = dark ? QColor(130, 110, 40): QColor(255, 225, 150);
    QColor gutterBg = t.lineNumberBg;
    QColor gutterFg = t.lineNumberFg;

    m_left->setColors(t.background, t.foreground, changed, inserted, removed,
                      inlineHi, gutterBg, gutterFg);
    m_right->setColors(t.background, t.foreground, changed, inserted, removed,
                       inlineHi, gutterBg, gutterFg);
}

#include "textcomparetab.moc"