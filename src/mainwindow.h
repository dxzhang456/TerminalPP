#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "editor/thememanager.h"
#include "editor/syntaxhighlighter.h"

class QTabWidget;
class QListWidget;
class QDockWidget;
class QLabel;
class QMenu;
class QActionGroup;
class QTimer;
class EditorTab;
class FindDialog;
class SshTab;
class SerialTab;
class SessionManager;

// The application main window. Hosts a central tab widget that can contain:
//   * Editor tabs (ndd / Notepad-- replica)
//   * SSH terminal tabs (MuxTerm replica)
//   * Serial/UART terminal tabs (PuTTY replica)
// with a shared menu/toolbar/status bar and a session sidebar.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // Open a file into a new editor tab (used by command line + File menu).
    EditorTab *openFile(const QString &path);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    // --- File / editor ---
    void newFile();
    void openFileDialog();
    bool saveFile();
    bool saveFileAs();
    bool saveAll();
    void printFile();
    void closeCurrentTab();

    // --- Edit ---
    void undo();
    void redo();
    void cut();
    void copy();
    void paste();
    void selectAll();
    void showFind();
    void showReplace();
    void gotoLine();

    // --- View ---
    void toggleLineNumbers(bool on);
    void toggleWordWrap(bool on);
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void applyThemeByName(const QString &name);

    // --- Language / encoding / EOL ---
    void setLanguage(int lang);
    void setEncoding(const QString &enc);
    void setEol(int eol);

    // --- Terminal: SSH ---
    void newSshSession();
    void manageSessions();
    void connectSelectedSession();
    void reconnectCurrent();

    // --- Terminal: Serial ---
    void newSerialSession();

    // --- Compare (Beyond Compare-style) ---
    void newComparison();
    void openTextCompare(const QString &left, const QString &right);
    void openFolderCompare(const QString &left, const QString &right);
    void openHexCompare(const QString &left, const QString &right);
    void openImageCompare(const QString &left, const QString &right);

    // --- Clipboard history / auto-save / UI language ---
    void showClipboardHistory();
    void toggleAutoSave(bool on);
    void onAutoSaveTimeout();
    void changeUiLanguage(const QString &code);

    // --- Misc ---
    void about();
    void onCurrentTabChanged(int index);
    void onTabCloseRequested(int index);
    void updateStatusBar();
    void updateWindowTitle();

private:
    void createActions();
    void createMenus();
    void createStatusBar();
    void retranslateUi();          // rebuild menu bar text for the current language
    void loadPreferences();        // restore auto-save (and related) settings

    EditorTab *currentEditorTab() const;
    EditorTab *addEditorTab();
    bool maybeSaveTab(EditorTab *tab);

    // Rebuild the Terminal > Sessions submenu from saved sessions.
    void rebuildSessionsMenu();
    void openSavedSshSession(int index);
    void openSavedSerialSession(int index);
    void deleteSavedSshSession(int index);
    void deleteSavedSerialSession(int index);

    QTabWidget *m_tabs = nullptr;

    SessionManager *m_sessions = nullptr;   // SSH session persistence

    FindDialog *m_findDialog = nullptr;
    ThemeManager m_themeManager;

    // The "Sessions" submenu under the Terminal menu (rebuilt dynamically).
    QMenu *m_sessionsMenu = nullptr;

    // Top-level menus kept so retranslateUi() can refresh their titles.
    QMenu *m_fileMenu = nullptr;
    QMenu *m_editMenu = nullptr;
    QMenu *m_viewMenu = nullptr;
    QMenu *m_langSyntaxMenu = nullptr;  // syntax "Language" menu
    QMenu *m_encMenu = nullptr;
    QMenu *m_termMenu = nullptr;
    QMenu *m_compareMenu = nullptr;     // "Compare" (Beyond Compare) menu
    QMenu *m_uiLangMenu = nullptr;      // "UI Language" menu
    QMenu *m_helpMenu = nullptr;

    // Status bar widgets.
    QLabel *m_posLabel = nullptr;
    QLabel *m_langLabel = nullptr;
    QLabel *m_encLabel = nullptr;
    QLabel *m_eolLabel = nullptr;
    QLabel *m_colLabel = nullptr;   // column (block) edit mode indicator

    // Actions kept for enable/disable toggling.
    QAction *m_actLineNumbers = nullptr;
    QAction *m_actWordWrap = nullptr;
    QAction *m_actAutoSave = nullptr;
    QActionGroup *m_langGroup = nullptr;
    QActionGroup *m_uiLangGroup = nullptr;

    // Auto-save support.
    QTimer *m_autoSaveTimer = nullptr;
    bool m_autoSaveEnabled = false;
    int  m_autoSaveIntervalSec = 30;
};

#endif // MAINWINDOW_H