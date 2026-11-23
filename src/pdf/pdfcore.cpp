#include "pdfcore.h"

#include <QBuffer>
#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QRegularExpression>
#include <QtEndian>
#include <QtGlobal>
#include <functional>
#include <zlib.h>

// ===========================================================================
// Low-level byte helpers
// ===========================================================================
namespace {

inline bool isPdfWhitespace(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\0';
}
inline bool isPdfDelimiter(char c)
{
    return c == '(' || c == ')' || c == '<' || c == '>' || c == '[' || c == ']'
           || c == '{' || c == '}' || c == '/' || c == '%';
}

// Inflate a zlib/raw-deflate stream (PDF FlateDecode uses the zlib header).
QByteArray zlibInflate(const QByteArray &in)
{
    if (in.isEmpty())
        return {};
    z_stream s;
    memset(&s, 0, sizeof(s));
    if (inflateInit(&s) != Z_OK)
        return {};
    s.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(in.constData()));
    s.avail_in = static_cast<uInt>(in.size());
    QByteArray out;
    char buf[65536];
    int ret = Z_OK;
    do {
        s.next_out = reinterpret_cast<Bytef *>(buf);
        s.avail_out = sizeof(buf);
        ret = inflate(&s, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            // Retry as raw deflate (some PDFs are malformed).
            inflateEnd(&s);
            memset(&s, 0, sizeof(s));
            if (inflateInit2(&s, -15) != Z_OK)
                return out;
            s.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(in.constData()));
            s.avail_in = static_cast<uInt>(in.size());
            out.clear();
            do {
                s.next_out = reinterpret_cast<Bytef *>(buf);
                s.avail_out = sizeof(buf);
                ret = inflate(&s, Z_NO_FLUSH);
                if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR)
                    break;
                out.append(buf, sizeof(buf) - s.avail_out);
            } while (ret != Z_STREAM_END && s.avail_in > 0);
            break;
        }
        out.append(buf, sizeof(buf) - s.avail_out);
    } while (ret != Z_STREAM_END && s.avail_in > 0);
    inflateEnd(&s);
    return out;
}

// Deflate (zlib) a buffer for writing FlateDecode streams.
QByteArray zlibDeflate(const QByteArray &in)
{
    z_stream s;
    memset(&s, 0, sizeof(s));
    if (deflateInit(&s, Z_BEST_COMPRESSION) != Z_OK)
        return in;
    s.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(in.constData()));
    s.avail_in = static_cast<uInt>(in.size());
    QByteArray out;
    char buf[65536];
    int ret;
    do {
        s.next_out = reinterpret_cast<Bytef *>(buf);
        s.avail_out = sizeof(buf);
        ret = deflate(&s, Z_FINISH);
        out.append(buf, sizeof(buf) - s.avail_out);
    } while (ret != Z_STREAM_END);
    deflateEnd(&s);
    return out;
}

// ASCII85 decode (for completeness).
QByteArray ascii85Decode(const QByteArray &in)
{
    QByteArray out;
    quint32 tuple = 0;
    int count = 0;
    for (int i = 0; i < in.size(); ++i) {
        char c = in[i];
        if (c == '~') break;
        if (isPdfWhitespace(c)) continue;
        if (c == 'z' && count == 0) { out.append(4, '\0'); continue; }
        if (c < '!' || c > 'u') continue;
        tuple = tuple * 85 + (c - '!');
        if (++count == 5) {
            for (int k = 3; k >= 0; --k) out.append(char((tuple >> (k * 8)) & 0xff));
            tuple = 0; count = 0;
        }
    }
    if (count > 0) {
        for (int k = count; k < 5; ++k) tuple = tuple * 85 + 84;
        for (int k = 3; k >= 4 - (count - 1); --k) out.append(char((tuple >> (k * 8)) & 0xff));
    }
    return out;
}

QByteArray asciiHexDecode(const QByteArray &in)
{
    QByteArray hex;
    for (char c : in) {
        if (c == '>') break;
        if (!isPdfWhitespace(c)) hex.append(c);
    }
    if (hex.size() % 2) hex.append('0');
    return QByteArray::fromHex(hex);
}

// --- RunLengthDecode (PDF 7.4.5) -------------------------------------------
// Length byte L: 0-127 -> copy the next L+1 bytes literally;
//                129-255 -> repeat the next single byte 257-L times;
//                128 -> EOD.
QByteArray runLengthDecode(const QByteArray &in)
{
    QByteArray out;
    int i = 0;
    const int n = in.size();
    while (i < n) {
        const int len = static_cast<unsigned char>(in[i++]);
        if (len == 128)
            break;                                  // EOD
        if (len < 128) {
            const int count = qMin(len + 1, n - i);
            if (count <= 0) break;
            out.append(in.constData() + i, count);
            i += count;
        } else {
            if (i >= n) break;
            out.append(257 - len, in[i]);
            ++i;
        }
    }
    return out;
}

// --- LZWDecode (PDF 7.4.4) -------------------------------------------------
// The variable-code-width LZW variant used by PDF/TIFF: codes start at 9 bits
// and grow to 12; 256 = ClearTable, 257 = EOD. `earlyChange` (default 1) makes
// the width grow one code early, as PDF specifies.
QByteArray lzwDecode(const QByteArray &in, int earlyChange = 1)
{
    QByteArray out;
    QVector<QByteArray> table;
    table.reserve(4096);
    auto resetTable = [&table]() {
        table.clear();
        for (int i = 0; i < 256; ++i)
            table.append(QByteArray(1, char(i)));
        table.append(QByteArray());     // 256 ClearTable
        table.append(QByteArray());     // 257 EOD
    };
    resetTable();

    int codeWidth = 9;
    quint32 bitBuf = 0;
    int bitCount = 0;
    int prev = -1;
    const int n = in.size();
    int i = 0;

    while (true) {
        while (bitCount < codeWidth && i < n) {
            bitBuf = (bitBuf << 8) | static_cast<unsigned char>(in[i++]);
            bitCount += 8;
        }
        if (bitCount < codeWidth)
            break;                                   // out of input
        const int code = int((bitBuf >> (bitCount - codeWidth))
                             & ((1u << codeWidth) - 1));
        bitCount -= codeWidth;

        if (code == 257)
            break;                                   // EOD
        if (code == 256) {
            resetTable();
            codeWidth = 9;
            prev = -1;
            continue;
        }

        QByteArray entry;
        if (code < table.size()) {
            entry = table[code];
        } else if (prev >= 0 && prev < table.size()) {
            // KwKwK case: the code is the one about to be added.
            entry = table[prev];
            if (entry.isEmpty()) break;
            entry.append(entry[0]);
        } else {
            break;                                   // corrupt stream
        }
        out.append(entry);

        if (prev >= 0 && prev < table.size() && table.size() < 4096) {
            QByteArray added = table[prev];
            if (!entry.isEmpty())
                added.append(entry[0]);
            table.append(added);
        }
        prev = code;

        const int limit = table.size() + earlyChange;
        if (limit >= 4096)      codeWidth = 12;
        else if (limit >= 2048) codeWidth = 12;
        else if (limit >= 1024) codeWidth = 11;
        else if (limit >= 512)  codeWidth = 10;
        else                    codeWidth = 9;
        if (codeWidth > 12) codeWidth = 12;
    }
    return out;
}

// --- Predictor reversal (PDF 7.4.4.4) --------------------------------------
// Predictor 1  : none.
// Predictor 2  : TIFF horizontal differencing.
// Predictor 10+: PNG filters, one leading filter-type byte per row.
QByteArray applyPredictor(const QByteArray &in, int predictor, int colors,
                          int bpc, int columns)
{
    if (predictor <= 1 || in.isEmpty())
        return in;
    colors  = qBound(1, colors, 32);
    bpc     = (bpc == 1 || bpc == 2 || bpc == 4 || bpc == 8 || bpc == 16) ? bpc : 8;
    columns = qMax(1, columns);

    const int bpp = qMax(1, (colors * bpc + 7) / 8);          // bytes per pixel
    const int rowLen = (columns * colors * bpc + 7) / 8;      // bytes per row

    if (predictor == 2) {
        // TIFF horizontal differencing — only the 8-bit case is well-defined
        // for byte-oriented data; sub-byte depths are left untouched.
        if (bpc != 8)
            return in;
        QByteArray out = in;
        char *p = out.data();
        const int rows = out.size() / rowLen;
        for (int r = 0; r < rows; ++r) {
            char *row = p + qsizetype(r) * rowLen;
            for (int k = bpp; k < rowLen; ++k)
                row[k] = char((static_cast<unsigned char>(row[k])
                               + static_cast<unsigned char>(row[k - bpp])) & 0xff);
        }
        return out;
    }

    // PNG predictors: each row is prefixed by one filter-type byte.
    QByteArray out;
    out.reserve(in.size());
    QByteArray prevRow(rowLen, '\0');
    QByteArray row(rowLen, '\0');
    int i = 0;
    const int n = in.size();
    while (i + 1 <= n) {
        const int ft = static_cast<unsigned char>(in[i++]);
        const int avail = qMin(rowLen, n - i);
        if (avail <= 0) break;
        memcpy(row.data(), in.constData() + i, avail);
        if (avail < rowLen)
            memset(row.data() + avail, 0, rowLen - avail);
        i += avail;

        unsigned char *cur = reinterpret_cast<unsigned char *>(row.data());
        const unsigned char *up =
            reinterpret_cast<const unsigned char *>(prevRow.constData());
        switch (ft) {
        case 0:                                        // None
            break;
        case 1:                                        // Sub
            for (int k = bpp; k < rowLen; ++k)
                cur[k] = (cur[k] + cur[k - bpp]) & 0xff;
            break;
        case 2:                                        // Up
            for (int k = 0; k < rowLen; ++k)
                cur[k] = (cur[k] + up[k]) & 0xff;
            break;
        case 3:                                        // Average
            for (int k = 0; k < rowLen; ++k) {
                const int left = (k >= bpp) ? cur[k - bpp] : 0;
                cur[k] = (cur[k] + ((left + up[k]) >> 1)) & 0xff;
            }
            break;
        case 4:                                        // Paeth
            for (int k = 0; k < rowLen; ++k) {
                const int a = (k >= bpp) ? cur[k - bpp] : 0;
                const int b = up[k];
                const int c = (k >= bpp) ? up[k - bpp] : 0;
                const int p = a + b - c;
                const int pa = qAbs(p - a), pb = qAbs(p - b), pc = qAbs(p - c);
                const int pred = (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
                cur[k] = (cur[k] + pred) & 0xff;
            }
            break;
        default:
            break;                                     // unknown: pass through
        }
        out.append(row);
        prevRow = row;
    }
    return out;
}

} // namespace

// ===========================================================================
// A recursive-descent PDF object parser operating on the whole file buffer.
// ===========================================================================
struct PdfLexer
{
    const QByteArray &data;
    qint64 pos;
    explicit PdfLexer(const QByteArray &d, qint64 p = 0) : data(d), pos(p) {}

    void skipWs()
    {
        while (pos < data.size()) {
            char c = data[pos];
            if (c == '%') {              // comment to EOL
                while (pos < data.size() && data[pos] != '\n' && data[pos] != '\r') ++pos;
            } else if (isPdfWhitespace(c)) {
                ++pos;
            } else {
                break;
            }
        }
    }

    bool match(const char *kw)
    {
        skipWs();
        int len = int(strlen(kw));
        if (pos + len <= data.size() && memcmp(data.constData() + pos, kw, len) == 0) {
            pos += len;
            return true;
        }
        return false;
    }

    // Parse one object value. `doc` used only to not resolve refs (we keep Ref).
    PdfObject parseValue()
    {
        skipWs();
        if (pos >= data.size()) return PdfObject::makeNull();
        char c = data[pos];

        if (c == '/') return parseName();
        if (c == '(') return parseLiteralString();
        if (c == '[') return parseArray();
        if (c == '<') {
            if (pos + 1 < data.size() && data[pos + 1] == '<')
                return parseDictOrStream();
            return parseHexString();
        }
        if (c == 't' && match("true"))  return PdfObject::makeBool(true);
        if (c == 'f' && match("false")) return PdfObject::makeBool(false);
        if (c == 'n' && match("null"))  return PdfObject::makeNull();
        if (c == '+' || c == '-' || c == '.' || (c >= '0' && c <= '9'))
            return parseNumberOrRef();
        // Unknown token; skip one char to avoid infinite loop.
        ++pos;
        return PdfObject::makeNull();
    }

    PdfObject parseName()
    {
        ++pos; // consume '/'
        QByteArray name;
        while (pos < data.size()) {
            char c = data[pos];
            if (isPdfWhitespace(c) || isPdfDelimiter(c)) break;
            if (c == '#' && pos + 2 < data.size()) {
                bool ok = false;
                int v = QByteArray(data.constData() + pos + 1, 2).toInt(&ok, 16);
                if (ok) { name.append(char(v)); pos += 3; continue; }
            }
            name.append(c); ++pos;
        }
        return PdfObject::makeName(name);
    }

    PdfObject parseLiteralString()
    {
        ++pos; // '('
        QByteArray s;
        int depth = 1;
        while (pos < data.size() && depth > 0) {
            char c = data[pos++];
            if (c == '\\') {
                if (pos >= data.size()) break;
                char e = data[pos++];
                switch (e) {
                case 'n': s.append('\n'); break;
                case 'r': s.append('\r'); break;
                case 't': s.append('\t'); break;
                case 'b': s.append('\b'); break;
                case 'f': s.append('\f'); break;
                case '(': s.append('('); break;
                case ')': s.append(')'); break;
                case '\\': s.append('\\'); break;
                case '\r': if (pos < data.size() && data[pos] == '\n') ++pos; break;
                case '\n': break;
                default:
                    if (e >= '0' && e <= '7') {
                        int oct = e - '0';
                        for (int k = 0; k < 2 && pos < data.size()
                                        && data[pos] >= '0' && data[pos] <= '7'; ++k)
                            oct = oct * 8 + (data[pos++] - '0');
                        s.append(char(oct));
                    } else {
                        s.append(e);
                    }
                }
            } else if (c == '(') { ++depth; s.append(c); }
            else if (c == ')') { --depth; if (depth > 0) s.append(c); }
            else s.append(c);
        }
        return PdfObject::makeStr(s);
    }

    PdfObject parseHexString()
    {
        ++pos; // '<'
        QByteArray hex;
        while (pos < data.size() && data[pos] != '>') {
            char c = data[pos++];
            if (!isPdfWhitespace(c)) hex.append(c);
        }
        if (pos < data.size()) ++pos; // '>'
        if (hex.size() % 2) hex.append('0');
        return PdfObject::makeStr(QByteArray::fromHex(hex));
    }

    PdfObject parseArray()
    {
        ++pos; // '['
        PdfObject a = PdfObject::makeArray();
        while (true) {
            skipWs();
            if (pos >= data.size()) break;
            if (data[pos] == ']') { ++pos; break; }
            a.arr.append(parseValue());
        }
        return a;
    }

    PdfObject parseDictOrStream()
    {
        pos += 2; // '<<'
        PdfObject d = PdfObject::makeDict();
        while (true) {
            skipWs();
            if (pos >= data.size()) break;
            if (data[pos] == '>' && pos + 1 < data.size() && data[pos + 1] == '>') {
                pos += 2; break;
            }
            if (data[pos] != '/') { ++pos; continue; }
            PdfObject key = parseName();
            PdfObject val = parseValue();
            d.dict.insert(key.strVal, val);
        }
        // Possible stream.
        skipWs();
        if (match("stream")) {
            // After 'stream' there is CRLF or LF.
            if (pos < data.size() && data[pos] == '\r') ++pos;
            if (pos < data.size() && data[pos] == '\n') ++pos;
            qint64 start = pos;
            // Determine length if present and literal.
            qint64 len = -1;
            if (d.dict.contains("Length") && d.dict["Length"].isNumber())
                len = d.dict["Length"].toInt();
            if (len >= 0 && start + len <= data.size()) {
                d.stream = QByteArray(data.constData() + start, int(len));
                pos = start + len;
            } else {
                // Scan for 'endstream'.
                int idx = data.indexOf("endstream", int(start));
                if (idx < 0) idx = data.size();
                int slen = idx - int(start);
                // Trim a single trailing EOL before endstream.
                if (slen > 0 && data[int(start) + slen - 1] == '\n') --slen;
                if (slen > 0 && data[int(start) + slen - 1] == '\r') --slen;
                d.stream = QByteArray(data.constData() + start, qMax(0, slen));
                pos = idx;
            }
            match("endstream");
            d.type = PdfObject::Stream;
        }
        return d;
    }

    PdfObject parseNumberOrRef()
    {
        qint64 save = pos;
        QByteArray num;
        bool isReal = false;
        if (pos < data.size() && (data[pos] == '+' || data[pos] == '-')) num.append(data[pos++]);
        while (pos < data.size()) {
            char c = data[pos];
            if (c >= '0' && c <= '9') { num.append(c); ++pos; }
            else if (c == '.') { isReal = true; num.append(c); ++pos; }
            else break;
        }
        if (!isReal) {
            // Could be "N G R" reference or "N G obj".
            qint64 after1 = pos;
            skipWs();
            QByteArray gen;
            qint64 genStart = pos;
            while (pos < data.size() && data[pos] >= '0' && data[pos] <= '9')
                gen.append(data[pos++]);
            if (!gen.isEmpty()) {
                qint64 afterGen = pos;
                skipWs();
                if (pos < data.size() && data[pos] == 'R'
                    && (pos + 1 >= data.size() || isPdfWhitespace(data[pos + 1])
                        || isPdfDelimiter(data[pos + 1]))) {
                    ++pos;
                    return PdfObject::makeRef(num.toInt(), gen.toInt());
                }
                // not a ref -> rewind to after the first number
                Q_UNUSED(afterGen);
                Q_UNUSED(genStart);
            }
            pos = after1;
            return PdfObject::makeInt(num.toInt());
        }
        Q_UNUSED(save);
        return PdfObject::makeReal(num.toDouble());
    }
};

// ===========================================================================
// PdfDocument: loading / parsing
// ===========================================================================
bool PdfDocument::load(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        m_error = QStringLiteral("Cannot open file");
        return false;
    }
    return loadData(f.readAll());
}

bool PdfDocument::loadData(const QByteArray &data)
{
    m_data = data;
    m_objects.clear();
    m_offsets.clear();
    m_pages.clear();
    m_maxObjNum = 0;
    m_valid = false;
    m_wasEncrypted = false;
    m_error.clear();
    m_warnings.clear();
    m_objStmExpanded = 0;
    return parse();
}

bool PdfDocument::parse()
{
    if (!m_data.startsWith("%PDF-")) {
        // Some files have junk before header; find it.
        int h = m_data.indexOf("%PDF-");
        if (h < 0) { m_error = QStringLiteral("Not a PDF file"); return false; }
        m_data = m_data.mid(h);
    }

    // The robust approach: brute-force scan every "N G obj" so we are tolerant
    // of broken xref tables, xref streams and object streams. This parses all
    // top-level indirect objects. (Objects inside object streams are not
    // extracted, but the vast majority of page/structural objects are
    // top-level, which is all our structural operations require.)
    collectObjectsBruteForce();

    // PDF 1.5+ puts page/catalog dictionaries inside compressed object
    // streams, which the scan above cannot see. Hoist them out now, *before*
    // the trailer/page-tree walk, otherwise such files fail with "no pages
    // found" even though they are perfectly valid.
    expandObjectStreams();

    // Find trailer (classic) for /Root; fall back to scanning for /Type /Catalog.
    parseXrefAndTrailer();

    // Detect encryption.
    if (m_trailer.dict.contains("Encrypt")) {
        m_wasEncrypted = true;
        m_warnings << QStringLiteral("The document is encrypted; some streams "
                                     "may not be readable without the password.");
    }

    buildPageList();

    if (m_pages.isEmpty()) {
        // Be specific about *why* so the failure is diagnosable rather than a
        // silent blank window.
        if (m_objects.isEmpty())
            m_error = QStringLiteral("No PDF objects could be parsed — the file "
                                     "is truncated or not a PDF.");
        else if (m_wasEncrypted)
            m_error = QStringLiteral("The document is encrypted and its page "
                                     "tree could not be read without the "
                                     "password.");
        else if (!m_trailer.dict.contains("Root"))
            m_error = QStringLiteral("No document catalog (/Root) was found — "
                                     "the cross-reference data is damaged.");
        else
            m_error = QStringLiteral("No pages found: parsed %1 object(s) but "
                                     "the page tree is empty or uses features "
                                     "the built-in reader does not support.")
                          .arg(m_objects.size());
        // Still mark valid=false but keep objects for diagnostics.
        return false;
    }

    m_valid = true;
    return true;
}

// ---------------------------------------------------------------------------
// expandObjectStreams — hoist objects out of /Type /ObjStm containers.
//
// An object stream holds N objects; its decoded payload is
//     <objNum1> <offset1> <objNum2> <offset2> ... <objNumN> <offsetN>
// followed, starting at byte /First, by the object bodies themselves. We parse
// the pair table and then each body, inserting every object we find into
// m_objects unless a top-level object with the same number already exists
// (top-level definitions win, mirroring how a real xref would resolve).
// ---------------------------------------------------------------------------
void PdfDocument::expandObjectStreams()
{
    m_objStmExpanded = 0;

    // Snapshot the current keys so inserting while iterating is safe.
    const QList<int> keys = m_objects.keys();
    int badStreams = 0;

    for (int key : keys) {
        const PdfObject container = m_objects.value(key);
        if (!container.isStream())
            continue;
        const PdfObject ty = resolve(container.dict.value("Type"));
        if (!(ty.isName() && ty.strVal == "ObjStm"))
            continue;

        const int count = resolve(container.dict.value("N")).toInt();
        const int first = resolve(container.dict.value("First")).toInt();
        const QByteArray payload = decodeStream(container);
        if (count <= 0 || first <= 0 || payload.isEmpty()) {
            ++badStreams;
            continue;
        }

        // 1) Read the (objNum, offset) pair table from the header area.
        QVector<QPair<int, int>> entries;
        entries.reserve(count);
        {
            PdfLexer lex(payload, 0);
            for (int k = 0; k < count; ++k) {
                lex.skipWs();
                if (lex.pos >= first || lex.pos >= payload.size())
                    break;
                const PdfObject a = lex.parseValue();
                lex.skipWs();
                const PdfObject b = lex.parseValue();
                if (!a.isNumber() || !b.isNumber())
                    break;
                entries.append(qMakePair(a.toInt(), b.toInt()));
            }
        }
        if (entries.isEmpty()) {
            ++badStreams;
            continue;
        }

        // 2) Parse each object body at /First + offset.
        for (const auto &e : entries) {
            const int objNum = e.first;
            const qint64 at = qint64(first) + e.second;
            if (objNum <= 0 || at < 0 || at >= payload.size())
                continue;
            // A top-level "N 0 obj" definition takes precedence.
            if (m_objects.contains(objNum) && m_offsets.contains(objNum))
                continue;
            PdfLexer lex(payload, at);
            const PdfObject val = lex.parseValue();
            if (val.type == PdfObject::Null && !val.isDict())
                continue;
            m_objects.insert(objNum, val);
            m_maxObjNum = qMax(m_maxObjNum, objNum);
            ++m_objStmExpanded;
        }
    }

    if (badStreams > 0)
        m_warnings << QStringLiteral("%1 compressed object stream(s) could not "
                                     "be decoded.").arg(badStreams);
}

QString PdfDocument::diagnostics() const
{
    QStringList bits;
    bits << QStringLiteral("%1 object(s)").arg(m_objects.size());
    if (m_objStmExpanded > 0)
        bits << QStringLiteral("%1 from object streams").arg(m_objStmExpanded);
    bits << QStringLiteral("%1 page(s)").arg(m_pages.size());
    if (m_wasEncrypted)
        bits << QStringLiteral("encrypted");
    if (!m_warnings.isEmpty())
        bits << m_warnings.join(QStringLiteral("; "));
    if (!m_error.isEmpty())
        bits << m_error;
    return bits.join(QStringLiteral(", "));
}

void PdfDocument::collectObjectsBruteForce()
{
    // Regex-like manual scan for "<int> <int> obj".
    const char *d = m_data.constData();
    int n = m_data.size();
    int i = 0;
    while (i < n) {
        // find next digit that begins "N G obj"
        if (d[i] >= '0' && d[i] <= '9'
            && (i == 0 || isPdfWhitespace(d[i - 1]) || isPdfDelimiter(d[i - 1]))) {
            int j = i;
            QByteArray a;
            while (j < n && d[j] >= '0' && d[j] <= '9') a.append(d[j++]);
            int k = j;
            while (k < n && isPdfWhitespace(d[k])) ++k;
            QByteArray b;
            while (k < n && d[k] >= '0' && d[k] <= '9') b.append(d[k++]);
            int m = k;
            while (m < n && isPdfWhitespace(d[m])) ++m;
            if (!b.isEmpty() && m + 3 <= n && memcmp(d + m, "obj", 3) == 0) {
                int objNum = a.toInt();
                qint64 bodyStart = m + 3;
                PdfLexer lex(m_data, bodyStart);
                PdfObject val = lex.parseValue();
                m_objects.insert(objNum, val);
                m_offsets.insert(objNum, i);
                m_maxObjNum = qMax(m_maxObjNum, objNum);
                i = int(lex.pos);
                continue;
            }
        }
        ++i;
    }
}

void PdfDocument::parseXrefAndTrailer()
{
    // Try to read the classic trailer dict (last "trailer" keyword).
    int t = m_data.lastIndexOf("trailer");
    if (t >= 0) {
        PdfLexer lex(m_data, t + 7);
        lex.skipWs();
        if (lex.pos < m_data.size() && m_data[lex.pos] == '<') {
            PdfObject tr = lex.parseValue();
            if (tr.isDict()) m_trailer = tr;
        }
    }
    if (!m_trailer.dict.contains("Root")) {
        // Fallback: look for an xref-stream trailer dict or scan for a Catalog.
        for (auto it = m_objects.constBegin(); it != m_objects.constEnd(); ++it) {
            const PdfObject &o = it.value();
            if (o.isDict() && o.dict.contains("Type")) {
                PdfObject ty = resolve(o.dict["Type"]);
                if (ty.isName() && ty.strVal == "Catalog") {
                    PdfObject root = PdfObject::makeRef(it.key(), 0);
                    if (!m_trailer.isDict()) m_trailer = PdfObject::makeDict();
                    m_trailer.dict.insert("Root", root);
                    // keep possibly-found Encrypt if a stream xref trailer had it
                    break;
                }
            }
            // XRef stream trailer carries Root/Encrypt in its own dict.
            if (o.isStream() && o.dict.contains("Type")) {
                PdfObject ty = resolve(o.dict["Type"]);
                if (ty.isName() && ty.strVal == "XRef") {
                    if (!m_trailer.isDict()) m_trailer = PdfObject::makeDict();
                    if (o.dict.contains("Root"))
                        m_trailer.dict.insert("Root", o.dict["Root"]);
                    if (o.dict.contains("Encrypt"))
                        m_trailer.dict.insert("Encrypt", o.dict["Encrypt"]);
                }
            }
        }
    }
}

PdfObject PdfDocument::resolve(const PdfObject &o) const
{
    if (o.type != PdfObject::Ref)
        return o;
    return object(o.refNum);
}

PdfObject PdfDocument::object(int num) const
{
    auto it = m_objects.constFind(num);
    if (it == m_objects.constEnd())
        return PdfObject::makeNull();
    return it.value();
}

static void collectPagesRecursive(const PdfDocument &doc, const PdfObject &node,
                                  int nodeObjNum, int inheritedRotate,
                                  double iw, double ih, QVector<PdfPageInfo> &out,
                                  QList<int> &visited)
{
    PdfObject n = doc.resolve(node);
    if (!n.isDict()) return;
    PdfObject ty = doc.resolve(n.dict.value("Type"));

    int rotate = inheritedRotate;
    if (n.dict.contains("Rotate")) {
        PdfObject r = doc.resolve(n.dict.value("Rotate"));
        if (r.isNumber()) rotate = r.toInt();
    }
    double w = iw, h = ih;
    if (n.dict.contains("MediaBox")) {
        PdfObject mb = doc.resolve(n.dict.value("MediaBox"));
        if (mb.isArray() && mb.arr.size() == 4) {
            double x0 = doc.resolve(mb.arr[0]).toReal();
            double y0 = doc.resolve(mb.arr[1]).toReal();
            double x1 = doc.resolve(mb.arr[2]).toReal();
            double y1 = doc.resolve(mb.arr[3]).toReal();
            w = qAbs(x1 - x0);
            h = qAbs(y1 - y0);
        }
    }

    const bool isPages = ty.isName() && ty.strVal == "Pages";
    const bool isPage  = ty.isName() && ty.strVal == "Page";

    if (isPage || (!isPages && n.dict.contains("Contents") && !n.dict.contains("Kids"))) {
        PdfPageInfo pi;
        pi.objNum = nodeObjNum;
        pi.width = w > 0 ? w : 612;
        pi.height = h > 0 ? h : 792;
        pi.rotation = ((rotate % 360) + 360) % 360;
        out.append(pi);
        return;
    }

    if (n.dict.contains("Kids")) {
        PdfObject kids = doc.resolve(n.dict.value("Kids"));
        if (kids.isArray()) {
            for (const PdfObject &kid : kids.arr) {
                int kidNum = kid.type == PdfObject::Ref ? kid.refNum : 0;
                if (kidNum && visited.contains(kidNum)) continue;
                if (kidNum) visited.append(kidNum);
                collectPagesRecursive(doc, kid, kidNum, rotate, w, h, out, visited);
            }
        }
    }
}

void PdfDocument::buildPageList()
{
    m_pages.clear();
    PdfObject root = resolve(m_trailer.dict.value("Root"));
    if (!root.isDict()) return;
    PdfObject pagesRoot = root.dict.value("Pages");
    int pagesNum = pagesRoot.type == PdfObject::Ref ? pagesRoot.refNum : 0;
    QList<int> visited;
    collectPagesRecursive(*this, pagesRoot, pagesNum, 0, 0, 0, m_pages, visited);

    // Fallback: if the tree walk yielded nothing, scan all objects for /Page.
    if (m_pages.isEmpty()) {
        QList<int> keys = m_objects.keys();
        std::sort(keys.begin(), keys.end());
        for (int k : keys) {
            const PdfObject &o = m_objects[k];
            if (o.isDict()) {
                PdfObject ty = resolve(o.dict.value("Type"));
                if (ty.isName() && ty.strVal == "Page") {
                    QVector<PdfPageInfo> one;
                    QList<int> v;
                    collectPagesRecursive(*this, PdfObject::makeRef(k, 0), k, 0, 0, 0, one, v);
                    m_pages += one;
                }
            }
        }
    }
}

PdfPageInfo PdfDocument::pageInfo(int index) const
{
    if (index < 0 || index >= m_pages.size()) return {};
    return m_pages[index];
}

// ===========================================================================
// Stream decoding
// ===========================================================================
QByteArray PdfDocument::decodeStream(const PdfObject &streamObj) const
{
    if (!streamObj.isStream()) return {};
    QByteArray data = streamObj.stream;
    PdfObject filter = resolve(streamObj.dict.value("Filter"));
    QList<PdfObject> filters;
    if (filter.isName()) filters.append(filter);
    else if (filter.isArray()) filters = filter.arr;

    // /DecodeParms is either one dict (single filter) or an array parallel to
    // /Filter. It carries the predictor settings used by Flate and LZW.
    PdfObject parms = resolve(streamObj.dict.value("DecodeParms"));
    if (!parms.isDict() && !parms.isArray())
        parms = resolve(streamObj.dict.value("DP"));
    QList<PdfObject> parmList;
    if (parms.isArray())      parmList = parms.arr;
    else if (parms.isDict())  parmList.append(parms);

    for (int fi = 0; fi < filters.size(); ++fi) {
        const PdfObject f = resolve(filters[fi]);
        if (!f.isName()) continue;
        const QByteArray name = f.strVal;

        // Image-only codecs: stop here and hand the raw codestream to the
        // caller (pageSingleImage passes it to Qt's image plugins).
        if (name == "DCTDecode"   || name == "DCT"
            || name == "JPXDecode"
            || name == "CCITTFaxDecode" || name == "CCF"
            || name == "JBIG2Decode")
            break;

        if (name == "FlateDecode" || name == "Fl")
            data = zlibInflate(data);
        else if (name == "LZWDecode" || name == "LZW")
            data = lzwDecode(data, 1);
        else if (name == "RunLengthDecode" || name == "RL")
            data = runLengthDecode(data);
        else if (name == "ASCII85Decode" || name == "A85")
            data = ascii85Decode(data);
        else if (name == "ASCIIHexDecode" || name == "AHx")
            data = asciiHexDecode(data);
        else if (name == "Crypt")
            continue;                 // /Identity crypt filter: nothing to do
        else
            break;                    // unknown filter — leave the bytes as-is

        // Reverse the predictor for the filters that support one.
        if (name == "FlateDecode" || name == "Fl"
            || name == "LZWDecode" || name == "LZW") {
            PdfObject pd = (fi < parmList.size()) ? resolve(parmList[fi])
                                                  : PdfObject::makeNull();
            if (!pd.isDict() && parmList.size() == 1 && filters.size() == 1)
                pd = resolve(parmList[0]);
            if (pd.isDict()) {
                const int predictor = pd.dict.contains("Predictor")
                                          ? resolve(pd.dict.value("Predictor")).toInt() : 1;
                if (predictor > 1) {
                    const int colors = pd.dict.contains("Colors")
                                           ? resolve(pd.dict.value("Colors")).toInt() : 1;
                    const int bpc = pd.dict.contains("BitsPerComponent")
                                        ? resolve(pd.dict.value("BitsPerComponent")).toInt() : 8;
                    const int columns = pd.dict.contains("Columns")
                                            ? resolve(pd.dict.value("Columns")).toInt() : 1;
                    data = applyPredictor(data, predictor, colors, bpc, columns);
                }
            }
        }
    }
    return data;
}

QByteArray PdfDocument::pageContent(int index) const
{
    if (index < 0 || index >= m_pages.size()) return {};
    PdfObject page = object(m_pages[index].objNum);
    if (!page.isDict()) return {};
    PdfObject contents = page.dict.value("Contents");
    QByteArray out;
    PdfObject c = resolve(contents);
    if (c.isStream()) {
        out = decodeStream(c);
    } else if (contents.isArray() || c.isArray()) {
        PdfObject arr = c.isArray() ? c : contents;
        for (const PdfObject &ref : arr.arr) {
            PdfObject s = resolve(ref);
            if (s.isStream()) { out += decodeStream(s); out += "\n"; }
        }
    }
    return out;
}

QByteArray PdfDocument::pageSingleImage(int index, QByteArray &outFmt) const
{
    outFmt.clear();
    if (index < 0 || index >= m_pages.size()) return {};
    PdfObject page = object(m_pages[index].objNum);
    if (!page.isDict()) return {};
    PdfObject res = resolve(page.dict.value("Resources"));
    if (!res.isDict()) return {};
    PdfObject xobjs = resolve(res.dict.value("XObject"));
    if (!xobjs.isDict() || xobjs.dict.isEmpty()) return {};

    // Find the first image XObject.
    for (auto it = xobjs.dict.constBegin(); it != xobjs.dict.constEnd(); ++it) {
        PdfObject xo = resolve(it.value());
        if (!xo.isStream()) continue;
        PdfObject sub = resolve(xo.dict.value("Subtype"));
        if (!(sub.isName() && sub.strVal == "Image")) continue;

        PdfObject filt = resolve(xo.dict.value("Filter"));
        QByteArray fname;
        if (filt.isName()) fname = filt.strVal;
        else if (filt.isArray() && !filt.arr.isEmpty())
            fname = resolve(filt.arr.last()).strVal;

        // --- Codecs Qt can decode directly from the raw codestream ---------
        if (fname == "DCTDecode" || fname == "DCT") {
            outFmt = "jpg";
            return xo.stream;                      // raw JPEG bytes
        }
        if (fname == "JPXDecode") {
            // JPEG 2000: handed to Qt in case a jp2 image plugin is present;
            // if not, the caller falls back to the schematic page.
            QImage probe;
            if (probe.loadFromData(xo.stream, "jp2") && !probe.isNull()) {
                outFmt = "jp2";
                return xo.stream;
            }
            continue;
        }

        // --- Flate / LZW / RunLength raster -> reconstruct a QImage ---------
        const int w = resolve(xo.dict.value("Width")).toInt();
        const int h = resolve(xo.dict.value("Height")).toInt();
        const int bpc = xo.dict.contains("BitsPerComponent")
                            ? resolve(xo.dict.value("BitsPerComponent")).toInt() : 8;
        PdfObject cs = resolve(xo.dict.value("ColorSpace"));
        const QByteArray raw = decodeStream(xo);
        if (w <= 0 || h <= 0 || raw.isEmpty()) continue;

        // An /ImageMask is a 1-bit stencil; /Decode [1 0] inverts it.
        const PdfObject maskFlag = resolve(xo.dict.value("ImageMask"));
        const bool isMask = maskFlag.type == PdfObject::Bool && maskFlag.boolVal;
        bool invert = false;
        PdfObject dec = resolve(xo.dict.value("Decode"));
        if (dec.isArray() && dec.arr.size() >= 2
            && resolve(dec.arr[0]).toReal() > resolve(dec.arr[1]).toReal())
            invert = true;

        // Resolve the colour space to a family name and a component count.
        QByteArray csName;
        int comps = 1;
        QByteArray palette;          // /Indexed lookup table (RGB triples)
        int paletteBase = 3;
        if (cs.isName()) {
            csName = cs.strVal;
        } else if (cs.isArray() && !cs.arr.isEmpty()) {
            csName = resolve(cs.arr[0]).strVal;
            if ((csName == "Indexed" || csName == "I") && cs.arr.size() >= 4) {
                // [/Indexed base hival lookup]
                const PdfObject base = resolve(cs.arr[1]);
                QByteArray baseName = base.isName()
                        ? base.strVal
                        : (base.isArray() && !base.arr.isEmpty()
                               ? resolve(base.arr[0]).strVal : QByteArray("DeviceRGB"));
                paletteBase = (baseName == "DeviceGray" || baseName == "CalGray") ? 1
                            : (baseName == "DeviceCMYK") ? 4 : 3;
                const PdfObject lk = resolve(cs.arr[3]);
                palette = lk.isStream() ? decodeStream(lk) : lk.strVal;
            } else if (csName == "ICCBased" && cs.arr.size() >= 2) {
                const PdfObject strm = resolve(cs.arr[1]);
                const int nComp = resolve(strm.dict.value("N")).toInt();
                csName = (nComp == 1) ? QByteArray("DeviceGray")
                       : (nComp == 4) ? QByteArray("DeviceCMYK")
                                      : QByteArray("DeviceRGB");
            } else if (csName == "DeviceN" && cs.arr.size() >= 2) {
                const PdfObject names = resolve(cs.arr[1]);
                comps = qMax(1, names.isArray() ? names.arr.size() : 1);
            }
        }
        if (csName == "DeviceRGB" || csName == "CalRGB" || csName == "Lab")
            comps = 3;
        else if (csName == "DeviceCMYK")
            comps = 4;
        else if (csName == "DeviceGray" || csName == "CalGray"
                 || csName == "Indexed" || csName == "I" || csName == "Separation")
            comps = 1;
        if (isMask) comps = 1;

        // Read the `idx`-th sample of a row honouring sub-byte bit depths.
        auto sampleAt = [&](const unsigned char *rowPtr, int idx) -> int {
            switch (bpc) {
            case 1:  return (rowPtr[idx >> 3] >> (7 - (idx & 7))) & 0x01;
            case 2:  return (rowPtr[idx >> 2] >> (6 - 2 * (idx & 3))) & 0x03;
            case 4:  return (rowPtr[idx >> 1] >> (4 - 4 * (idx & 1))) & 0x0f;
            case 8:  return rowPtr[idx];
            case 16: return rowPtr[idx * 2];        // take the high byte
            default: return rowPtr[idx];
            }
        };
        const int maxVal = (1 << qMin(bpc, 8)) - 1;
        const int rowBytes = (qsizetype(w) * comps * bpc + 7) / 8;
        if (rowBytes <= 0 || qsizetype(rowBytes) * h > raw.size() + rowBytes)
            continue;                                // not enough data at all

        QImage img(w, h, QImage::Format_RGB32);
        if (img.isNull()) continue;
        bool ok = true;
        for (int y = 0; y < h && ok; ++y) {
            const qsizetype off = qsizetype(y) * rowBytes;
            if (off + rowBytes > raw.size()) {
                // Tolerate a truncated last row by filling it white.
                for (int rest = y; rest < h; ++rest)
                    memset(img.scanLine(rest), 0xff, qsizetype(w) * 4);
                break;
            }
            const unsigned char *rowPtr =
                reinterpret_cast<const unsigned char *>(raw.constData()) + off;
            QRgb *dst = reinterpret_cast<QRgb *>(img.scanLine(y));
            for (int x = 0; x < w; ++x) {
                int r = 0, g = 0, b = 0;
                if (comps == 1) {
                    int v = sampleAt(rowPtr, x);
                    if (!palette.isEmpty()) {
                        const int base = v * paletteBase;
                        if (base + paletteBase <= palette.size()) {
                            if (paletteBase == 1) {
                                r = g = b = static_cast<unsigned char>(palette[base]);
                            } else if (paletteBase == 4) {
                                const int c = static_cast<unsigned char>(palette[base]);
                                const int m = static_cast<unsigned char>(palette[base + 1]);
                                const int yy = static_cast<unsigned char>(palette[base + 2]);
                                const int k = static_cast<unsigned char>(palette[base + 3]);
                                r = qBound(0, 255 - qMin(255, c + k), 255);
                                g = qBound(0, 255 - qMin(255, m + k), 255);
                                b = qBound(0, 255 - qMin(255, yy + k), 255);
                            } else {
                                r = static_cast<unsigned char>(palette[base]);
                                g = static_cast<unsigned char>(palette[base + 1]);
                                b = static_cast<unsigned char>(palette[base + 2]);
                            }
                        } else {
                            r = g = b = 255;
                        }
                    } else {
                        int gray = maxVal > 0 ? (v * 255) / maxVal : 0;
                        if (isMask || invert) gray = 255 - gray;
                        r = g = b = gray;
                    }
                } else if (comps == 3) {
                    const int i0 = x * 3;
                    r = (sampleAt(rowPtr, i0)     * 255) / qMax(1, maxVal);
                    g = (sampleAt(rowPtr, i0 + 1) * 255) / qMax(1, maxVal);
                    b = (sampleAt(rowPtr, i0 + 2) * 255) / qMax(1, maxVal);
                } else if (comps == 4) {
                    const int i0 = x * 4;
                    const int c = (sampleAt(rowPtr, i0)     * 255) / qMax(1, maxVal);
                    const int m = (sampleAt(rowPtr, i0 + 1) * 255) / qMax(1, maxVal);
                    const int yy = (sampleAt(rowPtr, i0 + 2) * 255) / qMax(1, maxVal);
                    const int k = (sampleAt(rowPtr, i0 + 3) * 255) / qMax(1, maxVal);
                    r = qBound(0, 255 - qMin(255, c + k), 255);
                    g = qBound(0, 255 - qMin(255, m + k), 255);
                    b = qBound(0, 255 - qMin(255, yy + k), 255);
                } else {
                    ok = false;
                    break;
                }
                dst[x] = qRgb(qBound(0, r, 255), qBound(0, g, 255), qBound(0, b, 255));
            }
        }
        if (!ok || img.isNull()) continue;

        QByteArray png;
        QBuffer b(&png);
        b.open(QIODevice::WriteOnly);
        img.save(&b, "PNG");
        outFmt = "png";
        return png;
    }
    return {};
}

// ===========================================================================
// Writer — always emits a classic xref-table PDF.
// ===========================================================================
struct PdfWriter
{
    const PdfDocument &doc;
    QByteArray out;
    QVector<qint64> offsets; // index == obj number
    int maxNum;

    explicit PdfWriter(const PdfDocument &d) : doc(d), maxNum(d.m_maxObjNum) {}

    static QByteArray escLiteral(const QByteArray &s)
    {
        QByteArray r;
        for (char c : s) {
            if (c == '(' || c == ')' || c == '\\') { r.append('\\'); r.append(c); }
            else r.append(c);
        }
        return r;
    }

    void writeValue(const PdfObject &o)
    {
        switch (o.type) {
        case PdfObject::Null: out += "null"; break;
        case PdfObject::Bool: out += (o.boolVal ? "true" : "false"); break;
        case PdfObject::Int: out += QByteArray::number(o.toInt()); break;
        case PdfObject::Real: {
            QByteArray n = QByteArray::number(o.numVal, 'f', 4);
            // trim trailing zeros
            while (n.contains('.') && (n.endsWith('0'))) n.chop(1);
            if (n.endsWith('.')) n.chop(1);
            out += n; break;
        }
        case PdfObject::Str: out += "(" + escLiteral(o.strVal) + ")"; break;
        case PdfObject::Name: out += "/" + o.strVal; break;
        case PdfObject::Ref:
            out += QByteArray::number(o.refNum) + " " + QByteArray::number(o.refGen) + " R";
            break;
        case PdfObject::Array: {
            out += "[";
            for (int i = 0; i < o.arr.size(); ++i) {
                if (i) out += " ";
                writeValue(o.arr[i]);
            }
            out += "]";
            break;
        }
        case PdfObject::Dict:
        case PdfObject::Stream: {
            out += "<<";
            for (auto it = o.dict.constBegin(); it != o.dict.constEnd(); ++it) {
                out += "/" + it.key() + " ";
                writeValue(it.value());
            }
            out += ">>";
            break;
        }
        }
    }

    void writeIndirect(int num, const PdfObject &o)
    {
        if (offsets.size() <= num) offsets.resize(num + 1);
        offsets[num] = out.size();
        out += QByteArray::number(num) + " 0 obj\n";
        if (o.isStream()) {
            PdfObject hdr = o;
            hdr.type = PdfObject::Dict;
            hdr.dict["Length"] = PdfObject::makeInt(o.stream.size());
            writeValue(hdr);
            out += "\nstream\n";
            out += o.stream;
            out += "\nendstream";
        } else {
            writeValue(o);
        }
        out += "\nendobj\n";
    }

    QByteArray build()
    {
        out = "%PDF-1.7\n%\xE2\xE3\xCF\xD3\n";
        offsets.clear();
        offsets.resize(maxNum + 1);

        // Collect all objects referenced; simplest: write every object we have.
        QList<int> keys = doc.m_objects.keys();
        std::sort(keys.begin(), keys.end());
        for (int k : keys)
            writeIndirect(k, doc.m_objects[k]);

        // xref
        qint64 xrefPos = out.size();
        out += "xref\n";
        out += "0 " + QByteArray::number(maxNum + 1) + "\n";
        out += "0000000000 65535 f \n";
        for (int i = 1; i <= maxNum; ++i) {
            qint64 off = (i < offsets.size()) ? offsets[i] : 0;
            if (off == 0 && !doc.m_objects.contains(i)) {
                out += "0000000000 65535 f \n";
            } else {
                QByteArray o = QByteArray::number(off);
                while (o.size() < 10) o.prepend('0');
                out += o + " 00000 n \n";
            }
        }
        // trailer
        out += "trailer\n<<";
        out += "/Size " + QByteArray::number(maxNum + 1);
        if (doc.m_trailer.dict.contains("Root")) {
            PdfObject r = doc.m_trailer.dict.value("Root");
            out += " /Root " + QByteArray::number(r.refNum) + " 0 R";
        }
        if (doc.m_trailer.dict.contains("Encrypt")) {
            PdfObject e = doc.m_trailer.dict.value("Encrypt");
            out += " /Encrypt " + QByteArray::number(e.refNum) + " 0 R";
            if (doc.m_trailer.dict.contains("ID")) {
                out += " /ID ";
                writeValue(doc.m_trailer.dict.value("ID"));
            }
        }
        out += ">>\n";
        out += "startxref\n" + QByteArray::number(xrefPos) + "\n%%EOF\n";
        return out;
    }
};

QByteArray PdfDocument::serialize() const
{
    PdfWriter w(*this);
    return w.build();
}

int PdfDocument::addObject(const PdfObject &o)
{
    int num = ++m_maxObjNum;
    m_objects.insert(num, o);
    return num;
}

// ===========================================================================
// Image -> PDF
// ===========================================================================
PdfDocument PdfDocument::fromImages(const QList<QByteArray> &imageFiles, QString *err)
{
    PdfDocument doc;
    doc.m_data.clear();

    // Create catalog + pages tree.
    PdfObject catalog = PdfObject::makeDict();
    catalog.dict["Type"] = PdfObject::makeName("Catalog");
    int catalogNum = doc.addObject(catalog);

    PdfObject pagesNode = PdfObject::makeDict();
    pagesNode.dict["Type"] = PdfObject::makeName("Pages");
    int pagesNum = doc.addObject(pagesNode);

    // Link catalog -> pages.
    doc.m_objects[catalogNum].dict["Pages"] = PdfObject::makeRef(pagesNum);

    PdfObject kids = PdfObject::makeArray();

    for (const QByteArray &path : imageFiles) {
        QImage img(QString::fromLocal8Bit(path));
        if (img.isNull()) {
            QFile f(QString::fromLocal8Bit(path));
            if (f.open(QIODevice::ReadOnly)) img.loadFromData(f.readAll());
        }
        if (img.isNull()) continue;
        img = img.convertToFormat(QImage::Format_RGB888);
        int w = img.width(), h = img.height();

        // Image XObject (Flate-compressed raw RGB).
        PdfObject imgObj = PdfObject::makeDict();
        imgObj.type = PdfObject::Stream;
        imgObj.dict["Type"] = PdfObject::makeName("XObject");
        imgObj.dict["Subtype"] = PdfObject::makeName("Image");
        imgObj.dict["Width"] = PdfObject::makeInt(w);
        imgObj.dict["Height"] = PdfObject::makeInt(h);
        imgObj.dict["ColorSpace"] = PdfObject::makeName("DeviceRGB");
        imgObj.dict["BitsPerComponent"] = PdfObject::makeInt(8);
        imgObj.dict["Filter"] = PdfObject::makeName("FlateDecode");
        QByteArray raw;
        raw.reserve(w * h * 3);
        for (int y = 0; y < h; ++y)
            raw.append(reinterpret_cast<const char *>(img.constScanLine(y)), w * 3);
        imgObj.stream = zlibDeflate(raw);
        int imgNum = doc.addObject(imgObj);

        // Page size: scale image to fit an A4-ish point box at 72 dpi max width.
        double pw = w, ph = h; // 1px = 1pt
        // content stream draws the image to fill the page box.
        QByteArray content =
            "q\n" + QByteArray::number(pw, 'f', 2) + " 0 0 "
            + QByteArray::number(ph, 'f', 2) + " 0 0 cm\n/Im0 Do\nQ\n";
        PdfObject contentObj = PdfObject::makeDict();
        contentObj.type = PdfObject::Stream;
        contentObj.stream = content;
        int contentNum = doc.addObject(contentObj);

        PdfObject resources = PdfObject::makeDict();
        PdfObject xobjDict = PdfObject::makeDict();
        xobjDict.dict["Im0"] = PdfObject::makeRef(imgNum);
        resources.dict["XObject"] = xobjDict;

        PdfObject page = PdfObject::makeDict();
        page.dict["Type"] = PdfObject::makeName("Page");
        page.dict["Parent"] = PdfObject::makeRef(pagesNum);
        PdfObject mediaBox = PdfObject::makeArray();
        mediaBox.arr = { PdfObject::makeInt(0), PdfObject::makeInt(0),
                         PdfObject::makeReal(pw), PdfObject::makeReal(ph) };
        page.dict["MediaBox"] = mediaBox;
        page.dict["Resources"] = resources;
        page.dict["Contents"] = PdfObject::makeRef(contentNum);
        int pageNum = doc.addObject(page);
        kids.arr.append(PdfObject::makeRef(pageNum));
    }

    if (kids.arr.isEmpty()) {
        if (err) *err = QStringLiteral("No readable images.");
        return doc;
    }

    doc.m_objects[pagesNum].dict["Kids"] = kids;
    doc.m_objects[pagesNum].dict["Count"] = PdfObject::makeInt(kids.arr.size());

    doc.m_trailer = PdfObject::makeDict();
    doc.m_trailer.dict["Root"] = PdfObject::makeRef(catalogNum);
    doc.buildPageList();
    doc.m_valid = !doc.m_pages.isEmpty();
    return doc;
}

// ===========================================================================
// Page manipulation
// ===========================================================================
PdfDocument PdfDocument::extractPages(const QList<int> &indices) const
{
    // Build a fresh document copying the needed page objects + their closure.
    // Simplest robust approach: deep-copy ALL objects, then rebuild the Pages
    // tree to include only the chosen pages in the requested order.
    PdfDocument d = *this;

    // Collect chosen page object numbers in order.
    QList<int> chosen;
    for (int idx : indices)
        if (idx >= 0 && idx < m_pages.size())
            chosen.append(m_pages[idx].objNum);

    // Find catalog + pages root.
    PdfObject root = d.resolve(d.m_trailer.dict.value("Root"));
    if (!root.isDict()) return d;
    PdfObject pagesRef = root.dict.value("Pages");
    int pagesNum = pagesRef.refNum;
    if (!d.m_objects.contains(pagesNum)) return d;

    PdfObject kids = PdfObject::makeArray();
    for (int objNum : chosen) {
        kids.arr.append(PdfObject::makeRef(objNum));
        // Fix each page's /Parent.
        if (d.m_objects.contains(objNum))
            d.m_objects[objNum].dict["Parent"] = PdfObject::makeRef(pagesNum);
    }
    d.m_objects[pagesNum].dict["Kids"] = kids;
    d.m_objects[pagesNum].dict["Count"] = PdfObject::makeInt(kids.arr.size());
    d.buildPageList();
    d.m_valid = !d.m_pages.isEmpty();
    return d;
}

void PdfDocument::appendDocument(const PdfDocument &other)
{
    insertDocument(pageCount(), other);
}

void PdfDocument::insertDocument(int index, const PdfDocument &other)
{
    if (!other.isValid()) return;
    index = qBound(0, index, pageCount());

    // Offset all of `other`'s object numbers so they don't collide.
    int offset = m_maxObjNum;
    QHash<int, PdfObject> remapped;
    auto remapRef = [offset](PdfObject o) -> PdfObject {
        // deep remap of refs within an object
        std::function<void(PdfObject &)> fix = [&](PdfObject &x) {
            if (x.type == PdfObject::Ref) { x.refNum += offset; return; }
            if (x.isArray()) for (auto &e : x.arr) fix(e);
            if (x.isDict()) for (auto &e : x.dict) fix(e);
        };
        fix(o);
        return o;
    };
    for (auto it = other.m_objects.constBegin(); it != other.m_objects.constEnd(); ++it)
        remapped.insert(it.key() + offset, remapRef(it.value()));

    // Merge objects.
    for (auto it = remapped.constBegin(); it != remapped.constEnd(); ++it)
        m_objects.insert(it.key(), it.value());
    m_maxObjNum += other.m_maxObjNum;

    // Our pages root.
    PdfObject root = resolve(m_trailer.dict.value("Root"));
    int pagesNum = root.dict.value("Pages").refNum;

    // Other's page object numbers (remapped), in order.
    QList<int> otherPageNums;
    for (const PdfPageInfo &pi : other.m_pages)
        otherPageNums.append(pi.objNum + offset);

    // Rebuild our kids list = existing page objnums with inserts.
    QList<int> cur;
    for (const PdfPageInfo &pi : m_pages) cur.append(pi.objNum);
    for (int i = 0; i < otherPageNums.size(); ++i)
        cur.insert(index + i, otherPageNums[i]);

    PdfObject kids = PdfObject::makeArray();
    for (int n : cur) {
        kids.arr.append(PdfObject::makeRef(n));
        if (m_objects.contains(n))
            m_objects[n].dict["Parent"] = PdfObject::makeRef(pagesNum);
    }
    m_objects[pagesNum].dict["Kids"] = kids;
    m_objects[pagesNum].dict["Count"] = PdfObject::makeInt(cur.size());
    buildPageList();
}

void PdfDocument::rotatePage(int index, int deltaDegrees)
{
    if (index < 0 || index >= m_pages.size()) return;
    int objNum = m_pages[index].objNum;
    if (!m_objects.contains(objNum)) return;
    int cur = m_pages[index].rotation;
    int neu = (((cur + deltaDegrees) % 360) + 360) % 360;
    m_objects[objNum].dict["Rotate"] = PdfObject::makeInt(neu);
    m_pages[index].rotation = neu;
}

void PdfDocument::movePage(int from, int to)
{
    if (from < 0 || from >= m_pages.size()) return;
    to = qBound(0, to, m_pages.size() - 1);
    if (from == to) return;
    QList<int> order;
    for (const PdfPageInfo &pi : m_pages) order.append(pi.objNum);
    int v = order.takeAt(from);
    order.insert(to, v);

    PdfObject root = resolve(m_trailer.dict.value("Root"));
    int pagesNum = root.dict.value("Pages").refNum;
    PdfObject kids = PdfObject::makeArray();
    for (int n : order) kids.arr.append(PdfObject::makeRef(n));
    m_objects[pagesNum].dict["Kids"] = kids;
    m_objects[pagesNum].dict["Count"] = PdfObject::makeInt(order.size());
    buildPageList();
}

void PdfDocument::deletePage(int index)
{
    if (index < 0 || index >= m_pages.size()) return;
    int delNum = m_pages[index].objNum;
    QList<int> order;
    for (const PdfPageInfo &pi : m_pages)
        if (pi.objNum != delNum) order.append(pi.objNum);

    PdfObject root = resolve(m_trailer.dict.value("Root"));
    int pagesNum = root.dict.value("Pages").refNum;
    PdfObject kids = PdfObject::makeArray();
    for (int n : order) kids.arr.append(PdfObject::makeRef(n));
    m_objects[pagesNum].dict["Kids"] = kids;
    m_objects[pagesNum].dict["Count"] = PdfObject::makeInt(order.size());
    buildPageList();
}

// ===========================================================================
// Content / resources helpers for watermark, stamp and annotation baking
// ===========================================================================
void PdfDocument::appendPageContent(int pageIndex, const QByteArray &contentFragment,
                                    const QMap<QByteArray, PdfObject> &extraResources)
{
    if (pageIndex < 0 || pageIndex >= m_pages.size()) return;
    int pageNum = m_pages[pageIndex].objNum;
    if (!m_objects.contains(pageNum)) return;
    PdfObject &page = m_objects[pageNum];

    // Wrap existing content in q...Q so our appended fragment is isolated, by
    // introducing a new combined /Contents array: [ existingContents newFrag ].
    PdfObject newFrag = PdfObject::makeDict();
    newFrag.type = PdfObject::Stream;
    newFrag.stream = QByteArray("\nq\n") + contentFragment + QByteArray("\nQ\n");
    int fragNum = addObject(newFrag);

    PdfObject contents = page.dict.value("Contents");
    PdfObject arr = PdfObject::makeArray();
    if (contents.type == PdfObject::Ref) {
        arr.arr.append(contents);
    } else if (contents.isArray()) {
        arr.arr = contents.arr;
    } else {
        PdfObject resolved = resolve(contents);
        if (resolved.isArray()) arr.arr = resolved.arr;
    }
    arr.arr.append(PdfObject::makeRef(fragNum));
    page.dict["Contents"] = arr;

    // Merge extra resources (e.g. /XObject image, /ExtGState) into the page's
    // Resources dict.
    if (!extraResources.isEmpty()) {
        PdfObject res = page.dict.value("Resources");
        PdfObject resObj;
        if (res.type == PdfObject::Ref) {
            // Make a local copy we can edit, replacing the ref with an inline dict.
            resObj = resolve(res);
        } else if (res.isDict()) {
            resObj = res;
        } else {
            resObj = PdfObject::makeDict();
        }
        if (!resObj.isDict()) resObj = PdfObject::makeDict();

        for (auto it = extraResources.constBegin(); it != extraResources.constEnd(); ++it) {
            const QByteArray &cat = it.key();     // e.g. "XObject" or "ExtGState"
            PdfObject incoming = it.value();      // a dict of name->ref
            PdfObject existing = resObj.dict.value(cat);
            PdfObject merged = existing.isDict() ? existing : PdfObject::makeDict();
            if (incoming.isDict())
                for (auto j = incoming.dict.constBegin(); j != incoming.dict.constEnd(); ++j)
                    merged.dict.insert(j.key(), j.value());
            resObj.dict[cat] = merged;
        }
        page.dict["Resources"] = resObj;
    }
}

void PdfDocument::addTextWatermark(const QString &text, int pageIndex,
                                   double fontSize, double opacity)
{
    QByteArray t = text.toUtf8();
    // Escape literal string.
    QByteArray esc;
    for (char c : t) {
        if (c == '(' || c == ')' || c == '\\') { esc.append('\\'); esc.append(c); }
        else esc.append(c);
    }

    // A shared Helvetica font + an ExtGState for transparency.
    PdfObject font = PdfObject::makeDict();
    font.dict["Type"] = PdfObject::makeName("Font");
    font.dict["Subtype"] = PdfObject::makeName("Type1");
    font.dict["BaseFont"] = PdfObject::makeName("Helvetica");
    int fontNum = addObject(font);

    PdfObject gs = PdfObject::makeDict();
    gs.dict["Type"] = PdfObject::makeName("ExtGState");
    gs.dict["ca"] = PdfObject::makeReal(opacity);
    gs.dict["CA"] = PdfObject::makeReal(opacity);
    int gsNum = addObject(gs);

    auto applyTo = [&](int idx) {
        PdfPageInfo pi = m_pages[idx];
        double cx = pi.width / 2.0;
        double cy = pi.height / 2.0;
        // 45° rotation matrix around center.
        const double a = 0.70710678, b = 0.70710678;
        QByteArray frag;
        frag += "/GSwm gs\n";
        frag += "BT\n";
        frag += "/Fwm " + QByteArray::number(fontSize, 'f', 1) + " Tf\n";
        frag += "0.6 0.6 0.6 rg\n";
        // Tm: [cos sin -sin cos tx ty], position roughly centered.
        double tx = cx - (t.size() * fontSize * 0.25) * a;
        double ty = cy - (t.size() * fontSize * 0.25) * b;
        frag += QByteArray::number(a, 'f', 5) + " " + QByteArray::number(b, 'f', 5) + " "
                + QByteArray::number(-b, 'f', 5) + " " + QByteArray::number(a, 'f', 5) + " "
                + QByteArray::number(tx, 'f', 2) + " " + QByteArray::number(ty, 'f', 2) + " Tm\n";
        frag += "(" + esc + ") Tj\n";
        frag += "ET\n";

        QMap<QByteArray, PdfObject> extra;
        PdfObject fontDict = PdfObject::makeDict();
        fontDict.dict["Fwm"] = PdfObject::makeRef(fontNum);
        extra["Font"] = fontDict;
        PdfObject gsDict = PdfObject::makeDict();
        gsDict.dict["GSwm"] = PdfObject::makeRef(gsNum);
        extra["ExtGState"] = gsDict;
        appendPageContent(idx, frag, extra);
    };

    if (pageIndex >= 0 && pageIndex < m_pages.size())
        applyTo(pageIndex);
    else
        for (int i = 0; i < m_pages.size(); ++i) applyTo(i);
}

void PdfDocument::addImageWatermark(const QByteArray &imageData, const QByteArray &fmt,
                                    int pageIndex, double scale, double opacity)
{
    stampImageInternalForAll(imageData, fmt, pageIndex, scale, opacity, true);
}

// Helper so both watermark-image and explicit stamp share image XObject code.
// Declared below (not in header) via a free function with friend access is not
// possible, so we implement the shared logic inline in stampImage and call it
// from addImageWatermark via a private path replicated here.
void PdfDocument::stampImage(int pageIndex, const QByteArray &imageData,
                             const QByteArray &fmt, double x, double y,
                             double w, double h, double opacity)
{
    if (pageIndex < 0 || pageIndex >= m_pages.size()) return;
    QImage img;
    img.loadFromData(imageData, fmt.constData());
    if (img.isNull()) img.loadFromData(imageData);
    if (img.isNull()) return;
    img = img.convertToFormat(QImage::Format_RGB888);
    int iw = img.width(), ih = img.height();

    PdfObject imgObj = PdfObject::makeDict();
    imgObj.type = PdfObject::Stream;
    imgObj.dict["Type"] = PdfObject::makeName("XObject");
    imgObj.dict["Subtype"] = PdfObject::makeName("Image");
    imgObj.dict["Width"] = PdfObject::makeInt(iw);
    imgObj.dict["Height"] = PdfObject::makeInt(ih);
    imgObj.dict["ColorSpace"] = PdfObject::makeName("DeviceRGB");
    imgObj.dict["BitsPerComponent"] = PdfObject::makeInt(8);
    imgObj.dict["Filter"] = PdfObject::makeName("FlateDecode");
    QByteArray raw; raw.reserve(iw * ih * 3);
    for (int yy = 0; yy < ih; ++yy)
        raw.append(reinterpret_cast<const char *>(img.constScanLine(yy)), iw * 3);
    imgObj.stream = zlibDeflate(raw);
    int imgNum = addObject(imgObj);

    PdfObject gs = PdfObject::makeDict();
    gs.dict["Type"] = PdfObject::makeName("ExtGState");
    gs.dict["ca"] = PdfObject::makeReal(opacity);
    gs.dict["CA"] = PdfObject::makeReal(opacity);
    int gsNum = addObject(gs);

    QByteArray frag;
    frag += "/GSst gs\n";
    frag += QByteArray::number(w, 'f', 2) + " 0 0 " + QByteArray::number(h, 'f', 2)
            + " " + QByteArray::number(x, 'f', 2) + " " + QByteArray::number(y, 'f', 2) + " cm\n";
    frag += "/Imst Do\n";

    QMap<QByteArray, PdfObject> extra;
    PdfObject xo = PdfObject::makeDict();
    xo.dict["Imst"] = PdfObject::makeRef(imgNum);
    extra["XObject"] = xo;
    PdfObject gsd = PdfObject::makeDict();
    gsd.dict["GSst"] = PdfObject::makeRef(gsNum);
    extra["ExtGState"] = gsd;
    appendPageContent(pageIndex, frag, extra);
}

void PdfDocument::stampImageInternalForAll(const QByteArray &imageData,
                                           const QByteArray &fmt, int pageIndex,
                                           double scale, double opacity, bool centered)
{
    Q_UNUSED(centered);
    QImage img;
    img.loadFromData(imageData, fmt.constData());
    if (img.isNull()) img.loadFromData(imageData);
    if (img.isNull()) return;
    double ar = img.height() > 0 ? double(img.width()) / img.height() : 1.0;

    auto applyTo = [&](int idx) {
        PdfPageInfo pi = m_pages[idx];
        double w = pi.width * scale;
        double h = w / (ar > 0 ? ar : 1.0);
        double x = (pi.width - w) / 2.0;
        double y = (pi.height - h) / 2.0;
        stampImage(idx, imageData, fmt, x, y, w, h, opacity);
    };
    if (pageIndex >= 0 && pageIndex < m_pages.size())
        applyTo(pageIndex);
    else
        for (int i = 0; i < m_pages.size(); ++i) applyTo(i);
}

// ===========================================================================
// RC4-128 standard-security encryption (open password)
// ===========================================================================
namespace {

// PDF standard padding string.
const unsigned char kPad[32] = {
    0x28,0xBF,0x4E,0x5E,0x4E,0x75,0x8A,0x41,0x64,0x00,0x4E,0x56,0xFF,0xFA,0x01,0x08,
    0x2E,0x2E,0x00,0xB6,0xD0,0x68,0x3E,0x80,0x2F,0x0C,0xA9,0xFE,0x64,0x53,0x69,0x7A
};

QByteArray padPassword(const QString &pw)
{
    QByteArray p = pw.toLatin1();
    QByteArray out = p.left(32);
    int need = 32 - out.size();
    out.append(reinterpret_cast<const char *>(kPad), need);
    return out;
}

// Minimal MD5 via Qt.
QByteArray md5(const QByteArray &d)
{
    return QCryptographicHash::hash(d, QCryptographicHash::Md5);
}

// RC4 keystream cipher.
QByteArray rc4(const QByteArray &key, const QByteArray &data)
{
    unsigned char s[256];
    for (int i = 0; i < 256; ++i) s[i] = i;
    int j = 0;
    int klen = key.size();
    for (int i = 0; i < 256; ++i) {
        j = (j + s[i] + (unsigned char)key[i % klen]) & 0xff;
        std::swap(s[i], s[j]);
    }
    QByteArray out(data.size(), Qt::Uninitialized);
    int a = 0, b = 0;
    for (int i = 0; i < data.size(); ++i) {
        a = (a + 1) & 0xff;
        b = (b + s[a]) & 0xff;
        std::swap(s[a], s[b]);
        unsigned char k = s[(s[a] + s[b]) & 0xff];
        out[i] = data[i] ^ k;
    }
    return out;
}

} // namespace

void PdfDocument::encrypt(const QString &userPassword, const QString &ownerPassword)
{
    // RC4-128, revision 3, V=2. Permissions: allow everything except nothing
    // special (0xFFFFFFFC base). We implement the standard algorithm 3.2/3.3.
    const int keyLenBytes = 16; // 128-bit
    const quint32 P = 0xFFFFFFFC; // allow all

    QByteArray uPad = padPassword(userPassword);
    QString oPwd = ownerPassword.isEmpty() ? userPassword : ownerPassword;
    QByteArray oPad = padPassword(oPwd);

    // Algorithm 3.3: compute /O.
    QByteArray ohash = md5(oPad);
    for (int i = 0; i < 50; ++i) ohash = md5(ohash.left(keyLenBytes));
    QByteArray rc4key = ohash.left(keyLenBytes);
    QByteArray O = rc4(rc4key, uPad);
    for (int i = 1; i <= 19; ++i) {
        QByteArray k(keyLenBytes, 0);
        for (int b = 0; b < keyLenBytes; ++b) k[b] = rc4key[b] ^ (char)i;
        O = rc4(k, O);
    }

    // Build /ID (needed for key derivation).
    QByteArray id0 = md5(QByteArray::number(QDateTime::currentMSecsSinceEpoch())
                         + QByteArray::number((qulonglong)this));
    // Algorithm 3.2: compute encryption key.
    QByteArray keyInput;
    keyInput += uPad;
    keyInput += O.left(32);
    quint32 p = P;
    keyInput.append(char(p & 0xff));
    keyInput.append(char((p >> 8) & 0xff));
    keyInput.append(char((p >> 16) & 0xff));
    keyInput.append(char((p >> 24) & 0xff));
    keyInput += id0;
    QByteArray key = md5(keyInput);
    for (int i = 0; i < 50; ++i) key = md5(key.left(keyLenBytes));
    key = key.left(keyLenBytes);

    // Algorithm 3.5: compute /U (revision 3).
    QByteArray uInput;
    uInput.append(reinterpret_cast<const char *>(kPad), 32);
    uInput += id0;
    QByteArray uHash = md5(uInput);
    QByteArray U = rc4(key, uHash);
    for (int i = 1; i <= 19; ++i) {
        QByteArray k(keyLenBytes, 0);
        for (int b = 0; b < keyLenBytes; ++b) k[b] = key[b] ^ (char)i;
        U = rc4(k, U);
    }
    U.append(16, 0); // pad to 32 bytes

    // Encrypt every string/stream object with per-object RC4 keys.
    auto objKey = [&](int num, int gen) -> QByteArray {
        QByteArray in = key;
        in.append(char(num & 0xff));
        in.append(char((num >> 8) & 0xff));
        in.append(char((num >> 16) & 0xff));
        in.append(char(gen & 0xff));
        in.append(char((gen >> 8) & 0xff));
        QByteArray h = md5(in);
        int n = qMin(keyLenBytes + 5, 16);
        return h.left(n);
    };

    QList<int> keys = m_objects.keys();
    for (int num : keys) {
        PdfObject &o = m_objects[num];
        QByteArray ok = objKey(num, 0);
        std::function<void(PdfObject &)> enc = [&](PdfObject &x) {
            if (x.type == PdfObject::Str)
                x.strVal = rc4(ok, x.strVal);
            else if (x.isArray())
                for (auto &e : x.arr) enc(e);
            else if (x.isDict())
                for (auto &e : x.dict) enc(e);
        };
        if (o.isStream()) {
            // encrypt dict strings then the stream
            for (auto &e : o.dict) enc(e);
            o.stream = rc4(ok, o.stream);
        } else {
            enc(o);
        }
    }

    // Add the /Encrypt dictionary object.
    PdfObject enc = PdfObject::makeDict();
    enc.dict["Filter"] = PdfObject::makeName("Standard");
    enc.dict["V"] = PdfObject::makeInt(2);
    enc.dict["R"] = PdfObject::makeInt(3);
    enc.dict["Length"] = PdfObject::makeInt(128);
    enc.dict["P"] = PdfObject::makeInt((int)P);
    enc.dict["O"] = PdfObject::makeStr(O.left(32));
    enc.dict["U"] = PdfObject::makeStr(U.left(32));
    int encNum = addObject(enc);

    m_trailer.dict["Encrypt"] = PdfObject::makeRef(encNum);
    PdfObject idArr = PdfObject::makeArray();
    idArr.arr.append(PdfObject::makeStr(id0));
    idArr.arr.append(PdfObject::makeStr(id0));
    m_trailer.dict["ID"] = idArr;

    m_encryptRequested = true;
    m_userPassword = userPassword;
    m_ownerPassword = ownerPassword;
}

// ===========================================================================
// AcroForm text-field filling
// ===========================================================================
QStringList PdfDocument::formTextFields() const
{
    QStringList names;
    PdfObject root = resolve(m_trailer.dict.value("Root"));
    if (!root.isDict()) return names;
    PdfObject acro = resolve(root.dict.value("AcroForm"));
    if (!acro.isDict()) return names;
    PdfObject fields = resolve(acro.dict.value("Fields"));
    if (!fields.isArray()) return names;
    for (const PdfObject &fref : fields.arr) {
        PdfObject f = resolve(fref);
        if (!f.isDict()) continue;
        PdfObject ft = resolve(f.dict.value("FT"));
        PdfObject tt = resolve(f.dict.value("T"));
        if (ft.isName() && ft.strVal == "Tx" && tt.type == PdfObject::Str)
            names.append(QString::fromUtf8(tt.strVal));
    }
    return names;
}

bool PdfDocument::fillFormField(const QString &fieldName, const QString &value)
{
    PdfObject root = resolve(m_trailer.dict.value("Root"));
    if (!root.isDict()) return false;
    PdfObject acroRef = root.dict.value("AcroForm");
    PdfObject acro = resolve(acroRef);
    if (!acro.isDict()) return false;
    PdfObject fields = resolve(acro.dict.value("Fields"));
    if (!fields.isArray()) return false;

    const QByteArray target = fieldName.toUtf8();
    bool changed = false;
    for (const PdfObject &fref : fields.arr) {
        if (fref.type != PdfObject::Ref) continue;
        int num = fref.refNum;
        if (!m_objects.contains(num)) continue;
        PdfObject &f = m_objects[num];
        PdfObject tt = resolve(f.dict.value("T"));
        PdfObject ft = resolve(f.dict.value("FT"));
        if (tt.type == PdfObject::Str && tt.strVal == target
            && ft.isName() && ft.strVal == "Tx") {
            f.dict["V"] = PdfObject::makeStr(value.toUtf8());
            changed = true;
        }
    }
    // Ask viewers to regenerate field appearances.
    if (changed && acroRef.type == PdfObject::Ref
        && m_objects.contains(acroRef.refNum)) {
        m_objects[acroRef.refNum].dict["NeedAppearances"] = PdfObject::makeBool(true);
    }
    return changed;
}

// ===========================================================================
// Text extraction (best-effort) for PDF -> Word / text export.
//
// We scan the decoded content stream for the text-showing operators:
//   (string) Tj        — show one string
//   [ (a) n (b) ] TJ   — show an array of strings / kerning numbers
//   (string) '         — move to next line and show
//   aw ac (string) "   — set spacing, next line, show
// and the text-positioning operators Td / TD / T* / Tm which start new lines.
// Layout/columns are NOT reconstructed (as noted: "有排版损耗"), but the
// readable text is recovered so it can be dropped into a Word/RTF/txt file.
// ===========================================================================
QString PdfDocument::pageText(int index) const
{
    const QByteArray content = pageContent(index);
    if (content.isEmpty())
        return QString();

    QString out;
    int i = 0;
    const int n = content.size();

    auto readLiteral = [&](int &p) -> QByteArray {
        // p points just after '('
        QByteArray s;
        int depth = 1;
        while (p < n && depth > 0) {
            char c = content[p++];
            if (c == '\\') {
                if (p >= n) break;
                char e = content[p++];
                switch (e) {
                case 'n': s.append('\n'); break;
                case 'r': s.append('\r'); break;
                case 't': s.append('\t'); break;
                case 'b': s.append('\b'); break;
                case 'f': s.append('\f'); break;
                case '(': s.append('('); break;
                case ')': s.append(')'); break;
                case '\\': s.append('\\'); break;
                default:
                    if (e >= '0' && e <= '7') {
                        int oct = e - '0';
                        for (int k = 0; k < 2 && p < n
                                        && content[p] >= '0' && content[p] <= '7'; ++k)
                            oct = oct * 8 + (content[p++] - '0');
                        s.append(char(oct));
                    } else s.append(e);
                }
            } else if (c == '(') { ++depth; s.append(c); }
            else if (c == ')') { --depth; if (depth > 0) s.append(c); }
            else s.append(c);
        }
        return s;
    };
    auto readHex = [&](int &p) -> QByteArray {
        QByteArray hex;
        while (p < n && content[p] != '>') {
            char c = content[p++];
            if (c != ' ' && c != '\n' && c != '\r' && c != '\t') hex.append(c);
        }
        if (p < n) ++p;
        if (hex.size() % 2) hex.append('0');
        return QByteArray::fromHex(hex);
    };
    // Decode PDF string bytes to QString. PDFDocEncoding/Latin1 is a reasonable
    // default; UTF-16BE is detected by the BOM.
    auto decode = [](const QByteArray &b) -> QString {
        if (b.size() >= 2 && (unsigned char)b[0] == 0xFE && (unsigned char)b[1] == 0xFF)
            return QString::fromUtf16(reinterpret_cast<const char16_t *>(b.constData() + 2),
                                      (b.size() - 2) / 2);
        return QString::fromLatin1(b);
    };

    // Collect "tokens" as we go; track the last few to recognize operators.
    QList<QByteArray> strings;   // pending shown strings for current TJ/Tj
    while (i < n) {
        char c = content[i];
        if (c == '(') {
            ++i;
            strings.append(readLiteral(i));
        } else if (c == '<' && (i + 1 >= n || content[i + 1] != '<')) {
            ++i;
            strings.append(readHex(i));
        } else if (c == 'T' && i + 1 < n &&
                   (content[i + 1] == 'j' || content[i + 1] == 'J')) {
            // flush pending strings as the shown text
            for (const QByteArray &s : strings) out += decode(s);
            strings.clear();
            i += 2;
        } else if (c == '\'' || c == '"') {
            // ' and " show text on a new line
            out += '\n';
            for (const QByteArray &s : strings) out += decode(s);
            strings.clear();
            ++i;
        } else if (c == 'T' && i + 1 < n &&
                   (content[i + 1] == 'd' || content[i + 1] == 'D' ||
                    content[i + 1] == '*')) {
            out += '\n';   // line move
            strings.clear();
            i += 2;
        } else if (c == 'T' && i + 1 < n && content[i + 1] == 'm') {
            out += '\n';
            strings.clear();
            i += 2;
        } else {
            // Strings not immediately followed by a show operator (e.g. array
            // elements between [ ]) are kept; drop stray operators/numbers.
            if (c == '[' || c == ']') { ++i; }
            else { ++i; }
        }
    }
    // Collapse excessive blank lines.
    out.replace(QRegularExpression("\n{3,}"), "\n\n");
    return out.trimmed();
}

QString PdfDocument::allText() const
{
    QStringList parts;
    for (int i = 0; i < m_pages.size(); ++i)
        parts << pageText(i);
    return parts.join("\n\n");
}
