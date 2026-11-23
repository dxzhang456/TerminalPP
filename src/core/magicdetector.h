#ifndef MAGICDETECTOR_H
#define MAGICDETECTOR_H

#include <QString>

// File type classification performed via libmagic (file-type identification
// utility / libmagic-1.dll + magic.mgc), with robust fallback.
//
// Categories:
//   - Text:  plain text, source code, scripts, markup, JSON, XML, INI, etc.
//            -> Opened with EditorTab (ndd replica)
//   - Image: raster/vector images (PNG, JPG, BMP, GIF, WebP, SVG, ICO, etc.)
//            -> Opened with ImageEditorTab
//   - Binary: executable, library, archive, compiled object, arbitrary binary
//            -> Opened with HexEditorTab
class MagicDetector
{
public:
    enum FileType {
        Text,
        Image,
        Binary,
        Pdf
    };

    // Detect the type of a local file by content using libmagic.
    static FileType detectType(const QString &filePath);

    // Human-readable MIME type string returned by libmagic (e.g. "image/png",
    // "text/x-c", "application/octet-stream").
    static QString detectMime(const QString &filePath);

    // Detailed human-readable description from libmagic (e.g. "PNG image data, 800 x 600...").
    static QString detectDescription(const QString &filePath);
};

#endif // MAGICDETECTOR_H