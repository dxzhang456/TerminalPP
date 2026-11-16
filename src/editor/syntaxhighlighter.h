#ifndef SYNTAXHIGHLIGHTER_H
#define SYNTAXHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QVector>

// Generic multi-language syntax highlighter, replicating ndd's highlighting
// for C/C++, Java, Python, JavaScript, JSON, XML/HTML, etc.
class SyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    enum Language {
        PlainText,
        Cpp,
        CSharp,
        Java,
        Python,
        JavaScript,
        Json,
        Xml,
        Html,
        Sql,
        Shell,
        Ini
    };

    explicit SyntaxHighlighter(QTextDocument *parent = nullptr);

    void setLanguage(Language lang);
    Language language() const { return m_language; }

    static Language languageForExtension(const QString &suffix);
    static QString  languageName(Language lang);

    // Allow themes to recolor tokens.
    void setColors(const QColor &keyword, const QColor &type,
                   const QColor &comment, const QColor &string,
                   const QColor &number, const QColor &preproc,
                   const QColor &function);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    void rebuildRules();
    void addKeywords(const QStringList &words, const QTextCharFormat &fmt);

    Language m_language = PlainText;
    QVector<Rule> m_rules;

    // Multi-line comment handling.
    QRegularExpression m_commentStart;
    QRegularExpression m_commentEnd;
    QTextCharFormat m_multiLineCommentFormat;

    QColor m_keywordColor { QColor(0,   0,   255) };
    QColor m_typeColor    { QColor(43,  145, 175) };
    QColor m_commentColor { QColor(0,   128, 0)   };
    QColor m_stringColor  { QColor(163, 21,  21)  };
    QColor m_numberColor  { QColor(128, 0,   128) };
    QColor m_preprocColor { QColor(128, 128, 0)   };
    QColor m_functionColor{ QColor(121, 94,  38)  };
};

#endif // SYNTAXHIGHLIGHTER_H