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

    // Detect BOM / encoding.
    QString text;
    QString usedEncoding = encoding;
    if (usedEncoding.isEmpty()) {
        auto encOpt = QStringConverter::encodingForData(raw);
        if (encOpt.has_value()) {
            QStringDecoder dec(encOpt.value());
            text = dec.decode(raw);
            usedEncoding = QStringConverter::nameForEncoding(encOpt.value());
        } else {
            QStringDecoder dec(QStringConverter::Utf8);
            text = dec.decode(raw);
            usedEncoding = "UTF-8";
        }
    } else {
        auto enc = QStringConverter::encodingForName(encoding.toUtf8().constData());
        QStringDecoder dec(enc.value_or(QStringConverter::Utf8));
        text = dec.decode(raw);
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

    auto enc = QStringConverter::encodingForName(m_encoding.toUtf8().constData());
    QStringEncoder encoder(enc.value_or(QStringConverter::Utf8));
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