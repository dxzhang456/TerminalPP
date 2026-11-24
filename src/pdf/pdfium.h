#ifndef PDFIUM_H
#define PDFIUM_H

#include <QByteArray>
#include <QImage>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QVector>

// One extracted character of real (non-image) page text, with the box it
// occupies on the page. `box` is in PDF points with a TOP-LEFT origin and y
// growing downwards, i.e. already converted to the same coordinate system the
// page view paints in, so it can be mapped to widget pixels by a plain scale.
struct PdfTextChar {
    uint ch = 0;        // Unicode code point
    QRectF box;         // points, top-left origin
};

// ---------------------------------------------------------------------------
// PdfiumRenderer — a real PDF rasteriser backed by PDFium (the Chrome/Edge PDF
// engine), loaded **dynamically at run time**.
//
// Why dynamic loading instead of linking?
//   * The mingw64 toolchain has no pdfium package, so there is no import
//     library to link against.
//   * PDFium's public API is plain C with a stable ABI and the DLLs found on
//     this machine are self-contained x86-64 builds, so GetProcAddress works
//     perfectly and adds zero build-time dependencies.
//   * If no pdfium.dll can be found the class simply reports isAvailable()
//     == false and the caller transparently falls back to the built-in
//     PdfDocument engine — the application never fails to start or to open a
//     file because of a missing DLL.
//
// Search order for the DLL (first hit wins):
//   1. the TERMINALPP_PDFIUM environment variable (explicit full path)
//   2. pdfium.dll next to our own executable (the shipped/bundled copy)
//   3. the same directory plus a "pdfium\" subfolder
//   4. a list of well-known locations where other installed products keep a
//      copy (Edge/Chrome style installs, Windows Defender, Netskope, ...)
//   5. plain "pdfium.dll" via the normal DLL search path
//
// All rendering happens through FPDF_RenderPageBitmap into a BGRA buffer that
// is wrapped into a QImage, so pages render exactly as they do in Chrome —
// text, vector graphics, transparency and embedded fonts all included.
// ---------------------------------------------------------------------------
class PdfiumRenderer
{
public:
    PdfiumRenderer();
    ~PdfiumRenderer();

    PdfiumRenderer(const PdfiumRenderer &) = delete;
    PdfiumRenderer &operator=(const PdfiumRenderer &) = delete;

    // True when a pdfium.dll was found, loaded and all required entry points
    // resolved. When false, every other method is a no-op.
    static bool isAvailable();
    // Absolute path of the DLL actually in use (for the About box / status).
    static QString libraryPath();
    // The locations that were tried, for diagnostics when loading failed.
    static QStringList searchLog();

    // --- Document lifetime -------------------------------------------------
    // Open from a file path or from memory. `password` may be empty.
    // Returns false and sets error() when the document cannot be opened.
    bool open(const QString &path, const QString &password = QString());
    bool openData(const QByteArray &bytes, const QString &password = QString());
    void close();

    bool isOpen() const { return m_doc != nullptr; }
    bool needsPassword() const { return m_needsPassword; }
    QString error() const { return m_error; }

    int pageCount() const { return m_pageCount; }

    // Page size in PDF points (1/72 inch). Width/height are already the
    // *visible* size: PDFium applies the page's /Rotate for us, so a
    // landscape page really reports width > height.
    QSizeF pageSizePts(int index) const;

    // True when the page is wider than it is tall — used to decide the
    // portrait/landscape presentation automatically.
    bool isLandscape(int index) const;

    // --- Rendering ---------------------------------------------------------
    // Render one page to an ARGB32 QImage.
    //   renderPage(i, dpi)            — at a given resolution (72 = 1:1)
    //   renderPageToFit(i, maxSize)   — largest render that fits maxSize while
    //                                   preserving the page's aspect ratio
    // Both return a null QImage on failure.
    QImage renderPage(int index, double dpi = 150.0) const;
    QImage renderPageToFit(int index, const QSize &maxSize) const;

    // Extracted text of a page (UTF-16 from PDFium, converted to QString).
    QString pageText(int index) const;

    // --- Real text selection ----------------------------------------------
    // Every character of the page's genuine text layer together with its
    // bounding box, in reading order (the order PDFium reports, which is the
    // order FPDFText_GetText concatenates). Empty for scanned/image-only pages
    // that carry no text layer at all.
    QVector<PdfTextChar> pageChars(int index) const;

    // True when the page has a text layer, i.e. its text really is text and
    // can be selected/copied rather than being pixels in a picture.
    bool pageHasText(int index) const;

private:
    void *m_doc = nullptr;          // FPDF_DOCUMENT
    int m_pageCount = 0;
    bool m_needsPassword = false;
    QString m_error;
    // Keeps the bytes alive for the lifetime of a memory-loaded document,
    // because FPDF_LoadMemDocument does not copy them.
    QByteArray m_data;

    QImage renderInternal(int index, int pxW, int pxH) const;
};

#endif // PDFIUM_H