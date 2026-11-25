#include "imageeditortab.h"
#include "core/i18n.h"

#include <QScrollArea>
#include <QLabel>
#include <QTimer>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QLineEdit>
#include <QButtonGroup>
#include <QToolButton>
#include <QToolBar>
#include "core/iconfactory.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QPushButton>
#include <QScrollBar>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QImageReader>
#include <QImageWriter>
#include <QImageWriter>
#include <QColorDialog>
#include <QFontDialog>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QtMath>
#include <QStack>
#include <QQueue>

static inline QString TRi(const char *s) { return I18n::instance()->tr(QString::fromUtf8(s)); }

// ===========================================================================
//  Tool ids
// ===========================================================================
namespace {
enum Tool {
    ToolNone = 0,
    ToolBrush,
    ToolEraser,
    ToolLine,
    ToolRect,
    ToolEllipse,
    ToolArrow,
    ToolText,
    ToolCrop,
    ToolMosaic,
    ToolMagicWand,
};
} // namespace

// ===========================================================================
//  ImageCanvas — the zoomable paint surface. Owns the working QImage, draws it
//  at the current zoom, and implements the interactive drawing/crop/mosaic
//  tools via mouse events. It calls back into the parent ImageEditorTab for
//  undo snapshots and status messages.
// ===========================================================================
class ImageCanvas : public QWidget
{
    Q_OBJECT
public:
    explicit ImageCanvas(ImageEditorTab *owner, QWidget *parent = nullptr)
        : QWidget(parent), m_owner(owner)
    {
        setMouseTracking(true);
        setBackgroundRole(QPalette::Dark);
    }

    void setImage(const QImage &img)
    {
        // Force an editable, uniform pixel format. Some PNGs come back from
        // QImageReader as Format_Mono / Format_Indexed8 / Format_RGBA8888
        // for which setPixelColor() silently ends up remapping to the closest
        // palette entry — which makes tools like Mosaic appear to do nothing
        // on specific files (e.g. certain Dropbox-exported PNGs).
        m_image = img.format() == QImage::Format_ARGB32
                      ? img
                      : img.convertToFormat(QImage::Format_ARGB32);
        updateGeometry();
        resize(scaledSize());
        update();
    }
    const QImage &image() const { return m_image; }
    QImage &imageRef() { return m_image; }

    // Preview overlay: a transient image shown instead of m_image (used for
    // live slider adjustments). Empty => show m_image.
    void setPreview(const QImage &img) { m_preview = img; update(); }
    void clearPreview() { m_preview = QImage(); update(); }

    double zoom() const { return m_zoom; }
    void setZoom(double z)
    {
        m_zoom = qBound(0.02, z, 32.0);
        resize(scaledSize());
        update();
    }
    void zoomFitTo(const QSize &viewport)
    {
        if (m_image.isNull()) return;
        // Viewport needs to have positive dimensions; subtract a small margin
        // (16px) for scrollbars/borders so the whole image fits cleanly.
        const int vw = viewport.width() - 16;
        const int vh = viewport.height() - 16;
        if (vw <= 0 || vh <= 0) return;

        const double zx = double(vw) / m_image.width();
        const double zy = double(vh) / m_image.height();
        // Scale the image to fit the viewport in BOTH dimensions regardless of
        // whether the image is large or small (large is shrunk, small is scaled up).
        double z = qMin(zx, zy);
        setZoom(z);
    }

    void setTool(int t) { m_tool = t; m_hasSelection = false; update(); }
    int  tool() const { return m_tool; }

    void setBrushColor(const QColor &c) { m_brushColor = c; }
    void setBrushWidth(int w) { m_brushWidth = w; }
    void setTextColor(const QColor &c) { m_textColor = c; }
    void setTextSize(int s) { m_textSize = s; }
    void setMosaicBlock(int b) { m_mosaicBlock = qMax(2, b); }
    void setTolerance(int t) { m_wandTol = t; }

    QRect selectionImageRect() const { return m_selection; }
    bool  hasSelection() const { return m_hasSelection && m_selection.isValid(); }

    QSize sizeHint() const override { return scaledSize(); }

protected:
    QSize scaledSize() const
    {
        if (m_image.isNull()) return QSize(100, 100);
        return QSize(qRound(m_image.width() * m_zoom),
                     qRound(m_image.height() * m_zoom));
    }

    QPoint widgetToImage(const QPoint &p) const
    {
        return QPoint(qBound(0, int(p.x() / m_zoom), m_image.width() - 1),
                      qBound(0, int(p.y() / m_zoom), m_image.height() - 1));
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        // Checkerboard behind transparent images.
        const int cs = 12;
        for (int y = 0; y < height(); y += cs)
            for (int x = 0; x < width(); x += cs) {
                const bool even = ((x / cs) + (y / cs)) % 2 == 0;
                p.fillRect(x, y, cs, cs, even ? QColor(200,200,200) : QColor(235,235,235));
            }
        const QImage &shown = m_preview.isNull() ? m_image : m_preview;
        if (!shown.isNull())
            p.drawImage(QRect(QPoint(0,0), scaledSize()), shown);

        // In-progress shape / crop rubber band.
        if (m_drawing && (m_tool == ToolLine || m_tool == ToolRect ||
                          m_tool == ToolEllipse || m_tool == ToolArrow ||
                          m_tool == ToolCrop)) {
            QPainter op(this);
            QPen pen(m_tool == ToolCrop ? QColor(0,120,255) : m_brushColor);
            pen.setWidthF(qMax(1.0, m_brushWidth * m_zoom));
            if (m_tool == ToolCrop) { pen.setStyle(Qt::DashLine); pen.setWidth(1); }
            op.setPen(pen);
            const QPoint a(m_start.x()*m_zoom, m_start.y()*m_zoom);
            const QPoint b(m_cur.x()*m_zoom, m_cur.y()*m_zoom);
            switch (m_tool) {
            case ToolLine:    op.drawLine(a, b); break;
            case ToolRect:    op.drawRect(QRect(a, b).normalized()); break;
            case ToolEllipse: op.drawEllipse(QRect(a, b).normalized()); break;
            case ToolArrow:   drawArrow(op, a, b); break;
            case ToolCrop:    op.drawRect(QRect(a, b).normalized()); break;
            }
        }
        // Committed crop selection outline.
        if (m_hasSelection && m_tool == ToolCrop) {
            QPainter op(this);
            QPen pen(QColor(0,120,255)); pen.setStyle(Qt::DashLine);
            op.setPen(pen);
            const QRect r(QPoint(m_selection.x()*m_zoom, m_selection.y()*m_zoom),
                          QSize(m_selection.width()*m_zoom, m_selection.height()*m_zoom));
            op.drawRect(r);
        }
    }

    static void drawArrow(QPainter &p, const QPointF &a, const QPointF &b)
    {
        p.drawLine(a, b);
        const double ang = std::atan2(b.y()-a.y(), b.x()-a.x());
        const double len = 14.0;
        const double spread = M_PI / 7.0;
        QPointF h1(b.x() - len*std::cos(ang - spread), b.y() - len*std::sin(ang - spread));
        QPointF h2(b.x() - len*std::cos(ang + spread), b.y() - len*std::sin(ang + spread));
        p.drawLine(b, h1);
        p.drawLine(b, h2);
    }

    void mousePressEvent(QMouseEvent *e) override
    {
        if (m_image.isNull() || e->button() != Qt::LeftButton) return;
        m_start = m_cur = widgetToImage(e->pos());
        m_drawing = true;

        switch (m_tool) {
        case ToolText: {
            m_drawing = false;
            placeText(m_start);
            return;
        }
        case ToolMagicWand: {
            m_drawing = false;
            magicWand(m_start);
            return;
        }
        case ToolBrush:
        case ToolEraser:
        case ToolMosaic:
            m_owner->statusMessage("");   // begin a freehand stroke
            emit beginStroke();
            applyFreehand(m_start, m_start);
            break;
        }
        update();
    }

    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (m_image.isNull()) return;
        const QPoint ip = widgetToImage(e->pos());
        if (m_drawing && (m_tool == ToolBrush || m_tool == ToolEraser || m_tool == ToolMosaic)) {
            applyFreehand(m_cur, ip);
        }
        m_cur = ip;
        if (m_drawing) update();
    }

    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (!m_drawing || e->button() != Qt::LeftButton) return;
        m_drawing = false;
        const QPoint a = m_start, b = m_cur;
        switch (m_tool) {
        case ToolLine: case ToolRect: case ToolEllipse: case ToolArrow:
            emit beginStroke();
            commitShape(a, b);
            break;
        case ToolCrop:
            m_selection = QRect(a, b).normalized()
                              .intersected(m_image.rect());
            m_hasSelection = m_selection.width() > 1 && m_selection.height() > 1;
            break;
        }
        update();
    }

signals:
    void beginStroke();   // ask the owner to push an undo snapshot once

private:
    void applyFreehand(const QPoint &a, const QPoint &b)
    {
        // IMPORTANT: the mosaic path must be handled BEFORE any QPainter is
        // opened on m_image. Qt allows only one painter per paint device, so
        // calling pixelateBlock() (which paints and reads pixels itself) while
        // an outer painter was still active silently did nothing — that was the
        // reason the mosaic tool appeared broken.
        if (m_tool == ToolMosaic) {
            pixelateAlong(a, b);
            update();
            return;
        }

        QPainter p(&m_image);
        p.setRenderHint(QPainter::Antialiasing, true);
        if (m_tool == ToolEraser) {
            p.setCompositionMode(QPainter::CompositionMode_Clear);
            QPen pen(Qt::transparent, m_brushWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            p.setPen(pen);
            p.drawLine(a, b);
        } else { // brush
            QPen pen(m_brushColor, m_brushWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            p.setPen(pen);
            p.drawLine(a, b);
        }
        update();
    }

    // Pixelate every grid cell the segment a->b passes through. Interpolating
    // along the segment (instead of only stamping the end point) means a fast
    // drag no longer leaves un-pixelated gaps between mouse events.
    void pixelateAlong(const QPoint &a, const QPoint &b)
    {
        const int block = qMax(2, m_mosaicBlock);
        const int dx = b.x() - a.x();
        const int dy = b.y() - a.y();
        const int dist = int(std::sqrt(double(dx) * dx + double(dy) * dy));
        // Step at most half a block so consecutive stamps always overlap.
        const int steps = qMax(1, dist / qMax(1, block / 2));
        for (int i = 0; i <= steps; ++i) {
            const int x = a.x() + dx * i / steps;
            const int y = a.y() + dy * i / steps;
            pixelateBlock(QPoint(x, y));
        }
    }

    // Average the colours of the grid cell containing `center` and flood the
    // whole cell with that average. Snapping to a fixed grid (rather than
    // centring on the cursor) is what produces the classic blocky mosaic and
    // keeps repeated passes over the same area stable.
    void pixelateBlock(const QPoint &center)
    {
        const int block = qMax(2, m_mosaicBlock);
        // Snap to a block-aligned grid cell.
        const int gx = (center.x() / block) * block;
        const int gy = (center.y() / block) * block;
        QRect r(gx, gy, block, block);
        r = r.intersected(m_image.rect());
        if (r.isEmpty())
            return;

        // Average colour over the cell. Read pixels with NO painter active.
        qint64 rr = 0, gg = 0, bb = 0, aa = 0, n = 0;
        for (int y = r.top(); y <= r.bottom(); ++y) {
            for (int x = r.left(); x <= r.right(); ++x) {
                const QColor c = m_image.pixelColor(x, y);
                rr += c.red(); gg += c.green(); bb += c.blue(); aa += c.alpha();
                ++n;
            }
        }
        if (n == 0)
            return;
        const QColor avg(int(rr / n), int(gg / n), int(bb / n), int(aa / n));

        // Fill the cell directly (setPixelColor avoids needing a painter and
        // works correctly for both ARGB32 and RGB32 images).
        for (int y = r.top(); y <= r.bottom(); ++y)
            for (int x = r.left(); x <= r.right(); ++x)
                m_image.setPixelColor(x, y, avg);
    }

    void commitShape(const QPoint &a, const QPoint &b)
    {
        QPainter p(&m_image);
        p.setRenderHint(QPainter::Antialiasing, true);
        QPen pen(m_brushColor, m_brushWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        switch (m_tool) {
        case ToolLine:    p.drawLine(a, b); break;
        case ToolRect:    p.drawRect(QRect(a, b).normalized()); break;
        case ToolEllipse: p.drawEllipse(QRect(a, b).normalized()); break;
        case ToolArrow:   drawArrow(p, a, b); break;
        }
    }

    void placeText(const QPoint &at)
    {
        bool ok = false;
        const QString text = QInputDialog::getMultiLineText(
            this, TRi("Add Text"), TRi("Text:"), QString(), &ok);
        if (!ok || text.isEmpty()) return;
        emit beginStroke();
        QPainter p(&m_image);
        p.setRenderHint(QPainter::Antialiasing, true);
        QFont f = p.font();
        f.setPixelSize(m_textSize);
        p.setFont(f);
        // Simple outline: draw black offset copies, then the fill color.
        QPainterPath path;
        path.addText(at.x(), at.y() + m_textSize, f, text);
        QPen outline(Qt::black, qMax(1, m_textSize/16));
        p.setPen(outline);
        p.setBrush(m_textColor);
        p.drawPath(path);
    }

    // Flood-fill selection to transparency from the clicked seed (magic wand):
    // removes a contiguous region of similar color (simple background cutout).
    void magicWand(const QPoint &seed)
    {
        if (m_image.isNull()) return;
        emit beginStroke();
        if (m_image.format() != QImage::Format_ARGB32)
            m_image = m_image.convertToFormat(QImage::Format_ARGB32);
        const QColor target = m_image.pixelColor(seed);
        const int W = m_image.width(), H = m_image.height();
        QVector<bool> visited(W*H, false);
        QQueue<QPoint> q;
        q.enqueue(seed);
        visited[seed.y()*W + seed.x()] = true;
        auto close = [this, target](const QColor &c) {
            const int d = qAbs(c.red()-target.red()) + qAbs(c.green()-target.green())
                        + qAbs(c.blue()-target.blue());
            return d <= m_wandTol * 3;
        };
        while (!q.isEmpty()) {
            const QPoint pt = q.dequeue();
            m_image.setPixelColor(pt, QColor(0,0,0,0));   // make transparent
            static const int dx[4] = {1,-1,0,0};
            static const int dy[4] = {0,0,1,-1};
            for (int i=0;i<4;++i) {
                const int nx = pt.x()+dx[i], ny = pt.y()+dy[i];
                if (nx<0||ny<0||nx>=W||ny>=H) continue;
                const int idx = ny*W+nx;
                if (visited[idx]) continue;
                if (close(m_image.pixelColor(nx,ny))) {
                    visited[idx] = true;
                    q.enqueue(QPoint(nx,ny));
                }
            }
        }
        update();
    }

    ImageEditorTab *m_owner = nullptr;
    QImage m_image;
    QImage m_preview;
    double m_zoom = 1.0;
    int    m_tool = ToolNone;

    bool   m_drawing = false;
    QPoint m_start, m_cur;

    QColor m_brushColor = Qt::red;
    int    m_brushWidth = 4;
    QColor m_textColor = Qt::white;
    int    m_textSize = 32;
    int    m_mosaicBlock = 8;
    int    m_wandTol = 32;

    QRect  m_selection;
    bool   m_hasSelection = false;
};

// ===========================================================================
//  ImageEditorTab
// ===========================================================================
ImageEditorTab::ImageEditorTab(QWidget *parent) : QWidget(parent)
{
    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(0,0,0,0);
    outer->setSpacing(0);

    // --- canvas (scrollable, zoomable) ---
    m_scroll = new QScrollArea(this);
    m_scroll->setAlignment(Qt::AlignCenter);
    m_scroll->setWidgetResizable(false);
    m_canvas = new ImageCanvas(this, m_scroll);
    m_scroll->setWidget(m_canvas);
    outer->addWidget(m_scroll, 1);

    connect(m_canvas, &ImageCanvas::beginStroke, this, [this]() {
        pushUndo();
        m_modified = true;
        updateTitleState();
    });

    // --- tool / adjustment side panel ---
    auto *side = new QWidget(this);
    side->setFixedWidth(260);
    auto *sl = new QVBoxLayout(side);
    sl->setContentsMargins(6,6,6,6);
    sl->setSpacing(6);

    // Tools group (brush/eraser/shapes/text/crop/mosaic/wand).
    auto *toolBox = new QGroupBox(TRi("Tools"), side);
    auto *tg = new QGridLayout(toolBox);
    auto *group = new QButtonGroup(this);
    group->setExclusive(true);
    struct T { const char *label; int id; };
    const T tools[] = {
        {"Brush", ToolBrush}, {"Eraser", ToolEraser}, {"Line", ToolLine},
        {"Rect", ToolRect}, {"Ellipse", ToolEllipse}, {"Arrow", ToolArrow},
        {"Text", ToolText}, {"Crop", ToolCrop}, {"Mosaic", ToolMosaic},
        {"Magic Wand", ToolMagicWand},
    };
    // Side-panel tool buttons use the same icon-above-text style as every
    // other side panel in the app (PDF side panel, etc.): vector icon drawn by
    // IconFactory, English label kept for live translation.
    int r = 0, c = 0;
    for (const auto &t : tools) {
        auto *b = new QToolButton(toolBox);
        b->setText(TRi(t.label));
        b->setToolTip(TRi(t.label));
        const QIcon ic = IconFactory::makeToolIcon(QString::fromUtf8(t.label));
        if (!ic.isNull()) {
            b->setIcon(ic);
            b->setIconSize(QSize(18, 18));
            b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        } else {
            b->setToolButtonStyle(Qt::ToolButtonTextOnly);
        }
        b->setCheckable(true);
        b->setMinimumWidth(72);
        b->setStyleSheet("QToolButton{padding:3px;}");
        group->addButton(b, t.id);
        tg->addWidget(b, r, c);
        if (++c == 3) { c = 0; ++r; }
    }
    connect(group, &QButtonGroup::idClicked, this, &ImageEditorTab::setTool);
    sl->addWidget(toolBox);

    // Brush / text style.
    auto *styleBox = new QGroupBox(TRi("Style"), side);
    auto *stf = new QFormLayout(styleBox);
    // The two colour buttons also gain vector icons (color wheel) so the side
    // panel is uniformly iconised.
    auto *brushColorBtn = new QPushButton(IconFactory::makeToolIcon(QStringLiteral("Color...")),
                                          TRi("Brush Color..."), styleBox);
    brushColorBtn->setIconSize(QSize(18, 18));
    brushColorBtn->setStyleSheet("text-align:left; padding:3px 6px;");
    connect(brushColorBtn, &QPushButton::clicked, this, &ImageEditorTab::pickBrushColor);
    auto *brushW = new QSpinBox(styleBox); brushW->setRange(1, 200); brushW->setValue(m_brushWidth);
    connect(brushW, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v){
        m_brushWidth = v; if (m_canvas) m_canvas->setBrushWidth(v);
    });
    auto *textColorBtn = new QPushButton(IconFactory::makeToolIcon(QStringLiteral("Color...")),
                                         TRi("Text Color..."), styleBox);
    textColorBtn->setIconSize(QSize(18, 18));
    textColorBtn->setStyleSheet("text-align:left; padding:3px 6px;");
    connect(textColorBtn, &QPushButton::clicked, this, &ImageEditorTab::pickTextColor);
    auto *textSz = new QSpinBox(styleBox); textSz->setRange(6, 512); textSz->setValue(m_textSize);
    connect(textSz, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v){
        m_textSize = v; if (m_canvas) m_canvas->setTextSize(v);
    });
    auto *mosaicSz = new QSpinBox(styleBox); mosaicSz->setRange(2, 100); mosaicSz->setValue(8);
    connect(mosaicSz, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v){
        if (m_canvas) m_canvas->setMosaicBlock(v);
    });
    auto *wandTol = new QSpinBox(styleBox); wandTol->setRange(0, 255); wandTol->setValue(32);
    connect(wandTol, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v){
        if (m_canvas) m_canvas->setTolerance(v);
    });
    stf->addRow(brushColorBtn);
    stf->addRow(TRi("Brush Width:"), brushW);
    stf->addRow(textColorBtn);
    stf->addRow(TRi("Text Size:"), textSz);
    stf->addRow(TRi("Mosaic Block:"), mosaicSz);
    stf->addRow(TRi("Wand Tolerance:"), wandTol);
    sl->addWidget(styleBox);

    // Basic adjustments.
    auto *adjBox = new QGroupBox(TRi("Adjustments"), side);
    auto *af = new QFormLayout(adjBox);
    auto makeSlider = [adjBox](int lo, int hi) {
        auto *s = new QSlider(Qt::Horizontal, adjBox);
        s->setRange(lo, hi); s->setValue(0);
        return s;
    };
    m_brightness  = makeSlider(-100, 100);
    m_contrast    = makeSlider(-100, 100);
    m_saturation  = makeSlider(-100, 100);
    m_temperature = makeSlider(-100, 100);
    m_exposure    = makeSlider(-100, 100);
    m_shadows     = makeSlider(-100, 100);
    m_highlights  = makeSlider(-100, 100);
    m_sharpen     = makeSlider(0, 100);
    af->addRow(TRi("Brightness"),  m_brightness);
    af->addRow(TRi("Contrast"),    m_contrast);
    af->addRow(TRi("Saturation"),  m_saturation);
    af->addRow(TRi("Temperature"), m_temperature);
    af->addRow(TRi("Exposure"),    m_exposure);
    af->addRow(TRi("Shadows"),     m_shadows);
    af->addRow(TRi("Highlights"),  m_highlights);
    af->addRow(TRi("Sharpen"),     m_sharpen);
    for (QSlider *s : { m_brightness, m_contrast, m_saturation, m_temperature,
                        m_exposure, m_shadows, m_highlights, m_sharpen })
        connect(s, &QSlider::valueChanged, this, &ImageEditorTab::onAdjustmentChanged);
    auto *adjBtns = new QHBoxLayout();
    auto *applyBtn = new QPushButton(TRi("Apply"), adjBox);
    auto *resetBtn = new QPushButton(TRi("Reset"), adjBox);
    connect(applyBtn, &QPushButton::clicked, this, &ImageEditorTab::applyAdjustments);
    connect(resetBtn, &QPushButton::clicked, this, &ImageEditorTab::resetAdjustments);
    adjBtns->addWidget(applyBtn); adjBtns->addWidget(resetBtn);
    af->addRow(adjBtns);
    sl->addWidget(adjBox);

    // Filters.
    auto *filtBox = new QGroupBox(TRi("Filters"), side);
    auto *ff = new QHBoxLayout(filtBox);
    auto *filtCombo = new QComboBox(filtBox);
    filtCombo->addItems({ "None", "Grayscale", "Sepia", "Invert", "Warm",
                          "Cool", "Vintage", "High Contrast", "Blur" });
    filtCombo->setObjectName("filterCombo");
    auto *filtApply = new QPushButton(TRi("Apply"), filtBox);
    connect(filtApply, &QPushButton::clicked, this, &ImageEditorTab::applyPresetFilter);
    ff->addWidget(filtCombo, 1); ff->addWidget(filtApply);
    sl->addWidget(filtBox);
    m_filterCombo = filtCombo;

    sl->addStretch(1);

    // Wrap the side panel in its own scroll area so it works on small screens.
    auto *sideScroll = new QScrollArea(this);
    sideScroll->setWidgetResizable(true);
    sideScroll->setWidget(side);
    sideScroll->setFixedWidth(280);
    outer->addWidget(sideScroll);

    // --- top toolbar (file/geometry/canvas/zoom) spanning the whole widget ---
    // We place it above via a vertical container.
    auto *container = new QWidget(this);
    auto *cv = new QVBoxLayout(container);
    cv->setContentsMargins(0,0,0,0);
    cv->setSpacing(0);

    auto *tb = new QToolBar(container);
    // Toolbar uses auto-generated monochrome vector icons; the original label
    // is kept as the tooltip (and still follows UI language changes live).
    tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
    tb->setIconSize(QSize(20, 20));
    auto addIconAction = [&](const char *src, auto slot) -> QAction * {
        QIcon icon = IconFactory::makeToolIcon(QString::fromUtf8(src));
        QAction *a = icon.isNull() ? tb->addAction(TRi(src), this, slot)
                                   : tb->addAction(icon, TRi(src), this, slot);
        a->setToolTip(TRi(src));
        return a;
    };
    addIconAction("Undo", &ImageEditorTab::undo);
    addIconAction("Redo", &ImageEditorTab::redo);
    tb->addSeparator();
    addIconAction("Rotate Left",  &ImageEditorTab::rotateLeft);
    addIconAction("Rotate Right", &ImageEditorTab::rotateRight);
    addIconAction("Free Rotate...", &ImageEditorTab::rotateFree);
    addIconAction("Straighten...", &ImageEditorTab::straighten);
    addIconAction("Flip H", &ImageEditorTab::flipHorizontal);
    addIconAction("Flip V", &ImageEditorTab::flipVertical);
    addIconAction("Crop", &ImageEditorTab::cropToSelection);
    tb->addSeparator();
    addIconAction("Resize...", &ImageEditorTab::resizeCanvas);
    addIconAction("Scale...", &ImageEditorTab::scaleImage);
    addIconAction("DPI...", &ImageEditorTab::setDpi);
    addIconAction("Expand Canvas...", &ImageEditorTab::expandCanvas);
    tb->addSeparator();
    addIconAction("Auto Cutout", &ImageEditorTab::autoCutout);
    addIconAction("Text Watermark...", &ImageEditorTab::addTextWatermark);
    addIconAction("Image Watermark...", &ImageEditorTab::addImageWatermark);
    tb->addSeparator();
    addIconAction("Export As...", &ImageEditorTab::exportAs);
    addIconAction("Compress & Export...", &ImageEditorTab::compressExport);
    tb->addSeparator();
    addIconAction("Zoom In", &ImageEditorTab::zoomIn);
    addIconAction("Zoom Out", &ImageEditorTab::zoomOut);
    addIconAction("Fit", &ImageEditorTab::zoomFit);
    addIconAction("Actual Size", &ImageEditorTab::zoomReset);
    cv->addWidget(tb);

    m_info = new QLabel(container);
    m_info->setContentsMargins(6,2,6,2);

    // Re-parent the HBox contents (canvas + side) into the container below the
    // toolbar: build a body widget holding the existing outer layout.
    auto *body = new QWidget(container);
    body->setLayout(outer);
    cv->addWidget(body, 1);
    cv->addWidget(m_info);

    // Replace this widget's layout with a simple one holding the container.
    auto *root = new QVBoxLayout();
    root->setContentsMargins(0,0,0,0);
    root->addWidget(container);
    // `this` currently owns `outer`; move to root. Since outer was set as the
    // layout of `this` in the ctor, re-set now.
    delete layout();          // detach the initial outer (now owned by body)
    setLayout(root);
}

QString ImageEditorTab::fileName() const
{
    return m_filePath.isEmpty() ? TRi("image") : QFileInfo(m_filePath).fileName();
}

bool ImageEditorTab::loadFile(const QString &path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);   // honour EXIF orientation
    QImage img = reader.read();
    if (img.isNull())
        return false;
    // Work in ARGB32 so transparency / all tools behave uniformly.
    img = img.convertToFormat(QImage::Format_ARGB32);
    m_filePath = path;
    m_modified = false;
    m_undo.clear();
    m_redo.clear();
    setImage(img, false);
    // Defer zoomFit until the tab has been added and its viewport geometry
    // has settled, so whatever the image size (small or huge) it always adapts
    // to the window size cleanly on open.
    m_firstShowFit = false;
    QTimer::singleShot(0, this, [this]() {
        zoomFit();
        m_firstShowFit = true;
    });
    updateTitleState();
    return true;
}

void ImageEditorTab::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // If the window is resized before or right at open, keep the fit
    if (!m_firstShowFit) {
        QTimer::singleShot(0, this, [this]() {
            if (!m_firstShowFit) {
                zoomFit();
                m_firstShowFit = true;
            }
        });
    }
}

void ImageEditorTab::setImage(const QImage &img, bool markModified)
{
    m_canvas->setImage(img);
    if (markModified) { m_modified = true; updateTitleState(); }
    if (m_info) {
        m_info->setText(QString("%1 x %2   %3 DPI   %4%")
            .arg(img.width()).arg(img.height()).arg(m_dpi)
            .arg(qRound(m_canvas->zoom()*100)));
    }
}

QImage ImageEditorTab::currentBaseImage() const
{
    return m_canvas->image();
}

void ImageEditorTab::refreshCanvas()
{
    m_canvas->update();
    if (m_info) {
        const QImage &img = m_canvas->image();
        m_info->setText(QString("%1 x %2   %3 DPI   %4%")
            .arg(img.width()).arg(img.height()).arg(m_dpi)
            .arg(qRound(m_canvas->zoom()*100)));
    }
}

void ImageEditorTab::updateTitleState()
{
    emit modificationChanged(m_modified);
}

// --- undo / redo ---
void ImageEditorTab::pushUndo()
{
    m_undo.push_back(m_canvas->image());
    if (m_undo.size() > 40) m_undo.removeFirst();   // cap memory
    m_redo.clear();
}

void ImageEditorTab::undo()
{
    if (m_undo.isEmpty()) return;
    m_redo.push_back(m_canvas->image());
    QImage img = m_undo.takeLast();
    m_canvas->setImage(img);
    m_modified = true;
    updateTitleState();
    refreshCanvas();
}

void ImageEditorTab::redo()
{
    if (m_redo.isEmpty()) return;
    m_undo.push_back(m_canvas->image());
    QImage img = m_redo.takeLast();
    m_canvas->setImage(img);
    m_modified = true;
    updateTitleState();
    refreshCanvas();
}

// --- zoom ---
void ImageEditorTab::zoomIn()   { m_canvas->setZoom(m_canvas->zoom() * 1.25); refreshCanvas(); }
void ImageEditorTab::zoomOut()  { m_canvas->setZoom(m_canvas->zoom() / 1.25); refreshCanvas(); }
void ImageEditorTab::zoomReset(){ m_canvas->setZoom(1.0); refreshCanvas(); }
void ImageEditorTab::zoomFit()
{
    if (m_scroll && m_scroll->viewport())
        m_canvas->zoomFitTo(m_scroll->viewport()->size());
    refreshCanvas();
}

// --- tool selection ---
void ImageEditorTab::setTool(int tool)
{
    if (m_canvas) {
        m_canvas->setTool(tool);
        m_canvas->setBrushColor(m_brushColor);
        m_canvas->setBrushWidth(m_brushWidth);
        m_canvas->setTextColor(m_textColor);
        m_canvas->setTextSize(m_textSize);
    }
}

// --- basic adjustments (live preview) ---
void ImageEditorTab::onAdjustmentChanged()
{
    rebuildPreview();
}

void ImageEditorTab::rebuildPreview()
{
    const bool anyAdj = m_brightness->value() || m_contrast->value() ||
                        m_saturation->value() || m_temperature->value() ||
                        m_exposure->value() || m_shadows->value() ||
                        m_highlights->value() || m_sharpen->value();
    if (!anyAdj) { m_canvas->clearPreview(); return; }
    QImage preview = adjustImage(m_canvas->image(),
        m_brightness->value(), m_contrast->value(), m_saturation->value(),
        m_temperature->value(), m_exposure->value(), m_shadows->value(),
        m_highlights->value(), m_sharpen->value());
    m_canvas->setPreview(preview);
}

void ImageEditorTab::applyAdjustments()
{
    const bool anyAdj = m_brightness->value() || m_contrast->value() ||
                        m_saturation->value() || m_temperature->value() ||
                        m_exposure->value() || m_shadows->value() ||
                        m_highlights->value() || m_sharpen->value();
    if (!anyAdj) return;
    pushUndo();
    QImage out = adjustImage(m_canvas->image(),
        m_brightness->value(), m_contrast->value(), m_saturation->value(),
        m_temperature->value(), m_exposure->value(), m_shadows->value(),
        m_highlights->value(), m_sharpen->value());
    m_canvas->clearPreview();
    resetAdjustments();   // zero the sliders without re-preview
    setImage(out);
    emit statusMessage(TRi("Adjustments applied."));
}

void ImageEditorTab::resetAdjustments()
{
    for (QSlider *s : { m_brightness, m_contrast, m_saturation, m_temperature,
                        m_exposure, m_shadows, m_highlights, m_sharpen }) {
        s->blockSignals(true); s->setValue(0); s->blockSignals(false);
    }
    m_canvas->clearPreview();
}

// --- geometry ---
void ImageEditorTab::rotateLeft()
{
    pushUndo();
    QTransform t; t.rotate(-90);
    setImage(m_canvas->image().transformed(t, Qt::SmoothTransformation));
}
void ImageEditorTab::rotateRight()
{
    pushUndo();
    QTransform t; t.rotate(90);
    setImage(m_canvas->image().transformed(t, Qt::SmoothTransformation));
}
void ImageEditorTab::rotateFree()
{
    bool ok = false;
    const double deg = QInputDialog::getDouble(this, TRi("Free Rotate"),
        TRi("Angle (degrees):"), 0, -360, 360, 1, &ok);
    if (!ok || deg == 0) return;
    pushUndo();
    QTransform t; t.rotate(deg);
    setImage(m_canvas->image().transformed(t, Qt::SmoothTransformation));
}
void ImageEditorTab::straighten()
{
    bool ok = false;
    const double deg = QInputDialog::getDouble(this, TRi("Straighten"),
        TRi("Tilt (degrees):"), 0, -45, 45, 1, &ok);
    if (!ok || deg == 0) return;
    pushUndo();
    QTransform t; t.rotate(deg);
    QImage rot = m_canvas->image().transformed(t, Qt::SmoothTransformation);
    setImage(rot);
}
void ImageEditorTab::flipHorizontal()
{
    pushUndo();
    setImage(m_canvas->image().flipped(Qt::Horizontal));
}
void ImageEditorTab::flipVertical()
{
    pushUndo();
    setImage(m_canvas->image().flipped(Qt::Vertical));
}
void ImageEditorTab::cropToSelection()
{
    if (!m_canvas->hasSelection()) {
        emit statusMessage(TRi("Select a crop region first (use the Crop tool)."));
        return;
    }
    pushUndo();
    setImage(m_canvas->image().copy(m_canvas->selectionImageRect()));
    m_canvas->setTool(ToolNone);
}

// --- canvas operations ---
void ImageEditorTab::resizeCanvas()
{
    const QImage cur = m_canvas->image();
    bool ok = false;
    const int w = QInputDialog::getInt(this, TRi("Resize"), TRi("Width:"),
                                       cur.width(), 1, 100000, 1, &ok);
    if (!ok) return;
    const int h = QInputDialog::getInt(this, TRi("Resize"), TRi("Height:"),
                                       cur.height(), 1, 100000, 1, &ok);
    if (!ok) return;
    pushUndo();
    setImage(cur.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}
void ImageEditorTab::scaleImage()
{
    bool ok = false;
    const double pct = QInputDialog::getDouble(this, TRi("Scale"),
        TRi("Scale (%):"), 100, 1, 2000, 1, &ok);
    if (!ok || pct == 100) return;
    pushUndo();
    const QImage cur = m_canvas->image();
    setImage(cur.scaled(qRound(cur.width()*pct/100.0),
                        qRound(cur.height()*pct/100.0),
                        Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}
void ImageEditorTab::setDpi()
{
    bool ok = false;
    const int dpi = QInputDialog::getInt(this, TRi("DPI"), TRi("DPI:"),
                                         m_dpi, 1, 9600, 1, &ok);
    if (!ok) return;
    m_dpi = dpi;
    QImage img = m_canvas->image();
    const int dpm = qRound(dpi / 0.0254);   // dots per metre
    img.setDotsPerMeterX(dpm);
    img.setDotsPerMeterY(dpm);
    pushUndo();
    setImage(img);
    emit statusMessage(TRi("DPI set to %1.").arg(dpi));
}
void ImageEditorTab::expandCanvas()
{
    bool ok = false;
    const int margin = QInputDialog::getInt(this, TRi("Expand Canvas"),
        TRi("Border (px):"), 40, 0, 10000, 1, &ok);
    if (!ok) return;
    pushUndo();
    const QImage cur = m_canvas->image();
    QImage out(cur.width()+margin*2, cur.height()+margin*2, QImage::Format_ARGB32);
    out.fill(Qt::transparent);   // transparent expanded canvas
    QPainter p(&out);
    p.drawImage(margin, margin, cur);
    setImage(out);
}

// --- filters ---
void ImageEditorTab::applyPresetFilter()
{
    if (!m_filterCombo) return;
    const QString name = m_filterCombo->currentText();
    if (name == "None") return;
    pushUndo();
    setImage(applyFilterNamed(m_canvas->image(), name));
    emit statusMessage(TRi("Filter applied: %1").arg(name));
}

// --- auto cutout (remove near-uniform border background) ---
void ImageEditorTab::autoCutout()
{
    pushUndo();
    QImage img = m_canvas->image().convertToFormat(QImage::Format_ARGB32);
    const int W = img.width(), H = img.height();
    if (W < 2 || H < 2) return;
    // Sample the four corners to estimate the background color.
    auto px = [&](int x,int y){ return img.pixelColor(x,y); };
    const QColor bg = px(0,0);
    const int tol = 40;
    auto close = [&](const QColor &c){
        return qAbs(c.red()-bg.red())+qAbs(c.green()-bg.green())
             + qAbs(c.blue()-bg.blue()) <= tol*3;
    };
    // Flood fill from all border pixels that match the background.
    QVector<bool> visited(W*H, false);
    QQueue<QPoint> q;
    auto seed = [&](int x,int y){
        if (close(px(x,y)) && !visited[y*W+x]) { visited[y*W+x]=true; q.enqueue({x,y}); }
    };
    for (int x=0;x<W;++x){ seed(x,0); seed(x,H-1); }
    for (int y=0;y<H;++y){ seed(0,y); seed(W-1,y); }
    while (!q.isEmpty()) {
        const QPoint p = q.dequeue();
        img.setPixelColor(p, QColor(0,0,0,0));
        static const int dx[4]={1,-1,0,0}, dy[4]={0,0,1,-1};
        for (int i=0;i<4;++i){
            const int nx=p.x()+dx[i], ny=p.y()+dy[i];
            if (nx<0||ny<0||nx>=W||ny>=H) continue;
            if (visited[ny*W+nx]) continue;
            if (close(img.pixelColor(nx,ny))) { visited[ny*W+nx]=true; q.enqueue({nx,ny}); }
        }
    }
    setImage(img);
    emit statusMessage(TRi("Background removed (auto cutout)."));
}

// --- watermark ---
void ImageEditorTab::addTextWatermark()
{
    bool ok = false;
    const QString text = QInputDialog::getText(this, TRi("Text Watermark"),
        TRi("Watermark text:"), QLineEdit::Normal, "Terminal++", &ok);
    if (!ok || text.isEmpty()) return;
    pushUndo();
    QImage img = m_canvas->image();
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    QFont f = p.font();
    f.setPixelSize(qMax(16, img.width()/16));
    p.setFont(f);
    p.setPen(QColor(255,255,255,140));
    // Tile diagonally across the image.
    p.rotate(-30);
    const int step = f.pixelSize()*6;
    for (int y=-img.height(); y<img.height()*2; y+=step)
        for (int x=-img.width(); x<img.width()*2; x+=step*2)
            p.drawText(x, y, text);
    setImage(img);
}
void ImageEditorTab::addImageWatermark()
{
    const QString path = QFileDialog::getOpenFileName(this,
        TRi("Choose Watermark Image"), QString(),
        TRi("Images (*.png *.jpg *.jpeg *.bmp *.webp)"));
    if (path.isEmpty()) return;
    QImage wm(path);
    if (wm.isNull()) return;
    pushUndo();
    QImage img = m_canvas->image();
    // Scale watermark to ~1/5 of the image width, place bottom-right.
    const int w = qMax(32, img.width()/5);
    wm = wm.scaledToWidth(w, Qt::SmoothTransformation);
    QPainter p(&img);
    p.setOpacity(0.6);
    p.drawImage(img.width()-wm.width()-12, img.height()-wm.height()-12, wm);
    setImage(img);
}

// --- export / compress ---
void ImageEditorTab::exportAs()
{
    const QString path = QFileDialog::getSaveFileName(this, TRi("Export As"),
        m_filePath,
        "PNG (*.png);;JPEG (*.jpg *.jpeg);;WebP (*.webp);;BMP (*.bmp)");
    if (path.isEmpty()) return;
    if (m_canvas->image().save(path))
        emit statusMessage(TRi("Exported to %1").arg(path));
    else
        QMessageBox::warning(this, TRi("Export As"), TRi("Cannot write file."));
}
void ImageEditorTab::compressExport()
{
    const QString path = QFileDialog::getSaveFileName(this, TRi("Compress & Export"),
        m_filePath, "JPEG (*.jpg *.jpeg);;WebP (*.webp);;PNG (*.png)");
    if (path.isEmpty()) return;
    bool ok = false;
    const int quality = QInputDialog::getInt(this, TRi("Compress & Export"),
        TRi("Quality (1-100):"), 80, 1, 100, 1, &ok);
    if (!ok) return;
    QImageWriter w(path);
    w.setQuality(quality);
    if (w.write(m_canvas->image()))
        emit statusMessage(TRi("Exported (quality %1) to %2").arg(quality).arg(path));
    else
        QMessageBox::warning(this, TRi("Compress & Export"), TRi("Cannot write file."));
}

// --- drawing style pickers ---
void ImageEditorTab::pickBrushColor()
{
    const QColor c = QColorDialog::getColor(m_brushColor, this, TRi("Brush Color"),
                                            QColorDialog::ShowAlphaChannel);
    if (c.isValid()) {
        m_brushColor = c;
        if (m_canvas) m_canvas->setBrushColor(c);
    }
}
void ImageEditorTab::pickTextColor()
{
    const QColor c = QColorDialog::getColor(m_textColor, this, TRi("Text Color"),
                                            QColorDialog::ShowAlphaChannel);
    if (c.isValid()) {
        m_textColor = c;
        if (m_canvas) m_canvas->setTextColor(c);
    }
}

// --- save ---
bool ImageEditorTab::save()
{
    if (m_filePath.isEmpty())
        return false;
    if (!m_canvas->image().save(m_filePath))
        return false;
    m_modified = false;
    updateTitleState();
    emit statusMessage(TRi("Saved %1").arg(m_filePath));
    return true;
}
bool ImageEditorTab::saveAs(const QString &path)
{
    if (path.isEmpty())
        return false;
    if (!m_canvas->image().save(path))
        return false;
    m_filePath = path;
    m_modified = false;
    updateTitleState();
    emit statusMessage(TRi("Saved %1").arg(path));
    return true;
}

void ImageEditorTab::applyTheme(const EditorTheme &theme)
{
    // The canvas keeps its checkerboard; just tint the surrounding scroll area.
    if (m_scroll)
        m_scroll->setStyleSheet(QString("QScrollArea{background:%1;}")
                                    .arg(theme.background.name()));
}

// ===========================================================================
//  Static pixel operations
// ===========================================================================
QImage ImageEditorTab::adjustImage(const QImage &src, int brightness, int contrast,
                                    int saturation, int temperature, int exposure,
                                    int shadows, int highlights, int sharpen)
{
    QImage img = src.convertToFormat(QImage::Format_ARGB32);

    // Precompute scalar factors.
    const double bf = brightness;                    // -100..100 added to each channel
    const double cf = (contrast + 100) / 100.0;      // contrast multiplier around 128
    const double sf = (saturation + 100) / 100.0;    // saturation multiplier
    const double ef = std::pow(2.0, exposure / 100.0); // exposure as stops-ish
    const double temp = temperature;                 // +warm (more red), -cool (more blue)

    const int W = img.width(), H = img.height();
    for (int y = 0; y < H; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < W; ++x) {
            QColor c = QColor::fromRgba(line[x]);
            const int a = c.alpha();
            double r = c.red(), g = c.green(), b = c.blue();

            // Exposure (multiplicative).
            r *= ef; g *= ef; b *= ef;
            // Brightness (additive).
            r += bf; g += bf; b += bf;
            // Contrast (around mid-gray).
            r = (r - 128) * cf + 128;
            g = (g - 128) * cf + 128;
            b = (b - 128) * cf + 128;
            // Color temperature: warm pushes red up / blue down.
            r += temp * 0.4;
            b -= temp * 0.4;
            // Shadows / highlights: lift darks / compress brights.
            const double lum = (0.299*r + 0.587*g + 0.114*b);
            if (lum < 128 && shadows != 0) {
                const double k = shadows / 100.0 * (1.0 - lum/128.0);
                r += 60*k; g += 60*k; b += 60*k;
            }
            if (lum >= 128 && highlights != 0) {
                const double k = highlights / 100.0 * ((lum-128)/127.0);
                r += 60*k; g += 60*k; b += 60*k;
            }
            // Saturation (toward/away from luminance).
            if (sf != 1.0) {
                const double l = 0.299*r + 0.587*g + 0.114*b;
                r = l + (r - l) * sf;
                g = l + (g - l) * sf;
                b = l + (b - l) * sf;
            }
            auto clamp = [](double v){ return uchar(qBound(0.0, v, 255.0)); };
            line[x] = qRgba(clamp(r), clamp(g), clamp(b), a);
        }
    }

    // Sharpen via a simple 3x3 unsharp kernel, strength 0..100.
    if (sharpen > 0) {
        const double amt = sharpen / 100.0;
        QImage copy = img;
        for (int y = 1; y < H-1; ++y) {
            QRgb *out = reinterpret_cast<QRgb *>(img.scanLine(y));
            const QRgb *up = reinterpret_cast<const QRgb *>(copy.scanLine(y-1));
            const QRgb *mid = reinterpret_cast<const QRgb *>(copy.scanLine(y));
            const QRgb *dn = reinterpret_cast<const QRgb *>(copy.scanLine(y+1));
            for (int x = 1; x < W-1; ++x) {
                auto sharpenChan = [&](auto chan) {
                    const double center = chan(mid[x]);
                    const double lap = 5*center - chan(mid[x-1]) - chan(mid[x+1])
                                     - chan(up[x]) - chan(dn[x]);
                    return qBound(0.0, center + (lap-center)*amt, 255.0);
                };
                const int rr = sharpenChan([](QRgb p){ return double(qRed(p)); });
                const int gg = sharpenChan([](QRgb p){ return double(qGreen(p)); });
                const int bb = sharpenChan([](QRgb p){ return double(qBlue(p)); });
                out[x] = qRgba(rr, gg, bb, qAlpha(mid[x]));
            }
        }
    }
    return img;
}

QImage ImageEditorTab::applyFilterNamed(const QImage &src, const QString &name)
{
    QImage img = src.convertToFormat(QImage::Format_ARGB32);
    const int W = img.width(), H = img.height();

    auto perPixel = [&](auto fn) {
        for (int y = 0; y < H; ++y) {
            QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
            for (int x = 0; x < W; ++x) {
                int r = qRed(line[x]), g = qGreen(line[x]), b = qBlue(line[x]);
                const int a = qAlpha(line[x]);
                fn(r, g, b);
                line[x] = qRgba(qBound(0,r,255), qBound(0,g,255), qBound(0,b,255), a);
            }
        }
    };

    if (name == "Grayscale") {
        perPixel([](int &r, int &g, int &b){ int l = qRound(0.299*r+0.587*g+0.114*b); r=g=b=l; });
    } else if (name == "Sepia") {
        perPixel([](int &r, int &g, int &b){
            const int tr = qRound(0.393*r + 0.769*g + 0.189*b);
            const int tg = qRound(0.349*r + 0.686*g + 0.168*b);
            const int tb = qRound(0.272*r + 0.534*g + 0.131*b);
            r=tr; g=tg; b=tb; });
    } else if (name == "Invert") {
        perPixel([](int &r, int &g, int &b){ r=255-r; g=255-g; b=255-b; });
    } else if (name == "Warm") {
        perPixel([](int &r, int &g, int &b){ r+=25; b-=15; Q_UNUSED(g) });
    } else if (name == "Cool") {
        perPixel([](int &r, int &g, int &b){ r-=15; b+=25; Q_UNUSED(g) });
    } else if (name == "Vintage") {
        perPixel([](int &r, int &g, int &b){
            const int tr = qRound(0.9*r + 0.5*g + 0.1*b);
            const int tg = qRound(0.3*r + 0.8*g + 0.1*b);
            const int tb = qRound(0.2*r + 0.3*g + 0.5*b);
            r=qMin(255,tr); g=qMin(255,tg); b=qMin(255,tb); });
    } else if (name == "High Contrast") {
        perPixel([](int &r, int &g, int &b){
            auto hc = [](int v){ return int(qBound(0.0, (v-128)*1.6+128, 255.0)); };
            r=hc(r); g=hc(g); b=hc(b); });
    } else if (name == "Blur") {
        // Separable 3x3 box blur.
        QImage copy = img;
        for (int y = 1; y < H-1; ++y) {
            QRgb *out = reinterpret_cast<QRgb *>(img.scanLine(y));
            const QRgb *up = reinterpret_cast<const QRgb *>(copy.scanLine(y-1));
            const QRgb *mid = reinterpret_cast<const QRgb *>(copy.scanLine(y));
            const QRgb *dn = reinterpret_cast<const QRgb *>(copy.scanLine(y+1));
            for (int x = 1; x < W-1; ++x) {
                auto avg = [&](auto chan){
                    return (chan(up[x-1])+chan(up[x])+chan(up[x+1])
                          + chan(mid[x-1])+chan(mid[x])+chan(mid[x+1])
                          + chan(dn[x-1])+chan(dn[x])+chan(dn[x+1])) / 9;
                };
                out[x] = qRgba(avg([](QRgb p){return qRed(p);}),
                               avg([](QRgb p){return qGreen(p);}),
                               avg([](QRgb p){return qBlue(p);}),
                               qAlpha(mid[x]));
            }
        }
    }
    return img;
}

#include "imageeditortab.moc"
