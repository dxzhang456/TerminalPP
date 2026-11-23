#ifndef IMAGEEDITORTAB_H
#define IMAGEEDITORTAB_H

#include <QWidget>
#include <QImage>
#include <QVector>
#include <QColor>
#include <QPoint>
#include "thememanager.h"

class QScrollArea;
class QLabel;
class QComboBox;
class QSpinBox;
class QSlider;
class QLineEdit;
class QButtonGroup;
class QComboBox;
class ImageCanvas;   // internal paint surface (defined in the .cpp)

// A full-featured, self-contained raster image editor tab.
//
// Opens most common image formats (anything QImageReader supports: PNG, JPG,
// JPEG, BMP, WebP, GIF, TIFF, ICO, etc.). Oversized images are automatically
// shrunk to fit the visible viewport on open (while keeping the full-resolution
// pixels for editing — the shrink is only the on-screen zoom).
//
// Implemented features (all pure Qt / QImage / QPainter, no external libs):
//   1. Basic adjustments : brightness, contrast, saturation, color temperature,
//                          sharpen, exposure, shadows/highlights.
//   2. Geometry          : crop, free rotate, flip (H/V), straighten.
//   3. Canvas            : resize, scale, DPI, expand canvas, transparent bg.
//   4. Drawing           : brush, eraser, line, rectangle, ellipse, arrow.
//   5. Text              : add text with font size / color / simple outline.
//   6. Shapes / stickers : basic geometric shapes.
//   7. Cutout (simple)   : auto subject cutout, magic-wand (flood) selection.
//   8. Filters           : preset filters (grayscale, sepia, invert, …) + LUT-
//                          style tone curves.
//   9. Watermark         : text or image watermark.
//  10. Format conversion : save as JPG / PNG / WebP / BMP; quality/size control.
//  11. Mosaic / blur redaction + freehand erase.
//
// Every mutating operation pushes the previous image onto an undo stack, so
// Undo/Redo work across all tools uniformly.
class ImageEditorTab : public QWidget
{
    Q_OBJECT
public:
    explicit ImageEditorTab(QWidget *parent = nullptr);

    bool loadFile(const QString &path);
    bool save();                 // save to current path (format from extension)
    bool saveAs(const QString &path);
    bool isModified() const { return m_modified; }
    QString filePath() const { return m_filePath; }
    QString fileName() const;

    // The committed image (without any un-applied live slider preview), used
    // by File > Print to render the picture onto the page.
    QImage printableImage() const { return currentBaseImage(); }

    void applyTheme(const EditorTheme &theme);

protected:
    void resizeEvent(QResizeEvent *event) override;

signals:
    void statusMessage(const QString &msg);
    void modificationChanged(bool modified);

public slots:
    void undo();
    void redo();
    void zoomIn();
    void zoomOut();
    void zoomFit();
    void zoomReset();

private slots:
    // Tool selection.
    void setTool(int tool);

    // Basic adjustments (live preview via sliders; committed on "Apply").
    void onAdjustmentChanged();
    void applyAdjustments();
    void resetAdjustments();

    // Geometry.
    void rotateLeft();
    void rotateRight();
    void rotateFree();
    void flipHorizontal();
    void flipVertical();
    void straighten();
    void cropToSelection();

    // Canvas.
    void resizeCanvas();
    void scaleImage();
    void setDpi();
    void expandCanvas();

    // Filters.
    void applyPresetFilter();

    // Cutout.
    void autoCutout();     // remove near-uniform background -> transparency

    // Watermark.
    void addTextWatermark();
    void addImageWatermark();

    // Format / export.
    void exportAs();
    void compressExport();

    // Style controls for drawing tools.
    void pickBrushColor();
    void pickTextColor();

private:
    void pushUndo();                       // snapshot current image
    void setImage(const QImage &img, bool markModified = true);
    void rebuildPreview();                 // apply live adjustments to a preview
    QImage currentBaseImage() const;       // image without live preview
    void refreshCanvas();
    void updateTitleState();

    // Pixel operations (operate on and return a new QImage).
    static QImage adjustImage(const QImage &src, int brightness, int contrast,
                              int saturation, int temperature, int exposure,
                              int shadows, int highlights, int sharpen);
    static QImage applyFilterNamed(const QImage &src, const QString &name);

    QString   m_filePath;
    bool      m_modified = false;

    ImageCanvas *m_canvas = nullptr;
    QScrollArea *m_scroll = nullptr;

    // Undo / redo stacks of full image snapshots.
    QVector<QImage> m_undo;
    QVector<QImage> m_redo;

    // Live-adjustment slider state (applied non-destructively until committed).
    QSlider *m_brightness = nullptr;
    QSlider *m_contrast = nullptr;
    QSlider *m_saturation = nullptr;
    QSlider *m_temperature = nullptr;
    QSlider *m_exposure = nullptr;
    QSlider *m_shadows = nullptr;
    QSlider *m_highlights = nullptr;
    QSlider *m_sharpen = nullptr;

    // Drawing style.
    QColor   m_brushColor = Qt::red;
    int      m_brushWidth = 4;
    QColor   m_textColor = Qt::white;
    int      m_textSize = 32;

    QLabel  *m_info = nullptr;       // shows size / dpi
    int      m_dpi = 96;

    QComboBox *m_filterCombo = nullptr;   // preset-filter chooser
    bool      m_firstShowFit = false;     // whether auto-fit on initial display has triggered
};

#endif // IMAGEEDITORTAB_H