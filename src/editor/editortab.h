#ifndef EDITORTAB_H
#define EDITORTAB_H

#include <QWidget>
#include <QPointer>
#include "syntaxhighlighter.h"
#include "thememanager.h"

class CodeEditor;
class CloudProvider;   // forward decl: cloud/ftp/sftp/shared-folder provider

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

    // --- Remote origin (set when this file was opened from a cloud drive /
    // shared folder / SFTP / FTP browser tab) ---------------------------------
    // Lets Save/Save As offer to push the edited local copy back to where it
    // came from. `provider` is not owned here; QPointer so it safely becomes
    // null if the originating browser tab (and its provider) is closed.
    //
    // These are implemented in the .cpp (not inline) because QPointer<T> needs
    // the complete type T for its internal static_cast, and CloudProvider is
    // only forward-declared in this header.
    void setRemoteOrigin(CloudProvider *provider, const QString &remotePath,
                         const QString &driveLabel);
    void clearRemoteOrigin();
    // True only while the provider is still alive AND a remote path is set.
    bool hasRemoteOrigin() const;
    CloudProvider *remoteProvider() const;
    QString remotePath() const { return m_remotePath; }
    QString remoteDriveLabel() const { return m_remoteDriveLabel; }

signals:
    void modificationChanged(bool modified);
    void cursorInfoChanged();

private:
    CodeEditor *m_editor = nullptr;
    SyntaxHighlighter *m_highlighter = nullptr;
    QString m_filePath;
    QString m_encoding = "UTF-8";
    Eol m_eol = CRLF;

    QPointer<CloudProvider> m_remoteProvider;
    QString m_remotePath;        // full remote path/id this file came from
    QString m_remoteDriveLabel;  // e.g. "BaiduPan-dx", "SharedFolder-nas"
};

#endif // EDITORTAB_H