#include "editortab.h"
#include "codeeditor.h"

#include <QVBoxLayout>
#include <QFile>
#include <QFileInfo>
#include "core/i18n.h"
static inline QString TRc(const char *s){ return I18n::instance()->tr(QString::fromUtf8(s)); }
#include <QTextStream>
#include <QStringDecoder>
#include <QStringEncoder>
#include <QStringConverter>
#include <QPalette>
#include <optional>

// Map an encoding name from the UI/status bar onto a Qt 6 QStringConverter
// encoding. Qt 6 dropped QTextCodec, so the Chinese multi-byte names are
// served by the System codec (CP936/GBK on Chinese Windows); mapping them to
// UTF-8 instead would corrupt the text.
static QStringConverter::Encoding encodingFromName(const QString &name)
{
    const QString norm = name.trimmed().toUpper();
    if (norm == "SYSTEM" || norm == "GB18030" || norm == "GBK"
        || norm == "GB2312" || norm == "ANSI")
        return QStringConverter::System;
    auto enc = QStringConverter::encodingForName(name.toUtf8().constData());
    return enc.value_or(QStringConverter::Utf8);
}

EditorTab::EditorTab(QWidget *parent)
    : QWidget(parent)
{
    m_editor = new CodeEditor(this);
    m_highlighter = new SyntaxHighlighter(m_editor->document());

    QVBoxLayout *l = new QVBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    l->addWidget(m_editor);

    connect(m_editor->document(), &QTextDocument::modificationChanged,
            this, &EditorTab::modificationChanged);
    connect(m_editor, &CodeEditor::cursorPositionChanged,
            this, &EditorTab::cursorInfoChanged);
}

QString EditorTab::fileName() const
{
    if (m_filePath.isEmpty())
        return TRc("new");
    return QFileInfo(m_filePath).fileName();
}

bool EditorTab::isModified() const
{
    return m_editor->document()->isModified();
}

void EditorTab::setModified(bool m)
{
    m_editor->document()->setModified(m);
}

QString EditorTab::eolName() const
{
    switch (m_eol) {
    case LF:   return "Unix (LF)";
    case CR:   return "Mac (CR)";
    default:   return "Windows (CRLF)";
    }
}

bool EditorTab::loadFile(const QString &path, const QString &encoding)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QByteArray raw = file.readAll();
    file.close();

    // Decode the bytes into text.
    QString text;
    QString usedEncoding = encoding;
    if (usedEncoding.isEmpty()) {
        // encodingForData() only recognises a BOM (and UTF-16/32 patterns). A
        // BOM-less GBK/GB18030/Shift-JIS file therefore reports "unknown", so
        // decoding it as UTF-8 would produce mojibake (乱码) — exactly what
        // happens with Chinese .txt files downloaded from a cloud drive.
        // Strategy: honour a BOM if present, otherwise verify the data really
        // is valid UTF-8 and fall back to the local 8-bit codec when it is not.
        auto encOpt = QStringConverter::encodingForData(raw);
        if (encOpt.has_value()) {
            QStringDecoder dec(encOpt.value());
            text = dec.decode(raw);
            usedEncoding = QStringConverter::nameForEncoding(encOpt.value());
        } else {
            QStringDecoder utf8(QStringConverter::Utf8);
            QString asUtf8 = utf8.decode(raw);
            if (!utf8.hasError()) {
                text = asUtf8;             // genuinely valid UTF-8
                usedEncoding = "UTF-8";
            } else {
                // Not UTF-8 -> decode with the system ANSI codepage (CP936 /
                // GBK on Chinese Windows, which covers GB2312/GBK text).
                QStringDecoder sys(QStringConverter::System);
                QString asSystem = sys.decode(raw);
                if (!sys.hasError()) {
                    text = asSystem;
                    usedEncoding = "System";
                } else {
                    // Last resort: Latin-1 never fails, so no byte is lost.
                    QStringDecoder l1(QStringConverter::Latin1);
                    text = l1.decode(raw);
                    usedEncoding = "ISO-8859-1";
                }
            }
        }
    } else {
        // Explicit encoding requested by the user. Qt 6's QStringConverter only
        // provides UTF-8/16/32, Latin-1 and System, so map the Chinese
        // multi-byte names onto System (the local ANSI codepage) instead of
        // silently falling back to UTF-8 and mangling the text.
        QStringDecoder dec(encodingFromName(encoding));
        text = dec.decode(raw);
        if (dec.hasError()) {
            // The chosen codec cannot represent these bytes; keep them intact
            // via Latin-1 rather than inserting replacement characters.
            QStringDecoder l1(QStringConverter::Latin1);
            text = l1.decode(raw);
        }
    }
    m_encoding = usedEncoding;

    // Detect EOL.
    if (text.contains("\r\n"))
        m_eol = CRLF;
    else if (text.contains("\r"))
        m_eol = CR;
    else
        m_eol = LF;

    // Normalize to \n internally.
    text.replace("\r\n", "\n");
    text.replace("\r", "\n");

    m_editor->setPlainText(text);
    m_editor->document()->setModified(false);

    m_filePath = path;
    detectLanguageFromPath();
    return true;
}

bool EditorTab::saveFile(const QString &path)
{
    QString text = m_editor->toPlainText();

    // Apply EOL.
    switch (m_eol) {
    case CRLF: text.replace("\n", "\r\n"); break;
    case CR:   text.replace("\n", "\r");   break;
    default:   break;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    // Write back with the same encoding the file was loaded/selected with, so a
    // GBK file stays GBK instead of silently becoming UTF-8.
    QStringEncoder encoder(encodingFromName(m_encoding));
    QByteArray out = encoder.encode(text);
    file.write(out);
    file.close();

    m_filePath = path;
    m_editor->document()->setModified(false);
    detectLanguageFromPath();
    return true;
}

bool EditorTab::save()
{
    if (m_filePath.isEmpty())
        return false;
    return saveFile(m_filePath);
}

void EditorTab::detectLanguageFromPath()
{
    const QString suffix = QFileInfo(m_filePath).suffix();
    m_highlighter->setLanguage(SyntaxHighlighter::languageForExtension(suffix));
}

void EditorTab::applyTheme(const EditorTheme &theme)
{
    QPalette pal = m_editor->palette();
    pal.setColor(QPalette::Base, theme.background);
    pal.setColor(QPalette::Text, theme.foreground);
    pal.setColor(QPalette::Highlight, theme.selectionBg);
    pal.setColor(QPalette::HighlightedText, theme.foreground);
    m_editor->setPalette(pal);

    m_editor->setCurrentLineColor(theme.currentLine);
    m_editor->setLineNumberColors(theme.lineNumberBg, theme.lineNumberFg);

    m_highlighter->setColors(theme.keyword, theme.type, theme.comment,
                             theme.string, theme.number, theme.preproc,
                             theme.function);
}