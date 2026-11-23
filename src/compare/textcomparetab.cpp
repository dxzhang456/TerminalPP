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
#include <QStringDecoder>
#include <QStringConverter>
#include <QFileInfo>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QEvent>
#include <QMenu>
#include <QInputDialog>
#include <QLineEdit>

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
        setFocusPolicy(Qt::StrongFocus);
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

    Side side() const { return m_side; }

    // --- row selection (single line or a multi-line range) ----------------
    // The selection is stored as an inclusive [first, last] range of *row*
    // indices (indices into the aligned m_rows list, not file line numbers).
    bool hasSelection() const { return m_selFirst >= 0 && m_selLast >= 0; }
    int selectionFirst() const { return qMin(m_selFirst, m_selLast); }
    int selectionLast()  const { return qMax(m_selFirst, m_selLast); }
    void clearSelection() { m_selFirst = m_selLast = -1; viewport()->update(); }
    void setSelection(int first, int last)
    {
        const int n = m_rows ? m_rows->size() : 0;
        if (n == 0) { clearSelection(); return; }
        m_selFirst = qBound(0, first, n - 1);
        m_selLast  = qBound(0, last,  n - 1);
        viewport()->update();
    }

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

    // Map a viewport y coordinate to a row index (-1 when past the last row).
    int rowAt(int y) const
    {
        if (!m_rows || m_rows->isEmpty())
            return -1;
        const int idx = verticalScrollBar()->value() + y / qMax(1, m_rowH);
        return (idx >= 0 && idx < m_rows->size()) ? idx : -1;
    }

signals:
    void scrolledVertically(int topRow);
    // The user changed the row selection by dragging/clicking in this pane.
    void selectionChanged(int first, int last, bool fromLeft);
    // Right-click inside the pane: ask the owner to pop up the row actions.
    void rowContextMenuRequested(const QPoint &globalPos, bool fromLeft);
    // Double-click a row: ask the owner to edit that line in place.
    void rowDoubleClicked(int row, bool fromLeft);

protected:
    void resizeEvent(QResizeEvent *e) override
    {
        QAbstractScrollArea::resizeEvent(e);
        updateScrollRange();
    }

    // QAbstractScrollArea delivers all mouse/context-menu events for its
    // content to the VIEWPORT, and routes them through viewportEvent() — this
    // (not mousePressEvent() on the scroll area, and not an installed event
    // filter) is the sanctioned hook, and it is what makes row selection and
    // the right-click menu actually fire.
    bool viewportEvent(QEvent *ev) override
    {
        switch (ev->type()) {
        case QEvent::MouseButtonPress:
            handleMousePress(static_cast<QMouseEvent *>(ev));
            return true;
        case QEvent::MouseMove:
            if (m_dragging) {
                handleMouseMove(static_cast<QMouseEvent *>(ev));
                return true;
            }
            break;
        case QEvent::MouseButtonRelease:
            if (m_dragging) {
                m_dragging = false;
                return true;
            }
            break;
        case QEvent::MouseButtonDblClick: {
            // Double-clicking a line selects it and asks the owner to edit it.
            auto *me = static_cast<QMouseEvent *>(ev);
            if (me->button() == Qt::LeftButton) {
                const int r = rowAt(me->position().toPoint().y());
                if (r >= 0) {
                    m_selFirst = m_selLast = r;
                    m_dragging = false;
                    viewport()->update();
                    emit selectionChanged(r, r, m_side == Left);
                    emit rowDoubleClicked(r, m_side == Left);
                }
                return true;
            }
            break;
        }
        case QEvent::ContextMenu: {
            // Also handle the dedicated context-menu event (this is what the
            // Menu key and some mice/styles send instead of a right-click).
            auto *ce = static_cast<QContextMenuEvent *>(ev);
            const int r = rowAt(ce->pos().y());
            if (r >= 0 && (!hasSelection()
                           || r < selectionFirst() || r > selectionLast())) {
                m_selFirst = m_selLast = r;
                viewport()->update();
                emit selectionChanged(r, r, m_side == Left);
            }
            emit rowContextMenuRequested(ce->globalPos(), m_side == Left);
            return true;
        }
        default:
            break;
        }
        return QAbstractScrollArea::viewportEvent(ev);
    }

    // --- mouse: click selects a row, drag extends the selection ------------
    // Called from viewportEvent() above.
    void handleMousePress(QMouseEvent *e)
    {
        if (e->button() == Qt::LeftButton) {
            const int r = rowAt(e->position().toPoint().y());
            if (r >= 0) {
                if ((e->modifiers() & Qt::ShiftModifier) && m_selFirst >= 0) {
                    m_selLast = r;                 // extend from the anchor
                } else {
                    m_selFirst = m_selLast = r;    // start a new selection
                }
                m_dragging = true;
                viewport()->update();
                emit selectionChanged(selectionFirst(), selectionLast(),
                                      m_side == Left);
            }
            return;
        }
        if (e->button() == Qt::RightButton) {
            // Right-clicking outside the current selection selects that row
            // first, so a plain right-click acts on the clicked line.
            const int r = rowAt(e->position().toPoint().y());
            if (r >= 0 && (!hasSelection()
                           || r < selectionFirst() || r > selectionLast())) {
                m_selFirst = m_selLast = r;
                viewport()->update();
                emit selectionChanged(r, r, m_side == Left);
            }
            emit rowContextMenuRequested(e->globalPosition().toPoint(),
                                         m_side == Left);
        }
    }

    void handleMouseMove(QMouseEvent *e)
    {
        const int r = rowAt(e->position().toPoint().y());
        if (r >= 0 && r != m_selLast) {
            m_selLast = r;
            viewport()->update();
            emit selectionChanged(selectionFirst(), selectionLast(),
                                  m_side == Left);
        }
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

        // Selection overlay, drawn LAST so it sits above the diff colours.
        // A translucent blue wash plus a solid border makes the selected
        // line(s) unmistakable on both light and dark themes.
        if (hasSelection()) {
            const int s = selectionFirst(), eRow = selectionLast();
            const QColor wash(64, 140, 255, 90);
            const QColor border(90, 170, 255);
            for (int r = s; r <= eRow; ++r) {
                const int y = (r - top) * m_rowH;
                if (y + m_rowH < 0 || y > viewport()->height())
                    continue;
                p.fillRect(0, y, viewport()->width(), m_rowH, wash);
            }
            // Outline the whole selected block.
            const int y0 = (s - top) * m_rowH;
            const int y1 = (eRow - top + 1) * m_rowH;
            p.setPen(QPen(border, 1));
            p.drawRect(QRect(0, y0, viewport()->width() - 1, y1 - y0 - 1));
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

    // Inclusive row-index selection range (-1 == nothing selected).
    int m_selFirst = -1;
    int m_selLast = -1;
    bool m_dragging = false;

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

    // Track the active row selection in either pane so the context menu and
    // the row operations know what to act on.
    auto trackSel = [this](int first, int last, bool fromLeft) {
        m_selFromLeft = fromLeft;
        m_selFirstRow = first;
        m_selLastRow = last;
        // Only one pane owns the selection at a time.
        if (fromLeft) m_right->clearSelection();
        else          m_left->clearSelection();
    };
    connect(m_left, &DiffPane::selectionChanged, this, trackSel);
    connect(m_right, &DiffPane::selectionChanged, this, trackSel);
    connect(m_left, &DiffPane::rowContextMenuRequested,
            this, &TextCompareTab::showRowMenu);
    connect(m_right, &DiffPane::rowContextMenuRequested,
            this, &TextCompareTab::showRowMenu);
    // Double-click a line to edit it in place.
    auto onDblClick = [this](int row, bool fromLeft) {
        m_selFromLeft = fromLeft;
        m_selFirstRow = m_selLastRow = row;
        editSelectedLine();
    };
    connect(m_left, &DiffPane::rowDoubleClicked, this, onDblClick);
    connect(m_right, &DiffPane::rowDoubleClicked, this, onDblClick);

    m_left->setOtherLines(&m_rightLines);
    m_right->setOtherLines(&m_leftLines);
}

// ---------------------------------------------------------------------------
//  Row-level operations on the current selection
//
//  The user can select a single line or a range of lines in either pane and
//  then right-click to copy / move them to the other side, or delete them.
//  Each operation edits the underlying line lists and calls recompute(), so
//  the side-by-side result (and the statistics) refresh immediately.
// ---------------------------------------------------------------------------
void TextCompareTab::showRowMenu(const QPoint &globalPos, bool fromLeft)
{
    // Always re-read the range straight from the pane that was clicked, so the
    // menu acts on what is actually highlighted (the cached values can be
    // stale after a previous edit shifted the rows).
    m_selFromLeft = fromLeft;
    DiffPane *pane = fromLeft ? m_left : m_right;
    if (pane->hasSelection()) {
        m_selFirstRow = pane->selectionFirst();
        m_selLastRow = pane->selectionLast();
    }
    if (m_selFirstRow < 0 || m_selLastRow < 0) {
        emit statusMessage(TRc("Click a line first, then right-click it."));
        return;
    }

    const int count = m_selLastRow - m_selFirstRow + 1;
    QMenu menu(this);
    menu.addAction(TRc("Selected line(s): %1").arg(count))->setEnabled(false);
    menu.addSeparator();
    // Always offer BOTH directions regardless of which pane was clicked, so
    // "copy the left line(s) to the right" and "copy the right line(s) to the
    // left" are both one click away.
    menu.addAction(TRc("Copy Left → Right"), this,
                   &TextCompareTab::copySelectionToRight);
    menu.addAction(TRc("Copy Right → Left"), this,
                   &TextCompareTab::copySelectionToLeft);
    menu.addSeparator();
    menu.addAction(TRc("Move Left → Right"), this,
                   &TextCompareTab::moveSelectionToRight);
    menu.addAction(TRc("Move Right → Left"), this,
                   &TextCompareTab::moveSelectionToLeft);
    menu.addSeparator();
    menu.addAction(TRc("Edit Line..."), this, &TextCompareTab::editSelectedLine);
    menu.addAction(TRc("Delete"), this, &TextCompareTab::deleteSelection);
    menu.exec(globalPos);
}

// Edit the first selected line in place (also reached by double-clicking a
// line). The edited text replaces that line on the side it came from, the
// diff is recomputed, and the file can then be saved with "Save L"/"Save R".
void TextCompareTab::editSelectedLine()
{
    if (m_selFirstRow < 0 || m_selFirstRow >= m_rows.size())
        return;
    const Diff::Row &row = m_rows[m_selFirstRow];
    const int lineIdx = m_selFromLeft ? row.left : row.right;
    QStringList &lines = m_selFromLeft ? m_leftLines : m_rightLines;
    if (lineIdx < 0 || lineIdx >= lines.size()) {
        emit statusMessage(TRc("That row is a gap on this side — nothing to edit."));
        return;
    }

    bool ok = false;
    const QString edited = QInputDialog::getText(
        this,
        m_selFromLeft ? TRc("Edit Left Line %1").arg(lineIdx + 1)
                      : TRc("Edit Right Line %1").arg(lineIdx + 1),
        TRc("Line content:"), QLineEdit::Normal, lines[lineIdx], &ok);
    if (!ok || edited == lines[lineIdx])
        return;

    lines[lineIdx] = edited;
    const int keepRow = m_selFirstRow;
    const bool keepLeft = m_selFromLeft;
    recompute();                       // live re-diff

    // Restore the selection so the user sees what they just changed.
    DiffPane *pane = keepLeft ? m_left : m_right;
    if (!m_rows.isEmpty()) {
        const int r = qBound(0, keepRow, m_rows.size() - 1);
        m_selFirstRow = m_selLastRow = r;
        m_selFromLeft = keepLeft;
        pane->setSelection(r, r);
    }
    emit statusMessage(
        TRc("Edited %1 line %2. Use \"Save L\" / \"Save R\" to write the file.")
            .arg(keepLeft ? TRc("left") : TRc("right")).arg(lineIdx + 1));
}

// Translate the selected row range into real line indices on `fromLeft`'s side.
// Gap rows (no line on that side) are skipped. `insertAtOther` receives the
// index on the OTHER side where the lines should land, derived from the same
// row range (or, when those rows are all gaps there, from the nearest
// preceding row that does have a line).
bool TextCompareTab::selectionLineRange(bool fromLeft, int *firstLine,
                                        int *lastLine, int *insertAtOther) const
{
    if (m_selFirstRow < 0 || m_selLastRow < 0 || m_rows.isEmpty())
        return false;
    const int s = qBound(0, m_selFirstRow, m_rows.size() - 1);
    const int e = qBound(0, m_selLastRow, m_rows.size() - 1);

    int fl = -1, ll = -1;          // this side's line range
    int otherFirst = -1, otherLast = -1;
    for (int i = s; i <= e; ++i) {
        const Diff::Row &r = m_rows[i];
        const int mine  = fromLeft ? r.left  : r.right;
        const int other = fromLeft ? r.right : r.left;
        if (mine >= 0) {
            if (fl < 0) fl = mine;
            ll = mine;
        }
        if (other >= 0) {
            if (otherFirst < 0) otherFirst = other;
            otherLast = other;
        }
    }
    if (fl < 0)
        return false;              // nothing but gaps on this side

    *firstLine = fl;
    *lastLine = ll;

    if (otherFirst >= 0) {
        // The other side has lines aligned with this selection: they will be
        // replaced, so insertion starts at the first of them.
        *insertAtOther = otherFirst;
    } else {
        // Pure insertion: find where to splice on the other side by walking
        // backwards to the closest row that exists there.
        int at = 0;
        for (int i = s - 1; i >= 0; --i) {
            const int other = fromLeft ? m_rows[i].right : m_rows[i].left;
            if (other >= 0) { at = other + 1; break; }
        }
        *insertAtOther = at;
    }
    Q_UNUSED(otherLast);
    return true;
}

void TextCompareTab::applyRowOperation(bool fromLeft, bool copy, bool erase)
{
    int firstLine = 0, lastLine = 0, insertAt = 0;
    if (!selectionLineRange(fromLeft, &firstLine, &lastLine, &insertAt)) {
        emit statusMessage(TRc("Select one or more lines first."));
        return;
    }

    QStringList &src = fromLeft ? m_leftLines  : m_rightLines;
    QStringList &dst = fromLeft ? m_rightLines : m_leftLines;

    // The lines being operated on.
    const int count = lastLine - firstLine + 1;
    QStringList block;
    for (int i = firstLine; i <= lastLine && i < src.size(); ++i)
        block << src[i];

    if (copy) {
        // How many lines on the other side are aligned with this selection?
        // Those get replaced; anything else is a plain insertion.
        const int s = qBound(0, m_selFirstRow, m_rows.size() - 1);
        const int e = qBound(0, m_selLastRow, m_rows.size() - 1);
        int otherCount = 0;
        for (int i = s; i <= e; ++i) {
            const int other = fromLeft ? m_rows[i].right : m_rows[i].left;
            if (other >= 0) ++otherCount;
        }
        insertAt = qBound(0, insertAt, dst.size());
        for (int i = 0; i < otherCount && insertAt < dst.size(); ++i)
            dst.removeAt(insertAt);
        for (int i = 0; i < block.size(); ++i)
            dst.insert(insertAt + i, block[i]);
    }

    if (erase) {
        // Remove the block from its own side. Deleting repeatedly at the same
        // index walks the list forward, so `count` removals drop exactly the
        // selected lines.
        int removed = 0;
        for (int i = 0; i < count && firstLine < src.size(); ++i) {
            src.removeAt(firstLine);
            ++removed;
        }
        if (removed == 0) {
            emit statusMessage(TRc("Nothing to delete on that side."));
            return;
        }
    }

    // Re-diff so the view and the statistics reflect the edit right away.
    const int keepRow = m_selFirstRow;
    recompute();

    // Keep a sensible selection where the user was working.
    DiffPane *pane = fromLeft ? m_left : m_right;
    if (!m_rows.isEmpty()) {
        const int row = qBound(0, keepRow, m_rows.size() - 1);
        m_selFirstRow = m_selLastRow = row;
        m_selFromLeft = fromLeft;
        pane->setSelection(row, row);
    } else {
        m_selFirstRow = m_selLastRow = -1;
        pane->clearSelection();
    }

    const QString what = copy ? (erase ? TRc("Moved") : TRc("Copied"))
                              : TRc("Deleted");
    emit statusMessage(TRc("%1 %2 line(s).").arg(what).arg(count));
}

void TextCompareTab::copySelectionToRight() { applyRowOperation(true,  true,  false); }
void TextCompareTab::copySelectionToLeft()  { applyRowOperation(false, true,  false); }
void TextCompareTab::moveSelectionToRight() { applyRowOperation(true,  true,  true);  }
void TextCompareTab::moveSelectionToLeft()  { applyRowOperation(false, true,  true);  }

void TextCompareTab::deleteSelection()
{
    // Delete the selected line(s) from the side they were selected in.
    // Resolve the row range from the pane itself so a stale cached value can
    // never make this silently do nothing.
    DiffPane *pane = m_selFromLeft ? m_left : m_right;
    if (pane && pane->hasSelection()) {
        m_selFirstRow = pane->selectionFirst();
        m_selLastRow = pane->selectionLast();
    }
    if (m_selFirstRow < 0 || m_selLastRow < 0) {
        emit statusMessage(TRc("Select one or more lines first."));
        return;
    }
    applyRowOperation(m_selFromLeft, false, true);
}

// Decode a text file into lines. QTextStream defaults to UTF-8 in Qt 6, which
// turns BOM-less GBK/GB2312 Chinese files into mojibake (乱码). Mirror the
// editor's logic: honour a BOM, else accept UTF-8 only when it really is valid
// UTF-8, otherwise fall back to the local ANSI codepage (CP936/GBK) and
// finally to Latin-1 (which never fails, so no byte is lost).
static QString decodeTextFile(const QByteArray &raw)
{
    auto encOpt = QStringConverter::encodingForData(raw);
    if (encOpt.has_value()) {
        QStringDecoder dec(encOpt.value());
        return dec.decode(raw);
    }
    QStringDecoder utf8(QStringConverter::Utf8);
    QString asUtf8 = utf8.decode(raw);
    if (!utf8.hasError())
        return asUtf8;
    QStringDecoder sys(QStringConverter::System);
    QString asSystem = sys.decode(raw);
    if (!sys.hasError())
        return asSystem;
    QStringDecoder l1(QStringConverter::Latin1);
    return l1.decode(raw);
}

static QStringList readLines(const QString &path, bool &ok)
{
    QStringList lines;
    QFile f(path);
    // Open in binary mode: the decoding and newline handling are done here so
    // the encoding can be detected from the raw bytes.
    ok = f.open(QIODevice::ReadOnly);
    if (!ok)
        return lines;
    QString text = decodeTextFile(f.readAll());
    f.close();
    // Normalize line endings, then split (keeping empty lines intact).
    text.replace("\r\n", "\n");
    text.replace('\r', '\n');
    lines = text.split('\n');
    // A trailing newline yields one empty element; drop it so the line count
    // matches what QTextStream::readLine() used to produce.
    if (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();
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