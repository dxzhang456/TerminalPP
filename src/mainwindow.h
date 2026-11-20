#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "editor/thememanager.h"
#include "editor/syntaxhighlighter.h"
#include "cloud/clouddrivestore.h"

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
class CloudBrowserTab;
class TransferPanel;
class QShortcut;
class CloudProvider;

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

    // Open a file, automatically detecting its type via libmagic:
    //   - image  -> ImageEditorTab (in image editor, fit to view)
    //   - text   -> EditorTab (in code/text editor)
    //   - binary -> HexEditorTab (in hex editor)
    // Returns the created/focused tab widget (EditorTab*, ImageEditorTab*, or HexEditorTab*).
    QWidget *openFile(const QString &path);
    // Open a local (downloaded) file that originated from a cloud drive /
    // shared folder / SFTP / FTP connection, using libmagic to pick the
    // appropriate editor and recording the remote origin if it is text.
    QWidget *openRemoteFile(const QString &localPath, CloudProvider *provider,
                            const QString &remotePath, const QString &driveLabel);

    // Remove the first tab if it is still the untouched default empty document
    // (used after opening command-line files so no blank tab lingers).
    void closeInitialEmptyTab();

protected:
    void closeEvent(QCloseEvent *event) override;
    // Catches double-clicks on empty tab-bar space to open a new file tab.
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    // --- File / editor ---
    void newFile();
    void openFileDialog();
    void openHexEditorDialog();     // pick a file and open it in the hex editor
    void openImageEditorDialog();   // pick a file and open it in the image editor
    void manageFileAssociations();  // register/unregister default-program + Open With
    void openRecentFile();          // open the file from a Recent menu action
    void clearRecentFiles();
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
    // Tick the Language-menu entry that matches the active editor's current
    // (possibly auto-detected) syntax language.
    void syncLanguageMenu();
    void setEncoding(const QString &enc);
    void setEol(int eol);

    // --- Terminal: SSH ---
    void newSshSession();
    void manageSessions();
    void connectSelectedSession();
    void reconnectCurrent();

    // --- Terminal: local shells ---
    void newCmdTerminal();           // open a local cmd.exe terminal tab
    void newPowerShellTerminal();    // open a local PowerShell terminal tab

    // --- Terminal: Serial ---
    void newSerialSession();

    // --- Cloud drives / shared folders / SFTP-FTP ---
    void connectCloudDrive();        // Baidu Pan / WebDAV
    void connectSharedFolder();      // Windows/Samba SMB share
    void connectSftpFtp();           // SFTP or FTP

    // --- Compare ---
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
    void onTabContextMenu(const QPoint &pos);   // right-click a tab
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
    // Close a single tab by index (honours unsaved-save prompts). Returns true
    // if the tab was actually closed (false if the user cancelled).
    bool closeTabAt(int index);
    // Tab-bar context menu helpers.
    void closeOtherTabs(int keepIndex);
    void closeTabsToRight(int index);
    void closeTabsToLeft(int index);
    // Open any file in an editable hex-editor tab (view + edit + save).
    void openHexEditor(const QString &path);
    // Open any image in the full image-editor tab (edit + filters + export).
    void openImageEditor(const QString &path);
    // Open a local shell terminal (0 = cmd, 1 = PowerShell).
    void openLocalShell(int shell);

    // Recent-files list (most-recent first, capped at 20). Persisted in
    // QSettings and rebuilt into the File > Open Recent submenu.
    void addRecentFile(const QString &path);
    void rebuildRecentMenu();
    QStringList recentFiles() const;
    bool maybeSaveTab(EditorTab *tab);
    // After a successful local save of a tab opened from a cloud drive /
    // shared folder / SFTP / FTP, ask the user whether to also push the
    // change back to the remote origin, and do so if confirmed.
    void maybeSaveBackToRemote(EditorTab *tab);

    // Rebuild the Terminal > Sessions submenu from saved sessions.
    void rebuildSessionsMenu();
    void openSavedSshSession(int index);
    void openSavedSerialSession(int index);
    void deleteSavedSshSession(int index);
    void deleteSavedSerialSession(int index);

    // Rebuild the Cloud menu: "Connect Cloud Drive..." followed by one entry
    // per saved drive (labelled "BaiduPan-<name>") with Open / Reconnect /
    // Edit / Delete actions.
    void rebuildCloudMenu();
    // Open a saved drive in a new browser tab (reuses an existing tab if the
    // same drive is already open unless `forceNew` is set).
    void openSavedCloudDrive(int index, bool forceNew = false);
    void reconnectSavedCloudDrive(int index);   // close the tab, then reopen
    void editSavedCloudDrive(int index);        // re-open the dialog to edit
    void deleteSavedCloudDrive(int index);
    // Create (or focus) a browser tab for a drive configuration.
    CloudBrowserTab *openCloudTab(const CloudDriveConfig &cfg);
    // Close any open tab belonging to this drive label. Returns true if one
    // was closed.
    bool closeCloudTab(const QString &label);

    QTabWidget *m_tabs = nullptr;

    SessionManager *m_sessions = nullptr;   // SSH session persistence
    CloudDriveStore m_cloudDrives;          // connected cloud-drive persistence

    // Dockable cloud transfers panel (uploads/downloads with progress).
    QDockWidget *m_transferDock = nullptr;
    TransferPanel *m_transferPanel = nullptr;

    FindDialog *m_findDialog = nullptr;
    ThemeManager m_themeManager;

    // The "Sessions" submenu under the Terminal menu (rebuilt dynamically).
    QMenu *m_sessionsMenu = nullptr;

    // Top-level menus kept so retranslateUi() can refresh their titles.
    QMenu *m_fileMenu = nullptr;
    QMenu *m_recentMenu = nullptr;      // File > Open Recent submenu
    QMenu *m_editMenu = nullptr;
    QMenu *m_viewMenu = nullptr;
    QMenu *m_langSyntaxMenu = nullptr;  // syntax "Language" menu
    QMenu *m_encMenu = nullptr;
    QMenu *m_termMenu = nullptr;
    QMenu *m_compareMenu = nullptr;     // "Compare" menu
    QMenu *m_cloudMenu = nullptr;       // "Connect" menu (cloud drives, shared folders, SFTP/FTP)
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

    // Dedicated zoom shortcuts (registered once; see createMenus()). Using
    // plain QShortcut objects instead of QAction shortcuts avoids ambiguous-
    // shortcut conflicts that previously broke Ctrl+- after fixing Ctrl++.
    QShortcut *m_zoomInShortcut = nullptr;
    QShortcut *m_zoomInShortcut2 = nullptr;
    QShortcut *m_zoomOutShortcut = nullptr;
    QShortcut *m_zoomResetShortcut = nullptr;
};

#endif // MAINWINDOW_H