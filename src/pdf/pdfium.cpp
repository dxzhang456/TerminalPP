#include "pdf/pdfium.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QProcessEnvironment>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

// ===========================================================================
// PDFium public C API — declared locally so we need no pdfium headers.
//
// These signatures are taken from PDFium's stable public headers
// (fpdfview.h / fpdf_text.h). The ABI is __stdcall on Win32 and the default
// C calling convention on x64, which is what FPDF_CALLCONV expands to.
// ===========================================================================
namespace {

#ifdef Q_OS_WIN
#  define PDFIUM_CALL __stdcall
#else
#  define PDFIUM_CALL
#endif

using FPDF_DOCUMENT = void *;
using FPDF_PAGE     = void *;
using FPDF_BITMAP   = void *;
using FPDF_TEXTPAGE = void *;

struct FPDF_LIBRARY_CONFIG_T {
    int version;
    const char **m_pUserFontPaths;
    void *m_pIsolate;
    unsigned int m_v8EmbedderSlot;
    void *m_pPlatform;
    int m_RendererType;
};

// PDFium bitmap formats / render flags we use.
enum { FPDFBitmap_BGRA = 4 };
enum { FPDF_ANNOT = 0x01, FPDF_LCD_TEXT = 0x02, FPDF_NO_NATIVETEXT = 0x04,
       FPDF_RENDER_LIMITEDIMAGECACHE = 0x200 };

// ---- function pointer types ----
typedef void (PDFIUM_CALL *Fn_InitLibrary)();
typedef void (PDFIUM_CALL *Fn_InitLibraryWithConfig)(const FPDF_LIBRARY_CONFIG_T *);
typedef void (PDFIUM_CALL *Fn_DestroyLibrary)();
typedef FPDF_DOCUMENT (PDFIUM_CALL *Fn_LoadDocument)(const char *, const char *);
typedef FPDF_DOCUMENT (PDFIUM_CALL *Fn_LoadMemDocument)(const void *, int, const char *);
typedef void (PDFIUM_CALL *Fn_CloseDocument)(FPDF_DOCUMENT);
typedef int  (PDFIUM_CALL *Fn_GetPageCount)(FPDF_DOCUMENT);
typedef FPDF_PAGE (PDFIUM_CALL *Fn_LoadPage)(FPDF_DOCUMENT, int);
typedef void (PDFIUM_CALL *Fn_ClosePage)(FPDF_PAGE);
typedef float (PDFIUM_CALL *Fn_GetPageWidthF)(FPDF_PAGE);
typedef float (PDFIUM_CALL *Fn_GetPageHeightF)(FPDF_PAGE);
typedef double (PDFIUM_CALL *Fn_GetPageWidth)(FPDF_PAGE);
typedef double (PDFIUM_CALL *Fn_GetPageHeight)(FPDF_PAGE);
typedef FPDF_BITMAP (PDFIUM_CALL *Fn_BitmapCreateEx)(int, int, int, void *, int);
typedef void (PDFIUM_CALL *Fn_BitmapFillRect)(FPDF_BITMAP, int, int, int, int, unsigned long);
typedef void (PDFIUM_CALL *Fn_BitmapDestroy)(FPDF_BITMAP);
typedef void (PDFIUM_CALL *Fn_RenderPageBitmap)(FPDF_BITMAP, FPDF_PAGE,
                                                int, int, int, int, int, int);
typedef unsigned long (PDFIUM_CALL *Fn_GetLastError)();
typedef FPDF_TEXTPAGE (PDFIUM_CALL *Fn_TextLoadPage)(FPDF_PAGE);
typedef void (PDFIUM_CALL *Fn_TextClosePage)(FPDF_TEXTPAGE);
typedef int  (PDFIUM_CALL *Fn_TextCountChars)(FPDF_TEXTPAGE);
typedef int  (PDFIUM_CALL *Fn_TextGetText)(FPDF_TEXTPAGE, int, int, unsigned short *);
// Per-character geometry + code point, used for real text selection.
typedef int  (PDFIUM_CALL *Fn_TextGetCharBox)(FPDF_TEXTPAGE, int,
                                              double *, double *, double *, double *);
typedef unsigned int (PDFIUM_CALL *Fn_TextGetUnicode)(FPDF_TEXTPAGE, int);

// ---------------------------------------------------------------------------
// The loader: resolves pdfium.dll once, lazily, and thread-safely.
// ---------------------------------------------------------------------------
struct PdfiumApi
{
    bool ok = false;
    QString path;
    QStringList log;

    Fn_InitLibrary            InitLibrary = nullptr;
    Fn_InitLibraryWithConfig  InitLibraryWithConfig = nullptr;
    Fn_DestroyLibrary         DestroyLibrary = nullptr;
    Fn_LoadDocument           LoadDocument = nullptr;
    Fn_LoadMemDocument        LoadMemDocument = nullptr;
    Fn_CloseDocument          CloseDocument = nullptr;
    Fn_GetPageCount           GetPageCount = nullptr;
    Fn_LoadPage               LoadPage = nullptr;
    Fn_ClosePage              ClosePage = nullptr;
    Fn_GetPageWidthF          GetPageWidthF = nullptr;
    Fn_GetPageHeightF         GetPageHeightF = nullptr;
    Fn_GetPageWidth           GetPageWidthD = nullptr;
    Fn_GetPageHeight          GetPageHeightD = nullptr;
    Fn_BitmapCreateEx         BitmapCreateEx = nullptr;
    Fn_BitmapFillRect         BitmapFillRect = nullptr;
    Fn_BitmapDestroy          BitmapDestroy = nullptr;
    Fn_RenderPageBitmap       RenderPageBitmap = nullptr;
    Fn_GetLastError           GetLastErrorFn = nullptr;
    Fn_TextLoadPage           TextLoadPage = nullptr;
    Fn_TextClosePage          TextClosePage = nullptr;
    Fn_TextCountChars         TextCountChars = nullptr;
    Fn_TextGetText            TextGetText = nullptr;
    Fn_TextGetCharBox         TextGetCharBox = nullptr;
    Fn_TextGetUnicode         TextGetUnicode = nullptr;
};

// Candidate locations for pdfium.dll, in priority order.
QStringList candidatePaths()
{
    QStringList c;
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    // 1) explicit override
    const QString override = env.value(QStringLiteral("TERMINALPP_PDFIUM"));
    if (!override.isEmpty())
        c << override;

    // 2) a copy sitting directly beside the executable
    const QString appDir = QCoreApplication::applicationDirPath();
    if (!appDir.isEmpty())
        c << appDir + QStringLiteral("/pdfium.dll");

    // 4) well-known copies shipped by other installed products. Many Windows
    //    machines already have a current PDFium from one of these, which means
    //    real rendering works out of the box with nothing to download.
    const QString pf   = env.value(QStringLiteral("ProgramFiles"),
                                   QStringLiteral("C:/Program Files"));
    const QString pf86 = env.value(QStringLiteral("ProgramFiles(x86)"),
                                   QStringLiteral("C:/Program Files (x86)"));
    const QString local = env.value(QStringLiteral("LOCALAPPDATA"));
    for (const QString &base : { pf, pf86 }) {
        if (base.isEmpty()) continue;
        c << base + QStringLiteral("/Netskope/EPDLP/win/pdfium.dll");
        c << base + QStringLiteral("/Netskope/EPDLP Deployment/x64/pdfium.dll");
        c << base + QStringLiteral("/Windows Defender Advanced Threat Protection"
                                   "/Classification/Dprt/pdfium.dll");
        c << base + QStringLiteral("/PDFium/pdfium.dll");
    }
    if (!local.isEmpty())
        c << local + QStringLiteral("/PDFium/pdfium.dll");

    // 5) let the OS search its normal DLL path
    c << QStringLiteral("pdfium.dll");

    // 6) LAST RESORT: the copy we ship ourselves, in a "pdfium" sub-directory
    //    next to the executable. Everything above is tried first so the
    //    machine's own (usually newer) PDFium wins, but this guarantees the
    //    viewer still renders on a machine with no PDFium installed at all.
    if (!appDir.isEmpty())
        c << appDir + QStringLiteral("/pdfium/pdfium.dll");
    return c;
}

PdfiumApi *loadApi()
{
    static PdfiumApi api;
    static bool tried = false;
    static QMutex mutex;
    QMutexLocker lock(&mutex);
    if (tried)
        return &api;
    tried = true;

#ifdef Q_OS_WIN
    for (const QString &cand : candidatePaths()) {
        // "pdfium.dll" (no separator) is the OS-search fallback; every other
        // candidate is an explicit path that must exist.
        const bool bare = !cand.contains('/') && !cand.contains('\\');
        if (!bare && !QFile::exists(cand)) {
            api.log << QStringLiteral("not found: %1").arg(cand);
            continue;
        }
        const QString native = QDir::toNativeSeparators(cand);
        HMODULE h = LoadLibraryW(reinterpret_cast<const wchar_t *>(
            native.utf16()));
        if (!h) {
            api.log << QStringLiteral("load failed (err %1): %2")
                           .arg(GetLastError()).arg(native);
            continue;
        }
        auto sym = [h](const char *name) -> void * {
            return reinterpret_cast<void *>(GetProcAddress(h, name));
        };

        api.InitLibrary           = (Fn_InitLibrary)           sym("FPDF_InitLibrary");
        api.InitLibraryWithConfig = (Fn_InitLibraryWithConfig) sym("FPDF_InitLibraryWithConfig");
        api.DestroyLibrary        = (Fn_DestroyLibrary)        sym("FPDF_DestroyLibrary");
        api.LoadDocument          = (Fn_LoadDocument)          sym("FPDF_LoadDocument");
        api.LoadMemDocument       = (Fn_LoadMemDocument)       sym("FPDF_LoadMemDocument");
        api.CloseDocument         = (Fn_CloseDocument)         sym("FPDF_CloseDocument");
        api.GetPageCount          = (Fn_GetPageCount)          sym("FPDF_GetPageCount");
        api.LoadPage              = (Fn_LoadPage)              sym("FPDF_LoadPage");
        api.ClosePage             = (Fn_ClosePage)             sym("FPDF_ClosePage");
        api.GetPageWidthF         = (Fn_GetPageWidthF)         sym("FPDF_GetPageWidthF");
        api.GetPageHeightF        = (Fn_GetPageHeightF)        sym("FPDF_GetPageHeightF");
        api.GetPageWidthD         = (Fn_GetPageWidth)          sym("FPDF_GetPageWidth");
        api.GetPageHeightD        = (Fn_GetPageHeight)         sym("FPDF_GetPageHeight");
        api.BitmapCreateEx        = (Fn_BitmapCreateEx)        sym("FPDFBitmap_CreateEx");
        api.BitmapFillRect        = (Fn_BitmapFillRect)        sym("FPDFBitmap_FillRect");
        api.BitmapDestroy         = (Fn_BitmapDestroy)         sym("FPDFBitmap_Destroy");
        api.RenderPageBitmap      = (Fn_RenderPageBitmap)      sym("FPDF_RenderPageBitmap");
        api.GetLastErrorFn        = (Fn_GetLastError)          sym("FPDF_GetLastError");
        api.TextLoadPage          = (Fn_TextLoadPage)          sym("FPDFText_LoadPage");
        api.TextClosePage         = (Fn_TextClosePage)         sym("FPDFText_ClosePage");
        api.TextCountChars        = (Fn_TextCountChars)        sym("FPDFText_CountChars");
        api.TextGetText           = (Fn_TextGetText)           sym("FPDFText_GetText");
        api.TextGetCharBox        = (Fn_TextGetCharBox)        sym("FPDFText_GetCharBox");
        api.TextGetUnicode        = (Fn_TextGetUnicode)        sym("FPDFText_GetUnicode");

        // The minimum set we cannot work without.
        const bool haveCore =
            (api.InitLibrary || api.InitLibraryWithConfig) &&
            api.LoadDocument && api.LoadMemDocument && api.CloseDocument &&
            api.GetPageCount && api.LoadPage && api.ClosePage &&
            api.BitmapCreateEx && api.BitmapFillRect && api.BitmapDestroy &&
            api.RenderPageBitmap &&
            (api.GetPageWidthF || api.GetPageWidthD) &&
            (api.GetPageHeightF || api.GetPageHeightD);
        if (!haveCore) {
            api.log << QStringLiteral("missing required exports: %1").arg(native);
            FreeLibrary(h);
            continue;
        }

        // Initialise the library exactly once.
        if (api.InitLibraryWithConfig) {
            FPDF_LIBRARY_CONFIG_T cfg;
            memset(&cfg, 0, sizeof(cfg));
            cfg.version = 2;
            api.InitLibraryWithConfig(&cfg);
        } else {
            api.InitLibrary();
        }
        api.ok = true;
        api.path = QDir::fromNativeSeparators(native);
        api.log << QStringLiteral("loaded: %1").arg(native);
        break;
    }
#else
    api.log << QStringLiteral("PDFium dynamic loading is implemented for "
                              "Windows only in this build.");
#endif
    if (!api.ok)
        api.log << QStringLiteral("PDFium unavailable — falling back to the "
                                  "built-in PDF engine.");
    return &api;
}

// PDFium is not guaranteed thread-safe across documents; the viewer only ever
// renders from the GUI thread, but guard anyway so a future background
// pre-render cannot corrupt state.
QMutex *renderMutex()
{
    static QMutex m;
    return &m;
}

} // namespace

// ===========================================================================
// PdfiumRenderer
// ===========================================================================
PdfiumRenderer::PdfiumRenderer() = default;

PdfiumRenderer::~PdfiumRenderer()
{
    close();
}

bool PdfiumRenderer::isAvailable()
{
    return loadApi()->ok;
}

QString PdfiumRenderer::libraryPath()
{
    return loadApi()->path;
}

QStringList PdfiumRenderer::searchLog()
{
    return loadApi()->log;
}

void PdfiumRenderer::close()
{
    PdfiumApi *api = loadApi();
    if (m_doc && api->ok && api->CloseDocument) {
        QMutexLocker lock(renderMutex());
        api->CloseDocument(m_doc);
    }
    m_doc = nullptr;
    m_pageCount = 0;
    m_data.clear();
}

// Translate FPDF_GetLastError() into something a user can act on.
static QString pdfiumErrorText(unsigned long code)
{
    switch (code) {
    case 0: return QStringLiteral("no error");
    case 1: return QStringLiteral("unknown error");
    case 2: return QStringLiteral("file not found or could not be opened");
    case 3: return QStringLiteral("not a PDF file or corrupted");
    case 4: return QStringLiteral("password required or incorrect password");
    case 5: return QStringLiteral("unsupported security scheme");
    case 6: return QStringLiteral("page not found or content error");
    default: return QStringLiteral("error code %1").arg(code);
    }
}

bool PdfiumRenderer::open(const QString &path, const QString &password)
{
    close();
    PdfiumApi *api = loadApi();
    m_error.clear();
    m_needsPassword = false;
    if (!api->ok) {
        m_error = QStringLiteral("PDFium is not available");
        return false;
    }

    // Read the file ourselves and use the memory loader: it sidesteps all
    // ANSI/UTF-8 path issues with FPDF_LoadDocument's char* parameter, which
    // would otherwise fail on Chinese or other non-ASCII file names.
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        m_error = QStringLiteral("cannot open file: %1").arg(f.errorString());
        return false;
    }
    const QByteArray bytes = f.readAll();
    f.close();
    if (bytes.isEmpty()) {
        m_error = QStringLiteral("file is empty");
        return false;
    }
    return openData(bytes, password);
}

bool PdfiumRenderer::openData(const QByteArray &bytes, const QString &password)
{
    PdfiumApi *api = loadApi();
    m_error.clear();
    m_needsPassword = false;
    if (!api->ok) {
        m_error = QStringLiteral("PDFium is not available");
        return false;
    }
    // Drop any previous document but keep our new bytes alive: PDFium does
    // NOT copy the buffer handed to FPDF_LoadMemDocument.
    if (m_doc && api->CloseDocument) {
        QMutexLocker lock(renderMutex());
        api->CloseDocument(m_doc);
        m_doc = nullptr;
    }
    m_data = bytes;
    m_pageCount = 0;

    const QByteArray pw = password.toUtf8();
    {
        QMutexLocker lock(renderMutex());
        m_doc = api->LoadMemDocument(m_data.constData(), m_data.size(),
                                     password.isEmpty() ? nullptr : pw.constData());
        if (!m_doc) {
            const unsigned long code = api->GetLastErrorFn ? api->GetLastErrorFn() : 1;
            m_needsPassword = (code == 4);
            m_error = pdfiumErrorText(code);
            m_data.clear();
            return false;
        }
        m_pageCount = api->GetPageCount(m_doc);
    }
    if (m_pageCount <= 0) {
        m_error = QStringLiteral("the document reports zero pages");
        close();
        return false;
    }
    return true;
}

QSizeF PdfiumRenderer::pageSizePts(int index) const
{
    PdfiumApi *api = loadApi();
    if (!api->ok || !m_doc || index < 0 || index >= m_pageCount)
        return QSizeF();
    QMutexLocker lock(renderMutex());
    FPDF_PAGE page = api->LoadPage(m_doc, index);
    if (!page)
        return QSizeF();
    double w, h;
    if (api->GetPageWidthF && api->GetPageHeightF) {
        w = api->GetPageWidthF(page);
        h = api->GetPageHeightF(page);
    } else {
        w = api->GetPageWidthD(page);
        h = api->GetPageHeightD(page);
    }
    api->ClosePage(page);
    return QSizeF(w, h);
}

bool PdfiumRenderer::isLandscape(int index) const
{
    const QSizeF s = pageSizePts(index);
    return s.isValid() && s.width() > s.height();
}

QImage PdfiumRenderer::renderPage(int index, double dpi) const
{
    const QSizeF pts = pageSizePts(index);
    if (!pts.isValid() || pts.isEmpty())
        return QImage();
    const double scale = (dpi > 0 ? dpi : 150.0) / 72.0;
    const int w = qMax(1, int(pts.width()  * scale + 0.5));
    const int h = qMax(1, int(pts.height() * scale + 0.5));
    return renderInternal(index, w, h);
}

QImage PdfiumRenderer::renderPageToFit(int index, const QSize &maxSize) const
{
    const QSizeF pts = pageSizePts(index);
    if (!pts.isValid() || pts.isEmpty() || maxSize.isEmpty())
        return QImage();
    // Preserve the page's own aspect ratio — this is what makes a portrait
    // page stay portrait and a landscape page stay landscape, at whatever
    // size the viewport allows.
    const QSizeF fitted = pts.scaled(QSizeF(maxSize), Qt::KeepAspectRatio);
    const int w = qMax(1, int(fitted.width()  + 0.5));
    const int h = qMax(1, int(fitted.height() + 0.5));
    return renderInternal(index, w, h);
}

QImage PdfiumRenderer::renderInternal(int index, int pxW, int pxH) const
{
    PdfiumApi *api = loadApi();
    if (!api->ok || !m_doc || index < 0 || index >= m_pageCount)
        return QImage();
    if (pxW <= 0 || pxH <= 0)
        return QImage();
    // Guard against absurd allocations (e.g. a huge zoom on a big page).
    const qint64 bytesNeeded = qint64(pxW) * pxH * 4;
    if (bytesNeeded > (qint64(512) << 20))      // 512 MiB ceiling
        return QImage();

    // ARGB32 matches PDFium's BGRA buffer layout on little-endian Windows,
    // so we can render straight into the QImage's own scanlines.
    QImage img(pxW, pxH, QImage::Format_ARGB32);
    if (img.isNull())
        return QImage();
    img.fill(Qt::white);

    QMutexLocker lock(renderMutex());
    FPDF_PAGE page = api->LoadPage(m_doc, index);
    if (!page)
        return QImage();

    FPDF_BITMAP bmp = api->BitmapCreateEx(pxW, pxH, FPDFBitmap_BGRA,
                                          img.bits(), int(img.bytesPerLine()));
    if (!bmp) {
        api->ClosePage(page);
        return QImage();
    }
    // White page background, then draw annotations too (so highlights and
    // form fields appear exactly as in other viewers).
    api->BitmapFillRect(bmp, 0, 0, pxW, pxH, 0xFFFFFFFF);
    api->RenderPageBitmap(bmp, page, 0, 0, pxW, pxH, 0,
                          FPDF_ANNOT | FPDF_LCD_TEXT);
    api->BitmapDestroy(bmp);
    api->ClosePage(page);

    // PDFium leaves alpha at 0xFF for opaque pixels; make sure the image is
    // treated as fully opaque regardless.
    img.reinterpretAsFormat(QImage::Format_RGB32);
    return img;
}

QString PdfiumRenderer::pageText(int index) const
{
    PdfiumApi *api = loadApi();
    if (!api->ok || !m_doc || index < 0 || index >= m_pageCount)
        return QString();
    if (!api->TextLoadPage || !api->TextCountChars || !api->TextGetText)
        return QString();

    QMutexLocker lock(renderMutex());
    FPDF_PAGE page = api->LoadPage(m_doc, index);
    if (!page)
        return QString();
    FPDF_TEXTPAGE tp = api->TextLoadPage(page);
    if (!tp) {
        api->ClosePage(page);
        return QString();
    }
    QString out;
    const int n = api->TextCountChars(tp);
    if (n > 0) {
        // FPDFText_GetText writes UTF-16LE plus a NUL terminator.
        QVector<unsigned short> buf(n + 1, 0);
        const int got = api->TextGetText(tp, 0, n, buf.data());
        if (got > 0) {
            out = QString::fromUtf16(reinterpret_cast<const char16_t *>(buf.constData()),
                                     qMin(got, n));
        }
    }
    if (api->TextClosePage)
        api->TextClosePage(tp);
    api->ClosePage(page);
    return out;
}

// ---------------------------------------------------------------------------
// Real text selection: every character of the page's text layer with its box.
//
// PDFium reports character boxes in *page* coordinates — points with the
// origin at the bottom-left corner and y growing upwards. The page view paints
// with a top-left origin and y growing downwards, so the boxes are flipped
// here once, which keeps all the selection maths in the view trivial.
// ---------------------------------------------------------------------------
QVector<PdfTextChar> PdfiumRenderer::pageChars(int index) const
{
    QVector<PdfTextChar> out;
    PdfiumApi *api = loadApi();
    if (!api->ok || !m_doc || index < 0 || index >= m_pageCount)
        return out;
    if (!api->TextLoadPage || !api->TextCountChars)
        return out;
    // Without per-character geometry there is nothing to select.
    if (!api->TextGetCharBox)
        return out;

    QMutexLocker lock(renderMutex());
    FPDF_PAGE page = api->LoadPage(m_doc, index);
    if (!page)
        return out;

    double pageH = 0.0;
    if (api->GetPageHeightF)      pageH = api->GetPageHeightF(page);
    else if (api->GetPageHeightD) pageH = api->GetPageHeightD(page);

    FPDF_TEXTPAGE tp = api->TextLoadPage(page);
    if (!tp) {
        api->ClosePage(page);
        return out;
    }

    const int n = api->TextCountChars(tp);
    if (n > 0) {
        // Fetch the whole string in one call: FPDFText_GetUnicode may be
        // missing from older DLLs, and the bulk call is also faster.
        QString text;
        if (api->TextGetText) {
            QVector<unsigned short> buf(n + 1, 0);
            const int got = api->TextGetText(tp, 0, n, buf.data());
            if (got > 0)
                text = QString::fromUtf16(
                    reinterpret_cast<const char16_t *>(buf.constData()),
                    qMin(got, n));
        }

        out.reserve(n);
        for (int i = 0; i < n; ++i) {
            PdfTextChar c;

            // Code point: prefer the bulk string (indices line up 1:1 with
            // the character indices), fall back to the per-char accessor.
            if (i < text.size())
                c.ch = text.at(i).unicode();
            else if (api->TextGetUnicode)
                c.ch = api->TextGetUnicode(tp, i);

            double left = 0, right = 0, bottom = 0, top = 0;
            if (api->TextGetCharBox(tp, i, &left, &right, &bottom, &top)) {
                // Flip to a top-left origin. PDFium's "top" is the larger y.
                const double yTop = pageH - qMax(top, bottom);
                const double h = qAbs(top - bottom);
                c.box = QRectF(qMin(left, right), yTop,
                               qAbs(right - left), h);
            }
            out.append(c);
        }
    }

    if (api->TextClosePage)
        api->TextClosePage(tp);
    api->ClosePage(page);
    return out;
}

bool PdfiumRenderer::pageHasText(int index) const
{
    PdfiumApi *api = loadApi();
    if (!api->ok || !m_doc || index < 0 || index >= m_pageCount)
        return false;
    if (!api->TextLoadPage || !api->TextCountChars)
        return false;

    QMutexLocker lock(renderMutex());
    FPDF_PAGE page = api->LoadPage(m_doc, index);
    if (!page)
        return false;
    FPDF_TEXTPAGE tp = api->TextLoadPage(page);
    if (!tp) {
        api->ClosePage(page);
        return false;
    }
    const int n = api->TextCountChars(tp);
    if (api->TextClosePage)
        api->TextClosePage(tp);
    api->ClosePage(page);
    return n > 0;
}
