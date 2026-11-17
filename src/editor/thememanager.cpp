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

    // --- VS Code Dark+ ---
    {
        EditorTheme t;
        t.name = "VS Code Dark+";
        t.background = QColor(30, 30, 30);
        t.foreground = QColor(212, 212, 212);
        t.currentLine = QColor(40, 40, 40);
        t.lineNumberBg = QColor(30, 30, 30);
        t.lineNumberFg = QColor(133, 133, 133);
        t.selectionBg = QColor(38, 79, 120);
        t.keyword = QColor(86, 156, 214);
        t.type = QColor(78, 201, 176);
        t.comment = QColor(106, 153, 85);
        t.string = QColor(206, 145, 120);
        t.number = QColor(181, 206, 168);
        t.preproc = QColor(197, 134, 192);
        t.function = QColor(220, 220, 170);
        registerTheme(t);
    }
    // --- VS Code Light+ ---
    {
        EditorTheme t;
        t.name = "VS Code Light+";
        t.background = QColor(255, 255, 255);
        t.foreground = QColor(0, 0, 0);
        t.currentLine = QColor(245, 245, 245);
        t.lineNumberBg = QColor(255, 255, 255);
        t.lineNumberFg = QColor(140, 140, 140);
        t.selectionBg = QColor(173, 214, 255);
        t.keyword = QColor(0, 0, 255);
        t.type = QColor(38, 127, 153);
        t.comment = QColor(0, 128, 0);
        t.string = QColor(163, 21, 21);
        t.number = QColor(9, 134, 88);
        t.preproc = QColor(175, 0, 219);
        t.function = QColor(121, 94, 38);
        registerTheme(t);
    }
    // --- VS Code High Contrast ---
    {
        EditorTheme t;
        t.name = "VS Code High Contrast";
        t.background = QColor(0, 0, 0);
        t.foreground = QColor(255, 255, 255);
        t.currentLine = QColor(30, 30, 30);
        t.lineNumberBg = QColor(0, 0, 0);
        t.lineNumberFg = QColor(255, 255, 255);
        t.selectionBg = QColor(0, 90, 160);
        t.keyword = QColor(86, 156, 214);
        t.type = QColor(78, 220, 200);
        t.comment = QColor(126, 206, 125);
        t.string = QColor(255, 170, 120);
        t.number = QColor(181, 206, 168);
        t.preproc = QColor(217, 154, 212);
        t.function = QColor(240, 240, 180);
        registerTheme(t);
    }
    // --- GitHub Dark ---
    {
        EditorTheme t;
        t.name = "GitHub Dark";
        t.background = QColor(13, 17, 23);
        t.foreground = QColor(201, 209, 217);
        t.currentLine = QColor(22, 27, 34);
        t.lineNumberBg = QColor(13, 17, 23);
        t.lineNumberFg = QColor(110, 118, 129);
        t.selectionBg = QColor(28, 60, 110);
        t.keyword = QColor(255, 123, 114);
        t.type = QColor(121, 192, 255);
        t.comment = QColor(139, 148, 158);
        t.string = QColor(165, 214, 255);
        t.number = QColor(121, 192, 255);
        t.preproc = QColor(210, 168, 255);
        t.function = QColor(210, 168, 255);
        registerTheme(t);
    }
    // --- Solarized Dark ---
    {
        EditorTheme t;
        t.name = "Solarized Dark";
        t.background = QColor(0, 43, 54);
        t.foreground = QColor(131, 148, 150);
        t.currentLine = QColor(7, 54, 66);
        t.lineNumberBg = QColor(0, 43, 54);
        t.lineNumberFg = QColor(88, 110, 117);
        t.selectionBg = QColor(7, 54, 66);
        t.keyword = QColor(133, 153, 0);
        t.type = QColor(181, 137, 0);
        t.comment = QColor(88, 110, 117);
        t.string = QColor(42, 161, 152);
        t.number = QColor(211, 54, 130);
        t.preproc = QColor(203, 75, 22);
        t.function = QColor(38, 139, 210);
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