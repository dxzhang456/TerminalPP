#include "syntaxhighlighter.h"

SyntaxHighlighter::SyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    rebuildRules();
}

void SyntaxHighlighter::setLanguage(Language lang)
{
    if (m_language == lang)
        return;
    m_language = lang;
    rebuildRules();
    rehighlight();
}

void SyntaxHighlighter::setColors(const QColor &keyword, const QColor &type,
                                  const QColor &comment, const QColor &string,
                                  const QColor &number, const QColor &preproc,
                                  const QColor &function)
{
    m_keywordColor = keyword;
    m_typeColor = type;
    m_commentColor = comment;
    m_stringColor = string;
    m_numberColor = number;
    m_preprocColor = preproc;
    m_functionColor = function;
    rebuildRules();
    rehighlight();
}

SyntaxHighlighter::Language SyntaxHighlighter::languageForExtension(const QString &suffix)
{
    QString s = suffix.toLower();
    if (s == "c" || s == "cpp" || s == "cc" || s == "cxx" || s == "h" ||
        s == "hpp" || s == "hxx" || s == "inl")
        return Cpp;
    if (s == "cs")
        return CSharp;
    if (s == "java")
        return Java;
    if (s == "py" || s == "pyw")
        return Python;
    if (s == "js" || s == "ts" || s == "jsx" || s == "mjs")
        return JavaScript;
    if (s == "json")
        return Json;
    if (s == "xml" || s == "xsl" || s == "xsd" || s == "svg")
        return Xml;
    if (s == "html" || s == "htm")
        return Html;
    if (s == "sql")
        return Sql;
    if (s == "sh" || s == "bash")
        return Shell;
    if (s == "ini" || s == "cfg" || s == "conf")
        return Ini;
    return PlainText;
}

QString SyntaxHighlighter::languageName(Language lang)
{
    switch (lang) {
    case Cpp:        return "C/C++";
    case CSharp:     return "C#";
    case Java:       return "Java";
    case Python:     return "Python";
    case JavaScript: return "JavaScript";
    case Json:       return "JSON";
    case Xml:        return "XML";
    case Html:       return "HTML";
    case Sql:        return "SQL";
    case Shell:      return "Shell";
    case Ini:        return "INI";
    default:         return "Plain Text";
    }
}

void SyntaxHighlighter::addKeywords(const QStringList &words, const QTextCharFormat &fmt)
{
    for (const QString &w : words) {
        Rule r;
        r.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(w));
        r.format = fmt;
        m_rules.append(r);
    }
}

void SyntaxHighlighter::rebuildRules()
{
    m_rules.clear();
    m_commentStart = QRegularExpression();
    m_commentEnd = QRegularExpression();

    QTextCharFormat keywordFmt;  keywordFmt.setForeground(m_keywordColor);  keywordFmt.setFontWeight(QFont::Bold);
    QTextCharFormat typeFmt;     typeFmt.setForeground(m_typeColor);
    QTextCharFormat commentFmt;  commentFmt.setForeground(m_commentColor);  commentFmt.setFontItalic(true);
    QTextCharFormat stringFmt;   stringFmt.setForeground(m_stringColor);
    QTextCharFormat numberFmt;   numberFmt.setForeground(m_numberColor);
    QTextCharFormat preprocFmt;  preprocFmt.setForeground(m_preprocColor);
    QTextCharFormat functionFmt; functionFmt.setForeground(m_functionColor);

    m_multiLineCommentFormat = commentFmt;

    if (m_language == PlainText)
        return;

    // Common: numbers.
    {
        Rule r;
        r.pattern = QRegularExpression(QStringLiteral("\\b[0-9]+(\\.[0-9]+)?([eE][+-]?[0-9]+)?\\b|\\b0[xX][0-9a-fA-F]+\\b"));
        r.format = numberFmt;
        m_rules.append(r);
    }

    switch (m_language) {
    case Cpp:
    case CSharp:
    case Java:
    case JavaScript: {
        QStringList keywords;
        if (m_language == Cpp)
            keywords = {"alignas","alignof","and","asm","auto","break","case","catch","class",
                        "const","constexpr","continue","default","delete","do","else","enum",
                        "explicit","extern","false","for","friend","goto","if","inline","mutable",
                        "namespace","new","noexcept","nullptr","operator","override","private",
                        "protected","public","register","return","sizeof","static","static_cast",
                        "struct","switch","template","this","throw","true","try","typedef","typename",
                        "union","using","virtual","volatile","while","final","public","dynamic_cast",
                        "reinterpret_cast","const_cast"};
        else if (m_language == CSharp)
            keywords = {"abstract","as","base","break","case","catch","checked","class","const",
                        "continue","default","delegate","do","else","enum","event","explicit",
                        "extern","false","finally","fixed","for","foreach","goto","if","implicit",
                        "in","interface","internal","is","lock","namespace","new","null","operator",
                        "out","override","params","private","protected","public","readonly","ref",
                        "return","sealed","sizeof","stackalloc","static","struct","switch","this",
                        "throw","true","try","typeof","unchecked","unsafe","using","virtual","void",
                        "volatile","while","async","await","var"};
        else if (m_language == Java)
            keywords = {"abstract","assert","break","case","catch","class","const","continue",
                        "default","do","else","enum","extends","final","finally","for","goto","if",
                        "implements","import","instanceof","interface","native","new","package",
                        "private","protected","public","return","static","strictfp","super","switch",
                        "synchronized","this","throw","throws","transient","try","volatile","while",
                        "true","false","null"};
        else
            keywords = {"break","case","catch","class","const","continue","debugger","default",
                        "delete","do","else","export","extends","finally","for","function","if",
                        "import","in","instanceof","let","new","return","super","switch","this",
                        "throw","try","typeof","var","void","while","with","yield","await","async",
                        "true","false","null","undefined"};
        addKeywords(keywords, keywordFmt);

        QStringList types = {"int","long","short","char","float","double","bool","boolean",
                             "void","unsigned","signed","wchar_t","size_t","string","String",
                             "byte","object","var"};
        addKeywords(types, typeFmt);

        // Function calls.
        {
            Rule r;
            r.pattern = QRegularExpression(QStringLiteral("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\s*\\()"));
            r.format = functionFmt;
            m_rules.append(r);
        }
        // Strings.
        {
            Rule r;
            r.pattern = QRegularExpression(QStringLiteral("\"([^\"\\\\]|\\\\.)*\"|'([^'\\\\]|\\\\.)*'"));
            r.format = stringFmt;
            m_rules.append(r);
        }
        // Single line comment.
        {
            Rule r;
            r.pattern = QRegularExpression(QStringLiteral("//[^\n]*"));
            r.format = commentFmt;
            m_rules.append(r);
        }
        // Preprocessor (C/C++).
        if (m_language == Cpp) {
            Rule r;
            r.pattern = QRegularExpression(QStringLiteral("^\\s*#[A-Za-z]+"));
            r.format = preprocFmt;
            m_rules.append(r);
        }
        m_commentStart = QRegularExpression(QStringLiteral("/\\*"));
        m_commentEnd   = QRegularExpression(QStringLiteral("\\*/"));
        break;
    }
    case Python: {
        QStringList keywords = {"and","as","assert","async","await","break","class","continue",
                                "def","del","elif","else","except","finally","for","from","global",
                                "if","import","in","is","lambda","nonlocal","not","or","pass","raise",
                                "return","try","while","with","yield","True","False","None","self"};
        addKeywords(keywords, keywordFmt);
        {
            Rule r;
            r.pattern = QRegularExpression(QStringLiteral("\\bdef\\s+([A-Za-z_][A-Za-z0-9_]*)"));
            r.format = functionFmt;
            m_rules.append(r);
        }
        {
            Rule r;
            r.pattern = QRegularExpression(QStringLiteral("\"([^\"\\\\]|\\\\.)*\"|'([^'\\\\]|\\\\.)*'"));
            r.format = stringFmt;
            m_rules.append(r);
        }
        {
            Rule r;
            r.pattern = QRegularExpression(QStringLiteral("#[^\n]*"));
            r.format = commentFmt;
            m_rules.append(r);
        }
        break;
    }
    case Json: {
        {
            Rule r;
            r.pattern = QRegularExpression(QStringLiteral("\"([^\"\\\\]|\\\\.)*\"\\s*:"));
            r.format = keywordFmt;
            m_rules.append(r);
        }
        {
            Rule r;
            r.pattern = QRegularExpression(QStringLiteral("\"([^\"\\\\]|\\\\.)*\""));
            r.format = stringFmt;
            m_rules.append(r);
        }
        addKeywords({"true","false","null"}, typeFmt);
        break;
    }
    case Xml:
    case Html: {
        {
            Rule r;
            r.pattern = QRegularExpression(QStringLiteral("</?[A-Za-z0-9_:\\-]+"));
            r.format = keywordFmt;
            m_rules.append(r);
        }
        {
            Rule r;
            r.pattern = QRegularExpression(QStringLiteral("[A-Za-z0-9_:\\-]+(?=\\s*=)"));
            r.format = typeFmt;
            m_rules.append(r);
        }
        {
            Rule r;
            r.pattern = QRegularExpression(QStringLiteral("\"[^\"]*\"|'[^']*'"));
            r.format = stringFmt;
            m_rules.append(r);
        }
        {
            Rule r;
            r.pattern = QRegularExpression(QStringLiteral(">"));
            r.format = keywordFmt;
            m_rules.append(r);
        }
        m_commentStart = QRegularExpression(QStringLiteral("<!--"));
        m_commentEnd   = QRegularExpression(QStringLiteral("-->"));
        break;
    }
    case Sql: {
        QStringList keywords = {"SELECT","FROM","WHERE","INSERT","INTO","VALUES","UPDATE","SET",
                                "DELETE","CREATE","TABLE","DROP","ALTER","ADD","JOIN","INNER","LEFT",
                                "RIGHT","OUTER","ON","GROUP","BY","ORDER","HAVING","AND","OR","NOT",
                                "NULL","AS","DISTINCT","LIMIT","UNION","INDEX","PRIMARY","KEY","FOREIGN",
                                "REFERENCES","DEFAULT","LIKE","IN","BETWEEN","IS","EXISTS"};
        addKeywords(keywords, keywordFmt);
        // case-insensitive keyword rules
        for (Rule &r : m_rules)
            r.pattern.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
        {
            Rule r;
            r.pattern = QRegularExpression(QStringLiteral("'[^']*'"));
            r.format = stringFmt;
            m_rules.append(r);
        }
        {
            Rule r;
            r.pattern = QRegularExpression(QStringLiteral("--[^\n]*"));
            r.format = commentFmt;
            m_rules.append(r);
        }
        break;
    }
    case Shell: {
        QStringList keywords = {"if","then","else","elif","fi","for","while","do","done","case",
                                "esac","function","in","return","exit","export","local","read",
                                "echo","cd","source"};
        addKeywords(keywords, keywordFmt);
        {
            Rule r;
            r.pattern = QRegularExpression(QStringLiteral("\\$[A-Za-z_][A-Za-z0-9_]*|\\$\\{[^}]*\\}"));
            r.format = typeFmt;
            m_rules.append(r);
        }
        {
            Rule r;
            r.pattern = QRegularExpression(QStringLiteral("\"([^\"\\\\]|\\\\.)*\"|'[^']*'"));
            r.format = stringFmt;
            m_rules.append(r);
        }
        {
            Rule r;
            r.pattern = QRegularExpression(QStringLiteral("#[^\n]*"));
            r.format = commentFmt;
            m_rules.append(r);
        }
        break;
    }
    case Ini: {
        {
            Rule r;
            r.pattern = QRegularExpression(QStringLiteral("^\\s*\\[[^\\]]*\\]"));
            r.format = keywordFmt;
            m_rules.append(r);
        }
        {
            Rule r;
            r.pattern = QRegularExpression(QStringLiteral("^\\s*[^=;\n]+(?==)"));
            r.format = typeFmt;
            m_rules.append(r);
        }
        {
            Rule r;
            r.pattern = QRegularExpression(QStringLiteral("[;#][^\n]*"));
            r.format = commentFmt;
            m_rules.append(r);
        }
        break;
    }
    default:
        break;
    }
}

void SyntaxHighlighter::highlightBlock(const QString &text)
{
    for (const Rule &rule : std::as_const(m_rules)) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            // If a capture group exists (e.g. def name), highlight it; else whole match.
            if (m.lastCapturedIndex() >= 1 && m.capturedStart(1) >= 0)
                setFormat(m.capturedStart(1), m.capturedLength(1), rule.format);
            else
                setFormat(m.capturedStart(), m.capturedLength(), rule.format);
        }
    }

    // Multi-line comments.
    if (!m_commentStart.pattern().isEmpty()) {
        setCurrentBlockState(0);
        int startIndex = 0;
        if (previousBlockState() != 1) {
            QRegularExpressionMatch m = m_commentStart.match(text);
            startIndex = m.hasMatch() ? m.capturedStart() : -1;
        }
        while (startIndex >= 0) {
            QRegularExpressionMatch endMatch = m_commentEnd.match(text, startIndex);
            int endIndex = endMatch.hasMatch() ? endMatch.capturedStart() : -1;
            int commentLength;
            if (endIndex == -1) {
                setCurrentBlockState(1);
                commentLength = text.length() - startIndex;
            } else {
                commentLength = endIndex - startIndex + endMatch.capturedLength();
            }
            setFormat(startIndex, commentLength, m_multiLineCommentFormat);
            QRegularExpressionMatch next = m_commentStart.match(text, startIndex + commentLength);
            startIndex = next.hasMatch() ? next.capturedStart() : -1;
        }
    }
}