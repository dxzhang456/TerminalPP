#ifndef HEXEDITORTAB_H
#define HEXEDITORTAB_H

#include <QWidget>
#include <QByteArray>
#include "editor/thememanager.h"

class HexView;   // internal editable hex grid (defined in .cpp)
class QLabel;

// A single-file hex viewer + editor with save. Shows offset / hex / ASCII
// columns, lets the user overwrite bytes (type hex digits in the hex column or
// printable chars in the ASCII column), and writes the result back to disk.
class HexEditorTab : public QWidget
{
    Q_OBJECT
public:
    explicit HexEditorTab(QWidget *parent = nullptr);

    bool loadFile(const QString &path);
    bool save();                       // write back to the loaded path
    bool saveAs(const QString &path);

    QString filePath() const { return m_path; }
    bool isModified() const;

    void applyTheme(const EditorTheme &theme);

signals:
    void statusMessage(const QString &msg);
    void modificationChanged(bool modified);

private:
    HexView *m_view = nullptr;
    QLabel *m_info = nullptr;
    QString m_path;
};

#endif // HEXEDITORTAB_H