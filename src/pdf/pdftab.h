#ifndef PDFTAB_H
#define PDFTAB_H

#include <QWidget>
#include <QVector>
#include <QPointF>
#include <QColor>
#include <QImage>
#include <QHash>
#include <QList>
#include <QByteArray>
#include "editor/thememanager.h"
#include "pdf/pdfcore.h"
#include "pdf/pdfium.h"

class QAction;
class QListWidget;
class QScrollArea;
class QLabel;
class QToolBar;
class QComboBox;
class QSpinBox;
class PdfPageView;   // internal annotation overlay surface (defined in the .cpp)

// ---------------------------------------------------------------------------
// PdfTab — a PDF viewer + lightweight editor tab.
//
// Rendering strategy (two backends, best one wins per file):
//   1. PDFium (preferred) — the same engine Chrome/Edge use, loaded
//      dynamically at run time from pdfium.dll. It is a full rasteriser, so
//      text, vector graphics, embedded fonts and transparency all render
//      exactly as in a browser. Pages are re-rendered at the current zoom so
//      the result is always crisp rather than an upscaled bitmap.
//   2. The built-in PdfDocument — used when no pdfium.dll is present. It is
//      not a rasteriser, so a page shows either its embedded full-page image
//      (scanned documents) or a schematic sheet with a short explanation.
//
// Either way the page is presented at its true aspect ratio, and the
// portrait/landscape orientation follows the PDF's own page box, so a
// landscape slide deck is laid out landscape without any user action.
//
// All *structural* editing always goes through the built-in PdfDocument, and
// the annotation overlay bakes vector marks back into the PDF content stream
// on save, so the produced file is a real, standards-compliant PDF.
//
// Toolbar features (all implemented against PdfDocument):
//   * Annotate : highlight, underline, strikethrough, text box, sticky note,
//                freehand pen, stamp
//   * Pages    : merge, split, delete / insert / extract, rotate, reorder
//   * Sign     : hand-drawn signature, image signature
//   * Convert  : PDF <-> image (JPG/PNG)
//   * Compress : shrink file size
//   * Watermark: text / image
//   * Protect  : set an open password
//   * Forms    : fill AcroForm text fields
// ---------------------------------------------------------------------------
class PdfTab : public QWidget
{
    Q_OBJECT
public:
    explicit PdfTab(QWidget *parent = nullptr);

    bool loadFile(const QString &path);
    bool save();                       // save to current path
    bool saveAs(const QString &path);
    bool isModified() const { return m_modified; }
    QString filePath() const { return m_filePath; }
    QString fileName() const;

    // Rasterised pages for File > Print — one QImage per PDF page.
    // Pages that embed a full-page image (scanned documents) return that
    // image; text-based pages return a synthesised white page with the
    // extracted text drawn onto it, so printing never produces blank sheets.
    // Non-const because the underlying PdfDocument lazily parses objects while
    // the pages are being read.
    QVector<QImage> printablePages();

    void applyTheme(const EditorTheme &theme);

signals:
    void statusMessage(const QString &msg);
    void modificationChanged(bool modified);

public slots:
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void fitWidth();
    void fitPage();

    // Page navigation (also exposed on the toolbar as Previous / Next).
    void prevPage();
    void nextPage();
    void firstPage();
    void lastPage();

    // Re-apply the current UI language to every toolbar action / label. Wired
    // to I18n::languageChanged so the PDF toolbar follows the global language
    // switch live, without having to reopen the tab.
    void retranslate();

private slots:
    void onPageSelected(int row);
    void onPageSpinChanged(int value);

    // Annotation tools.
    void setToolHighlight();
    void setToolUnderline();
    void setToolStrike();
    void setToolTextBox();
    void setToolStickyNote();
    void setToolPen();
    void setToolStamp();
    void pickAnnotColor();

    // Page management.
    void mergePdf();
    void splitPdf();
    void deleteCurrentPage();
    void insertPages();
    void extractPages();
    void rotateCurrentLeft();
    void rotateCurrentRight();
    void movePageUp();
    void movePageDown();

    // Signatures.
    void drawSignature();
    void imageSignature();

    // Conversion.
    void exportToImages();
    void exportToWord();        // PDF -> Word (.rtf text, layout loss)
    void imagesToPdfReplace();

    // Compression.
    void compressPdf();

    // Watermark.
    void addTextWatermark();
    void addImageWatermark();

    // Protection.
    void setOpenPassword();

    // Forms.
    void fillForms();

private:
    void rebuildThumbnails();
    void showPage(int index);
    void markModified(bool on = true);
    QByteArray currentPdfBytes() const;           // serialize working doc
    void reloadFromBytes(const QByteArray &data);  // reparse after a structural op
    void bakeAnnotationsForCurrentPage();          // flush overlay -> PDF content

    void setZoom(double z);            // clamp + apply + refresh the zoom label
    void updateNavState();             // enable/disable nav buttons, sync spin
    void updatePageInfoLabel();        // bottom status line

    // Page count from whichever backend opened the file (PDFium can read
    // documents the built-in parser cannot).
    int pageCount() const;
    // Rasterise one page at the given on-screen pixel size, preferring PDFium
    // and falling back to the built-in engine's embedded-image extraction.
    // The result is cached per (page, size) so repaints do not re-render.
    QImage renderPage(int index, const QSize &targetPx) const;
    // Page size in PDF points, taken from whichever backend opened the file.
    QSizeF pageSizePts(int index) const;
    // Drop the cached raster (after a zoom change or a structural edit).
    void invalidatePageCache();

    QString m_filePath;
    bool m_modified = false;

    PdfDocument m_doc;             // working document (mutated in place)

    // --- Rendering backend -------------------------------------------------
    // PDFium is a real rasteriser (the Chrome PDF engine) loaded dynamically
    // at run time. When it is available every page renders exactly as it does
    // in a browser — text, vector art, embedded fonts and transparency. The
    // built-in PdfDocument stays responsible for all *structural* editing
    // (merge/split/rotate/annotate/watermark/...) and is also the rendering
    // fallback on machines where no pdfium.dll can be found.
    PdfiumRenderer m_pdfium;
    bool m_usePdfium = false;      // true once the current file opened in PDFium
    // Re-render on demand and cache, so panning/repaints are cheap but a zoom
    // change still produces a crisp raster instead of an upscaled blur.
    mutable QImage m_pageCache;
    mutable int m_pageCacheIndex = -1;
    mutable QSize m_pageCacheSize;

    int m_currentPage = 0;
    double m_zoom = 1.0;

    QToolBar *m_tb = nullptr;          // main toolbar
    QListWidget *m_thumbs = nullptr;   // page list (left)
    QScrollArea *m_scroll = nullptr;   // page viewport (center)
    PdfPageView *m_view = nullptr;     // annotation surface
    QLabel *m_info = nullptr;          // status / page info

    // --- Navigation / zoom widgets (request: Prev/Next + Zoom Out/In) ---
    QAction *m_actPrev = nullptr;
    QAction *m_actNext = nullptr;
    QAction *m_actFirst = nullptr;
    QAction *m_actLast = nullptr;
    QAction *m_actZoomOut = nullptr;
    QAction *m_actZoomIn = nullptr;
    QAction *m_actZoomReset = nullptr;
    QAction *m_actFitWidth = nullptr;
    QAction *m_actFitPage = nullptr;
    QSpinBox *m_pageSpin = nullptr;    // current page number indicator / jump
    QLabel *m_pageTotal = nullptr;     // " / N"
    QLabel *m_zoomLabel = nullptr;     // "100%"

    // Every translatable toolbar action keeps its English source string so the
    // toolbar can be retranslated in place when the UI language changes.
    QHash<QAction *, QByteArray> m_actionSources;
    QList<QAction *> m_tooltipOnly;    // actions whose text is a glyph/symbol

    QColor m_annotColor = QColor(255, 220, 0);  // highlight yellow by default
};

#endif // PDFTAB_H