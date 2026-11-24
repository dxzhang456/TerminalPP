#include "pdf/pdftab.h"
#include "core/i18n.h"

#include <QBuffer>
#include <QColorDialog>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QDir>
#include <QSpinBox>
#include <QScrollBar>
#include <QKeySequence>
#include <QAction>
#include <QSignalBlocker>
#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QShortcut>
#include <QGuiApplication>
#include <QFontDatabase>
#include <QGroupBox>
#include <QStack>
#include <QRegularExpression>
#include <QtMath>

static inline QString TR(const char *s)
{
    return I18n::instance()->tr(QString::fromUtf8(s));
}

// ===========================================================================
// PdfPageView — the page surface + annotation overlay.
//
// Annotations are stored in *PDF point* coordinates (origin bottom-left, y
// upward) so they can be baked directly into the content stream. The view
// converts between widget pixels (origin top-left) and PDF points.
//
// Interaction model:
//   * No tool selected (default) -> drag selects text.
//   * Highlight/Underline/Strike/Pen/TextBox/StickyNote/Stamp -> left-drag
//     creates an annotation of that kind. After creation the tool resets to
//     "Select" so the user can immediately drag / resize what they just made.
//   * Select tool -> click an annotation to pick it up. The eight selection
//     handles resize it; dragging the body moves it. Delete key removes it.
// Every change (create, move, resize, delete) is pushed onto an undo stack so
// Ctrl+Z / Ctrl+Y work.
// ===========================================================================
class PdfPageView : public QWidget
{
    Q_OBJECT
public:
    enum Tool { Select, Highlight, Underline, Strike, TextBox, StickyNote, Pen, Stamp };

    struct Annotation {
        Tool tool = Select;
        QColor color;
        QVector<QPointF> points;   // PDF points (for Pen)
        QRectF rect;               // PDF points, normalised (for box tools)
        QString text;              // for TextBox / StickyNote
        QImage image;              // for Stamp / signature (ARGB)
        double rotation = 0.0;     // degrees (reserved)
    };

    // One per-page list — annotations stay on their original page even when
    // the user flips through the document before saving.
    struct PageState {
        QVector<Annotation> items;
    };

    explicit PdfPageView(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMouseTracking(true);
        setMinimumSize(200, 200);
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::IBeamCursor);
    }

    void setPage(int pageIndex, double wPts, double hPts, int rotation,
                 const QImage &bg)
    {
        m_pageIndex = pageIndex;
        m_pageW = wPts; m_pageH = hPts;
        int r = rotation % 360;
        if (r < 0) r += 360;
        m_rotation = (r / 90) * 90;
        m_background = bg;
        clearSelection();
        m_chars.clear();
        m_pickedIndex = -1;
        updateGeometryToPage();
        update();
    }

    // Real text layer (for selection/copy).
    void setTextChars(const QVector<PdfTextChar> &chars)
    {
        m_chars = chars;
        clearSelection();
        update();
    }
    bool hasTextLayer() const { return !m_chars.isEmpty(); }
    int textCharCount() const { return m_chars.size(); }

    bool hasSelection() const { return m_selEnd > m_selStart; }

    QString selectedText() const
    {
        if (!hasSelection()) return QString();
        QString out;
        out.reserve(m_selEnd - m_selStart + 8);
        QRectF prev;
        for (int i = m_selStart; i < m_selEnd && i < m_chars.size(); ++i) {
            const PdfTextChar &c = m_chars.at(i);
            const QChar ch = c.ch ? QChar(c.ch) : QChar(' ');
            if (!prev.isNull() && !c.box.isNull()
                && !out.endsWith('\n') && !out.endsWith('\r')
                && c.box.top() > prev.bottom() - prev.height() * 0.35
                && c.box.top() > prev.top() + prev.height() * 0.55) {
                out += '\n';
            }
            out += ch;
            if (!c.box.isNull()) prev = c.box;
        }
        return out;
    }
    QString allText() const
    {
        QString out;
        out.reserve(m_chars.size() + 8);
        QRectF prev;
        for (const PdfTextChar &c : m_chars) {
            const QChar ch = c.ch ? QChar(c.ch) : QChar(' ');
            if (!prev.isNull() && !c.box.isNull()
                && !out.endsWith('\n') && !out.endsWith('\r')
                && c.box.top() > prev.top() + prev.height() * 0.55) {
                out += '\n';
            }
            out += ch;
            if (!c.box.isNull()) prev = c.box;
        }
        return out;
    }
    void selectAllText()
    {
        if (m_chars.isEmpty()) return;
        m_selStart = 0; m_selEnd = m_chars.size();
        update(); emit selectionChanged();
    }
    void clearSelection()
    {
        const bool had = hasSelection();
        m_selStart = m_selEnd = 0;
        m_selecting = false;
        if (had) { update(); emit selectionChanged(); }
    }
    int copySelectionToClipboard()
    {
        QString txt = selectedText();
        if (txt.isEmpty()) txt = allText();
        if (txt.isEmpty()) return 0;
        QGuiApplication::clipboard()->setText(txt);
        return txt.size();
    }

    void setScale(double s)
    {
        m_scale = qBound(0.05, s, 10.0);
        updateGeometryToPage();
        update();
    }
    double scale() const { return m_scale; }

    double displayWidthPts() const
    { return (m_rotation == 90 || m_rotation == 270) ? m_pageH : m_pageW; }
    double displayHeightPts() const
    { return (m_rotation == 90 || m_rotation == 270) ? m_pageW : m_pageH; }

    void setTool(Tool t)
    {
        m_tool = t;
        m_pickedIndex = -1;
        setCursor(t == Select ? Qt::IBeamCursor : Qt::CrossCursor);
        update();
    }
    Tool tool() const { return m_tool; }
    void setColor(const QColor &c) { m_color = c; }
    void setPlaceholderNote(const QString &s) { m_placeholder = s; update(); }

    // --- Per-page annotation store --------------------------------------
    const QVector<Annotation> &annotationsOfPage(int page) const
    {
        static const QVector<Annotation> empty;
        auto it = m_pages.constFind(page);
        return it == m_pages.constEnd() ? empty : it.value().items;
    }
    QList<int> pagesWithAnnotations() const
    {
        QList<int> out;
        for (auto it = m_pages.constBegin(); it != m_pages.constEnd(); ++it)
            if (!it.value().items.isEmpty()) out.append(it.key());
        return out;
    }
    bool hasAnyAnnotations() const
    {
        for (auto it = m_pages.constBegin(); it != m_pages.constEnd(); ++it)
            if (!it.value().items.isEmpty()) return true;
        return false;
    }
    void clearAllAnnotations()
    {
        m_pages.clear();
        m_undo.clear();
        m_redo.clear();
        m_pickedIndex = -1;
        update();
    }
    // Insert an annotation on the current page (used by the signature flow).
    void insertAnnotationOnCurrentPage(const Annotation &a)
    {
        if (m_pageIndex < 0) return;
        pushUndoSnapshot();
        m_pages[m_pageIndex].items.append(a);
        m_pickedIndex = m_pages[m_pageIndex].items.size() - 1;
        setTool(Select);
        update();
        emit annotationsChanged();
    }

    // Undo / redo over the full per-page annotation state.
    void undo()
    {
        if (m_undo.isEmpty()) return;
        m_redo.push(takeSnapshot());
        restoreSnapshot(m_undo.pop());
        update();
        emit annotationsChanged();
    }
    void redo()
    {
        if (m_redo.isEmpty()) return;
        m_undo.push(takeSnapshot());
        restoreSnapshot(m_redo.pop());
        update();
        emit annotationsChanged();
    }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }

    double pageWidthPts() const { return m_pageW; }
    double pageHeightPts() const { return m_pageH; }
    int currentPageIndex() const { return m_pageIndex; }

signals:
    void annotationsChanged();         // overlay was mutated (create/move/...)
    void selectionChanged();

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), QColor(80, 80, 80));

        const QRectF pageRect = computePageRect();
        m_pageRect = pageRect;
        m_pageOrigin = pageRect.topLeft();

        p.fillRect(pageRect.translated(3, 3), QColor(50, 50, 50));
        p.fillRect(pageRect, Qt::white);

        if (!m_background.isNull()) {
            QSizeF target = QSizeF(m_background.size())
                                .scaled(pageRect.size(), Qt::KeepAspectRatio);
            QRectF dst(QPointF(0, 0), target);
            dst.moveCenter(pageRect.center());
            if (m_rotation == 0) {
                p.drawImage(dst, m_background);
            } else {
                QRectF src = (m_rotation == 90 || m_rotation == 270)
                                 ? QRectF(0, 0, dst.height(), dst.width())
                                 : dst;
                p.save();
                p.translate(dst.center());
                p.rotate(m_rotation);
                src.moveCenter(QPointF(0, 0));
                p.drawImage(src, m_background);
                p.restore();
            }
        } else {
            p.setPen(QColor(170, 170, 170));
            QFont f = p.font();
            f.setPointSizeF(qMax(7.0, qMin(16.0, pageRect.height() / 28.0)));
            p.setFont(f);
            QString note = TR("%1 × %2 pt").arg(int(m_pageW)).arg(int(m_pageH));
            if (m_rotation) note += TR("   (rotated %1°)").arg(m_rotation);
            if (!m_placeholder.isEmpty()) note += "\n\n" + m_placeholder;
            p.drawText(pageRect.adjusted(12, 12, -12, -12),
                       Qt::AlignCenter | Qt::TextWordWrap, note);
        }

        // Text selection highlight.
        if (hasSelection()) {
            QColor sel(60, 120, 240, 90);
            p.setPen(Qt::NoPen);
            p.setBrush(sel);
            QRectF run;
            for (int i = m_selStart; i < m_selEnd && i < m_chars.size(); ++i) {
                const QRectF b = m_chars.at(i).box;
                if (b.isNull() || b.isEmpty()) continue;
                const QRectF px = boxToPx(b);
                if (run.isNull()) { run = px; }
                else if (qAbs(px.top() - run.top()) <= run.height() * 0.6
                         && px.left() >= run.left() - 1) { run |= px; }
                else { p.drawRect(run); run = px; }
            }
            if (!run.isNull()) p.drawRect(run);
            p.setBrush(Qt::NoBrush);
        }

        p.setPen(QPen(QColor(120, 120, 120)));
        p.setBrush(Qt::NoBrush);
        p.drawRect(pageRect);

        // Committed-to-overlay annotations on this page.
        const QVector<Annotation> &list = annotationsOfPage(m_pageIndex);
        for (int i = 0; i < list.size(); ++i)
            drawAnnotation(p, list[i], i == m_pickedIndex);

        // In-progress annotation.
        if (m_drawing)
            drawAnnotation(p, m_current, false);
    }

    void mousePressEvent(QMouseEvent *e) override
    {
        setFocus(Qt::MouseFocusReason);

        if (e->button() != Qt::LeftButton) {
            QWidget::mousePressEvent(e);
            return;
        }

        if (m_tool == Select) {
            // 1) If an annotation is already picked, see if the user grabbed a
            //    resize handle.
            if (m_pickedIndex >= 0) {
                const int handle = handleAt(m_pickedIndex, e->pos());
                if (handle >= 0) {
                    pushUndoSnapshot();
                    m_resizingHandle = handle;
                    m_dragStart = e->pos();
                    m_origRect = m_pages[m_pageIndex].items[m_pickedIndex].rect;
                    return;
                }
            }
            // 2) Otherwise pick whichever annotation lies under the mouse.
            const int hit = annotationAt(e->pos());
            if (hit >= 0) {
                m_pickedIndex = hit;
                pushUndoSnapshot();
                m_dragging = true;
                m_dragStart = e->pos();
                const Annotation &a = m_pages[m_pageIndex].items[hit];
                m_origRect = a.rect;
                m_origPoints = a.points;
                update();
                return;
            }
            m_pickedIndex = -1;

            // 3) No annotation under the mouse -> start a text selection.
            if (m_chars.isEmpty()) { clearSelection(); return; }
            const int idx = charIndexAt(e->pos());
            m_selAnchor = idx;
            m_selStart = idx;
            m_selEnd = idx;
            m_selecting = true;
            update();
            emit selectionChanged();
            return;
        }

        // A drawing tool: start creating a new annotation.
        m_drawing = true;
        m_current = Annotation();
        m_current.tool = m_tool;
        m_current.color = m_color;
        m_current.image = m_stampImage;             // used by Stamp tool
        QPointF pt = toPdf(e->pos());
        if (m_tool == Pen) {
            m_current.points.append(pt);
        } else {
            m_current.rect = QRectF(pt, pt);
        }
    }

    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (m_tool == Select && m_pickedIndex >= 0 && m_resizingHandle >= 0) {
            resizePicked(e->pos());
            return;
        }
        if (m_tool == Select && m_dragging && m_pickedIndex >= 0) {
            movePicked(e->pos());
            return;
        }
        if (m_selecting) {
            const int idx = charIndexAt(e->pos());
            if (idx >= m_selAnchor) {
                m_selStart = m_selAnchor;
                m_selEnd = qMin(m_chars.size(), idx + 1);
            } else {
                m_selStart = idx;
                m_selEnd = qMin(m_chars.size(), m_selAnchor + 1);
            }
            update(); emit selectionChanged();
            return;
        }

        // Live cursor hint for the Select tool.
        if (m_tool == Select && !m_drawing) {
            if (m_pickedIndex >= 0) {
                const int h = handleAt(m_pickedIndex, e->pos());
                if (h >= 0) setCursor(cursorForHandle(h));
                else if (annotationAt(e->pos()) >= 0) setCursor(Qt::SizeAllCursor);
                else setCursor(Qt::IBeamCursor);
            } else if (annotationAt(e->pos()) >= 0) {
                setCursor(Qt::SizeAllCursor);
            } else {
                setCursor(Qt::IBeamCursor);
            }
        }

        if (!m_drawing) return;
        QPointF pt = toPdf(e->pos());
        if (m_tool == Pen) m_current.points.append(pt);
        else m_current.rect.setBottomRight(pt);
        update();
    }

    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (m_tool == Select) {
            if (m_resizingHandle >= 0) {
                m_resizingHandle = -1;
                emit annotationsChanged();
                return;
            }
            if (m_dragging) {
                m_dragging = false;
                emit annotationsChanged();
                return;
            }
            if (m_selecting) {
                m_selecting = false;
                emit selectionChanged();
                return;
            }
        }
        if (!m_drawing) return;
        m_drawing = false;
        QPointF pt = toPdf(e->pos());

        if (m_tool == TextBox || m_tool == StickyNote) {
            bool ok = false;
            QString txt = QInputDialog::getText(this, TR("Text"),
                                                TR("Enter annotation text:"),
                                                QLineEdit::Normal, QString(), &ok);
            if (!ok || txt.isEmpty()) { update(); return; }
            m_current.text = txt;
            if (m_current.rect.width() < 5) {
                // click (not drag): give the box a sensible default size.
                if (m_tool == StickyNote)
                    m_current.rect = QRectF(pt.x(), pt.y() + 20, 24, 24);
                else
                    m_current.rect = QRectF(pt.x(), pt.y() + 20, 200, 60);
            }
        }
        if (m_tool == Stamp && m_current.image.isNull()) {
            // The toolbar should have prompted for an image before entering
            // Stamp mode; if somehow we got here without one, cancel.
            update(); return;
        }
        if (m_tool != Pen) {
            m_current.rect = m_current.rect.normalized();
            // Drag-created boxes need some real size. Click-created text/sticky
            // boxes have already been sized above, so accept them.
            if (m_current.rect.width() < 2 && m_current.rect.height() < 2
                && m_tool != StickyNote && m_tool != TextBox) {
                update(); return;
            }
        } else if (m_current.points.size() < 2) {
            update(); return;
        }

        pushUndoSnapshot();
        m_pages[m_pageIndex].items.append(m_current);
        m_pickedIndex = m_pages[m_pageIndex].items.size() - 1;
        // After creating an annotation, switch back to Select so it can be
        // moved/resized without having to click a toolbar button first.
        setTool(Select);
        emit annotationsChanged();
        update();
    }

    void mouseDoubleClickEvent(QMouseEvent *e) override
    {
        if (m_tool != Select || m_chars.isEmpty()) {
            QWidget::mouseDoubleClickEvent(e);
            return;
        }
        // Edit the picked textbox/sticky note on double-click.
        const int hit = annotationAt(e->pos());
        if (hit >= 0) {
            Annotation &a = m_pages[m_pageIndex].items[hit];
            if (a.tool == TextBox || a.tool == StickyNote) {
                bool ok = false;
                QString txt = QInputDialog::getMultiLineText(
                    this, TR("Edit Text"), TR("Edit annotation text:"),
                    a.text, &ok);
                if (ok) {
                    pushUndoSnapshot();
                    a.text = txt;
                    emit annotationsChanged();
                    update();
                }
                return;
            }
        }
        const int idx = charIndexAt(e->pos());
        if (idx < 0 || idx >= m_chars.size()) return;
        auto isWordChar = [this](int i) {
            if (i < 0 || i >= m_chars.size()) return false;
            const QChar c(m_chars.at(i).ch);
            return c.isLetterOrNumber() || c == '_';
        };
        if (!isWordChar(idx)) { m_selStart = idx; m_selEnd = idx + 1; }
        else {
            int a = idx, b = idx;
            while (isWordChar(a - 1)) --a;
            while (isWordChar(b + 1)) ++b;
            m_selStart = a; m_selEnd = b + 1;
        }
        m_selAnchor = m_selStart;
        update();
        emit selectionChanged();
    }

    void keyPressEvent(QKeyEvent *e) override
    {
        if (e->matches(QKeySequence::Copy)) { copySelectionToClipboard(); e->accept(); return; }
        if (e->matches(QKeySequence::SelectAll)) { selectAllText(); e->accept(); return; }
        if (e->key() == Qt::Key_Escape) {
            if (m_pickedIndex >= 0) { m_pickedIndex = -1; update(); e->accept(); return; }
            if (hasSelection()) { clearSelection(); e->accept(); return; }
        }
        if ((e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace)
            && m_pickedIndex >= 0) {
            pushUndoSnapshot();
            auto &list = m_pages[m_pageIndex].items;
            list.removeAt(m_pickedIndex);
            m_pickedIndex = -1;
            update();
            emit annotationsChanged();
            e->accept();
            return;
        }
        QWidget::keyPressEvent(e);
    }

public:
    // Stamp configuration.
    void setStampImage(const QImage &img) { m_stampImage = img; }

private:
    // --- Geometry helpers ------------------------------------------------
    QRectF computePageRect() const
    {
        QRectF r(0, 0, displayWidthPts() * m_scale, displayHeightPts() * m_scale);
        r.moveCenter(QPointF(width() / 2.0,
                             qMax(r.height() / 2.0 + 10, height() / 2.0)));
        return r;
    }
    void updateGeometryToPage()
    {
        int w = int(displayWidthPts() * m_scale) + 40;
        int h = int(displayHeightPts() * m_scale) + 40;
        setMinimumSize(qMax(200, w), qMax(200, h));
        resize(minimumSize());
    }
    // Widget pixel -> PDF point (origin bottom-left).
    QPointF toPdf(const QPoint &px) const
    {
        double x = (px.x() - m_pageOrigin.x()) / m_scale;
        double yTop = (px.y() - m_pageOrigin.y()) / m_scale;
        double y = m_pageH - yTop;
        return QPointF(x, y);
    }
    // PDF point -> widget pixel.
    QPointF toPx(const QPointF &pt) const
    {
        return QPointF(m_pageOrigin.x() + pt.x() * m_scale,
                       m_pageOrigin.y() + (m_pageH - pt.y()) * m_scale);
    }
    QRectF pdfRectToPx(const QRectF &r) const
    {
        QRectF rc(toPx(QPointF(r.left(), r.top())),
                  toPx(QPointF(r.right(), r.bottom())));
        return rc.normalized();
    }
    QRectF boxToPx(const QRectF &box) const
    {
        return QRectF(m_pageOrigin.x() + box.x() * m_scale,
                      m_pageOrigin.y() + box.y() * m_scale,
                      box.width() * m_scale,
                      box.height() * m_scale);
    }
    QPointF toPageTopLeft(const QPoint &px) const
    {
        return QPointF((px.x() - m_pageOrigin.x()) / m_scale,
                       (px.y() - m_pageOrigin.y()) / m_scale);
    }

    int charIndexAt(const QPoint &px) const
    {
        if (m_chars.isEmpty()) return 0;
        const QPointF p = toPageTopLeft(px);
        int best = -1; double bestScore = 0;
        for (int i = 0; i < m_chars.size(); ++i) {
            const QRectF b = m_chars.at(i).box;
            if (b.isNull() || b.isEmpty()) continue;
            if (b.contains(p)) return i;
            const double dx = (p.x() < b.left())  ? b.left() - p.x()
                            : (p.x() > b.right()) ? p.x() - b.right() : 0.0;
            const double dy = (p.y() < b.top())    ? b.top() - p.y()
                            : (p.y() > b.bottom()) ? p.y() - b.bottom() : 0.0;
            const double score = dx + dy * 6.0;
            if (best < 0 || score < bestScore) { best = i; bestScore = score; }
        }
        return best < 0 ? 0 : best;
    }

    // Annotation hit testing (topmost wins).
    int annotationAt(const QPoint &px) const
    {
        const QVector<Annotation> &list = annotationsOfPage(m_pageIndex);
        for (int i = list.size() - 1; i >= 0; --i) {
            const Annotation &a = list[i];
            QRectF bb = pxBoundingRect(a);
            // For sticky notes make the hit area at least 24x24 so the little
            // marker is grabbable.
            if (a.tool == StickyNote && bb.width() < 24) bb.setWidth(24);
            if (a.tool == StickyNote && bb.height() < 24) bb.setHeight(24);
            if (bb.adjusted(-3, -3, 3, 3).contains(px))
                return i;
        }
        return -1;
    }

    // The axis-aligned pixel bounding rect of an annotation (where the user
    // can grab it to move it).
    QRectF pxBoundingRect(const Annotation &a) const
    {
        if (a.tool == Pen) {
            if (a.points.isEmpty()) return QRectF();
            QRectF bb;
            for (const QPointF &p : a.points) {
                QPointF q = toPx(p);
                if (bb.isNull()) bb = QRectF(q, QSizeF(1, 1));
                else bb = bb.united(QRectF(q, QSizeF(1, 1)));
            }
            return bb.adjusted(-4, -4, 4, 4);
        }
        return pdfRectToPx(a.rect);
    }

    int handleAt(int annotIndex, const QPoint &px) const
    {
        const QVector<Annotation> &list = annotationsOfPage(m_pageIndex);
        if (annotIndex < 0 || annotIndex >= list.size()) return -1;
        const QRectF bb = pxBoundingRect(list[annotIndex]);
        // Eight handles; index: 0 TL, 1 T, 2 TR, 3 R, 4 BR, 5 B, 6 BL, 7 L.
        const QPointF handles[8] = {
            bb.topLeft(), QPointF(bb.center().x(), bb.top()),   bb.topRight(),
            QPointF(bb.right(), bb.center().y()),
            bb.bottomRight(), QPointF(bb.center().x(), bb.bottom()),
            bb.bottomLeft(), QPointF(bb.left(), bb.center().y()),
        };
        const double r = 5.5;
        for (int i = 0; i < 8; ++i) {
            if ((QPointF(px) - handles[i]).manhattanLength() <= r * 2)
                return i;
        }
        return -1;
    }
    Qt::CursorShape cursorForHandle(int h) const
    {
        switch (h) {
        case 0: case 4: return Qt::SizeFDiagCursor;
        case 2: case 6: return Qt::SizeBDiagCursor;
        case 1: case 5: return Qt::SizeVerCursor;
        case 3: case 7: return Qt::SizeHorCursor;
        default: return Qt::ArrowCursor;
        }
    }

    void movePicked(const QPoint &pos)
    {
        auto &a = m_pages[m_pageIndex].items[m_pickedIndex];
        const QPointF dpx(pos - m_dragStart);
        const double dx = dpx.x() / m_scale;
        const double dy = -dpx.y() / m_scale;    // y flips: screen down == PDF down
        if (a.tool == Pen) {
            a.points.clear();
            for (const QPointF &p : m_origPoints)
                a.points.append(QPointF(p.x() + dx, p.y() + dy));
        } else {
            a.rect = m_origRect.translated(dx, dy);
        }
        update();
    }

    void resizePicked(const QPoint &pos)
    {
        auto &a = m_pages[m_pageIndex].items[m_pickedIndex];
        if (a.tool == Pen) return;              // pens keep their shape
        const QPointF dpx(pos - m_dragStart);
        const double dx = dpx.x() / m_scale;
        const double dy = -dpx.y() / m_scale;
        QRectF r = m_origRect;
        // Rect is in PDF coords (y grows UP). m_origRect may or may not be
        // normalised; use left/right/top/bottom as "logical" edges.
        double left = r.left(), right = r.right();
        double top = r.top(), bottom = r.bottom();
        // PDF-coord: top > bottom when the rect was created with TopLeft on
        // screen. We normalise at save time, so just treat the handle indices
        // as screen directions.
        switch (m_resizingHandle) {
        case 0: left += dx; top -= dy; break;        // TL (screen)
        case 1: top -= dy; break;                    // T
        case 2: right += dx; top -= dy; break;       // TR
        case 3: right += dx; break;                  // R
        case 4: right += dx; bottom -= dy; break;    // BR
        case 5: bottom -= dy; break;                 // B
        case 6: left += dx; bottom -= dy; break;     // BL
        case 7: left += dx; break;                   // L
        default: break;
        }
        a.rect = QRectF(QPointF(left, top), QPointF(right, bottom));
        update();
    }

    void drawAnnotation(QPainter &p, const Annotation &a, bool picked) const
    {
        p.save();
        switch (a.tool) {
        case Highlight: {
            QColor c = a.color; c.setAlpha(110);
            p.fillRect(pdfRectToPx(a.rect), c);
            break;
        }
        case Underline:
        case Strike: {
            QRectF r = pdfRectToPx(a.rect);
            double y = (a.tool == Underline) ? r.bottom() : r.center().y();
            p.setPen(QPen(a.color, 2));
            p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
            break;
        }
        case Pen: {
            p.setPen(QPen(a.color, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            QPainterPath path;
            for (int i = 0; i < a.points.size(); ++i) {
                QPointF q = toPx(a.points[i]);
                if (i == 0) path.moveTo(q); else path.lineTo(q);
            }
            p.drawPath(path);
            break;
        }
        case TextBox: {
            QRectF r = pdfRectToPx(a.rect);
            p.setPen(QPen(a.color.isValid() ? a.color : QColor(0, 0, 0), 1));
            p.setBrush(QColor(255, 255, 255, 180));
            p.drawRect(r);
            p.setBrush(Qt::NoBrush);
            p.setPen(QColor(0, 0, 0));
            // Qt knows how to render Chinese — the bake step will rasterise
            // this same text block to a QImage, so WYSIWYG holds.
            QFont f = p.font();
            f.setPointSizeF(qMax(8.0, 11.0 * m_scale));
            p.setFont(f);
            p.drawText(r.adjusted(4, 2, -4, -2),
                       Qt::AlignLeft | Qt::TextWordWrap, a.text);
            break;
        }
        case StickyNote: {
            QRectF r = pdfRectToPx(a.rect);
            if (r.width() < 18) r.setWidth(18);
            if (r.height() < 18) r.setHeight(18);
            // Yellow icon + small body if the note has text.
            QRectF icon(r.topLeft(), QSizeF(20, 20));
            p.setBrush(QColor(255, 220, 90));
            p.setPen(QPen(QColor(180, 150, 0)));
            p.drawRect(icon);
            p.setPen(QColor(90, 70, 0));
            p.drawText(icon, Qt::AlignCenter, QStringLiteral("✎"));
            if (!a.text.isEmpty()) {
                QRectF body(icon.right() + 4, icon.top(),
                            qMax(60.0, r.width() - icon.width() - 4),
                            qMax(20.0, r.height()));
                p.setBrush(QColor(255, 245, 190, 230));
                p.setPen(QPen(QColor(200, 170, 40), 1));
                p.drawRect(body);
                p.setPen(QColor(50, 40, 0));
                QFont f = p.font();
                f.setPointSizeF(qMax(8.0, 10.0 * m_scale));
                p.setFont(f);
                p.drawText(body.adjusted(4, 2, -4, -2),
                           Qt::AlignLeft | Qt::TextWordWrap, a.text);
            }
            break;
        }
        case Stamp: {
            if (!a.image.isNull())
                p.drawImage(pdfRectToPx(a.rect), a.image);
            break;
        }
        default: break;
        }
        p.restore();

        if (picked) {
            // Selection rectangle + handles.
            QRectF bb = pxBoundingRect(a);
            QPen dash(QColor(30, 100, 220), 1, Qt::DashLine);
            p.save();
            p.setPen(dash);
            p.setBrush(Qt::NoBrush);
            p.drawRect(bb);
            const QPointF handles[8] = {
                bb.topLeft(), QPointF(bb.center().x(), bb.top()),   bb.topRight(),
                QPointF(bb.right(), bb.center().y()),
                bb.bottomRight(), QPointF(bb.center().x(), bb.bottom()),
                bb.bottomLeft(), QPointF(bb.left(), bb.center().y()),
            };
            p.setPen(QPen(QColor(30, 100, 220)));
            p.setBrush(Qt::white);
            for (const QPointF &h : handles)
                p.drawRect(QRectF(h.x() - 3.5, h.y() - 3.5, 7, 7));
            p.restore();
        }
    }

    // --- Undo / redo snapshots ------------------------------------------
    struct Snapshot {
        QHash<int, PageState> pages;
        int currentPage = -1;
        int picked = -1;
    };
    Snapshot takeSnapshot() const
    {
        Snapshot s; s.pages = m_pages; s.currentPage = m_pageIndex; s.picked = m_pickedIndex;
        return s;
    }
    void restoreSnapshot(const Snapshot &s)
    {
        m_pages = s.pages;
        m_pickedIndex = s.picked;
        // (page index is the view's responsibility; undo doesn't flip pages)
    }
    void pushUndoSnapshot()
    {
        if (m_undo.size() >= 128)
            m_undo.remove(0, 1);
        m_undo.push(takeSnapshot());
        m_redo.clear();
    }

    // --- State ----------------------------------------------------------
    double m_pageW = 612, m_pageH = 792;
    int m_rotation = 0;
    double m_scale = 1.0;
    int m_pageIndex = -1;
    QImage m_background;
    QPointF m_pageOrigin;
    QRectF m_pageRect;
    QString m_placeholder;

    Tool m_tool = Select;
    QColor m_color = QColor(255, 220, 0);
    QImage m_stampImage;

    bool m_drawing = false;
    Annotation m_current;

    // Per-page uncommitted annotations + undo/redo history.
    QHash<int, PageState> m_pages;
    QStack<Snapshot> m_undo;
    QStack<Snapshot> m_redo;

    // Selection / drag state.
    int m_pickedIndex = -1;
    bool m_dragging = false;
    int m_resizingHandle = -1;
    QPoint m_dragStart;
    QRectF m_origRect;
    QVector<QPointF> m_origPoints;

    // Text layer selection.
    QVector<PdfTextChar> m_chars;
    int m_selStart = 0;
    int m_selEnd = 0;
    int m_selAnchor = 0;
    bool m_selecting = false;
};

// ===========================================================================
// Helpers: rasterise text into a transparent QImage so non-Latin characters
// (Chinese/Japanese/Korean) can be stamped back into the PDF via an SMask.
// ===========================================================================
namespace {

// Pick a font that actually renders CJK on this machine. Qt's default
// Helvetica substitute has no glyphs for Chinese characters, which is why
// baking a Chinese watermark used to produce the "乱码" squares.
QFont pickCJKFont(double pointSize, bool bold = false)
{
    static const QStringList candidates = {
        QStringLiteral("Microsoft YaHei UI"),
        QStringLiteral("Microsoft YaHei"),
        QStringLiteral("SimHei"),
        QStringLiteral("SimSun"),
        QStringLiteral("Noto Sans CJK SC"),
        QStringLiteral("Noto Sans SC"),
        QStringLiteral("PingFang SC"),
        QStringLiteral("Hiragino Sans GB"),
        QStringLiteral("Arial Unicode MS"),
    };
    QFont f;
    const QStringList families = QFontDatabase::families();
    for (const QString &name : candidates) {
        if (families.contains(name, Qt::CaseInsensitive)) {
            f.setFamily(name);
            break;
        }
    }
    f.setPointSizeF(pointSize);
    f.setBold(bold);
    f.setHintingPreference(QFont::PreferFullHinting);
    f.setStyleStrategy(QFont::PreferAntialias);
    return f;
}

// Render `text` into a transparent ARGB32 image at ~200 dpi scale factor so
// the resulting PDF looks crisp at 100%.
QImage renderTextToImage(const QString &text, QFont font, const QColor &color,
                         double maxWidthPts, int flags)
{
    const double k = 2.5;                  // oversample factor
    QFontMetricsF fm(font);
    QRectF br = fm.boundingRect(
        QRectF(0, 0, maxWidthPts > 0 ? maxWidthPts : 10000, 0),
        flags, text);
    if (br.width() < 2) br.setWidth(fm.horizontalAdvance(text) + 2);
    if (br.height() < 2) br.setHeight(fm.height());

    const int w = qMax(1, int(std::ceil(br.width()  * k)));
    const int h = qMax(1, int(std::ceil(br.height() * k)));
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QFont scaled = font;
    scaled.setPointSizeF(font.pointSizeF() * k);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setFont(scaled);
    p.setPen(color);
    p.drawText(img.rect(), flags, text);
    p.end();
    return img;
}

// Render an entire diagonal watermark (with rotation) into a transparent
// image sized to the page. The returned image is centred on the page.
QImage renderWatermarkToImage(const QString &text, double pageWpts, double pageHpts,
                              double fontSize, const QColor &color, double opacity)
{
    const double k = 2.0;
    const int W = qMax(1, int(pageWpts * k));
    const int H = qMax(1, int(pageHpts * k));
    QImage img(W, H, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    QColor c = color; c.setAlphaF(opacity);
    p.setPen(c);
    QFont f = pickCJKFont(fontSize * k, true);
    p.setFont(f);
    p.translate(W / 2.0, H / 2.0);
    // 45° diagonal across the page.
    p.rotate(-30);
    QFontMetricsF fm(f);
    QRectF br = fm.boundingRect(text);
    p.drawText(QPointF(-br.width() / 2.0, br.height() / 4.0), text);
    p.end();
    return img;
}

} // namespace

// ===========================================================================
// PdfTab
// ===========================================================================
PdfTab::PdfTab(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_tb = new QToolBar(this);
    QToolBar *tb = m_tb;
    tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    auto addBtn = [&](const char *src, auto slot) {
        QAction *a = tb->addAction(TR(src));
        a->setToolTip(TR(src));
        m_actionSources.insert(a, QByteArray(src));
        connect(a, &QAction::triggered, this, slot);
        return a;
    };
    auto addGlyphBtn = [&](const QString &glyph, const char *src, auto slot) {
        QAction *a = tb->addAction(glyph);
        a->setToolTip(TR(src));
        m_actionSources.insert(a, QByteArray(src));
        m_tooltipOnly.append(a);
        connect(a, &QAction::triggered, this, slot);
        return a;
    };

    // --- Save (first button on the toolbar, as requested) ----------------
    m_actSave = addBtn("Save", &PdfTab::saveFromToolbar);
    m_actSave->setShortcut(QKeySequence::Save);
    m_actSave->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    // --- Undo / Redo for the annotation overlay --------------------------
    m_actUndo = addBtn("Undo", &PdfTab::annotUndo);
    m_actUndo->setShortcut(QKeySequence::Undo);
    m_actUndo->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_actRedo = addBtn("Redo", &PdfTab::annotRedo);
    m_actRedo->setShortcut(QKeySequence::Redo);
    m_actRedo->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    tb->addSeparator();

    // --- Navigation ------------------------------------------------------
    m_actFirst = addGlyphBtn(QStringLiteral("⏮"), "First Page", &PdfTab::firstPage);
    m_actPrev  = addGlyphBtn(QStringLiteral("◀"), "Previous Page", &PdfTab::prevPage);
    m_actPrev->setShortcut(QKeySequence(Qt::Key_PageUp));

    m_pageSpin = new QSpinBox(tb);
    m_pageSpin->setRange(1, 1);
    m_pageSpin->setMaximumWidth(64);
    m_pageSpin->setAlignment(Qt::AlignRight);
    m_pageSpin->setToolTip(TR("Go to page"));
    connect(m_pageSpin, &QSpinBox::valueChanged, this, &PdfTab::onPageSpinChanged);
    tb->addWidget(m_pageSpin);

    m_pageTotal = new QLabel(QStringLiteral(" / 0 "), tb);
    tb->addWidget(m_pageTotal);

    m_actNext = addGlyphBtn(QStringLiteral("▶"), "Next Page", &PdfTab::nextPage);
    m_actNext->setShortcut(QKeySequence(Qt::Key_PageDown));
    m_actLast = addGlyphBtn(QStringLiteral("⏭"), "Last Page", &PdfTab::lastPage);
    tb->addSeparator();

    m_actZoomOut = addGlyphBtn(QStringLiteral("－"), "Zoom Out", &PdfTab::zoomOut);
    m_zoomLabel = new QLabel(QStringLiteral(" 100% "), tb);
    m_zoomLabel->setMinimumWidth(52);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    tb->addWidget(m_zoomLabel);
    m_actZoomIn   = addGlyphBtn(QStringLiteral("＋"), "Zoom In", &PdfTab::zoomIn);
    m_actZoomReset = addBtn("Actual Size", &PdfTab::zoomReset);
    m_actFitWidth  = addBtn("Fit Width", &PdfTab::fitWidth);
    m_actFitPage   = addBtn("Fit Page", &PdfTab::fitPage);
    tb->addSeparator();

    m_actCopyText = addBtn("Copy Text", &PdfTab::copySelectedText);
    m_actCopyText->setShortcut(QKeySequence::Copy);
    m_actCopyText->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_actSelectAllText = addBtn("Select All Text", &PdfTab::selectAllText);
    m_actSelectAllText->setShortcut(QKeySequence::SelectAll);
    m_actSelectAllText->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_actCopyPageText = addBtn("Copy Page Text", &PdfTab::copyPageText);
<<<<<<< HEAD
    tb->addSeparator();

    // The top toolbar still carries every feature button (Annotation tools,
    // Pages, Sign/Convert/etc.) to keep them one click away and avoid the
    // widget-tree complexity of a separate side panel — earlier attempts to
    // host the same buttons inside a right-hand QScrollArea / QGroupBox stack
    // produced intermittent 0xC0000005 access violations on some PDFs during
    // the initial paint while the thumbnails were still rendering.
    addBtn("Select", &PdfTab::setToolSelect);
    addBtn("Highlight", &PdfTab::setToolHighlight);
    addBtn("Underline", &PdfTab::setToolUnderline);
    addBtn("Strike", &PdfTab::setToolStrike);
    addBtn("Text Box", &PdfTab::setToolTextBox);
    addBtn("Sticky Note", &PdfTab::setToolStickyNote);
    addBtn("Pen", &PdfTab::setToolPen);
    addBtn("Stamp", &PdfTab::setToolStamp);
    addBtn("Color...", &PdfTab::pickAnnotColor);
    tb->addSeparator();

    addBtn("Merge", &PdfTab::mergePdf);
    addBtn("Split", &PdfTab::splitPdf);
    addBtn("Delete Page", &PdfTab::deleteCurrentPage);
    addBtn("Insert", &PdfTab::insertPages);
    addBtn("Extract", &PdfTab::extractPages);
    addGlyphBtn(QStringLiteral("↺"), "Rotate Left", &PdfTab::rotateCurrentLeft);
    addGlyphBtn(QStringLiteral("↻"), "Rotate Right", &PdfTab::rotateCurrentRight);
    addBtn("Move Up", &PdfTab::movePageUp);
    addBtn("Move Down", &PdfTab::movePageDown);
    tb->addSeparator();

    addBtn("Sign (draw)", &PdfTab::drawSignature);
    addBtn("Sign (image)", &PdfTab::imageSignature);
    addBtn("To Images", &PdfTab::exportToImages);
    addBtn("To Word", &PdfTab::exportToWord);
    addBtn("Compress", &PdfTab::compressPdf);
    addBtn("Text WM", &PdfTab::addTextWatermark);
    addBtn("Image WM", &PdfTab::addImageWatermark);
    addBtn("Password", &PdfTab::setOpenPassword);
    addBtn("Fill Form", &PdfTab::fillForms);
=======

    root->addWidget(tb);

    // --- Side toolbar with the rest of the feature buttons -----------------
    // The top toolbar keeps Save / Undo / Redo / navigation / zoom / text-copy.
    // The side panel (right) groups Annotations, Pages, Sign, Convert, etc.,
    // mirroring the image editor's side toolbar for a consistent look.
    auto *sideBar = new QWidget(this);
    sideBar->setFixedWidth(200);
    auto *sl = new QVBoxLayout(sideBar);
    sl->setContentsMargins(6, 6, 6, 6);
    sl->setSpacing(6);

    // A helper that creates one labelled button inside a given group box.
    auto addSideBtn = [&](QVBoxLayout *gv, const char *src, auto slot) {
        auto *btn = new QPushButton(TR(src), sideBar);
        btn->setToolTip(TR(src));
        // Stash the source so retranslate() can update the button's text when
        // the UI language changes live.
        QAction *act = new QAction(TR(src), btn);
        m_actionSources.insert(act, QByteArray(src));
        connect(btn, &QPushButton::clicked, this, slot);
        // Keep button text in sync with the (dummy) action, so retranslate()
        // updates both at once.
        connect(act, &QAction::changed, btn, [btn, act]() { btn->setText(act->text()); });
        gv->addWidget(btn);
        return btn;
    };
>>>>>>> f56ebedd4ac859088bdbb3d5d37f7a7ad8bdd6ea

    // Annotations group.
    auto *annotBox = new QGroupBox(TR("Annotate"), sideBar);
    m_annotGroup = annotBox;
    auto *av = new QVBoxLayout(annotBox);
    av->setSpacing(4);
    addSideBtn(av, "Select", &PdfTab::setToolSelect);
    addSideBtn(av, "Highlight", &PdfTab::setToolHighlight);
    addSideBtn(av, "Underline", &PdfTab::setToolUnderline);
    addSideBtn(av, "Strike", &PdfTab::setToolStrike);
    addSideBtn(av, "Text Box", &PdfTab::setToolTextBox);
    addSideBtn(av, "Sticky Note", &PdfTab::setToolStickyNote);
    addSideBtn(av, "Pen", &PdfTab::setToolPen);
    addSideBtn(av, "Stamp", &PdfTab::setToolStamp);
    addSideBtn(av, "Color...", &PdfTab::pickAnnotColor);
    sl->addWidget(annotBox);

    // Pages group.
    auto *pagesBox = new QGroupBox(TR("Pages"), sideBar);
    m_pagesGroup = pagesBox;
    auto *pv = new QVBoxLayout(pagesBox);
    pv->setSpacing(4);
    addSideBtn(pv, "Merge", &PdfTab::mergePdf);
    addSideBtn(pv, "Split", &PdfTab::splitPdf);
    addSideBtn(pv, "Delete Page", &PdfTab::deleteCurrentPage);
    addSideBtn(pv, "Insert", &PdfTab::insertPages);
    addSideBtn(pv, "Extract", &PdfTab::extractPages);
    addSideBtn(pv, "Rotate Left", &PdfTab::rotateCurrentLeft);
    addSideBtn(pv, "Rotate Right", &PdfTab::rotateCurrentRight);
    addSideBtn(pv, "Move Up", &PdfTab::movePageUp);
    addSideBtn(pv, "Move Down", &PdfTab::movePageDown);
    sl->addWidget(pagesBox);

    // Sign / convert / etc. group.
    auto *moreBox = new QGroupBox(TR("More"), sideBar);
    m_moreGroup = moreBox;
    auto *mv = new QVBoxLayout(moreBox);
    mv->setSpacing(4);
    addSideBtn(mv, "Sign (draw)", &PdfTab::drawSignature);
    addSideBtn(mv, "Sign (image)", &PdfTab::imageSignature);
    addSideBtn(mv, "To Images", &PdfTab::exportToImages);
    addSideBtn(mv, "To Word", &PdfTab::exportToWord);
    addSideBtn(mv, "Compress", &PdfTab::compressPdf);
    addSideBtn(mv, "Text WM", &PdfTab::addTextWatermark);
    addSideBtn(mv, "Image WM", &PdfTab::addImageWatermark);
    addSideBtn(mv, "Password", &PdfTab::setOpenPassword);
    addSideBtn(mv, "Fill Form", &PdfTab::fillForms);
    sl->addWidget(moreBox);

    sl->addStretch(1);

    // Wrap the side panel in its own scroll area so it works on short windows
    // and small screens.
    m_sideScroll = new QScrollArea(this);
    m_sideScroll->setWidgetResizable(true);
    m_sideScroll->setWidget(sideBar);
    m_sideScroll->setFixedWidth(220);
    m_sideScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *mid = new QHBoxLayout();
    mid->setContentsMargins(0, 0, 0, 0);

    m_thumbs = new QListWidget(this);
    m_thumbs->setFixedWidth(150);
    m_thumbs->setIconSize(QSize(120, 160));
    connect(m_thumbs, &QListWidget::currentRowChanged, this, &PdfTab::onPageSelected);
    mid->addWidget(m_thumbs);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(false);
    m_scroll->setAlignment(Qt::AlignCenter);
    m_view = new PdfPageView(m_scroll);
    connect(m_view, &PdfPageView::annotationsChanged, this, &PdfTab::onOverlayChanged);
    connect(m_view, &PdfPageView::selectionChanged, this, &PdfTab::onSelectionChanged);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_view, &QWidget::customContextMenuRequested,
            this, &PdfTab::onViewContextMenu);
    m_scroll->setWidget(m_view);
    mid->addWidget(m_scroll, 1);

    // The side-toolbar lives on the right of the page viewport, like the
    // image editor's side panel.
    mid->addWidget(m_sideScroll);

    auto *midWrap = new QWidget(this);
    midWrap->setLayout(mid);
    root->addWidget(midWrap, 1);

    m_info = new QLabel(this);
    m_info->setContentsMargins(6, 2, 6, 2);
    root->addWidget(m_info);

    connect(I18n::instance(), &I18n::languageChanged,
            this, [this](const QString &) { retranslate(); });

    updateNavState();
    onSelectionChanged();
    onOverlayChanged();
}

PdfTab::~PdfTab()
{
    if (!m_renderTempPath.isEmpty())
        QFile::remove(m_renderTempPath);
}

// ---------------------------------------------------------------------------
// Text selection & copy
// ---------------------------------------------------------------------------
QString PdfTab::pageTextOf(int index) const
{
    if (index < 0) return QString();
    if (m_usePdfium) {
        const QString t = m_pdfium.pageText(index);
        if (!t.isEmpty()) return t;
    }
    return m_doc.pageText(index);
}

void PdfTab::loadTextLayerForCurrentPage()
{
    if (!m_view) return;
    m_fallbackPageText.clear();
    if (m_usePdfium) {
        const QVector<PdfTextChar> chars = m_pdfium.pageChars(m_currentPage);
        m_view->setTextChars(chars);
        if (chars.isEmpty())
            m_fallbackPageText = pageTextOf(m_currentPage);
    } else {
        m_view->setTextChars(QVector<PdfTextChar>());
        m_fallbackPageText = pageTextOf(m_currentPage);
    }
    onSelectionChanged();
}

void PdfTab::copySelectedText()
{
    if (!m_view) return;
    const int n = m_view->copySelectionToClipboard();
    if (n > 0) {
        emit statusMessage(m_view->hasSelection()
                               ? TR("Copied %1 characters of selected text.").arg(n)
                               : TR("Copied the whole page text (%1 characters).").arg(n));
        return;
    }
    if (!m_fallbackPageText.isEmpty()) {
        QGuiApplication::clipboard()->setText(m_fallbackPageText);
        emit statusMessage(TR("Copied the whole page text (%1 characters).")
                               .arg(m_fallbackPageText.size()));
        return;
    }
    emit statusMessage(TR("This page has no text layer — its text is part of "
                          "an image, so there is nothing to copy."));
}

void PdfTab::selectAllText()
{
    if (!m_view) return;
    if (!m_view->hasTextLayer()) {
        emit statusMessage(TR("This page has no text layer."));
        return;
    }
    m_view->selectAllText();
    emit statusMessage(TR("Selected all %1 characters on this page.")
                           .arg(m_view->textCharCount()));
}

void PdfTab::copyPageText()
{
    QString txt = m_view ? m_view->allText() : QString();
    if (txt.isEmpty())
        txt = m_fallbackPageText.isEmpty() ? pageTextOf(m_currentPage)
                                           : m_fallbackPageText;
    if (txt.isEmpty()) {
        emit statusMessage(TR("This page has no text layer."));
        return;
    }
    QGuiApplication::clipboard()->setText(txt);
    emit statusMessage(TR("Copied the whole page text (%1 characters).")
                           .arg(txt.size()));
}

void PdfTab::copyAllText()
{
    QStringList parts;
    const int n = pageCount();
    for (int i = 0; i < n; ++i) {
        const QString t = pageTextOf(i);
        if (!t.isEmpty()) parts << t;
    }
    const QString all = parts.join(QStringLiteral("\n\n"));
    if (all.isEmpty()) {
        emit statusMessage(TR("No extractable text was found in this document."));
        return;
    }
    QGuiApplication::clipboard()->setText(all);
    emit statusMessage(TR("Copied the text of %1 page(s) (%2 characters).")
                           .arg(parts.size()).arg(all.size()));
}

void PdfTab::onSelectionChanged()
{
    const bool hasLayer = m_view && m_view->hasTextLayer();
    const bool hasAnyText = hasLayer || !m_fallbackPageText.isEmpty();
    if (m_actCopyText) m_actCopyText->setEnabled(hasAnyText);
    if (m_actSelectAllText) m_actSelectAllText->setEnabled(hasLayer);
    if (m_actCopyPageText) m_actCopyPageText->setEnabled(hasAnyText);
}

void PdfTab::onOverlayChanged()
{
    const bool anyPending = m_view && m_view->hasAnyAnnotations();
    if (anyPending && !m_modified) markModified(true);
    if (m_actUndo) m_actUndo->setEnabled(m_view && m_view->canUndo());
    if (m_actRedo) m_actRedo->setEnabled(m_view && m_view->canRedo());
    if (m_view) m_view->update();
}

void PdfTab::onViewContextMenu(const QPoint &pos)
{
    if (!m_view) return;
    QMenu menu(this);
    const bool hasSel = m_view->hasSelection();
    const bool hasLayer = m_view->hasTextLayer();
    const bool hasAnyText = hasLayer || !m_fallbackPageText.isEmpty();

    QAction *copySel = menu.addAction(TR("Copy"), this, &PdfTab::copySelectedText);
    copySel->setShortcut(QKeySequence::Copy);
    copySel->setEnabled(hasSel || hasAnyText);
    QAction *selAll = menu.addAction(TR("Select All Text"), this, &PdfTab::selectAllText);
    selAll->setShortcut(QKeySequence::SelectAll);
    selAll->setEnabled(hasLayer);
    menu.addSeparator();
    menu.addAction(TR("Copy Page Text"), this, &PdfTab::copyPageText)->setEnabled(hasAnyText);
    menu.addAction(TR("Copy All Text"), this, &PdfTab::copyAllText);
    menu.exec(m_view->mapToGlobal(pos));
}

void PdfTab::retranslate()
{
    for (auto it = m_actionSources.cbegin(); it != m_actionSources.cend(); ++it) {
        QAction *a = it.key();
        if (!a) continue;
        const QString txt = I18n::instance()->tr(QString::fromUtf8(it.value()));
        a->setToolTip(txt);
        if (!m_tooltipOnly.contains(a)) a->setText(txt);
    }
    if (m_pageSpin) m_pageSpin->setToolTip(TR("Go to page"));
    updateNavState();
    updatePageInfoLabel();
    if (m_thumbs && m_thumbs->count() == pageCount()) {
        for (int i = 0; i < m_thumbs->count(); ++i)
            m_thumbs->item(i)->setText(TR("Page %1").arg(i + 1));
    }
}

QString PdfTab::fileName() const
{
    return m_filePath.isEmpty() ? TR("untitled.pdf")
                                : QFileInfo(m_filePath).fileName();
}

bool PdfTab::loadFile(const QString &path)
{
    invalidatePageCache();
    m_usePdfium = m_pdfium.open(path);
    m_openPassword.clear();
    if (!m_usePdfium && m_pdfium.needsPassword()) {
        for (int attempt = 0; attempt < 3 && !m_usePdfium; ++attempt) {
            bool ok = false;
            const QString pw = QInputDialog::getText(
                this, TR("Password Required"),
                attempt == 0
                    ? TR("This PDF is password-protected.\nEnter the password to open it:")
                    : TR("Wrong password. Please try again:"),
                QLineEdit::Password, QString(), &ok);
            if (!ok) break;
            if (pw.isEmpty()) continue;
            if (m_pdfium.open(path, pw)) {
                m_usePdfium = true;
                m_openPassword = pw;
            }
        }
        if (!m_usePdfium) {
            emit statusMessage(TR("This PDF is password-protected."));
            return false;
        }
    }

    const bool coreOk = m_doc.load(path);
    m_readOnlyEncrypted = (!m_openPassword.isEmpty() && !coreOk);
    if (!m_usePdfium && !coreOk) {
        emit statusMessage(TR("Failed to open PDF: %1").arg(m_doc.error()));
        return false;
    }

    m_filePath = path;
    m_currentPage = 0;
    rebuildThumbnails();
    showPage(0);
    markModified(false);
    emit statusMessage(TR("Opened %1 (%2 pages)")
                           .arg(QFileInfo(path).fileName()).arg(pageCount()));
    return true;
}

void PdfTab::invalidatePageCache()
{
    m_pageCache = QImage();
    m_pageCacheIndex = -1;
    m_pageCacheSize = QSize();
}

QSizeF PdfTab::pageSizePts(int index) const
{
    if (m_usePdfium) {
        const QSizeF s = m_pdfium.pageSizePts(index);
        if (s.isValid() && !s.isEmpty()) return s;
    }
    const PdfPageInfo pi = m_doc.pageInfo(index);
    return QSizeF(pi.width, pi.height);
}

QImage PdfTab::renderPage(int index, const QSize &targetPx) const
{
    if (index < 0) return QImage();
    if (m_pageCacheIndex == index && m_pageCacheSize == targetPx
        && !m_pageCache.isNull())
        return m_pageCache;
    QImage img;
    if (m_usePdfium && targetPx.isValid() && !targetPx.isEmpty())
        img = m_pdfium.renderPageToFit(index, targetPx);
    if (img.isNull()) {
        QByteArray fmt;
        const QByteArray bytes = m_doc.pageSingleImage(index, fmt);
        if (!bytes.isEmpty()) img.loadFromData(bytes, fmt.constData());
    }
    if (!img.isNull()) {
        m_pageCache = img; m_pageCacheIndex = index; m_pageCacheSize = targetPx;
    }
    return img;
}

int PdfTab::pageCount() const
{
    return m_usePdfium ? m_pdfium.pageCount() : m_doc.pageCount();
}

void PdfTab::rebuildThumbnails()
{
    m_thumbs->blockSignals(true);
    m_thumbs->clear();
    const int n = pageCount();
    for (int i = 0; i < n; ++i) {
        QImage thumb;
        if (m_usePdfium) thumb = m_pdfium.renderPageToFit(i, QSize(120, 160));
        if (thumb.isNull()) {
            QByteArray fmt;
            const QByteArray img = m_doc.pageSingleImage(i, fmt);
            if (!img.isEmpty()) {
                QImage pageImg;
                if (pageImg.loadFromData(img, fmt.constData()) && !pageImg.isNull())
                    thumb = pageImg.scaled(120, 160, Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation);
            }
        }
        if (thumb.isNull()) {
            const QSizeF pts = pageSizePts(i);
            QSizeF box = pts.isEmpty() ? QSizeF(120, 160)
                                       : pts.scaled(QSizeF(120, 160), Qt::KeepAspectRatio);
            thumb = QImage(qMax(1, int(box.width())), qMax(1, int(box.height())),
                           QImage::Format_RGB32);
            thumb.fill(Qt::white);
        }
        m_thumbs->addItem(new QListWidgetItem(QIcon(QPixmap::fromImage(thumb)),
                                              TR("Page %1").arg(i + 1)));
    }
    m_thumbs->blockSignals(false);
    if (n > 0) m_thumbs->setCurrentRow(qBound(0, m_currentPage, n - 1));
    updateNavState();
}

void PdfTab::showPage(int index)
{
    if (index < 0 || index >= pageCount()) return;
    const bool pageChanged = (index != m_currentPage);
    m_currentPage = index;
    if (pageChanged) invalidatePageCache();

    const QSizeF pts = pageSizePts(index);
    const PdfPageInfo pi = m_doc.pageInfo(index);
    const int rotation = m_usePdfium ? 0 : pi.rotation;

    const int pxW = qMax(1, int(pts.width()  * m_zoom + 0.5));
    const int pxH = qMax(1, int(pts.height() * m_zoom + 0.5));
    const QImage bg = renderPage(index, QSize(pxW, pxH));

    m_view->setPage(index, pts.width(), pts.height(), rotation, bg);
    m_view->setScale(m_zoom);

    loadTextLayerForCurrentPage();

    if (m_thumbs && m_thumbs->currentRow() != index) {
        QSignalBlocker b(m_thumbs);
        m_thumbs->setCurrentRow(index);
    }
    updateNavState();
    updatePageInfoLabel();
    onOverlayChanged();
}

void PdfTab::updatePageInfoLabel()
{
    if (!m_info) return;
    const int n = pageCount();
    if (n <= 0) { m_info->clear(); return; }
    const QSizeF pts = pageSizePts(m_currentPage);
    const PdfPageInfo pi = m_doc.pageInfo(m_currentPage);
    const QString orient = (pts.width() > pts.height()) ? TR("landscape") : TR("portrait");
    QString text = TR("Page %1 / %2   —   %3 × %4 pt (%5)   —   rotation %6°   —   zoom %7%")
            .arg(m_currentPage + 1).arg(n)
            .arg(int(pts.width())).arg(int(pts.height()))
            .arg(orient).arg(pi.rotation).arg(qRound(m_zoom * 100));
    m_info->setText(text);
}

void PdfTab::updateNavState()
{
    const int n = pageCount();
    const bool has = n > 0;
    if (m_actFirst) m_actFirst->setEnabled(has && m_currentPage > 0);
    if (m_actPrev)  m_actPrev->setEnabled(has && m_currentPage > 0);
    if (m_actNext)  m_actNext->setEnabled(has && m_currentPage < n - 1);
    if (m_actLast)  m_actLast->setEnabled(has && m_currentPage < n - 1);
    if (m_actZoomOut) m_actZoomOut->setEnabled(m_zoom > 0.1001);
    if (m_actZoomIn)  m_actZoomIn->setEnabled(m_zoom < 7.999);
    if (m_pageSpin) {
        QSignalBlocker b(m_pageSpin);
        m_pageSpin->setRange(1, qMax(1, n));
        m_pageSpin->setValue(qBound(1, m_currentPage + 1, qMax(1, n)));
        m_pageSpin->setEnabled(has);
    }
    if (m_pageTotal) m_pageTotal->setText(QStringLiteral(" / %1 ").arg(n));
    if (m_zoomLabel) m_zoomLabel->setText(QStringLiteral(" %1% ").arg(qRound(m_zoom * 100)));
}

void PdfTab::onPageSelected(int row)
{
    if (row < 0 || row == m_currentPage) return;
    showPage(row);
}

void PdfTab::onPageSpinChanged(int value)
{
    const int idx = value - 1;
    if (idx < 0 || idx >= pageCount() || idx == m_currentPage) return;
    showPage(idx);
}

void PdfTab::prevPage()
{ if (m_currentPage <= 0) return; showPage(m_currentPage - 1);
  if (m_scroll) m_scroll->verticalScrollBar()->setValue(0); }
void PdfTab::nextPage()
{ if (m_currentPage >= pageCount() - 1) return; showPage(m_currentPage + 1);
  if (m_scroll) m_scroll->verticalScrollBar()->setValue(0); }
void PdfTab::firstPage()
{ if (pageCount() <= 0 || m_currentPage == 0) return; showPage(0);
  if (m_scroll) m_scroll->verticalScrollBar()->setValue(0); }
void PdfTab::lastPage()
{ const int last = pageCount() - 1; if (last < 0 || m_currentPage == last) return;
  showPage(last); if (m_scroll) m_scroll->verticalScrollBar()->setValue(0); }

void PdfTab::markModified(bool on)
{
    if (m_modified == on) return;
    m_modified = on;
    emit modificationChanged(on);
}

void PdfTab::annotUndo() { if (m_view) m_view->undo(); onOverlayChanged(); }
void PdfTab::annotRedo() { if (m_view) m_view->redo(); onOverlayChanged(); }

// ---------------------------------------------------------------------------
// Bake every page's uncommitted annotations into the real PDF.
// ---------------------------------------------------------------------------
void PdfTab::bakeAllPendingAnnotations()
{
    if (!m_view || !m_view->hasAnyAnnotations()) return;

    const QList<int> pages = m_view->pagesWithAnnotations();
    for (int pageIdx : pages) {
        if (pageIdx < 0 || pageIdx >= m_doc.pageCount()) continue;
        const QVector<PdfPageView::Annotation> items = m_view->annotationsOfPage(pageIdx);

        // Vector marks go into the content stream.
        QByteArray frag;
        auto num = [](double v) { return QByteArray::number(v, 'f', 3); };
        for (const auto &a : items) {
            double r = a.color.redF(), g = a.color.greenF(), b = a.color.blueF();
            switch (a.tool) {
            case PdfPageView::Highlight: {
                QRectF rc = a.rect.normalized();
                frag += "q\n" + num(r) + " " + num(g) + " " + num(b) + " rg\n"
                     + num(rc.left()) + " " + num(rc.top()) + " "
                     + num(rc.width()) + " " + num(rc.height()) + " re f\nQ\n";
                break;
            }
            case PdfPageView::Underline:
            case PdfPageView::Strike: {
                QRectF rc = a.rect.normalized();
                double y = (a.tool == PdfPageView::Underline) ? rc.bottom()
                                                              : rc.center().y();
                frag += "q\n1.5 w\n" + num(r) + " " + num(g) + " " + num(b) + " RG\n"
                     + num(rc.left()) + " " + num(y) + " m "
                     + num(rc.right()) + " " + num(y) + " l S\nQ\n";
                break;
            }
            case PdfPageView::Pen: {
                if (a.points.size() < 2) break;
                frag += "q\n2 w\n" + num(r) + " " + num(g) + " " + num(b) + " RG\n1 J 1 j\n";
                for (int i = 0; i < a.points.size(); ++i)
                    frag += num(a.points[i].x()) + " " + num(a.points[i].y())
                            + (i == 0 ? " m\n" : " l\n");
                frag += "S\nQ\n";
                break;
            }
            default: break;
            }
        }
        if (!frag.isEmpty()) m_doc.appendPageContent(pageIdx, frag);

        // TextBox / StickyNote / Stamp: render as transparent images so CJK
        // renders correctly and viewers don't need an embedded font.
        for (const auto &a : items) {
            switch (a.tool) {
            case PdfPageView::TextBox: {
                QRectF rc = a.rect.normalized();
                if (rc.width() < 2 || rc.height() < 2) break;
                QByteArray boxFrag;
                boxFrag += "q\n0.75 w\n"
                        + num(a.color.redF()) + " " + num(a.color.greenF()) + " "
                        + num(a.color.blueF()) + " RG\n"
                        + num(rc.left()) + " " + num(rc.bottom()) + " "
                        + num(rc.width()) + " " + num(rc.height()) + " re S\nQ\n";
                m_doc.appendPageContent(pageIdx, boxFrag);
                QFont f = pickCJKFont(11.0);
                QImage txt = renderTextToImage(a.text, f, QColor(0, 0, 0),
                                               rc.width() - 6,
                                               Qt::AlignLeft | Qt::TextWordWrap);
                if (!txt.isNull()) {
                    double tw = rc.width() - 6;
                    double th = qMin(rc.height() - 4,
                                     tw * txt.height() / qMax(1, txt.width()));
                    double tx = rc.left() + 3;
                    double ty = rc.top() - th - 2;
                    m_doc.stampQImage(pageIdx, txt, tx, ty, tw, th, 1.0, 0.0);
                }
                break;
            }
            case PdfPageView::StickyNote: {
                QRectF rc = a.rect.normalized();
                const double iconW = 24;
                const double gapW = 4;
                const double bodyW = a.text.isEmpty() ? 0 : qMax(100.0, rc.width() - iconW - gapW);
                const double totalW = iconW + (bodyW > 0 ? gapW + bodyW : 0);
                QFont bf = pickCJKFont(10.0);
                QFontMetricsF bfm(bf);
                QRectF tbr = bodyW > 0
                    ? bfm.boundingRect(QRectF(0, 0, bodyW - 6, 10000),
                                       Qt::AlignLeft | Qt::TextWordWrap, a.text)
                    : QRectF();
                const double bodyH = qMax(24.0, tbr.height() + 6);
                const double totalH = qMax(24.0, bodyH);

                const double k = 2.5;
                QImage img(int(totalW * k), int(totalH * k),
                           QImage::Format_ARGB32_Premultiplied);
                img.fill(Qt::transparent);
                QPainter p(&img);
                p.setRenderHint(QPainter::Antialiasing, true);
                p.setRenderHint(QPainter::TextAntialiasing, true);
                QRectF iconR(0, 0, iconW * k, 24 * k);
                p.setBrush(QColor(255, 220, 90));
                p.setPen(QPen(QColor(180, 150, 0), 1 * k));
                p.drawRect(iconR);
                QFont ifont = pickCJKFont(14 * k);
                p.setFont(ifont);
                p.setPen(QColor(90, 70, 0));
                p.drawText(iconR, Qt::AlignCenter, QStringLiteral("✎"));
                if (bodyW > 0) {
                    QRectF bodyR(iconR.right() + gapW * k, 0, bodyW * k, bodyH * k);
                    p.setBrush(QColor(255, 245, 190, 235));
                    p.setPen(QPen(QColor(200, 170, 40), 1 * k));
                    p.drawRect(bodyR);
                    p.setPen(QColor(50, 40, 0));
                    QFont tf = pickCJKFont(10 * k);
                    p.setFont(tf);
                    p.drawText(bodyR.adjusted(3 * k, 2 * k, -3 * k, -2 * k),
                               Qt::AlignLeft | Qt::TextWordWrap, a.text);
                }
                p.end();

                double x = rc.left();
                double yAnchor = rc.top() - totalH;
                m_doc.stampQImage(pageIdx, img, x, yAnchor, totalW, totalH, 1.0, 0.0);
                break;
            }
            case PdfPageView::Stamp: {
                if (a.image.isNull()) break;
                QRectF rc = a.rect.normalized();
                if (rc.width() < 2 || rc.height() < 2) break;
                m_doc.stampQImage(pageIdx, a.image, rc.left(), rc.bottom(),
                                  rc.width(), rc.height(), 1.0, 0.0);
                break;
            }
            default: break;
            }
        }
    }

    m_view->clearAllAnnotations();
    onOverlayChanged();
    markModified(true);
}

void PdfTab::refreshAfterEdit()
{
    invalidatePageCache();

    if (PdfiumRenderer::isAvailable()) {
        const QByteArray bytes = m_doc.serialize();
        if (!bytes.isEmpty()) {
            if (m_renderTempPath.isEmpty()) {
                m_renderTempPath = QDir::temp().filePath(
                    QStringLiteral("terminalpp_pdf_%1.pdf")
                        .arg(quintptr(this), 0, 16));
            }
            QFile f(m_renderTempPath);
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                f.write(bytes);
                f.close();
                if (m_pdfium.open(m_renderTempPath))
                    m_usePdfium = true;
            }
        }
    }

    if (m_currentPage >= pageCount())
        m_currentPage = qMax(0, pageCount() - 1);
    rebuildThumbnails();
    showPage(m_currentPage);
}

QByteArray PdfTab::currentPdfBytes() const { return m_doc.serialize(); }

void PdfTab::reloadFromBytes(const QByteArray &data)
{
    PdfDocument nd;
    if (nd.loadData(data)) {
        m_doc = nd;
        if (m_currentPage >= m_doc.pageCount())
            m_currentPage = qMax(0, m_doc.pageCount() - 1);
        refreshAfterEdit();
<<<<<<< HEAD
    }
}

void PdfTab::saveFromToolbar()
{
    if (m_filePath.isEmpty()) {
        const QString path = QFileDialog::getSaveFileName(
            this, TR("Save As"), QString(), TR("PDF Files (*.pdf)"));
        if (path.isEmpty()) return;
        saveAs(path);
        return;
=======
>>>>>>> f56ebedd4ac859088bdbb3d5d37f7a7ad8bdd6ea
    }
    save();
}

void PdfTab::saveFromToolbar()
{
    if (m_filePath.isEmpty()) {
        const QString path = QFileDialog::getSaveFileName(
            this, TR("Save As"), QString(), TR("PDF Files (*.pdf)"));
        if (path.isEmpty()) return;
        saveAs(path);
        return;
    }
    save();
}

bool PdfTab::save()
{
    if (m_filePath.isEmpty()) return false;
    bakeAllPendingAnnotations();
    QByteArray data = m_doc.serialize();
    QFile f(m_filePath);
    if (!f.open(QIODevice::WriteOnly)) {
        emit statusMessage(TR("Cannot write %1").arg(m_filePath));
        return false;
    }
    f.write(data); f.close();
    markModified(false);
    emit statusMessage(TR("Saved %1").arg(m_filePath));
    refreshAfterEdit();
    return true;
}

bool PdfTab::saveAs(const QString &path)
{
    bakeAllPendingAnnotations();
    QByteArray data = m_doc.serialize();
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(data); f.close();
    m_filePath = path;
    markModified(false);
    emit statusMessage(TR("Saved %1").arg(path));
    refreshAfterEdit();
    return true;
}

void PdfTab::applyTheme(const EditorTheme &) {}

QVector<QImage> PdfTab::printablePages()
{
    QVector<QImage> out;
    const int count = pageCount();
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        if (m_usePdfium) {
            const QImage img = m_pdfium.renderPage(i, 150.0);
            if (!img.isNull()) { out.append(img); continue; }
        }
        QByteArray fmt;
        const QByteArray data = m_doc.pageSingleImage(i, fmt);
        if (!data.isEmpty()) {
            QImage img;
            if (img.loadFromData(data, fmt.constData()) && !img.isNull()) {
                out.append(img); continue;
            }
        }
        const PdfPageInfo pi = m_doc.pageInfo(i);
        const int w = qMax(1, int(pi.width  * 150.0 / 72.0));
        const int h = qMax(1, int(pi.height * 150.0 / 72.0));
        QImage page(w, h, QImage::Format_RGB32);
        page.fill(Qt::white);
        const QString text = m_doc.pageText(i);
        if (!text.isEmpty()) {
            QPainter p(&page);
            p.setPen(Qt::black);
            QFont f = pickCJKFont(11.0);
            p.setFont(f);
            const int margin = 75;
            p.drawText(QRect(margin, margin, w - 2 * margin, h - 2 * margin),
                       Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, text);
        }
        out.append(page);
    }
    return out;
}

// --- Zoom -----------------------------------------------------------------
void PdfTab::setZoom(double z)
{
    const double old = m_zoom;
    m_zoom = qBound(0.10, z, 8.0);
    if (m_view) m_view->setScale(m_zoom);
    if (!qFuzzyCompare(old, m_zoom) && pageCount() > 0) {
        invalidatePageCache();
        const QSizeF pts = pageSizePts(m_currentPage);
        const int pxW = qMax(1, int(pts.width()  * m_zoom + 0.5));
        const int pxH = qMax(1, int(pts.height() * m_zoom + 0.5));
        const QImage bg = renderPage(m_currentPage, QSize(pxW, pxH));
        if (!bg.isNull() && m_view) {
            const PdfPageInfo pi = m_doc.pageInfo(m_currentPage);
            m_view->setPage(m_currentPage, pts.width(), pts.height(),
                            m_usePdfium ? 0 : pi.rotation, bg);
            m_view->setScale(m_zoom);
            loadTextLayerForCurrentPage();
        }
    }
    updateNavState();
    updatePageInfoLabel();
}
void PdfTab::zoomIn()    { setZoom(m_zoom * 1.25); }
void PdfTab::zoomOut()   { setZoom(m_zoom / 1.25); }
void PdfTab::zoomReset() { setZoom(1.0); }

void PdfTab::fitWidth()
{
    if (!m_view || !m_scroll || pageCount() <= 0) return;
    const double pw = m_view->displayWidthPts();
    if (pw <= 0) return;
    const int sb = m_scroll->verticalScrollBar()
                       ? m_scroll->verticalScrollBar()->sizeHint().width() : 16;
    const double avail = qMax(80, m_scroll->viewport()->width() - sb - 48);
    setZoom(avail / pw);
}
void PdfTab::fitPage()
{
    if (!m_view || !m_scroll || pageCount() <= 0) return;
    const double pw = m_view->displayWidthPts();
    const double ph = m_view->displayHeightPts();
    if (pw <= 0 || ph <= 0) return;
    const double availW = qMax(80, m_scroll->viewport()->width() - 48);
    const double availH = qMax(80, m_scroll->viewport()->height() - 48);
    setZoom(qMin(availW / pw, availH / ph));
}

// --- Annotation tools -----------------------------------------------------
void PdfTab::setToolSelect()    { if (m_view) m_view->setTool(PdfPageView::Select); }
void PdfTab::setToolHighlight() { m_view->setTool(PdfPageView::Highlight);  m_view->setColor(m_annotColor); }
void PdfTab::setToolUnderline() { m_view->setTool(PdfPageView::Underline);  m_view->setColor(m_annotColor); }
void PdfTab::setToolStrike()    { m_view->setTool(PdfPageView::Strike);     m_view->setColor(m_annotColor); }
void PdfTab::setToolTextBox()   { m_view->setTool(PdfPageView::TextBox);    m_view->setColor(Qt::black); }
void PdfTab::setToolStickyNote(){ m_view->setTool(PdfPageView::StickyNote); m_view->setColor(m_annotColor); }
void PdfTab::setToolPen()       { m_view->setTool(PdfPageView::Pen);        m_view->setColor(m_annotColor); }

void PdfTab::setToolStamp()
{
    if (!m_view) return;
    // The Stamp tool was previously a no-op because it needed an image but no
    // file picker was ever shown. Ask for one now, then enter stamp mode.
    QString f = QFileDialog::getOpenFileName(
        this, TR("Choose stamp image"), QString(),
        TR("Images (*.png *.jpg *.jpeg *.bmp *.gif)"));
    if (f.isEmpty()) { setToolSelect(); return; }
    QImage img(f);
    if (img.isNull()) {
        QFile fd(f);
        if (fd.open(QIODevice::ReadOnly)) img.loadFromData(fd.readAll());
    }
    if (img.isNull()) {
        QMessageBox::warning(this, TR("Stamp"), TR("Could not load the image."));
        setToolSelect();
        return;
    }
    m_view->setStampImage(img);
    m_view->setTool(PdfPageView::Stamp);
    emit statusMessage(TR("Stamp tool: drag a rectangle on the page to place the image."));
}

void PdfTab::pickAnnotColor()
{
    QColor c = QColorDialog::getColor(m_annotColor, this, TR("Annotation Color"));
    if (c.isValid()) { m_annotColor = c; m_view->setColor(c); }
}

// --- Page management ------------------------------------------------------
void PdfTab::mergePdf()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, TR("Select PDF(s) to append"), QString(), TR("PDF Files (*.pdf)"));
    if (files.isEmpty()) return;
    for (const QString &f : files) {
        PdfDocument other;
        if (other.load(f)) m_doc.appendDocument(other);
    }
    refreshAfterEdit();
    markModified(true);
    emit statusMessage(TR("Merged %1 file(s).").arg(files.size()));
}

bool PdfTab::writeAndOpenDerived(const PdfDocument &part, const QString &suggestedPath)
{
    QFile f(suggestedPath);
    if (!f.open(QIODevice::WriteOnly)) {
        emit statusMessage(TR("Cannot write %1").arg(suggestedPath));
        return false;
    }
    f.write(part.serialize()); f.close();
    // Ask MainWindow to open it as a new tab.
    emit openPdfRequested(suggestedPath);
    return true;
}

void PdfTab::splitPdf()
{
    if (m_doc.pageCount() < 2) {
        QMessageBox::information(this, TR("Split"), TR("Nothing to split."));
        return;
    }
    bool ok = false;
    int at = QInputDialog::getInt(this, TR("Split PDF"),
                                  TR("Split AFTER page number:"), 1, 1,
                                  m_doc.pageCount() - 1, 1, &ok);
    if (!ok) return;

    // Auto-generate output filenames in the SAME folder as the source PDF,
    // prompting only once to allow renaming; "originalname_pages_1-N.pdf" and
    // "originalname_pages_(N+1)-M.pdf".
    QString base = QFileInfo(m_filePath.isEmpty() ? "document.pdf" : m_filePath)
                       .completeBaseName();
    QString dir  = m_filePath.isEmpty() ? QDir::homePath()
                                        : QFileInfo(m_filePath).absolutePath();
    const int total = m_doc.pageCount();
    QString out1Default = QDir(dir).filePath(
        QStringLiteral("%1_pages_1-%2.pdf").arg(base).arg(at));
    QString out2Default = QDir(dir).filePath(
        QStringLiteral("%1_pages_%2-%3.pdf").arg(base).arg(at + 1).arg(total));

    QString out1 = QFileDialog::getSaveFileName(this,
        TR("Save first part"), out1Default, TR("PDF Files (*.pdf)"));
    if (out1.isEmpty()) return;
    QString out2 = QFileDialog::getSaveFileName(this,
        TR("Save second part"), out2Default, TR("PDF Files (*.pdf)"));
    if (out2.isEmpty()) return;

    QList<int> first, second;
    for (int i = 0; i < at; ++i) first.append(i);
    for (int i = at; i < total; ++i) second.append(i);

    PdfDocument p1 = m_doc.extractPages(first);
    PdfDocument p2 = m_doc.extractPages(second);
    const bool ok1 = writeAndOpenDerived(p1, out1);
    const bool ok2 = writeAndOpenDerived(p2, out2);
    if (ok1 && ok2)
        emit statusMessage(TR("Split into 2 files and opened them in new tabs."));
}

void PdfTab::deleteCurrentPage()
{
    if (m_doc.pageCount() <= 1) {
        QMessageBox::information(this, TR("Delete Page"),
                                 TR("A PDF must have at least one page."));
        return;
    }
    m_doc.deletePage(m_currentPage);
    if (m_currentPage >= m_doc.pageCount())
        m_currentPage = m_doc.pageCount() - 1;
    refreshAfterEdit();
    markModified(true);
}

void PdfTab::insertPages()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, TR("Select PDF(s) to insert"), QString(), TR("PDF Files (*.pdf)"));
    if (files.isEmpty()) return;
    int at = m_currentPage;
    for (const QString &f : files) {
        PdfDocument other;
        if (other.load(f)) {
            m_doc.insertDocument(at, other);
            at += other.pageCount();
        }
    }
    refreshAfterEdit();
    markModified(true);
    emit statusMessage(TR("Inserted %1 file(s).").arg(files.size()));
}

void PdfTab::extractPages()
{
    bool ok = false;
    QString spec = QInputDialog::getText(
        this, TR("Extract Pages"),
        TR("Pages to extract (e.g. 1-3,5,8):"), QLineEdit::Normal,
        QString("1-%1").arg(m_doc.pageCount()), &ok);
    if (!ok || spec.trimmed().isEmpty()) return;

    QList<int> idx;
    for (const QString &part : spec.split(',', Qt::SkipEmptyParts)) {
        QString p = part.trimmed();
        if (p.contains('-')) {
            QStringList ab = p.split('-');
            if (ab.size() == 2) {
                int a = ab[0].toInt(), b = ab[1].toInt();
                for (int i = a; i <= b; ++i)
                    if (i >= 1 && i <= m_doc.pageCount()) idx.append(i - 1);
            }
        } else {
            int i = p.toInt();
            if (i >= 1 && i <= m_doc.pageCount()) idx.append(i - 1);
        }
    }
    if (idx.isEmpty()) return;

    // Auto-generate the output filename: original base + "_extract_<spec>.pdf"
    // in the same folder as the source. The user can still rename it in the
    // Save-As dialog before confirming.
    QString base = QFileInfo(m_filePath.isEmpty() ? "document.pdf" : m_filePath)
                       .completeBaseName();
    QString dir  = m_filePath.isEmpty() ? QDir::homePath()
                                        : QFileInfo(m_filePath).absolutePath();
    QString safeSpec = spec;
    safeSpec.replace(QRegularExpression("[^0-9A-Za-z_,\\-]"), "_");
    QString suggested = QDir(dir).filePath(
        QStringLiteral("%1_extract_%2.pdf").arg(base, safeSpec));

    QString out = QFileDialog::getSaveFileName(this, TR("Save extracted PDF"),
                                               suggested, TR("PDF Files (*.pdf)"));
    if (out.isEmpty()) return;
    PdfDocument part = m_doc.extractPages(idx);
    if (writeAndOpenDerived(part, out))
        emit statusMessage(TR("Extracted %1 page(s) to %2 and opened it in a new tab.")
                               .arg(idx.size()).arg(out));
}

void PdfTab::rotateCurrentLeft()  { m_doc.rotatePage(m_currentPage, -90); refreshAfterEdit(); markModified(true); }
void PdfTab::rotateCurrentRight() { m_doc.rotatePage(m_currentPage,  90); refreshAfterEdit(); markModified(true); }

void PdfTab::movePageUp()
{
    if (m_currentPage <= 0) return;
    m_doc.movePage(m_currentPage, m_currentPage - 1);
    m_currentPage--;
    refreshAfterEdit();
    markModified(true);
}
void PdfTab::movePageDown()
{
    if (m_currentPage >= m_doc.pageCount() - 1) return;
    m_doc.movePage(m_currentPage, m_currentPage + 1);
    m_currentPage++;
    refreshAfterEdit();
    markModified(true);
}

// --- Signatures ----------------------------------------------------------
// After the drawing/image picker, the signature is added as an OVERLAY
// annotation on the current page, not baked straight into the PDF. That is
// what lets the user drag/resize it before saving, which was request "签名
// 保存前可以修改和拖动位置和修改大小".
namespace {
class SignatureCanvas : public QWidget
{
public:
    explicit SignatureCanvas(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedSize(500, 200);
        m_img = QImage(size(), QImage::Format_ARGB32);
        m_img.fill(Qt::transparent);
    }
    QImage image() const { return m_img; }
    void clearCanvas() { m_img.fill(Qt::transparent); update(); }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), Qt::white);
        p.setPen(QColor(200, 200, 200));
        p.drawRect(rect().adjusted(0, 0, -1, -1));
        p.drawImage(0, 0, m_img);
    }
    void mousePressEvent(QMouseEvent *e) override { m_last = e->pos(); m_drawing = true; }
    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (!m_drawing) return;
        QPainter p(&m_img);
        p.setPen(QPen(Qt::black, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(m_last, e->pos());
        m_last = e->pos();
        update();
    }
    void mouseReleaseEvent(QMouseEvent *) override { m_drawing = false; }
private:
    QImage m_img;
    QPoint m_last;
    bool m_drawing = false;
};
} // namespace

void PdfTab::drawSignature()
{
    if (!m_view) return;
    QDialog dlg(this);
    dlg.setWindowTitle(TR("Draw Signature"));
    auto *v = new QVBoxLayout(&dlg);
    v->addWidget(new QLabel(TR("Draw your signature below, then click OK. "
                               "You can drag / resize it on the page before saving."), &dlg));
    auto *canvas = new SignatureCanvas(&dlg);
    v->addWidget(canvas);
    auto *clearBtn = new QPushButton(TR("Clear"), &dlg);
    connect(clearBtn, &QPushButton::clicked, canvas, &SignatureCanvas::clearCanvas);
    v->addWidget(clearBtn);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    v->addWidget(bb);
    if (dlg.exec() != QDialog::Accepted) return;

    QImage sig = canvas->image();
    if (sig.isNull()) return;

    // Drop the signature on the page as an overlay Stamp. Default size is
    // 150 x 60 pts, centred near the bottom-right — the user can drag and
    // resize before saving.
    PdfPageView::Annotation a;
    a.tool = PdfPageView::Stamp;
    a.image = sig;
    PdfPageInfo pi = m_doc.pageInfo(m_currentPage);
    const double w = 150, h = 60;
    const double x = qMax(20.0, pi.width - w - 40);
    const double y = 40;                           // 40pt margin from bottom
    a.rect = QRectF(x, y + h, w, -h);              // y-up, drawn at the bottom
    m_view->insertAnnotationOnCurrentPage(a);
    emit statusMessage(TR("Signature added. Drag or resize it, then Save."));
}

void PdfTab::imageSignature()
{
    if (!m_view) return;
    QString f = QFileDialog::getOpenFileName(
        this, TR("Choose signature image"), QString(),
        TR("Images (*.png *.jpg *.jpeg *.bmp)"));
    if (f.isEmpty()) return;
    QImage img(f);
    if (img.isNull()) {
        QFile fd(f);
        if (fd.open(QIODevice::ReadOnly)) img.loadFromData(fd.readAll());
    }
    if (img.isNull()) {
        QMessageBox::warning(this, TR("Signature"), TR("Could not load the image."));
        return;
    }
    PdfPageView::Annotation a;
    a.tool = PdfPageView::Stamp;
    a.image = img;
    PdfPageInfo pi = m_doc.pageInfo(m_currentPage);
    double w = 160;
    double h = w * (img.height() / double(qMax(1, img.width())));
    if (h < 20) h = 20;
    const double x = qMax(20.0, pi.width - w - 40);
    const double y = 40;
    a.rect = QRectF(x, y + h, w, -h);
    m_view->insertAnnotationOnCurrentPage(a);
    emit statusMessage(TR("Image signature added. Drag or resize it, then Save."));
}

// --- Conversion ----------------------------------------------------------
void PdfTab::exportToImages()
{
    QString dir = QFileDialog::getExistingDirectory(this, TR("Output folder"));
    if (dir.isEmpty()) return;
    QString fmt = QInputDialog::getItem(this, TR("Image Format"),
                                        TR("Export each page as:"),
                                        {"PNG", "JPG"}, 0, false);
    QString base = QFileInfo(m_filePath.isEmpty() ? "page.pdf" : m_filePath)
                       .completeBaseName();
    int saved = 0;
    const int n = pageCount();
    for (int i = 0; i < n; ++i) {
        QImage page;
        if (m_usePdfium) page = m_pdfium.renderPage(i, 150.0);
        if (page.isNull()) {
            QByteArray f;
            const QByteArray img = m_doc.pageSingleImage(i, f);
            if (!img.isEmpty()) page.loadFromData(img, f.constData());
        }
        if (page.isNull()) {
            const QSizeF pts = pageSizePts(i);
            page = QImage(qMax(1, int(pts.width()  * 150 / 72)),
                          qMax(1, int(pts.height() * 150 / 72)),
                          QImage::Format_RGB32);
            page.fill(Qt::white);
        }
        QString outPath = QString("%1/%2_%3.%4").arg(dir, base)
                              .arg(i + 1, 3, 10, QChar('0'))
                              .arg(fmt.toLower());
        if (page.save(outPath, fmt.toLatin1().constData(),
                      fmt == "JPG" ? 90 : -1))
            ++saved;
    }
    emit statusMessage(TR("Exported %1 page image(s) to %2").arg(saved).arg(dir));
}

void PdfTab::imagesToPdfReplace() { /* reserved */ }

void PdfTab::exportToWord()
{
    QString text;
    if (m_usePdfium) {
        QStringList parts;
        const int n = m_pdfium.pageCount();
        for (int i = 0; i < n; ++i) parts << m_pdfium.pageText(i);
        text = parts.join(QStringLiteral("\n\n"));
    }
    if (text.trimmed().isEmpty()) text = m_doc.allText();
    if (text.trimmed().isEmpty()) {
        QMessageBox::information(this, TR("To Word"),
            TR("No extractable text was found. This PDF may be a scanned image; "
               "use \"To Images\" instead."));
        return;
    }
    QString out = QFileDialog::getSaveFileName(
        this, TR("Export to Word (RTF)"),
        m_filePath.isEmpty() ? QString()
                             : QFileInfo(m_filePath).completeBaseName() + ".rtf",
        TR("Word / RTF document (*.rtf)"));
    if (out.isEmpty()) return;
    if (!out.endsWith(".rtf", Qt::CaseInsensitive)) out += ".rtf";

    QByteArray rtf = "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0 Calibri;}}\\fs22\n";
    for (QChar ch : text) {
        ushort u = ch.unicode();
        if (ch == '\n') rtf += "\\par\n";
        else if (ch == '\\' ) rtf += "\\\\";
        else if (ch == '{') rtf += "\\{";
        else if (ch == '}') rtf += "\\}";
        else if (u < 128) rtf += char(u);
        else rtf += "\\u" + QByteArray::number((short)u) + "?";
    }
    rtf += "}";

    QFile f(out);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, TR("To Word"), TR("Cannot write %1").arg(out));
        return;
    }
    f.write(rtf); f.close();
    emit statusMessage(TR("Exported text to %1 (layout not preserved).").arg(out));
}

// --- Compression ---------------------------------------------------------
void PdfTab::compressPdf()
{
    bakeAllPendingAnnotations();
    qint64 originalSize = m_filePath.isEmpty() ? 0 : QFileInfo(m_filePath).size();
    const QByteArray before = m_doc.serialize();
    if (originalSize <= 0) originalSize = before.size();

    QByteArray after = before;
    PdfDocument work;
    if (work.loadData(before)) {
        work.compressStreams();
        const QByteArray squeezed = work.serialize();
        if (!squeezed.isEmpty() && squeezed.size() < after.size())
            after = squeezed;
    }
    PdfDocument nd;
    if (nd.loadData(after) && nd.isValid()) {
        const QByteArray re = nd.serialize();
        if (!re.isEmpty() && re.size() < after.size()) after = re;
    }

    QString out = QFileDialog::getSaveFileName(
        this, TR("Save compressed PDF"),
        m_filePath.isEmpty() ? QString() : m_filePath, TR("PDF Files (*.pdf)"));
    if (out.isEmpty()) return;
    QFile f(out);
    if (!f.open(QIODevice::WriteOnly)) {
        emit statusMessage(TR("Cannot write %1").arg(out));
        return;
    }
    f.write(after); f.close();

    const qint64 writtenSize = QFileInfo(out).size();
    if (writtenSize < originalSize) {
        const double ratio = 100.0 * (originalSize - writtenSize) / originalSize;
        emit statusMessage(TR("Compressed: %1 KB → %2 KB (%3% smaller)")
                               .arg(originalSize / 1024).arg(writtenSize / 1024)
                               .arg(QString::number(ratio, 'f', 1)));
    } else {
        emit statusMessage(TR("Already fully compressed: %1 KB → %2 KB.")
                               .arg(originalSize / 1024).arg(writtenSize / 1024));
    }
}

// --- Watermark -----------------------------------------------------------
void PdfTab::addTextWatermark()
{
    bool ok = false;
    QString text = QInputDialog::getText(this, TR("Text Watermark"),
                                          TR("Watermark text:"), QLineEdit::Normal,
                                          TR("CONFIDENTIAL"), &ok);
    if (!ok || text.isEmpty()) return;

    // Render the watermark as a transparent image per page, so Chinese and
    // every other non-Latin script just works. This fixes the "乱码" bug.
    const int total = m_doc.pageCount();
    for (int i = 0; i < total; ++i) {
        const PdfPageInfo pi = m_doc.pageInfo(i);
        QImage wm = renderWatermarkToImage(text, pi.width, pi.height,
                                           56, QColor(150, 150, 150), 0.30);
        if (wm.isNull()) continue;
        m_doc.stampQImage(i, wm, 0, 0, pi.width, pi.height, 1.0, 0.0);
    }
    refreshAfterEdit();
    markModified(true);
    emit statusMessage(TR("Text watermark applied to all pages."));
}

void PdfTab::addImageWatermark()
{
    QString f = QFileDialog::getOpenFileName(
        this, TR("Choose watermark image"), QString(),
        TR("Images (*.png *.jpg *.jpeg *.bmp)"));
    if (f.isEmpty()) return;
    QFile file(f);
    if (!file.open(QIODevice::ReadOnly)) return;
    QByteArray data = file.readAll();
    QByteArray fmt = QFileInfo(f).suffix().toLower().toLatin1();
    m_doc.addImageWatermark(data, fmt, -1, 0.4, 0.25);
    refreshAfterEdit();
    markModified(true);
    emit statusMessage(TR("Image watermark applied to all pages."));
}

// --- Protection ----------------------------------------------------------
void PdfTab::setOpenPassword()
{
    bool ok = false;
    QString pw = QInputDialog::getText(this, TR("Set Open Password"),
                                       TR("Password required to open the PDF:"),
                                       QLineEdit::Password, QString(), &ok);
    if (!ok || pw.isEmpty()) return;
    QString out = QFileDialog::getSaveFileName(
        this, TR("Save encrypted PDF"),
        m_filePath.isEmpty() ? QString() : m_filePath, TR("PDF Files (*.pdf)"));
    if (out.isEmpty()) return;

    bakeAllPendingAnnotations();
    PdfDocument copy;
    copy.loadData(m_doc.serialize());
    copy.encrypt(pw);
    QFile f(out);
    if (f.open(QIODevice::WriteOnly)) { f.write(copy.serialize()); f.close(); }
    emit statusMessage(TR("Encrypted PDF written to %1").arg(out));
}

// --- Forms ---------------------------------------------------------------
void PdfTab::fillForms()
{
    QStringList fields = m_doc.formTextFields();
    if (fields.isEmpty()) {
        QMessageBox::information(this, TR("Fill Form"),
                                 TR("No fillable text fields were found in this PDF."));
        return;
    }
    QDialog dlg(this);
    dlg.setWindowTitle(TR("Fill Form"));
    auto *form = new QFormLayout(&dlg);
    QList<QLineEdit *> edits;
    for (const QString &name : fields) {
        auto *edit = new QLineEdit(&dlg);
        form->addRow(name, edit);
        edits.append(edit);
    }
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(bb);
    if (dlg.exec() != QDialog::Accepted) return;

    int n = 0;
    for (int i = 0; i < fields.size(); ++i) {
        if (!edits[i]->text().isEmpty()) {
            if (m_doc.fillFormField(fields[i], edits[i]->text())) ++n;
        }
    }
    refreshAfterEdit();
    markModified(true);
    emit statusMessage(TR("Filled %1 field(s).").arg(n));
}

#include "pdftab.moc"
