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
class QGroupBox;
<<<<<<< HEAD
class QPushButton;
=======
>>>>>>> f56ebedd4ac859088bdbb3d5d37f7a7ad8bdd6ea
class PdfPageView;   // internal annotation overlay surface (defined in the .cpp)

// ---------------------------------------------------------------------------
// PdfTab — a PDF viewer + lightweight editor tab.
//
// Rendering strategy (two backends, best one wins per file):
//   1. PDFium (preferred) — the same engine Chrome/Edge use, loaded
//      dynamically at run time from pdfium.dll.
//   2. The built-in PdfDocument — used when no pdfium.dll is present.
//
// Annotations (highlight, underline, strike, text box, sticky note, pen,
// stamp, image signature) live on an overlay layer per page until Save. Before
// saving they can all be dragged and resized, and the overlay supports full
// undo/redo via Ctrl+Z / Ctrl+Y.
//
// Text with non-Latin characters (Chinese/Japanese/Korean) is rendered through
// Qt's font engine into a transparent QImage and stamped into the PDF with an
// SMask, which is what lets watermarks, text boxes and sticky note titles
// display correctly without us having to embed a CID font in the PDF.
// ---------------------------------------------------------------------------
class PdfTab : public QWidget
{
    Q_OBJECT
public:
    explicit PdfTab(QWidget *parent = nullptr);
    ~PdfTab() override;

    bool loadFile(const QString &path);
    bool save();                       // save to current path
    bool saveAs(const QString &path);
    bool isModified() const { return m_modified; }
    QString filePath() const { return m_filePath; }
    QString fileName() const;

    QVector<QImage> printablePages();

    void applyTheme(const EditorTheme &theme);

signals:
    void statusMessage(const QString &msg);
    void modificationChanged(bool modified);
    // A new PDF was produced by Split / Extract: ask the main window to open
    // it as a new tab so the result is immediately visible.
    void openPdfRequested(const QString &path);

public slots:
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void fitWidth();
    void fitPage();

    void prevPage();
    void nextPage();
    void firstPage();
    void lastPage();

    void copySelectedText();
    void selectAllText();
    void copyPageText();
    void copyAllText();

    void retranslate();
    void saveFromToolbar();

    // Annotation edit-history slots (Ctrl+Z / Ctrl+Y). These only affect the
    // uncommitted (overlay) annotations; a Save bakes whatever remains.
    void annotUndo();
    void annotRedo();

private slots:
    void onPageSelected(int row);
    void onPageSpinChanged(int value);
    void onViewContextMenu(const QPoint &pos);
    void onSelectionChanged();
    void onOverlayChanged();           // view says: annotations list changed

    // Annotation tools.
    void setToolHighlight();
    void setToolUnderline();
    void setToolStrike();
    void setToolTextBox();
    void setToolStickyNote();
    void setToolPen();
    void setToolStamp();
    void setToolSelect();              // the default: pick/drag/resize
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
    void exportToWord();
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
    QByteArray currentPdfBytes() const;
    void reloadFromBytes(const QByteArray &data);
    // Bake every uncommitted annotation (on *every* page) into the real PDF.
    // Called from save() / saveAs() so uncommitted marks survive a round-trip.
    void bakeAllPendingAnnotations();

    void setZoom(double z);
    void updateNavState();
    void updatePageInfoLabel();

    int pageCount() const;
    QImage renderPage(int index, const QSize &targetPx) const;
    QSizeF pageSizePts(int index) const;
    void invalidatePageCache();

    void loadTextLayerForCurrentPage();
    QString pageTextOf(int index) const;

    void refreshAfterEdit();

    // Save as helper that writes a sub-document to disk and asks MainWindow
    // to open it as a new tab (used by Split / Extract so the just-produced
    // PDF appears immediately).
    bool writeAndOpenDerived(const PdfDocument &part, const QString &suggestedPath);

    QString m_filePath;
    bool m_modified = false;

    PdfDocument m_doc;             // working document (mutated in place)

    PdfiumRenderer m_pdfium;
    bool m_usePdfium = false;
    mutable QImage m_pageCache;
    mutable int m_pageCacheIndex = -1;
    mutable QSize m_pageCacheSize;

    int m_currentPage = 0;
    double m_zoom = 1.0;

    QToolBar *m_tb = nullptr;
    QListWidget *m_thumbs = nullptr;
    QScrollArea *m_scroll = nullptr;
    PdfPageView *m_view = nullptr;
    QLabel *m_info = nullptr;

    QAction *m_actPrev = nullptr;
    QAction *m_actNext = nullptr;
    QAction *m_actFirst = nullptr;
    QAction *m_actLast = nullptr;
    QAction *m_actZoomOut = nullptr;
    QAction *m_actZoomIn = nullptr;
    QAction *m_actZoomReset = nullptr;
    QAction *m_actFitWidth = nullptr;
    QAction *m_actFitPage = nullptr;
    QAction *m_actSave = nullptr;
    QAction *m_actUndo = nullptr;
    QAction *m_actRedo = nullptr;
    QAction *m_actCopyText = nullptr;
    QAction *m_actSelectAllText = nullptr;
    QAction *m_actCopyPageText = nullptr;
    QSpinBox *m_pageSpin = nullptr;
    QLabel *m_pageTotal = nullptr;
    QLabel *m_zoomLabel = nullptr;

    // Side toolbar (mirrors the image editor's side panel).
    QScrollArea *m_sideScroll = nullptr;
    QGroupBox *m_annotGroup = nullptr;
    QGroupBox *m_pagesGroup = nullptr;
    QGroupBox *m_moreGroup = nullptr;

    QHash<QAction *, QByteArray> m_actionSources;
    QList<QAction *> m_tooltipOnly;
    // Side-panel buttons keep their own source-string map so retranslate()
    // can update their text live when the UI language changes. Kept separate
    // from the QAction map above because the side panel uses QPushButton.
    QHash<QPushButton *, QByteArray> m_sideBtnSources;

    QColor m_annotColor = QColor(255, 220, 0);

    QString m_fallbackPageText;

    QString m_renderTempPath;

    QString m_openPassword;
    bool m_readOnlyEncrypted = false;
};

#endif // PDFTAB_H