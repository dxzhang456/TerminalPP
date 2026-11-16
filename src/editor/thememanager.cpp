#include "thememanager.h"

ThemeManager::ThemeManager()
{
    // --- Default (light) ---
    {
        EditorTheme t;
        t.name = "Default";
        t.background = QColor(255, 255, 255);
        t.foreground = QColor(0, 0, 0);
        t.currentLine = QColor(232, 242, 254);
        t.lineNumberBg = QColor(240, 240, 240);
        t.lineNumberFg = QColor(130, 130, 130);
        t.selectionBg = QColor(173, 214, 255);
        t.keyword = QColor(0, 0, 255);
        t.type = QColor(43, 145, 175);
        t.comment = QColor(0, 128, 0);
        t.string = QColor(163, 21, 21);
        t.number = QColor(128, 0, 128);
        t.preproc = QColor(128, 128, 0);
        t.function = QColor(121, 94, 38);
        registerTheme(t);
    }
    // --- Monokai ---
    {
        EditorTheme t;
        t.name = "Monokai";
        t.background = QColor(39, 40, 34);
        t.foreground = QColor(248, 248, 242);
        t.currentLine = QColor(62, 61, 50);
        t.lineNumberBg = QColor(39, 40, 34);
        t.lineNumberFg = QColor(144, 145, 148);
        t.selectionBg = QColor(73, 72, 62);
        t.keyword = QColor(249, 38, 114);
        t.type = QColor(102, 217, 239);
        t.comment = QColor(117, 113, 94);
        t.string = QColor(230, 219, 116);
        t.number = QColor(174, 129, 255);
        t.preproc = QColor(166, 226, 46);
        t.function = QColor(166, 226, 46);
        registerTheme(t);
    }
    // --- Deep Black ---
    {
        EditorTheme t;
        t.name = "Deep Black";
        t.background = QColor(0, 0, 0);
        t.foreground = QColor(220, 220, 220);
        t.currentLine = QColor(25, 25, 25);
        t.lineNumberBg = QColor(10, 10, 10);
        t.lineNumberFg = QColor(110, 110, 110);
        t.selectionBg = QColor(50, 50, 80);
        t.keyword = QColor(86, 156, 214);
        t.type = QColor(78, 201, 176);
        t.comment = QColor(87, 166, 74);
        t.string = QColor(214, 157, 133);
        t.number = QColor(181, 206, 168);
        t.preproc = QColor(155, 155, 155);
        t.function = QColor(220, 220, 170);
        registerTheme(t);
    }
    // --- Blue light ---
    {
        EditorTheme t;
        t.name = "Blue light";
        t.background = QColor(224, 235, 255);
        t.foreground = QColor(20, 30, 60);
        t.currentLine = QColor(200, 218, 250);
        t.lineNumberBg = QColor(205, 222, 250);
        t.lineNumberFg = QColor(90, 110, 150);
        t.selectionBg = QColor(160, 190, 240);
        t.keyword = QColor(0, 0, 180);
        t.type = QColor(0, 110, 140);
        t.comment = QColor(0, 120, 0);
        t.string = QColor(150, 20, 20);
        t.number = QColor(120, 0, 120);
        t.preproc = QColor(120, 100, 0);
        t.function = QColor(90, 70, 20);
        registerTheme(t);
    }
    // --- Twilight ---
    {
        EditorTheme t;
        t.name = "Twilight";
        t.background = QColor(31, 31, 31);
        t.foreground = QColor(228, 228, 228);
        t.currentLine = QColor(45, 45, 45);
        t.lineNumberBg = QColor(26, 26, 26);
        t.lineNumberFg = QColor(120, 120, 120);
        t.selectionBg = QColor(68, 68, 68);
        t.keyword = QColor(205, 160, 90);
        t.type = QColor(140, 190, 200);
        t.comment = QColor(90, 100, 110);
        t.string = QColor(140, 170, 110);
        t.number = QColor(190, 140, 110);
        t.preproc = QColor(180, 150, 100);
        t.function = QColor(200, 190, 120);
        registerTheme(t);
    }
    // --- Choco ---
    {
        EditorTheme t;
        t.name = "Choco";
        t.background = QColor(46, 36, 30);
        t.foreground = QColor(230, 220, 200);
        t.currentLine = QColor(62, 50, 40);
        t.lineNumberBg = QColor(40, 30, 24);
        t.lineNumberFg = QColor(150, 130, 110);
        t.selectionBg = QColor(90, 70, 55);
        t.keyword = QColor(230, 170, 90);
        t.type = QColor(190, 200, 120);
        t.comment = QColor(150, 120, 90);
        t.string = QColor(200, 180, 120);
        t.number = QColor(220, 150, 120);
        t.preproc = QColor(200, 160, 100);
        t.function = QColor(230, 200, 140);
        registerTheme(t);
    }

    m_current = m_themes.value("Default");
}

void ThemeManager::registerTheme(const EditorTheme &t)
{
    m_themes.insert(t.name, t);
    m_order.append(t.name);
}

QStringList ThemeManager::themeNames() const
{
    return m_order;
}

EditorTheme ThemeManager::theme(const QString &name) const
{
    return m_themes.value(name, m_themes.value("Default"));
}

void ThemeManager::setCurrent(const QString &name)
{
    if (m_themes.contains(name))
        m_current = m_themes.value(name);
}