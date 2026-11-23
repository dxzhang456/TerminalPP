#include "pdf/pdftab.h"
#include "core/i18n.h"

#include <QBuffer>
#include <QColorDialog>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
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

static inline QString TR(const char *s)
{
    return I18n::instance()->tr(QString::fromUtf8(s));
}

// ===========================================================================
// PdfPageView — the page surface + annotation overlay.
//
// Coordinates: the widget draws the page scaled by `m_scale`. Annotations are
// stored in *PDF point* coordinates (origin bottom-left, y upward), so they
// can be baked directly into the PDF content stream. The view converts between
// widget pixels (origin top-left, y down) and PDF points.
// ===========================================================================
class PdfPageView : public QWidget
{
    Q_OBJECT
public:
    enum Tool { None, Highlight, Underline, Strike, TextBox, StickyNote, Pen, Stamp };

    struct Annotation {
        Tool tool = None;
        QColor color;
        QVector<QPointF> points;   // PDF points (for Pen / line tools)
        QRectF rect;               // PDF points (for box tools)
        QString text;              // for TextBox / StickyNote
        QImage image;              // for Stamp / signature
    };

    explicit PdfPageView(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMouseTracking(true);
        setMinimumSize(200, 200);
    }

    void setPage(double wPts, double hPts, int rotation, const QImage &bg)
    {
        m_pageW = wPts; m_pageH = hPts;
        // Normalise the /Rotate value to one of 0/90/180/270.
        int r = rotation % 360;
        if (r < 0) r += 360;
        m_rotation = (r / 90) * 90;
        m_background = bg;
        m_annotations.clear();
        updateGeometryToPage();
        update();
    }

    void setScale(double s)
    {
        m_scale = qBound(0.05, s, 10.0);
        updateGeometryToPage();
        update();
    }
    double scale() const { return m_scale; }

    // Visible (rotation-applied) page extent, in PDF points. A page with a
    // /Rotate of 90 or 270 is presented with width/height swapped, exactly as
    // SumatraPDF / Acrobat do.
    double displayWidthPts() const
    {
        return (m_rotation == 90 || m_rotation == 270) ? m_pageH : m_pageW;
    }
    double displayHeightPts() const
    {
        return (m_rotation == 90 || m_rotation == 270) ? m_pageW : m_pageH;
    }

    void setTool(Tool t) { m_tool = t; }
    void setColor(const QColor &c) { m_color = c; }

    const QVector<Annotation> &annotations() const { return m_annotations; }
    bool hasAnnotations() const { return !m_annotations.isEmpty(); }
    void clearAnnotations() { m_annotations.clear(); update(); }

    double pageWidthPts() const { return m_pageW; }
    double pageHeightPts() const { return m_pageH; }

signals:
    void annotationAdded();

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

        // Page background + a thin border and a soft drop shadow, so the page
        // reads as a sheet of paper on the grey canvas (SumatraPDF-like).
        p.fillRect(pageRect.translated(3, 3), QColor(50, 50, 50));
        p.fillRect(pageRect, Qt::white);

        if (!m_background.isNull()) {
            // --- request #2: NEVER stretch. Fit the rendered page image into
            // the page rect while preserving its own aspect ratio, then centre
            // it. drawImage(QRectF, QImage) would distort the page.
            QSizeF target = QSizeF(m_background.size())
                                .scaled(pageRect.size(), Qt::KeepAspectRatio);
            QRectF dst(QPointF(0, 0), target);
            dst.moveCenter(pageRect.center());

            if (m_rotation == 0) {
                p.drawImage(dst, m_background);
            } else {
                // Honour /Rotate: rotate about the centre of the destination
                // rect. Note dst was computed against the already-swapped
                // display extent, so we rotate an un-swapped rect in place.
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
            // No raster available for this page — draw a readable placeholder
            // instead of a silently blank sheet.
            p.setPen(QColor(170, 170, 170));
            QFont f = p.font();
            f.setPointSizeF(qMax(7.0, qMin(16.0, pageRect.height() / 28.0)));
            p.setFont(f);
            // Both of these are user-visible, so they go through TR() like
            // every other PDF string (request #4).
            QString note = TR("%1 × %2 pt").arg(int(m_pageW)).arg(int(m_pageH));
            if (m_rotation)
                note += TR("   (rotated %1°)").arg(m_rotation);
            if (!m_placeholder.isEmpty())
                note += "\n\n" + m_placeholder;
            p.drawText(pageRect.adjusted(12, 12, -12, -12),
                       Qt::AlignCenter | Qt::TextWordWrap, note);
        }

        p.setPen(QPen(QColor(120, 120, 120)));
        p.setBrush(Qt::NoBrush);
        p.drawRect(pageRect);

        // Draw committed annotations.
        for (const Annotation &a : m_annotations)
            drawAnnotation(p, a);

        // Draw the in-progress annotation.
        if (m_drawing)
            drawAnnotation(p, m_current);
    }

    void mousePressEvent(QMouseEvent *e) override
    {
        if (m_tool == None) return;
        m_drawing = true;
        m_current = Annotation();
        m_current.tool = m_tool;
        m_current.color = m_color;
        QPointF pt = toPdf(e->pos());
        if (m_tool == Pen) {
            m_current.points.append(pt);
        } else {
            m_current.rect = QRectF(pt, pt);
        }
    }

    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (!m_drawing) return;
        QPointF pt = toPdf(e->pos());
        if (m_tool == Pen) {
            m_current.points.append(pt);
        } else {
            m_current.rect.setBottomRight(pt);
        }
        update();
    }

    void mouseReleaseEvent(QMouseEvent *e) override
    {
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
            if (m_current.rect.width() < 5)   // click, not drag
                m_current.rect = QRectF(pt.x(), pt.y() - 20, 160, 24);
        }
        if (m_tool != Pen) {
            m_current.rect = m_current.rect.normalized();
            if (m_current.rect.width() < 2 && m_current.rect.height() < 2
                && m_tool != StickyNote && m_tool != TextBox) {
                update(); return;
            }
        } else if (m_current.points.size() < 2) {
            update(); return;
        }
        m_annotations.append(m_current);
        emit annotationAdded();
        update();
    }

public:
    // A short note shown on pages the engine could not rasterise (keeps the
    // failure visible and diagnosable instead of showing a blank sheet).
    void setPlaceholderNote(const QString &s) { m_placeholder = s; update(); }

private:
    // The page rect in widget pixels: the rotation-aware page extent scaled by
    // m_scale, centred horizontally and placed below a small top margin.
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

    // Widget pixel -> PDF point.
    QPointF toPdf(const QPoint &px) const
    {
        double x = (px.x() - m_pageOrigin.x()) / m_scale;
        double yTop = (px.y() - m_pageOrigin.y()) / m_scale;
        double y = m_pageH - yTop;   // flip to bottom-left origin
        return QPointF(x, y);
    }

    // PDF point -> widget pixel.
    QPointF toPx(const QPointF &pt) const
    {
        double x = m_pageOrigin.x() + pt.x() * m_scale;
        double y = m_pageOrigin.y() + (m_pageH - pt.y()) * m_scale;
        return QPointF(x, y);
    }

    void drawAnnotation(QPainter &p, const Annotation &a)
    {
        p.save();
        switch (a.tool) {
        case Highlight: {
            QColor c = a.color; c.setAlpha(110);
            QRectF r(toPx(QPointF(a.rect.left(), a.rect.top())),
                     toPx(QPointF(a.rect.right(), a.rect.bottom())));
            p.fillRect(r.normalized(), c);
            break;
        }
        case Underline:
        case Strike: {
            QRectF r(toPx(QPointF(a.rect.left(), a.rect.top())),
                     toPx(QPointF(a.rect.right(), a.rect.bottom())));
            r = r.normalized();
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
            QRectF r(toPx(QPointF(a.rect.left(), a.rect.top())),
                     toPx(QPointF(a.rect.right(), a.rect.bottom())));
            r = r.normalized();
            p.setPen(QPen(a.color, 1));
            p.drawRect(r);
            p.drawText(r.adjusted(3, 2, -3, -2),
                       Qt::AlignLeft | Qt::TextWordWrap, a.text);
            break;
        }
        case StickyNote: {
            QPointF pos = toPx(QPointF(a.rect.left(), a.rect.top()));
            QRectF note(pos.x(), pos.y(), 18, 18);
            p.setBrush(QColor(255, 230, 120));
            p.setPen(QPen(QColor(180, 150, 0)));
            p.drawRect(note);
            p.drawText(note, Qt::AlignCenter, "✎");
            break;
        }
        case Stamp: {
            if (!a.image.isNull()) {
                QRectF r(toPx(QPointF(a.rect.left(), a.rect.top())),
                         toPx(QPointF(a.rect.right(), a.rect.bottom())));
                p.drawImage(r.normalized(), a.image);
            }
            break;
        }
        default: break;
        }
        p.restore();
    }

    double m_pageW = 612, m_pageH = 792;
    int m_rotation = 0;
    double m_scale = 1.0;
    QImage m_background;
    QPointF m_pageOrigin;
    QRectF m_pageRect;
    QString m_placeholder;

    Tool m_tool = None;
    QColor m_color = QColor(255, 220, 0);

    bool m_drawing = false;
    Annotation m_current;
    QVector<Annotation> m_annotations;
};

// ===========================================================================
// PdfTab
// ===========================================================================
PdfTab::PdfTab(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // --- Toolbar ---
    m_tb = new QToolBar(this);
    QToolBar *tb = m_tb;
    tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    // addBtn() keeps the English source string so retranslate() can re-apply
    // the current UI language to the already-built toolbar (request #4: live
    // language switching for every PDF control).
    auto addBtn = [&](const char *src, auto slot) {
        QAction *a = tb->addAction(TR(src));
        a->setToolTip(TR(src));
        m_actionSources.insert(a, QByteArray(src));
        connect(a, &QAction::triggered, this, slot);
        return a;
    };
    // addGlyphBtn() is for compact symbol buttons: the visible label stays a
    // language-neutral glyph while the tooltip is fully translated.
    auto addGlyphBtn = [&](const QString &glyph, const char *src, auto slot) {
        QAction *a = tb->addAction(glyph);
        a->setToolTip(TR(src));
        m_actionSources.insert(a, QByteArray(src));
        m_tooltipOnly.append(a);
        connect(a, &QAction::triggered, this, slot);
        return a;
    };

    // --- Navigation group (request #3: Previous / Next page) ---------------
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

    // --- Zoom group (request #3: Zoom Out / Zoom In) ----------------------
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

    // Annotation group.
    addBtn("Highlight", &PdfTab::setToolHighlight);
    addBtn("Underline", &PdfTab::setToolUnderline);
    addBtn("Strike", &PdfTab::setToolStrike);
    addBtn("Text Box", &PdfTab::setToolTextBox);
    addBtn("Sticky Note", &PdfTab::setToolStickyNote);
    addBtn("Pen", &PdfTab::setToolPen);
    addBtn("Stamp", &PdfTab::setToolStamp);
    addBtn("Color...", &PdfTab::pickAnnotColor);
    tb->addSeparator();

    // Pages group.
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

    // Sign / convert / etc.
    addBtn("Sign (draw)", &PdfTab::drawSignature);
    addBtn("Sign (image)", &PdfTab::imageSignature);
    addBtn("To Images", &PdfTab::exportToImages);
    addBtn("To Word", &PdfTab::exportToWord);
    addBtn("Compress", &PdfTab::compressPdf);
    addBtn("Text WM", &PdfTab::addTextWatermark);
    addBtn("Image WM", &PdfTab::addImageWatermark);
    addBtn("Password", &PdfTab::setOpenPassword);
    addBtn("Fill Form", &PdfTab::fillForms);

    root->addWidget(tb);

    // --- Main area: thumbnails | page view ---
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
    connect(m_view, &PdfPageView::annotationAdded, this, [this]() { markModified(true); });
    m_scroll->setWidget(m_view);
    mid->addWidget(m_scroll, 1);

    auto *midWrap = new QWidget(this);
    midWrap->setLayout(mid);
    root->addWidget(midWrap, 1);

    m_info = new QLabel(this);
    m_info->setContentsMargins(6, 2, 6, 2);
    root->addWidget(m_info);

    // Follow the global UI language live — the toolbar is built once here, so
    // without this the PDF controls would stay in whatever language was active
    // when the tab was opened.
    connect(I18n::instance(), &I18n::languageChanged,
            this, [this](const QString &) { retranslate(); });

    updateNavState();
}

// ---------------------------------------------------------------------------
// retranslate — re-apply the active UI language to every PDF control.
// ---------------------------------------------------------------------------
void PdfTab::retranslate()
{
    for (auto it = m_actionSources.cbegin(); it != m_actionSources.cend(); ++it) {
        QAction *a = it.key();
        if (!a) continue;
        const QString txt = I18n::instance()->tr(QString::fromUtf8(it.value()));
        a->setToolTip(txt);
        if (!m_tooltipOnly.contains(a))
            a->setText(txt);
    }
    if (m_pageSpin)
        m_pageSpin->setToolTip(TR("Go to page"));
    updateNavState();
    updatePageInfoLabel();
    // Refresh the per-page thumbnail captions ("Page N") too.
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

    // --- 1) Try PDFium first: it is a real rasteriser, so when it is
    // available every page renders exactly as it does in a browser. This is
    // what fixes "most pages say the page cannot be rendered".
    m_usePdfium = m_pdfium.open(path);

    // --- 2) Always also parse with the built-in engine, because every
    // structural operation (merge/split/rotate/annotate/watermark/save) is
    // implemented against PdfDocument. If PDFium opened the file we tolerate
    // a built-in parse failure — viewing still works.
    const bool coreOk = m_doc.load(path);

    if (!m_usePdfium && !coreOk) {
        // Neither backend could read it: report the concrete reason from
        // whichever engine has the more specific message.
        QString why = m_doc.error();
        const QString diag = m_doc.diagnostics();
        if (!diag.isEmpty() && !why.contains(diag))
            why += QStringLiteral("  [") + diag + QStringLiteral("]");
        if (!m_pdfium.error().isEmpty())
            why += QStringLiteral("  |  PDFium: ") + m_pdfium.error();
        emit statusMessage(TR("Failed to open PDF: %1").arg(why));
        return false;
    }

    m_filePath = path;
    m_currentPage = 0;
    rebuildThumbnails();
    showPage(0);
    markModified(false);

    const int pages = m_usePdfium ? m_pdfium.pageCount() : m_doc.pageCount();
    QString msg = TR("Opened %1 (%2 pages)")
                      .arg(QFileInfo(path).fileName())
                      .arg(pages);
    // Say which engine is drawing the pages — it explains the visual quality
    // and makes a missing pdfium.dll immediately obvious.
    msg += m_usePdfium ? QStringLiteral("  —  ") + TR("rendered by PDFium")
                       : QStringLiteral("  —  ") + TR("built-in renderer "
                                                      "(install pdfium.dll for "
                                                      "full page rendering)");
    // Surface non-fatal parse notes (e.g. an undecodable object stream) so
    // partially-rendered documents are explainable.
    if (!m_usePdfium) {
        const QStringList warn = m_doc.warnings();
        if (!warn.isEmpty())
            msg += QStringLiteral("  —  ") + warn.join(QStringLiteral("; "));
    }
    emit statusMessage(msg);
    return true;
}

// ---------------------------------------------------------------------------
// Rendering helpers: PDFium when available, built-in engine otherwise.
// ---------------------------------------------------------------------------
void PdfTab::invalidatePageCache()
{
    m_pageCache = QImage();
    m_pageCacheIndex = -1;
    m_pageCacheSize = QSize();
}

// Page size in PDF points. PDFium already applies the page's /Rotate, so the
// value it returns is the *visible* size — a landscape page really reports
// width > height, which is exactly what the layout needs.
QSizeF PdfTab::pageSizePts(int index) const
{
    if (m_usePdfium) {
        const QSizeF s = m_pdfium.pageSizePts(index);
        if (s.isValid() && !s.isEmpty())
            return s;
    }
    const PdfPageInfo pi = m_doc.pageInfo(index);
    return QSizeF(pi.width, pi.height);
}

QImage PdfTab::renderPage(int index, const QSize &targetPx) const
{
    if (index < 0)
        return QImage();
    // Serve from cache when the same page was already rendered at this size.
    if (m_pageCacheIndex == index && m_pageCacheSize == targetPx
        && !m_pageCache.isNull())
        return m_pageCache;

    QImage img;
    if (m_usePdfium && targetPx.isValid() && !targetPx.isEmpty()) {
        // Render at the on-screen size so the result is crisp at any zoom.
        img = m_pdfium.renderPageToFit(index, targetPx);
    }
    if (img.isNull()) {
        // Fallback: the built-in engine can only hand back a page that embeds
        // a full-page image (typical scanned document).
        QByteArray fmt;
        const QByteArray bytes = m_doc.pageSingleImage(index, fmt);
        if (!bytes.isEmpty())
            img.loadFromData(bytes, fmt.constData());
    }
    if (!img.isNull()) {
        m_pageCache = img;
        m_pageCacheIndex = index;
        m_pageCacheSize = targetPx;
    }
    return img;
}

// The authoritative page count: PDFium when it opened the file (it can read
// documents the built-in parser cannot), otherwise the built-in engine.
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
        // Thumbnails follow each page's own orientation: a landscape page
        // produces a wide thumbnail, a portrait page a tall one, both fitted
        // into the 120x160 icon box without distortion.
        QImage thumb;
        if (m_usePdfium)
            thumb = m_pdfium.renderPageToFit(i, QSize(120, 160));
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
            // Blank sheet at the page's real aspect ratio, so the list still
            // shows portrait/landscape correctly.
            const QSizeF pts = pageSizePts(i);
            QSizeF box = pts.isEmpty() ? QSizeF(120, 160)
                                       : pts.scaled(QSizeF(120, 160),
                                                    Qt::KeepAspectRatio);
            thumb = QImage(qMax(1, int(box.width())), qMax(1, int(box.height())),
                           QImage::Format_RGB32);
            thumb.fill(Qt::white);
        }
        m_thumbs->addItem(new QListWidgetItem(QIcon(QPixmap::fromImage(thumb)),
                                              TR("Page %1").arg(i + 1)));
    }
    m_thumbs->blockSignals(false);
    if (n > 0)
        m_thumbs->setCurrentRow(qBound(0, m_currentPage, n - 1));
    updateNavState();
}

void PdfTab::showPage(int index)
{
    if (index < 0 || index >= pageCount()) return;
    const bool pageChanged = (index != m_currentPage);
    m_currentPage = index;
    if (pageChanged)
        invalidatePageCache();

    // --- Page geometry: always the page's OWN size, so the presentation is
    // automatically portrait for a portrait page and landscape for a
    // landscape page (new request #2). PDFium already folds /Rotate into the
    // size it reports, so in that case the view must not rotate again.
    const QSizeF pts = pageSizePts(index);
    const PdfPageInfo pi = m_doc.pageInfo(index);
    const int rotation = m_usePdfium ? 0 : pi.rotation;

    // --- Raster: render at the on-screen pixel size for a crisp result.
    const int pxW = qMax(1, int(pts.width()  * m_zoom + 0.5));
    const int pxH = qMax(1, int(pts.height() * m_zoom + 0.5));
    const QImage bg = renderPage(index, QSize(pxW, pxH));

    m_view->setPage(pts.width(), pts.height(), rotation, bg);
    // When neither backend could produce a raster, say so on the page instead
    // of showing a silently blank sheet, and point at the actual remedy.
    m_view->setPlaceholderNote(
        bg.isNull() ? (PdfiumRenderer::isAvailable()
                           ? TR("This page could not be rendered as an image. "
                                "Text extraction and all page / annotation tools "
                                "still work.")
                           : TR("Full page rendering needs pdfium.dll. Place it "
                                "next to TerminalPP.exe to render every page; "
                                "text extraction and all page / annotation "
                                "tools work without it."))
                    : QString());
    m_view->setScale(m_zoom);

    // Keep the thumbnail list selection in sync when the page was changed by
    // the toolbar (Prev/Next) rather than by clicking a thumbnail.
    if (m_thumbs && m_thumbs->currentRow() != index) {
        QSignalBlocker b(m_thumbs);
        m_thumbs->setCurrentRow(index);
    }
    updateNavState();
    updatePageInfoLabel();
}

void PdfTab::updatePageInfoLabel()
{
    if (!m_info) return;
    const int n = pageCount();
    if (n <= 0) { m_info->clear(); return; }
    const QSizeF pts = pageSizePts(m_currentPage);
    const PdfPageInfo pi = m_doc.pageInfo(m_currentPage);
    // Spell out the detected orientation so the automatic portrait/landscape
    // handling is visible to the user.
    const QString orient = (pts.width() > pts.height()) ? TR("landscape")
                                                        : TR("portrait");
    m_info->setText(TR("Page %1 / %2   —   %3 × %4 pt (%5)   —   rotation %6°   —   zoom %7%")
                        .arg(m_currentPage + 1).arg(n)
                        .arg(int(pts.width())).arg(int(pts.height()))
                        .arg(orient)
                        .arg(pi.rotation)
                        .arg(qRound(m_zoom * 100)));
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
    if (m_pageTotal)
        m_pageTotal->setText(QStringLiteral(" / %1 ").arg(n));
    if (m_zoomLabel)
        m_zoomLabel->setText(QStringLiteral(" %1% ").arg(qRound(m_zoom * 100)));
}

void PdfTab::onPageSelected(int row)
{
    if (row < 0 || row == m_currentPage) return;
    // Bake the previous page's annotations before switching away.
    bakeAnnotationsForCurrentPage();
    showPage(row);
}

void PdfTab::onPageSpinChanged(int value)
{
    const int idx = value - 1;
    if (idx < 0 || idx >= pageCount() || idx == m_currentPage) return;
    bakeAnnotationsForCurrentPage();
    showPage(idx);
}

// --- Page navigation (request #3) ------------------------------------------
void PdfTab::prevPage()
{
    if (m_currentPage <= 0) return;
    bakeAnnotationsForCurrentPage();
    showPage(m_currentPage - 1);
    if (m_scroll) m_scroll->verticalScrollBar()->setValue(0);
}

void PdfTab::nextPage()
{
    if (m_currentPage >= pageCount() - 1) return;
    bakeAnnotationsForCurrentPage();
    showPage(m_currentPage + 1);
    if (m_scroll) m_scroll->verticalScrollBar()->setValue(0);
}

void PdfTab::firstPage()
{
    if (pageCount() <= 0 || m_currentPage == 0) return;
    bakeAnnotationsForCurrentPage();
    showPage(0);
    if (m_scroll) m_scroll->verticalScrollBar()->setValue(0);
}

void PdfTab::lastPage()
{
    const int last = pageCount() - 1;
    if (last < 0 || m_currentPage == last) return;
    bakeAnnotationsForCurrentPage();
    showPage(last);
    if (m_scroll) m_scroll->verticalScrollBar()->setValue(0);
}

void PdfTab::markModified(bool on)
{
    if (m_modified == on) return;
    m_modified = on;
    emit modificationChanged(on);
}

// Convert the overlay's vector annotations into a PDF content fragment and bake
// them into the current page, then clear the overlay.
void PdfTab::bakeAnnotationsForCurrentPage()
{
    if (!m_view || !m_view->hasAnnotations()) return;
    const int idx = m_currentPage;
    if (idx < 0 || idx >= m_doc.pageCount()) { m_view->clearAnnotations(); return; }

    QByteArray frag;
    auto num = [](double v) { return QByteArray::number(v, 'f', 2); };

    for (const auto &a : m_view->annotations()) {
        double r = a.color.redF(), g = a.color.greenF(), b = a.color.blueF();
        switch (a.tool) {
        case PdfPageView::Highlight: {
            QRectF rc = a.rect.normalized();
            frag += "q\n0.4 w\n" + num(r) + " " + num(g) + " " + num(b) + " rg\n";
            frag += num(rc.left()) + " " + num(rc.top()) + " "
                    + num(rc.width()) + " " + num(rc.height()) + " re\nf\nQ\n";
            break;
        }
        case PdfPageView::Underline:
        case PdfPageView::Strike: {
            QRectF rc = a.rect.normalized();
            double y = (a.tool == PdfPageView::Underline) ? rc.top()
                                                          : rc.center().y();
            frag += "q\n1.5 w\n" + num(r) + " " + num(g) + " " + num(b) + " RG\n";
            frag += num(rc.left()) + " " + num(y) + " m "
                    + num(rc.right()) + " " + num(y) + " l S\nQ\n";
            break;
        }
        case PdfPageView::Pen: {
            if (a.points.size() < 2) break;
            frag += "q\n2 w\n" + num(r) + " " + num(g) + " " + num(b)
                    + " RG\n1 J 1 j\n";
            for (int i = 0; i < a.points.size(); ++i) {
                frag += num(a.points[i].x()) + " " + num(a.points[i].y())
                        + (i == 0 ? " m\n" : " l\n");
            }
            frag += "S\nQ\n";
            break;
        }
        case PdfPageView::TextBox:
        case PdfPageView::StickyNote: {
            QRectF rc = a.rect.normalized();
            // Draw a faint box for TextBox.
            if (a.tool == PdfPageView::TextBox) {
                frag += "q\n0.5 w\n" + num(r) + " " + num(g) + " " + num(b) + " RG\n";
                frag += num(rc.left()) + " " + num(rc.top() - rc.height()) + " "
                        + num(rc.width()) + " " + num(rc.height()) + " re S\nQ\n";
            }
            QByteArray esc;
            for (char c : a.text.toUtf8()) {
                if (c == '(' || c == ')' || c == '\\') { esc.append('\\'); esc.append(c); }
                else esc.append(c);
            }
            frag += "q\nBT\n/F1wm 11 Tf\n0 0 0 rg\n";
            frag += num(rc.left() + 2) + " " + num(rc.top() - 12) + " Td\n";
            frag += "(" + esc + ") Tj\nET\nQ\n";
            break;
        }
        case PdfPageView::Stamp:
            // Stamps/signatures baked separately via stampImage.
            break;
        default: break;
        }
    }

    if (!frag.isEmpty()) {
        // Supply a Helvetica font resource for text annotations.
        PdfObject font = PdfObject::makeDict();
        font.dict["Type"] = PdfObject::makeName("Font");
        font.dict["Subtype"] = PdfObject::makeName("Type1");
        font.dict["BaseFont"] = PdfObject::makeName("Helvetica");
        // We don't have addObject public; route through appendPageContent with
        // an inline font by adding it as an object first is not accessible here.
        // Instead, embed the font through a resource dict carrying a new indirect
        // object is unnecessary — use the page's existing fonts if present. For
        // robustness we add the font via a dedicated path: appendPageContent only
        // accepts name->ref dicts, so we register the font by serializing and
        // reparsing. Simpler: skip custom font ref and rely on a standard /F1wm
        // mapping injected here using a direct dictionary (allowed in Resources).
        QMap<QByteArray, PdfObject> extra;
        PdfObject fontDict = PdfObject::makeDict();
        fontDict.dict["F1wm"] = font;   // inline font dict (valid in Resources)
        extra["Font"] = fontDict;
        m_doc.appendPageContent(idx, frag, extra);
    }
    m_view->clearAnnotations();
    markModified(true);
}

QByteArray PdfTab::currentPdfBytes() const
{
    return m_doc.serialize();
}

void PdfTab::reloadFromBytes(const QByteArray &data)
{
    PdfDocument nd;
    if (nd.loadData(data)) {
        m_doc = nd;
        if (m_currentPage >= m_doc.pageCount())
            m_currentPage = qMax(0, m_doc.pageCount() - 1);
        rebuildThumbnails();
        showPage(m_currentPage);
    }
}

bool PdfTab::save()
{
    if (m_filePath.isEmpty())
        return false;
    bakeAnnotationsForCurrentPage();
    QByteArray data = m_doc.serialize();
    QFile f(m_filePath);
    if (!f.open(QIODevice::WriteOnly)) {
        emit statusMessage(TR("Cannot write %1").arg(m_filePath));
        return false;
    }
    f.write(data);
    f.close();
    markModified(false);
    emit statusMessage(TR("Saved %1").arg(m_filePath));
    return true;
}

bool PdfTab::saveAs(const QString &path)
{
    bakeAnnotationsForCurrentPage();
    QByteArray data = m_doc.serialize();
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(data);
    f.close();
    m_filePath = path;
    markModified(false);
    emit statusMessage(TR("Saved %1").arg(path));
    return true;
}

void PdfTab::applyTheme(const EditorTheme &) { /* page surface stays white */ }

// Rasterise every page for printing. Scanned pages print their embedded image;
// text pages are synthesised at ~150 dpi with their extracted text drawn in, so
// File > Print never emits blank sheets for a text-based PDF.
QVector<QImage> PdfTab::printablePages()
{
    QVector<QImage> out;
    const int count = pageCount();
    out.reserve(count);

    for (int i = 0; i < count; ++i) {
        // 0) PDFium: a true 150-dpi render of the page, so printing matches
        //    exactly what is on screen (and what a browser would print).
        if (m_usePdfium) {
            const QImage img = m_pdfium.renderPage(i, 150.0);
            if (!img.isNull()) {
                out.append(img);
                continue;
            }
        }

        // 1) A page that is a single full-page image (typical scan).
        QByteArray fmt;
        const QByteArray data = m_doc.pageSingleImage(i, fmt);
        if (!data.isEmpty()) {
            QImage img;
            if (img.loadFromData(data, fmt.constData()) && !img.isNull()) {
                out.append(img);
                continue;
            }
        }

        // 2) Otherwise synthesise a white page at 150 dpi and draw the page's
        //    extracted text onto it (layout is not reconstructed, but the
        //    content is printable instead of empty).
        const PdfPageInfo pi = m_doc.pageInfo(i);
        const int w = qMax(1, int(pi.width  * 150.0 / 72.0));
        const int h = qMax(1, int(pi.height * 150.0 / 72.0));
        QImage page(w, h, QImage::Format_RGB32);
        page.fill(Qt::white);

        const QString text = m_doc.pageText(i);
        if (!text.isEmpty()) {
            QPainter p(&page);
            p.setPen(Qt::black);
            QFont f("Helvetica", 11);
            p.setFont(f);
            // Leave a ~0.5 inch margin on every side.
            const int margin = 75;
            p.drawText(QRect(margin, margin, w - 2 * margin, h - 2 * margin),
                       Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, text);
        }
        out.append(page);
    }
    return out;
}

// --- Zoom (request #3) -----------------------------------------------------
void PdfTab::setZoom(double z)
{
    const double old = m_zoom;
    m_zoom = qBound(0.10, z, 8.0);
    if (m_view) m_view->setScale(m_zoom);
    // A zoom change means the cached raster is the wrong resolution, so
    // re-render the page through PDFium at the new on-screen size. That is
    // what keeps text sharp at 400% instead of showing an upscaled bitmap.
    if (!qFuzzyCompare(old, m_zoom) && pageCount() > 0) {
        invalidatePageCache();
        const QSizeF pts = pageSizePts(m_currentPage);
        const int pxW = qMax(1, int(pts.width()  * m_zoom + 0.5));
        const int pxH = qMax(1, int(pts.height() * m_zoom + 0.5));
        const QImage bg = renderPage(m_currentPage, QSize(pxW, pxH));
        if (!bg.isNull() && m_view) {
            const PdfPageInfo pi = m_doc.pageInfo(m_currentPage);
            m_view->setPage(pts.width(), pts.height(),
                            m_usePdfium ? 0 : pi.rotation, bg);
            m_view->setScale(m_zoom);
        }
    }
    updateNavState();
    updatePageInfoLabel();
}

void PdfTab::zoomIn()    { setZoom(m_zoom * 1.25); }
void PdfTab::zoomOut()   { setZoom(m_zoom / 1.25); }
void PdfTab::zoomReset() { setZoom(1.0); }

// Fit the page width into the viewport (SumatraPDF's "Fit Width").
void PdfTab::fitWidth()
{
    if (!m_view || !m_scroll || pageCount() <= 0) return;
    const double pw = m_view->displayWidthPts();
    if (pw <= 0) return;
    // Leave room for the vertical scroll bar + the 20 px page margin each side.
    const int sb = m_scroll->verticalScrollBar()
                       ? m_scroll->verticalScrollBar()->sizeHint().width() : 16;
    const double avail = qMax(80, m_scroll->viewport()->width() - sb - 48);
    setZoom(avail / pw);
}

// Fit the whole page into the viewport (SumatraPDF's "Fit Page").
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

// --- Annotation tools ------------------------------------------------------
void PdfTab::setToolHighlight()  { m_view->setTool(PdfPageView::Highlight);  m_view->setColor(m_annotColor); }
void PdfTab::setToolUnderline()  { m_view->setTool(PdfPageView::Underline);  m_view->setColor(m_annotColor); }
void PdfTab::setToolStrike()     { m_view->setTool(PdfPageView::Strike);     m_view->setColor(m_annotColor); }
void PdfTab::setToolTextBox()    { m_view->setTool(PdfPageView::TextBox);    m_view->setColor(Qt::black); }
void PdfTab::setToolStickyNote() { m_view->setTool(PdfPageView::StickyNote); m_view->setColor(m_annotColor); }
void PdfTab::setToolPen()        { m_view->setTool(PdfPageView::Pen);        m_view->setColor(m_annotColor); }
void PdfTab::setToolStamp()      { m_view->setTool(PdfPageView::Stamp);      m_view->setColor(m_annotColor); }

void PdfTab::pickAnnotColor()
{
    QColor c = QColorDialog::getColor(m_annotColor, this, TR("Annotation Color"));
    if (c.isValid()) { m_annotColor = c; m_view->setColor(c); }
}

// --- Page management -------------------------------------------------------
void PdfTab::mergePdf()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, TR("Select PDF(s) to append"), QString(), TR("PDF Files (*.pdf)"));
    if (files.isEmpty()) return;
    bakeAnnotationsForCurrentPage();
    for (const QString &f : files) {
        PdfDocument other;
        if (other.load(f))
            m_doc.appendDocument(other);
    }
    rebuildThumbnails();
    showPage(m_currentPage);
    markModified(true);
    emit statusMessage(TR("Merged %1 file(s).").arg(files.size()));
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
    QString dir = QFileDialog::getExistingDirectory(this, TR("Output folder"));
    if (dir.isEmpty()) return;

    QList<int> first, second;
    for (int i = 0; i < at; ++i) first.append(i);
    for (int i = at; i < m_doc.pageCount(); ++i) second.append(i);

    QString base = QFileInfo(m_filePath.isEmpty() ? "document.pdf" : m_filePath)
                       .completeBaseName();
    auto write = [&](const QList<int> &idx, const QString &suffix) {
        PdfDocument part = m_doc.extractPages(idx);
        QFile f(dir + "/" + base + suffix + ".pdf");
        if (f.open(QIODevice::WriteOnly)) { f.write(part.serialize()); f.close(); }
    };
    write(first, "_part1");
    write(second, "_part2");
    emit statusMessage(TR("Split into 2 files in %1").arg(dir));
}

void PdfTab::deleteCurrentPage()
{
    if (m_doc.pageCount() <= 1) {
        QMessageBox::information(this, TR("Delete Page"),
                                 TR("A PDF must have at least one page."));
        return;
    }
    m_view->clearAnnotations();
    m_doc.deletePage(m_currentPage);
    if (m_currentPage >= m_doc.pageCount())
        m_currentPage = m_doc.pageCount() - 1;
    rebuildThumbnails();
    showPage(m_currentPage);
    markModified(true);
}

void PdfTab::insertPages()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, TR("Select PDF(s) to insert"), QString(), TR("PDF Files (*.pdf)"));
    if (files.isEmpty()) return;
    bakeAnnotationsForCurrentPage();
    int at = m_currentPage;
    for (const QString &f : files) {
        PdfDocument other;
        if (other.load(f)) {
            m_doc.insertDocument(at, other);
            at += other.pageCount();
        }
    }
    rebuildThumbnails();
    showPage(m_currentPage);
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

    QString out = QFileDialog::getSaveFileName(this, TR("Save extracted PDF"),
                                               QString(), TR("PDF Files (*.pdf)"));
    if (out.isEmpty()) return;
    PdfDocument part = m_doc.extractPages(idx);
    QFile f(out);
    if (f.open(QIODevice::WriteOnly)) { f.write(part.serialize()); f.close(); }
    emit statusMessage(TR("Extracted %1 page(s) to %2").arg(idx.size()).arg(out));
}

void PdfTab::rotateCurrentLeft()
{
    m_doc.rotatePage(m_currentPage, -90);
    rebuildThumbnails();
    showPage(m_currentPage);
    markModified(true);
}

void PdfTab::rotateCurrentRight()
{
    m_doc.rotatePage(m_currentPage, 90);
    rebuildThumbnails();
    showPage(m_currentPage);
    markModified(true);
}

void PdfTab::movePageUp()
{
    if (m_currentPage <= 0) return;
    bakeAnnotationsForCurrentPage();
    m_doc.movePage(m_currentPage, m_currentPage - 1);
    m_currentPage--;
    rebuildThumbnails();
    showPage(m_currentPage);
    markModified(true);
}

void PdfTab::movePageDown()
{
    if (m_currentPage >= m_doc.pageCount() - 1) return;
    bakeAnnotationsForCurrentPage();
    m_doc.movePage(m_currentPage, m_currentPage + 1);
    m_currentPage++;
    rebuildThumbnails();
    showPage(m_currentPage);
    markModified(true);
}

// --- Signatures ------------------------------------------------------------
namespace {
// A tiny modal canvas for hand-drawing a signature.
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
    QDialog dlg(this);
    dlg.setWindowTitle(TR("Draw Signature"));
    auto *v = new QVBoxLayout(&dlg);
    v->addWidget(new QLabel(TR("Draw your signature below, then click OK."), &dlg));
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
    QByteArray png;
    QBuffer buf(&png); buf.open(QIODevice::WriteOnly);
    sig.save(&buf, "PNG");

    PdfPageInfo pi = m_doc.pageInfo(m_currentPage);
    double w = 150, h = 60;
    m_doc.stampImage(m_currentPage, png, "png", pi.width - w - 40, 40, w, h, 1.0);
    rebuildThumbnails();
    showPage(m_currentPage);
    markModified(true);
    emit statusMessage(TR("Signature added to page %1.").arg(m_currentPage + 1));
}

void PdfTab::imageSignature()
{
    QString f = QFileDialog::getOpenFileName(
        this, TR("Choose signature image"), QString(),
        TR("Images (*.png *.jpg *.jpeg *.bmp)"));
    if (f.isEmpty()) return;
    QFile file(f);
    if (!file.open(QIODevice::ReadOnly)) return;
    QByteArray data = file.readAll();
    QByteArray fmt = QFileInfo(f).suffix().toLower().toLatin1();

    PdfPageInfo pi = m_doc.pageInfo(m_currentPage);
    double w = 160, h = 70;
    m_doc.stampImage(m_currentPage, data, fmt, pi.width - w - 40, 40, w, h, 1.0);
    rebuildThumbnails();
    showPage(m_currentPage);
    markModified(true);
    emit statusMessage(TR("Image signature added."));
}

// --- Conversion ------------------------------------------------------------
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
        // Prefer a true PDFium render at 150 dpi: every page exports as a real
        // picture of the page, not just the embedded scan (or a blank sheet).
        if (m_usePdfium)
            page = m_pdfium.renderPage(i, 150.0);
        if (page.isNull()) {
            QByteArray f;
            const QByteArray img = m_doc.pageSingleImage(i, f);
            if (!img.isEmpty()) page.loadFromData(img, f.constData());
        }
        if (page.isNull()) {
            // Synthesize a blank white page at 150 dpi, at the page's own
            // aspect ratio, as the last resort.
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

void PdfTab::imagesToPdfReplace()
{
    // Reserved: building a PDF from images is exposed via File > Open as PDF
    // (image selection). This slot is a placeholder for future UI.
}

// PDF -> Word: extract the document text (layout is lost, as expected for the
// free-tier style conversion) and write a Word-openable .rtf file. RTF opens
// directly in Microsoft Word and LibreOffice while being trivial to generate
// without any Office/OOXML dependency.
void PdfTab::exportToWord()
{
    // PDFium's text extraction understands embedded font encodings and CMaps,
    // so it recovers text the built-in extractor cannot. Use it when present.
    QString text;
    if (m_usePdfium) {
        QStringList parts;
        const int n = m_pdfium.pageCount();
        for (int i = 0; i < n; ++i)
            parts << m_pdfium.pageText(i);
        text = parts.join(QStringLiteral("\n\n"));
    }
    if (text.trimmed().isEmpty())
        text = m_doc.allText();
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
    if (out.isEmpty())
        return;
    if (!out.endsWith(".rtf", Qt::CaseInsensitive))
        out += ".rtf";

    // Build a minimal but valid RTF. Non-ASCII characters are emitted as
    // \uN? Unicode escapes so Chinese / accented text survives.
    QByteArray rtf = "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0 Calibri;}}\\fs22\n";
    for (QChar ch : text) {
        ushort u = ch.unicode();
        if (ch == '\n') { rtf += "\\par\n"; }
        else if (ch == '\\' ) { rtf += "\\\\"; }
        else if (ch == '{')   { rtf += "\\{"; }
        else if (ch == '}')   { rtf += "\\}"; }
        else if (u < 128)     { rtf += char(u); }
        else                  { rtf += "\\u" + QByteArray::number((short)u) + "?"; }
    }
    rtf += "}";

    QFile f(out);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, TR("To Word"), TR("Cannot write %1").arg(out));
        return;
    }
    f.write(rtf);
    f.close();
    emit statusMessage(TR("Exported text to %1 (layout not preserved).").arg(out));
}

// --- Compression -----------------------------------------------------------
void PdfTab::compressPdf()
{
    // Our writer re-emits every stream; to actually shrink, re-compress all
    // uncompressed streams with FlateDecode and downsample embedded images.
    // Simplest reliable win: re-serialize (drops incremental-update bloat and
    // duplicate xref sections), then report the size delta.
    bakeAnnotationsForCurrentPage();
    QByteArray before = m_doc.serialize();
    // Reparse + reserialize collapses redundancy.
    PdfDocument nd;
    nd.loadData(before);
    QByteArray after = nd.isValid() ? nd.serialize() : before;

    QString out = QFileDialog::getSaveFileName(
        this, TR("Save compressed PDF"),
        m_filePath.isEmpty() ? QString() : m_filePath, TR("PDF Files (*.pdf)"));
    if (out.isEmpty()) return;
    QFile f(out);
    if (f.open(QIODevice::WriteOnly)) { f.write(after); f.close(); }
    double ratio = before.size() > 0
                       ? 100.0 * (before.size() - after.size()) / before.size()
                       : 0.0;
    emit statusMessage(TR("Compressed: %1 KB → %2 KB (%3% smaller)")
                           .arg(before.size() / 1024)
                           .arg(after.size() / 1024)
                           .arg(QString::number(qMax(0.0, ratio), 'f', 1)));
}

// --- Watermark -------------------------------------------------------------
void PdfTab::addTextWatermark()
{
    bool ok = false;
    QString text = QInputDialog::getText(this, TR("Text Watermark"),
                                          TR("Watermark text:"), QLineEdit::Normal,
                                          TR("CONFIDENTIAL"), &ok);
    if (!ok || text.isEmpty()) return;
    bakeAnnotationsForCurrentPage();
    m_doc.addTextWatermark(text, -1);
    rebuildThumbnails();
    showPage(m_currentPage);
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
    bakeAnnotationsForCurrentPage();
    m_doc.addImageWatermark(data, fmt, -1, 0.4, 0.25);
    rebuildThumbnails();
    showPage(m_currentPage);
    markModified(true);
    emit statusMessage(TR("Image watermark applied to all pages."));
}

// --- Protection ------------------------------------------------------------
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

    // Encrypt a COPY so the open working document stays editable.
    bakeAnnotationsForCurrentPage();
    PdfDocument copy;
    copy.loadData(m_doc.serialize());
    copy.encrypt(pw);
    QFile f(out);
    if (f.open(QIODevice::WriteOnly)) { f.write(copy.serialize()); f.close(); }
    emit statusMessage(TR("Encrypted PDF written to %1").arg(out));
}

// --- Forms -----------------------------------------------------------------
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
    rebuildThumbnails();
    showPage(m_currentPage);
    markModified(true);
    emit statusMessage(TR("Filled %1 field(s).").arg(n));
}

#include "pdftab.moc"