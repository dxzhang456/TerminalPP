#ifndef PDFCORE_H
#define PDFCORE_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QVariant>

class QImage;

// ---------------------------------------------------------------------------
// PdfCore — a lightweight, self-contained PDF reader/writer.
//
// This is NOT a full renderer (no content-stream rasterizer). It parses the
// PDF object graph (xref + trailer + indirect objects) well enough to perform
// *structural* operations that do not require drawing:
//
//   * enumerate pages, their size (MediaBox) and rotation
//   * merge several PDFs, split, delete / insert / extract / reorder pages
//   * rotate pages
//   * add a text or image watermark (as an extra content stream + XObject)
//   * encrypt with an open ("user") password (RC4-128, standard security)
//   * build a brand-new PDF from a list of images (image -> PDF)
//   * extract a full-page image when a page is a single image XObject
//     (covers the common "scanned document" case for PDF -> image)
//   * append annotation objects (highlight / text box / ink / stamp / signature)
//   * fill simple AcroForm text fields
//
// Everything is emitted as an incremental-friendly, freshly serialized PDF so
// the output is always a clean, valid file regardless of the input layout
// (even if the input used cross-reference streams / object streams we only
// need to *read* it; we always *write* classic xref tables).
// ---------------------------------------------------------------------------

// A parsed PDF object. `type` describes the active union member.
struct PdfObject
{
    enum Type { Null, Bool, Int, Real, Str, Name, Array, Dict, Stream, Ref };
    Type type = Null;

    bool          boolVal = false;
    double        numVal = 0;         // Int + Real
    QByteArray    strVal;             // Str (raw bytes) / Name (without '/')
    QList<PdfObject> arr;             // Array
    QMap<QByteArray, PdfObject> dict; // Dict / Stream dictionary
    QByteArray    stream;             // Stream raw (already decoded? no: raw)
    int           refNum = 0;         // Ref object number
    int           refGen = 0;         // Ref generation

    bool isRef() const { return type == Ref; }
    bool isDict() const { return type == Dict || type == Stream; }
    bool isStream() const { return type == Stream; }
    bool isArray() const { return type == Array; }
    bool isName() const { return type == Name; }
    bool isNumber() const { return type == Int || type == Real; }

    int  toInt() const { return static_cast<int>(numVal); }
    double toReal() const { return numVal; }

    static PdfObject makeInt(int v) { PdfObject o; o.type = Int; o.numVal = v; return o; }
    static PdfObject makeReal(double v) { PdfObject o; o.type = Real; o.numVal = v; return o; }
    static PdfObject makeName(const QByteArray &n) { PdfObject o; o.type = Name; o.strVal = n; return o; }
    static PdfObject makeStr(const QByteArray &s) { PdfObject o; o.type = Str; o.strVal = s; return o; }
    static PdfObject makeRef(int n, int g = 0) { PdfObject o; o.type = Ref; o.refNum = n; o.refGen = g; return o; }
    static PdfObject makeBool(bool b) { PdfObject o; o.type = Bool; o.boolVal = b; return o; }
    static PdfObject makeNull() { PdfObject o; o.type = Null; return o; }
    static PdfObject makeArray() { PdfObject o; o.type = Array; return o; }
    static PdfObject makeDict() { PdfObject o; o.type = Dict; return o; }
};

struct PdfPageInfo
{
    int objNum = 0;                 // indirect object number of the /Page
    double width = 612, height = 792;
    int rotation = 0;               // 0/90/180/270
};

// A simple in-memory PDF document model built by parsing. Object numbers are
// 1-based; index 0 is unused (the free list head in PDF).
class PdfDocument
{
public:
    PdfDocument() = default;

    // Load/parse from a file or raw bytes. Returns false on failure.
    bool load(const QString &path);
    bool loadData(const QByteArray &data);

    bool isValid() const { return m_valid; }
    QString error() const { return m_error; }

    // Non-fatal notes collected while parsing (unsupported filters, broken
    // object streams, ...). Useful to explain *why* a page renders blank
    // instead of silently showing an empty sheet.
    QStringList warnings() const { return m_warnings; }

    // A one-line human-readable summary of what the parser found — shown in the
    // status bar when a document opens, so problems are diagnosable.
    QString diagnostics() const;

    // Was the source encrypted? (We can read some encrypted docs but writing
    // always produces an un-encrypted file unless encrypt() is called.)
    bool wasEncrypted() const { return m_wasEncrypted; }

    // Page access.
    int pageCount() const { return m_pages.size(); }
    const QVector<PdfPageInfo> &pages() const { return m_pages; }
    PdfPageInfo pageInfo(int index) const;

    // Resolve a possibly-indirect object to its concrete value.
    PdfObject resolve(const PdfObject &o) const;
    PdfObject object(int num) const;      // get object by number (resolved form)
    bool hasObject(int num) const { return m_objects.contains(num); }

    // Fully-decoded content bytes for a page (all its content streams,
    // FlateDecode-inflated and concatenated). Empty if none / undecodable.
    QByteArray pageContent(int index) const;

    // If the page's only meaningful graphic is a single image XObject, return
    // that image decoded as PNG/JPEG bytes (+ out format). Used for PDF->image
    // of scanned documents. Returns empty if the page is not a single image.
    QByteArray pageSingleImage(int index, QByteArray &outFmt) const;

    // --- Mutating / building operations (all return a NEW serialized PDF) ---
    // These operate on copies so the loaded document is never corrupted.

    // Serialize the current model to a valid PDF byte stream.
    QByteArray serialize() const;

    // Flate-compress every stream that is still stored uncompressed (no
    // /Filter), which is what actually shrinks a PDF — our writer otherwise
    // re-emits such streams verbatim. Returns the number of streams that were
    // compressed. Lossless: only the encoding changes.
    int compressStreams();

    // Build helpers return brand-new PdfDocument models.
    static PdfDocument fromImages(const QList<QByteArray> &imageFiles,
                                  QString *err = nullptr);

    // Produce a new document that is a subset / reordering of pages (0-based
    // indices into *this* document, in the desired output order).
    PdfDocument extractPages(const QList<int> &indices) const;

    // Merge: append all pages of `other` after this document's pages.
    void appendDocument(const PdfDocument &other);

    // Rotate a page by +90/-90/180 (added to existing rotation, normalized).
    void rotatePage(int index, int deltaDegrees);

    // Move a page from `from` to `before` index (reorder).
    void movePage(int from, int to);

    // Delete a page.
    void deletePage(int index);

    // Insert all pages of `other` before `index` (index==pageCount -> append).
    void insertDocument(int index, const PdfDocument &other);

    // Add a diagonal text watermark to every page (or a single page if
    // pageIndex >= 0). Uses Helvetica, gray, 45°.
    void addTextWatermark(const QString &text, int pageIndex = -1,
                          double fontSize = 48, double opacity = 0.25);

    // Add an image watermark (centered, scaled to `scale` of page width) to
    // every page (or a single page). `imageData` is PNG/JPEG file bytes.
    void addImageWatermark(const QByteArray &imageData, const QByteArray &fmt,
                           int pageIndex = -1, double scale = 0.4,
                           double opacity = 0.3);

    // Stamp an arbitrary image at page coordinates (points, origin bottom-left)
    // on one page — used for image signatures and stamps. If the image carries
    // an alpha channel (PNG transparency, Qt QImage with ARGB), an SMask is
    // written so the background shows through instead of being filled black.
    void stampImage(int pageIndex, const QByteArray &imageData,
                    const QByteArray &fmt, double x, double y,
                    double w, double h, double opacity = 1.0);

    // Same, but takes a QImage directly — used by the annotation layer to bake
    // rendered text / stickies / stamps that need an alpha channel to keep the
    // page content visible around them.
    void stampQImage(int pageIndex, const QImage &img, double x, double y,
                     double w, double h, double opacity = 1.0,
                     double rotationDeg = 0.0);

    // Append raw marking content (already a valid content-stream fragment in
    // page coordinate space) to a page — used by the annotation layer to bake
    // highlights / ink / shapes / text drawn in the viewer.
    void appendPageContent(int pageIndex, const QByteArray &contentFragment,
                           const QMap<QByteArray, PdfObject> &extraResources
                               = QMap<QByteArray, PdfObject>());

    // Encrypt the output with an open (user) password using RC4-128 standard
    // security. After this, serialize() emits an encrypted PDF.
    void encrypt(const QString &userPassword, const QString &ownerPassword = QString());

    // Fill a simple AcroForm text field by its /T name with `value`.
    bool fillFormField(const QString &fieldName, const QString &value);
    // All text field names found in the AcroForm (for the fill UI).
    QStringList formTextFields() const;

    // Extract the visible text of a page (best-effort, from the content
    // stream's Tj/TJ/' /" text operators). Layout is NOT preserved — this is
    // the "PDF -> Word (with layout loss)" text extraction path.
    QString pageText(int index) const;
    // All pages' text joined with blank lines between pages.
    QString allText() const;

private:
    // --- Parsing ---
    bool parse();
    void parseXrefAndTrailer();
    void collectObjectsBruteForce();     // fallback scan "N G obj"
    // PDF 1.5+ stores most structural objects (including /Page dictionaries)
    // *inside* compressed /Type /ObjStm streams, which the brute-force scan
    // above cannot see. This walks every object stream found and hoists the
    // objects it contains into m_objects. Without this step such documents
    // parse to "no pages found" — the single biggest cause of PDFs that
    // "cannot be opened" by the built-in reader.
    void expandObjectStreams();
    void buildPageList();
    PdfObject parseIndirectObjectAt(qint64 offset) const;

    // Decode a stream's bytes applying its /Filter chain. Supported:
    // FlateDecode, LZWDecode, RunLengthDecode, ASCIIHexDecode, ASCII85Decode,
    // plus /DecodeParms PNG (predictors 10-15) and TIFF (predictor 2)
    // un-prediction. Image-only codecs (DCTDecode / JPXDecode / CCITTFaxDecode
    // / JBIG2Decode) are passed through so pageSingleImage() can hand the raw
    // codestream to Qt's image plugins.
    QByteArray decodeStream(const PdfObject &streamObj) const;

    // Allocate a new object number and store it.
    int addObject(const PdfObject &o);

    // Shared image-stamp helper used by addImageWatermark() to place a centered
    // scaled image on one or all pages.
    void stampImageInternalForAll(const QByteArray &imageData, const QByteArray &fmt,
                                  int pageIndex, double scale, double opacity,
                                  bool centered);

    // --- Members ---
    QByteArray m_data;                   // the source file bytes
    QHash<int, PdfObject> m_objects;     // objNum -> parsed object
    QHash<int, qint64> m_offsets;        // objNum -> byte offset (lazy parse)
    QVector<PdfPageInfo> m_pages;
    PdfObject m_trailer;
    int m_maxObjNum = 0;

    bool m_valid = false;
    bool m_wasEncrypted = false;
    QString m_error;
    QStringList m_warnings;
    int m_objStmExpanded = 0;      // objects hoisted out of /ObjStm streams

    // Encryption request (applied during serialize()).
    bool m_encryptRequested = false;
    QString m_userPassword;
    QString m_ownerPassword;

    // When serializing, the Root/Pages are rebuilt from m_pages + m_objects.
    friend struct PdfWriter;
};

#endif // PDFCORE_H