#include "imagecomparetab.h"
#include "core/i18n.h"

static inline QString TRc(const char *s) { return I18n::instance()->tr(QString::fromUtf8(s)); }

#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include "core/iconfactory.h"
#include <QAction>
#include <QFileInfo>
#include <QPainter>
#include <QTimer>

// A simple zoomable image view backed by a QLabel inside a QScrollArea.
class ImageView : public QScrollArea
{
    Q_OBJECT
public:
    explicit ImageView(QWidget *parent = nullptr) : QScrollArea(parent)
    {
        m_label = new QLabel(this);
        m_label->setBackgroundRole(QPalette::Dark);
        m_label->setAlignment(Qt::AlignCenter);
        m_label->setScaledContents(true);
        setWidget(m_label);
        setWidgetResizable(false);
        setAlignment(Qt::AlignCenter);
    }

    void setImage(const QImage &img)
    {
        m_img = img;
        applyZoom(m_zoom);
    }

    void applyZoom(double z)
    {
        m_zoom = z;
        if (m_img.isNull()) {
            m_label->clear();
            m_label->resize(0, 0);
            return;
        }
        QSize sz = m_img.size() * m_zoom;
        m_label->setPixmap(QPixmap::fromImage(m_img));
        m_label->resize(sz);
    }

    void fitTo(const QSize &viewport)
    {
        if (m_img.isNull())
            return;
        if (viewport.width() <= 0 || viewport.height() <= 0)
            return;
        double zx = double(viewport.width())  / m_img.width();
        double zy = double(viewport.height()) / m_img.height();
        // Fit BOTH directions: small images are upscaled so they are
        // comfortably visible; large images are downscaled to fit the viewport.
        double z = qMin(zx, zy) * 0.98;   // tiny margin
        applyZoom(qBound(0.01, z, 32.0));
    }

    double zoom() const { return m_zoom; }

signals:
    void scrolled(int h, int v);

protected:
    void scrollContentsBy(int dx, int dy) override
    {
        QScrollArea::scrollContentsBy(dx, dy);
        emit scrolled(horizontalScrollBar()->value(),
                      verticalScrollBar()->value());
    }

public:
    void setScroll(int h, int v)
    {
        horizontalScrollBar()->setValue(h);
        verticalScrollBar()->setValue(v);
    }

private:
    QLabel *m_label;
    QImage m_img;
    double m_zoom = 1.0;
};

ImageCompareTab::ImageCompareTab(QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *tb = new QToolBar(this);
    tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
    tb->setIconSize(QSize(20, 20));
    auto addIconAction = [&](const char *src, auto slot) -> QAction * {
        QIcon icon = IconFactory::makeToolIcon(QString::fromUtf8(src));
        QAction *a = icon.isNull() ? tb->addAction(TRc(src), this, slot)
                                   : tb->addAction(icon, TRc(src), this, slot);
        a->setToolTip(TRc(src));
        return a;
    };
    addIconAction("Zoom In", &ImageCompareTab::zoomIn);
    addIconAction("Zoom Out", &ImageCompareTab::zoomOut);
    addIconAction("Fit", &ImageCompareTab::zoomFit);
    addIconAction("1:1", &ImageCompareTab::zoomReset);
    tb->addSeparator();
    QAction *diffAct = tb->addAction(
        IconFactory::makeToolIcon(QStringLiteral("Difference Overlay")),
        TRc("Difference Overlay"));
    diffAct->setToolTip(TRc("Difference Overlay"));
    diffAct->setCheckable(true);
    connect(diffAct, &QAction::toggled, this, &ImageCompareTab::toggleDiffOverlay);
    tb->addSeparator();
    addIconAction("Reload", &ImageCompareTab::reload);
    tb->addSeparator();
    {
        QIcon icon = IconFactory::makeToolIcon(QStringLiteral("As Text"));
        QAction *a = icon.isNull() ? tb->addAction(TRc("As Text"))
                                   : tb->addAction(icon, TRc("As Text"));
        a->setToolTip(TRc("As Text"));
        connect(a, &QAction::triggered, this, [this]() {
            if (!m_leftPath.isEmpty() && !m_rightPath.isEmpty())
                emit requestCompareAs(0, m_leftPath, m_rightPath);
        });
    }
    {
        QIcon icon = IconFactory::makeToolIcon(QStringLiteral("As Hex"));
        QAction *a = icon.isNull() ? tb->addAction(TRc("As Hex"))
                                   : tb->addAction(icon, TRc("As Hex"));
        a->setToolTip(TRc("As Hex"));
        connect(a, &QAction::triggered, this, [this]() {
            if (!m_leftPath.isEmpty() && !m_rightPath.isEmpty())
                emit requestCompareAs(2, m_leftPath, m_rightPath);
        });
    }
    outer->addWidget(tb);

    auto *hdr = new QHBoxLayout();
    hdr->setContentsMargins(4, 2, 4, 2);
    m_leftLabel = new QLabel(TRc("(left)"), this);
    m_rightLabel = new QLabel(TRc("(right)"), this);
    m_leftLabel->setStyleSheet("font-weight:bold;");
    m_rightLabel->setStyleSheet("font-weight:bold;");
    hdr->addWidget(m_leftLabel, 1);
    hdr->addWidget(m_rightLabel, 1);
    outer->addLayout(hdr);

    auto *panes = new QHBoxLayout();
    panes->setContentsMargins(0, 0, 0, 0);
    panes->setSpacing(1);
    m_left = new ImageView(this);
    m_right = new ImageView(this);
    panes->addWidget(m_left, 1);
    panes->addWidget(m_right, 1);
    outer->addLayout(panes, 1);

    m_statsLabel = new QLabel(this);
    m_statsLabel->setContentsMargins(6, 2, 6, 2);
    outer->addWidget(m_statsLabel);

    // Synchronized pan.
    connect(m_left, &ImageView::scrolled, this, [this](int h, int v) {
        m_right->setScroll(h, v);
    });
    connect(m_right, &ImageView::scrolled, this, [this](int h, int v) {
        m_left->setScroll(h, v);
    });
}

bool ImageCompareTab::loadFiles(const QString &leftPath, const QString &rightPath)
{
    m_leftImg = QImage(leftPath);
    m_rightImg = QImage(rightPath);
    m_leftPath = leftPath;
    m_rightPath = rightPath;
    m_leftLabel->setText(QFileInfo(leftPath).fileName() + "  —  " + leftPath);
    m_rightLabel->setText(QFileInfo(rightPath).fileName() + "  —  " + rightPath);
    rebuildDiff();
    m_left->setImage(m_leftImg);
    m_right->setImage(m_diffMode ? m_diffImg : m_rightImg);
    // Fit the image into the visible area on open so a large picture is scaled
    // down to the viewport instead of overflowing. Deferred one event-loop turn
    // so the view already has its final size when we compute the fit scale.
    QTimer::singleShot(0, this, [this]() { zoomFit(); });
    updateStats();
    return !m_leftImg.isNull() && !m_rightImg.isNull();
}

void ImageCompareTab::reload()
{
    if (!m_leftPath.isEmpty() && !m_rightPath.isEmpty())
        loadFiles(m_leftPath, m_rightPath);
}

void ImageCompareTab::rebuildDiff()
{
    if (m_leftImg.isNull() || m_rightImg.isNull()) {
        m_diffImg = QImage();
        return;
    }
    const int w = qMax(m_leftImg.width(), m_rightImg.width());
    const int h = qMax(m_leftImg.height(), m_rightImg.height());
    QImage a = m_leftImg.convertToFormat(QImage::Format_ARGB32);
    QImage b = m_rightImg.convertToFormat(QImage::Format_ARGB32);

    m_diffImg = QImage(w, h, QImage::Format_ARGB32);
    m_diffImg.fill(Qt::black);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            QRgb pa = (x < a.width() && y < a.height()) ? a.pixel(x, y) : 0;
            QRgb pb = (x < b.width() && y < b.height()) ? b.pixel(x, y) : 0;
            if (pa == pb) {
                // Same: dim grayscale.
                int g = qGray(pa) / 3;
                m_diffImg.setPixel(x, y, qRgb(g, g, g));
            } else {
                // Different: bright magenta/red marker.
                m_diffImg.setPixel(x, y, qRgb(255, 40, 160));
            }
        }
    }
}

void ImageCompareTab::updateStats()
{
    if (m_leftImg.isNull() || m_rightImg.isNull()) {
        const QString msg = TRc("One or both images failed to load.");
        m_statsLabel->setText(msg);
        emit statusMessage(msg);
        return;
    }
    const int w = qMax(m_leftImg.width(), m_rightImg.width());
    const int h = qMax(m_leftImg.height(), m_rightImg.height());
    QImage a = m_leftImg.convertToFormat(QImage::Format_ARGB32);
    QImage b = m_rightImg.convertToFormat(QImage::Format_ARGB32);
    qint64 diff = 0;
    const qint64 total = qint64(w) * h;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            QRgb pa = (x < a.width() && y < a.height()) ? a.pixel(x, y) : 0;
            QRgb pb = (x < b.width() && y < b.height()) ? b.pixel(x, y) : 0;
            if (pa != pb) ++diff;
        }
    double pct = total ? (100.0 * diff / total) : 0.0;
    const QString msg = TRc("Left: %1x%2   Right: %3x%4   Differing pixels: %5 (%6%)")
                            .arg(m_leftImg.width()).arg(m_leftImg.height())
                            .arg(m_rightImg.width()).arg(m_rightImg.height())
                            .arg(diff).arg(QString::number(pct, 'f', 2));
    m_statsLabel->setText(msg);
    emit statusMessage(msg);
}

void ImageCompareTab::zoomIn()  { m_zoom *= 1.25; m_left->applyZoom(m_zoom); m_right->applyZoom(m_zoom); }
void ImageCompareTab::zoomOut() { m_zoom /= 1.25; m_left->applyZoom(m_zoom); m_right->applyZoom(m_zoom); }
void ImageCompareTab::zoomReset(){ m_zoom = 1.0;  m_left->applyZoom(m_zoom); m_right->applyZoom(m_zoom); }
void ImageCompareTab::zoomFit()
{
    m_left->fitTo(m_left->viewport()->size());
    m_zoom = m_left->zoom();
    m_right->applyZoom(m_zoom);
}

void ImageCompareTab::toggleDiffOverlay(bool on)
{
    m_diffMode = on;
    m_right->setImage(on ? m_diffImg : m_rightImg);
    m_right->applyZoom(m_zoom);
    emit statusMessage(on ? TRc("Difference overlay ON (right pane).")
                          : TRc("Difference overlay OFF."));
}

void ImageCompareTab::applyTheme(const EditorTheme &t)
{
    QString qss = QString("QScrollArea{background:%1;}")
                      .arg(t.background.darker(110).name());
    m_left->setStyleSheet(qss);
    m_right->setStyleSheet(qss);
}

#include "imagecomparetab.moc"