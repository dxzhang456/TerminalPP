#ifndef EDITORTAB_H
#define EDITORTAB_H

#include <QWidget>
#include "syntaxhighlighter.h"
#include "thememanager.h"

class CodeEditor;

// A single document tab: editor + syntax highlighter + file state.
// Encapsulates all file I/O (open/save, encoding, line-ending) just like ndd.
class EditorTab : public QWidget
{
    Q_OBJECT
public:
    enum Eol { LF, CRLF, CR };

    explicit EditorTab(QWidget *parent = nullptr);

    CodeEditor *editor() const { return m_editor; }

    bool loadFile(const QString &path, const QString &encoding = QString());
    bool saveFile(const QString &path);
    bool save();          // save to current path
    bool isUntitled() const { return m_filePath.isEmpty(); }

    QString filePath() const { return m_filePath; }
    QString fileName() const;
    bool isModified() const;
    void setModified(bool m);

    QString encoding() const { return m_encoding; }
    void setEncoding(const QString &enc) { m_encoding = enc; }

    Eol eol() const { return m_eol; }
    void setEol(Eol e) { m_eol = e; }
    QString eolName() const;

    SyntaxHighlighter *highlighter() const { return m_highlighter; }
    void detectLanguageFromPath();

    void applyTheme(const EditorTheme &theme);

signals:
    void modificationChanged(bool modified);
    void cursorInfoChanged();

private:
    CodeEditor *m_editor = nullptr;
    SyntaxHighlighter *m_highlighter = nullptr;
    QString m_filePath;
    QString m_encoding = "UTF-8";
    Eol m_eol = CRLF;
};

#endif // EDITORTAB_H