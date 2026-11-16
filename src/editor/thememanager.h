#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QString>
#include <QColor>
#include <QMap>
#include <QStringList>

// Editor color theme, replicating the themes shipped with ndd
// (Monokai, Deep Black, Blue light, default light, etc.).
struct EditorTheme {
    QString name;
    QColor background;
    QColor foreground;
    QColor currentLine;
    QColor lineNumberBg;
    QColor lineNumberFg;
    QColor selectionBg;

    // Syntax token colors.
    QColor keyword;
    QColor type;
    QColor comment;
    QColor string;
    QColor number;
    QColor preproc;
    QColor function;
};

// Holds all built-in themes and the currently selected one.
class ThemeManager
{
public:
    ThemeManager();

    QStringList themeNames() const;
    EditorTheme theme(const QString &name) const;
    EditorTheme current() const { return m_current; }
    void setCurrent(const QString &name);
    QString currentName() const { return m_current.name; }

private:
    void registerTheme(const EditorTheme &t);

    QMap<QString, EditorTheme> m_themes;
    QStringList m_order;
    EditorTheme m_current;
};

#endif // THEMEMANAGER_H