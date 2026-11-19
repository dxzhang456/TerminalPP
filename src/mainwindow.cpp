#include "mainwindow.h"

#include "editor/editortab.h"
#include "editor/codeeditor.h"
#include "editor/finddialog.h"
#include "editor/gotodialog.h"
#include "editor/hexeditortab.h"
#include "terminal/sshtab.h"
#include "terminal/serialtab.h"
#include "terminal/sessiondialog.h"
#include "terminal/serialdialog.h"
#include "terminal/sessionmanager.h"
#include "terminal/terminalwidget.h"
#include "core/i18n.h"
#include "core/cliphistory.h"
#include "core/cliphistorydialog.h"
#include "compare/comparedialog.h"
#include "compare/textcomparetab.h"
#include "compare/foldercomparetab.h"
#include "compare/hexcomparetab.h"
#include "compare/imagecomparetab.h"
#include "cloud/cloudconnectdialog.h"
#include "cloud/cloudbrowsertab.h"
#include "cloud/transferpanel.h"

#include <QTabWidget>
#include <QTabBar>
#include <QListWidget>
#include <QDockWidget>
#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QFileInfo>
#include <QTextCursor>
#include <QInputDialog>
#include <QPrinter>
#include <QPrintDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidgetItem>
#include <QTimer>
#include <QSettings>
#include <QPalette>
#include <QShortcut>

// Shorthand for the runtime UI translator.
static inline QString TR(const char *s) { return I18n::instance()->tr(QString::fromUtf8(s)); }

// Build a Fusion palette from an editor theme so the whole application window
// (menus, dialogs, tab bar, status bar) matches the chosen theme — giving SSH
// and serial tabs the same look as everything else.
static QPalette paletteFromTheme(const EditorTheme &t)
{
    const QColor bg = t.background;
    const QColor fg = t.foreground;
    // A mid shade derived from the background for panels/buttons.
    const bool dark = bg.lightness() < 128;
    const QColor base = dark ? bg.darker(110) : bg.lighter(102);
    const QColor alt  = dark ? bg.lighter(130) : bg.darker(104);
    const QColor button = dark ? bg.lighter(140) : bg.darker(106);

    QPalette p;
    p.setColor(QPalette::Window, bg);
    p.setColor(QPalette::WindowText, fg);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, alt);
    p.setColor(QPalette::Text, fg);
    p.setColor(QPalette::Button, button);
    p.setColor(QPalette::ButtonText, fg);
    p.setColor(QPalette::ToolTipBase, base);
    p.setColor(QPalette::ToolTipText, fg);
    p.setColor(QPalette::Highlight, t.selectionBg);
    p.setColor(QPalette::HighlightedText, dark ? QColor(255, 255, 255) : QColor(0, 0, 0));
    p.setColor(QPalette::PlaceholderText, QColor(fg.red(), fg.green(), fg.blue(), 128));
    // Disabled roles.
    const QColor disabled(fg.red(), fg.green(), fg.blue(), 110);
    p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    p.setColor(QPalette::Disabled, QPalette::Text, disabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    return p;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Terminal++");
    resize(1100, 750);

    m_tabs = new QTabWidget(this);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);
    setCentralWidget(m_tabs);

    connect(m_tabs, &QTabWidget::currentChanged,
            this, &MainWindow::onCurrentTabChanged);
    connect(m_tabs, &QTabWidget::tabCloseRequested,
            this, &MainWindow::onTabCloseRequested);
    // Right-clicking a tab shows a context menu (close current/others/left/right).
    m_tabs->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tabs->tabBar(), &QTabBar::customContextMenuRequested,
            this, &MainWindow::onTabContextMenu);
    // Double-clicking empty space on the tab bar (to the right of the last
    // tab) opens a new file tab, like many editors (VS Code, Notepad++).
    m_tabs->tabBar()->installEventFilter(this);

    m_sessions = new SessionManager();
    m_findDialog = new FindDialog(this);

    // Dockable "Transfers" panel showing all cloud uploads/downloads.
    m_transferDock = new QDockWidget(TR("Transfers"), this);
    m_transferDock->setObjectName("TransfersDock");
    m_transferPanel = new TransferPanel(m_transferDock);
    m_transferDock->setWidget(m_transferPanel);
    addDockWidget(Qt::BottomDockWidgetArea, m_transferDock);
    m_transferDock->hide();   // shown on demand / when a transfer starts
    connect(m_transferPanel, &TransferPanel::openLocalFile, this,
            [this](const QString &lp) { openFile(lp); });
    connect(m_findDialog, &FindDialog::statusMessage, this, [this](const QString &m) {
        statusBar()->showMessage(m, 4000);
    });

    createActions();
    createMenus();
    createStatusBar();

    // Auto-save timer (interval applied when enabled via loadPreferences()).
    m_autoSaveTimer = new QTimer(this);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::onAutoSaveTimeout);

    // Re-translate the whole menu bar whenever the UI language changes.
    connect(I18n::instance(), &I18n::languageChanged,
            this, [this](const QString &) { retranslateUi(); });

    loadPreferences();

    // No document is opened automatically at startup; the window starts empty
    // (files are opened via File > Open / Open Recent / command line).
    updateWindowTitle();
    updateStatusBar();
}

MainWindow::~MainWindow()
{
    delete m_sessions;
}

// ------------------------------------------------------------------
// Actions
// ------------------------------------------------------------------
void MainWindow::createActions()
{
    m_actLineNumbers = new QAction(TR("Show &Line Numbers"), this);
    m_actLineNumbers->setCheckable(true);
    m_actLineNumbers->setChecked(true);
    connect(m_actLineNumbers, &QAction::toggled, this, &MainWindow::toggleLineNumbers);

    m_actWordWrap = new QAction(TR("&Word Wrap"), this);
    m_actWordWrap->setCheckable(true);
    connect(m_actWordWrap, &QAction::toggled, this, &MainWindow::toggleWordWrap);
}

void MainWindow::createMenus()
{
    // ---------- File ----------
    m_fileMenu = menuBar()->addMenu(TR("&File"));
    m_fileMenu->addAction(TR("&New"), QKeySequence::New, this, &MainWindow::newFile);
    m_fileMenu->addAction(TR("&Open..."), QKeySequence::Open, this, &MainWindow::openFileDialog);
    m_fileMenu->addAction(TR("Open as &Hex..."), this, &MainWindow::openHexEditorDialog);
    m_recentMenu = m_fileMenu->addMenu(TR("Open &Recent"));
    rebuildRecentMenu();
    m_fileMenu->addAction(TR("&Save"), QKeySequence::Save, this, &MainWindow::saveFile);
    m_fileMenu->addAction(TR("Save &As..."), QKeySequence::SaveAs, this, &MainWindow::saveFileAs);
    m_fileMenu->addAction(TR("Save A&ll"), QKeySequence("Ctrl+Shift+S"), this, &MainWindow::saveAll);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(TR("&Print..."), QKeySequence::Print, this, &MainWindow::printFile);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(TR("&Close Tab"), QKeySequence("Ctrl+W"), this, &MainWindow::closeCurrentTab);
    m_fileMenu->addAction(TR("E&xit"), QKeySequence("Ctrl+Q"), this, &MainWindow::close);

    // ---------- Edit ----------
    m_editMenu = menuBar()->addMenu(TR("&Edit"));
    m_editMenu->addAction(TR("&Undo"), QKeySequence::Undo, this, &MainWindow::undo);
    m_editMenu->addAction(TR("&Redo"), QKeySequence::Redo, this, &MainWindow::redo);
    m_editMenu->addSeparator();
    m_editMenu->addAction(TR("Cu&t"), QKeySequence::Cut, this, &MainWindow::cut);
    m_editMenu->addAction(TR("&Copy"), QKeySequence::Copy, this, &MainWindow::copy);
    m_editMenu->addAction(TR("&Paste"), QKeySequence::Paste, this, &MainWindow::paste);
    m_editMenu->addAction(TR("Select &All"), QKeySequence::SelectAll, this, &MainWindow::selectAll);
    m_editMenu->addSeparator();
    m_editMenu->addAction(TR("&Find..."), QKeySequence::Find, this, &MainWindow::showFind);
    m_editMenu->addAction(TR("&Replace..."), QKeySequence::Replace, this, &MainWindow::showReplace);
    m_editMenu->addAction(TR("&Go To Line..."), QKeySequence("Ctrl+G"), this, &MainWindow::gotoLine);
    m_editMenu->addSeparator();
    // Clipboard history picker (Ctrl+Shift+V).
    m_editMenu->addAction(TR("Clipboard &History"), QKeySequence("Ctrl+Shift+V"),
                          this, &MainWindow::showClipboardHistory);
    {
        // Column (block / rectangular) editing: a non-clickable hint entry so
        // users discover both the Alt+drag gesture and the VS Code style
        // Ctrl+Shift+Up/Down keyboard shortcut.
        QAction *colHint = m_editMenu->addAction(
            TR("Column / Block Edit (Alt+Drag or Ctrl+Shift+Up/Down)"));
        colHint->setEnabled(false);
    }

    // ---------- View ----------
    m_viewMenu = menuBar()->addMenu(TR("&View"));
    // Keep the two persistent toggle actions' text in sync with the language.
    m_actLineNumbers->setText(TR("Show &Line Numbers"));
    m_actWordWrap->setText(TR("&Word Wrap"));
    m_viewMenu->addAction(m_actLineNumbers);
    m_viewMenu->addAction(m_actWordWrap);
    m_viewMenu->addSeparator();
    // Zoom In/Out/Reset: the menu actions carry NO keyboard shortcut of their
    // own (mouse-clickable only). The actual accelerators are registered once,
    // below, as plain QShortcut objects — this avoids any chance of duplicate/
    // ambiguous shortcut registration between the action and other widgets,
    // which previously made Ctrl+- stop working after Ctrl++ was fixed.
    QAction *zIn = m_viewMenu->addAction(TR("Zoom &In"), this, &MainWindow::zoomIn);
    QAction *zOut = m_viewMenu->addAction(TR("Zoom &Out"), this, &MainWindow::zoomOut);
    QAction *zReset = m_viewMenu->addAction(TR("&Reset Zoom"), this, &MainWindow::zoomReset);
    Q_UNUSED(zIn); Q_UNUSED(zOut); Q_UNUSED(zReset);
    if (!m_zoomInShortcut) {
        // '+' is a shifted key on most layouts (Ctrl+Shift+Equal physically),
        // while '=' is unshifted; register both physical-key variants so
        // Ctrl++ always works regardless of keyboard layout/driver quirks.
        m_zoomInShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus), this);
        m_zoomInShortcut->setContext(Qt::ApplicationShortcut);
        connect(m_zoomInShortcut, &QShortcut::activated, this, &MainWindow::zoomIn);
        m_zoomInShortcut2 = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal), this);
        m_zoomInShortcut2->setContext(Qt::ApplicationShortcut);
        connect(m_zoomInShortcut2, &QShortcut::activated, this, &MainWindow::zoomIn);

        m_zoomOutShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus), this);
        m_zoomOutShortcut->setContext(Qt::ApplicationShortcut);
        connect(m_zoomOutShortcut, &QShortcut::activated, this, &MainWindow::zoomOut);

        m_zoomResetShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_0), this);
        m_zoomResetShortcut->setContext(Qt::ApplicationShortcut);
        connect(m_zoomResetShortcut, &QShortcut::activated, this, &MainWindow::zoomReset);
    }
    m_viewMenu->addSeparator();
    // Auto-save toggle.
    m_actAutoSave = m_viewMenu->addAction(TR("&Auto Save"));
    m_actAutoSave->setCheckable(true);
    connect(m_actAutoSave, &QAction::toggled, this, &MainWindow::toggleAutoSave);
    m_viewMenu->addSeparator();
    // Show/hide the cloud Transfers dock. The dock provides its own
    // toggleViewAction so the checkbox state tracks visibility automatically.
    if (m_transferDock) {
        QAction *showTransfers = m_transferDock->toggleViewAction();
        showTransfers->setText(TR("Show &Transfers Panel"));
        m_viewMenu->addAction(showTransfers);
    }

    // Themes submenu.
    QMenu *themeMenu = m_viewMenu->addMenu(TR("&Theme"));
    QActionGroup *themeGroup = new QActionGroup(this);
    for (const QString &name : m_themeManager.themeNames()) {
        QAction *a = themeMenu->addAction(name);
        a->setCheckable(true);
        if (name == m_themeManager.currentName())
            a->setChecked(true);
        themeGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, name]() { applyThemeByName(name); });
    }

    // ---------- Language (syntax) ----------
    m_langSyntaxMenu = menuBar()->addMenu(TR("&Language"));
    m_langGroup = new QActionGroup(this);
    struct LangItem { const char *name; int id; };
    const LangItem langs[] = {
        {"Plain Text", SyntaxHighlighter::PlainText},
        {"C/C++",      SyntaxHighlighter::Cpp},
        {"C#",         SyntaxHighlighter::CSharp},
        {"Java",       SyntaxHighlighter::Java},
        {"Python",     SyntaxHighlighter::Python},
        {"JavaScript", SyntaxHighlighter::JavaScript},
        {"JSON",       SyntaxHighlighter::Json},
        {"XML",        SyntaxHighlighter::Xml},
        {"HTML",       SyntaxHighlighter::Html},
        {"SQL",        SyntaxHighlighter::Sql},
        {"Shell",      SyntaxHighlighter::Shell},
        {"INI",        SyntaxHighlighter::Ini},
    };
    for (const auto &li : langs) {
        QAction *a = m_langSyntaxMenu->addAction(li.name);
        a->setCheckable(true);
        m_langGroup->addAction(a);
        int id = li.id;
        connect(a, &QAction::triggered, this, [this, id]() { setLanguage(id); });
    }

    // ---------- Encoding ----------
    m_encMenu = menuBar()->addMenu(TR("E&ncoding"));
    const QStringList encs = {"UTF-8", "UTF-16LE", "UTF-16BE", "System", "ISO-8859-1", "GB18030"};
    for (const QString &e : encs) {
        QAction *a = m_encMenu->addAction(e);
        connect(a, &QAction::triggered, this, [this, e]() { setEncoding(e); });
    }
    m_encMenu->addSeparator();
    QMenu *eolMenu = m_encMenu->addMenu(TR("Line Ending"));
    eolMenu->addAction(TR("Windows (CRLF)"), this, [this]() { setEol(EditorTab::CRLF); });
    eolMenu->addAction(TR("Unix (LF)"), this, [this]() { setEol(EditorTab::LF); });
    eolMenu->addAction(TR("Mac (CR)"), this, [this]() { setEol(EditorTab::CR); });

    // ---------- Terminal ----------
    m_termMenu = menuBar()->addMenu(TR("&Terminal"));
    m_termMenu->addAction(TR("New &SSH Session..."), QKeySequence("Ctrl+N"), this, &MainWindow::newSshSession)
        ->setShortcutContext(Qt::ApplicationShortcut);
    m_termMenu->addAction(TR("New Serial / &UART Session..."), this, &MainWindow::newSerialSession);
    m_termMenu->addAction(TR("&Reconnect Current"), QKeySequence("Ctrl+Shift+R"), this, &MainWindow::reconnectCurrent);
    m_termMenu->addSeparator();

    // SSH / Serial Sessions submenu.
    m_sessionsMenu = m_termMenu->addMenu(TR("SSH / Serial &Sessions"));
    rebuildSessionsMenu();

    // ---------- Compare ----------
    m_compareMenu = menuBar()->addMenu(TR("&Compare"));
    m_compareMenu->addAction(TR("&New Comparison..."), QKeySequence("Ctrl+Shift+K"),
                             this, &MainWindow::newComparison);
    m_compareMenu->addSeparator();
    m_compareMenu->addAction(TR("&Text Compare..."), this, [this]() {
        CompareDialog dlg(this);
        dlg.setType(CompareDialog::TextCompare);   // preselect matching type
        if (dlg.exec() == QDialog::Accepted)
            openTextCompare(dlg.leftPath(), dlg.rightPath());
    });
    m_compareMenu->addAction(TR("&Folder Compare..."), this, [this]() {
        CompareDialog dlg(this);
        dlg.setType(CompareDialog::FolderCompare);
        if (dlg.exec() == QDialog::Accepted)
            openFolderCompare(dlg.leftPath(), dlg.rightPath());
    });
    m_compareMenu->addAction(TR("&Hex Compare..."), this, [this]() {
        CompareDialog dlg(this);
        dlg.setType(CompareDialog::HexCompare);
        if (dlg.exec() == QDialog::Accepted)
            openHexCompare(dlg.leftPath(), dlg.rightPath());
    });
    m_compareMenu->addAction(TR("&Picture Compare..."), this, [this]() {
        CompareDialog dlg(this);
        dlg.setType(CompareDialog::ImageCompare);
        if (dlg.exec() == QDialog::Accepted)
            openImageCompare(dlg.leftPath(), dlg.rightPath());
    });

    // ---------- Connect (cloud drives / shared folders / SFTP-FTP) ----------
    // Three "Connect ..." entries followed by every saved connection; the
    // entries are (re)built by rebuildCloudMenu() so they stay in sync after
    // add/delete, regardless of which backend type they are.
    m_cloudMenu = menuBar()->addMenu(TR("Co&nnect"));
    rebuildCloudMenu();

    // ---------- UI Language ----------
    m_uiLangMenu = menuBar()->addMenu(TR("&UI Language"));
    m_uiLangGroup = new QActionGroup(this);
    m_uiLangGroup->setExclusive(true);
    for (const QString &code : I18n::instance()->languageCodes()) {
        QAction *a = m_uiLangMenu->addAction(I18n::instance()->languageDisplayName(code));
        a->setCheckable(true);
        a->setData(code);
        if (code == I18n::instance()->currentLanguage())
            a->setChecked(true);
        m_uiLangGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, code]() { changeUiLanguage(code); });
    }

    // ---------- Help ----------
    m_helpMenu = menuBar()->addMenu(TR("&Help"));
    m_helpMenu->addAction(TR("&About Terminal++"), this, &MainWindow::about);
}

void MainWindow::createStatusBar()
{
    m_posLabel = new QLabel("Ln 1, Col 1", this);
    m_langLabel = new QLabel("Plain Text", this);
    m_encLabel = new QLabel("UTF-8", this);
    m_eolLabel = new QLabel("Windows (CRLF)", this);
    m_colLabel = new QLabel("", this);
    // Make the column-mode indicator stand out when active.
    m_colLabel->setStyleSheet("QLabel { color: #1060d0; font-weight: bold; }");

    statusBar()->addPermanentWidget(m_colLabel);
    statusBar()->addPermanentWidget(m_posLabel);
    statusBar()->addPermanentWidget(m_langLabel);
    statusBar()->addPermanentWidget(m_encLabel);
    statusBar()->addPermanentWidget(m_eolLabel);
    statusBar()->showMessage(TR("Ready"));
}

// ------------------------------------------------------------------
// Sessions submenu (Terminal > SSH / Serial Sessions)
// ------------------------------------------------------------------
void MainWindow::rebuildSessionsMenu()
{
    if (!m_sessionsMenu)
        return;
    m_sessionsMenu->clear();

    // ---- SSH saved sessions ----
    QMenu *sshMenu = m_sessionsMenu->addMenu(TR("SSH"));
    sshMenu->addAction(TR("+ New SSH Session..."), this, &MainWindow::newSshSession);
    sshMenu->addSeparator();
    const auto &ssh = m_sessions->sessions();
    if (ssh.isEmpty()) {
        QAction *none = sshMenu->addAction(TR("(no saved SSH sessions)"));
        none->setEnabled(false);
    } else {
        for (int i = 0; i < ssh.size(); ++i) {
            QMenu *item = sshMenu->addMenu(ssh[i].displayName());
            int idx = i;
            item->addAction(TR("Open"), this, [this, idx]() { openSavedSshSession(idx); });
            item->addAction(TR("Delete"), this, [this, idx]() { deleteSavedSshSession(idx); });
        }
    }

    // ---- Serial saved sessions ----
    QMenu *serMenu = m_sessionsMenu->addMenu(TR("Serial / UART"));
    serMenu->addAction(TR("+ New Serial Session..."), this, &MainWindow::newSerialSession);
    serMenu->addSeparator();
    const auto &ser = m_sessions->serialSessions();
    if (ser.isEmpty()) {
        QAction *none = serMenu->addAction(TR("(no saved serial sessions)"));
        none->setEnabled(false);
    } else {
        for (int i = 0; i < ser.size(); ++i) {
            QMenu *item = serMenu->addMenu(ser[i].displayName());
            int idx = i;
            item->addAction(TR("Open"), this, [this, idx]() { openSavedSerialSession(idx); });
            item->addAction(TR("Delete"), this, [this, idx]() { deleteSavedSerialSession(idx); });
        }
    }
}

// ------------------------------------------------------------------
// Cloud menu: "Connect Cloud Drive..." + one submenu per saved drive
// ------------------------------------------------------------------
void MainWindow::rebuildCloudMenu()
{
    if (!m_cloudMenu)
        return;
    m_cloudMenu->clear();

    // Three distinct "Connect ..." entries, one per CloudConnectDialog mode.
    m_cloudMenu->addAction(TR("&Connect Cloud Drive..."), this,
                           &MainWindow::connectCloudDrive);
    m_cloudMenu->addAction(TR("Connect S&hared Folder..."), this,
                           &MainWindow::connectSharedFolder);
    m_cloudMenu->addAction(TR("Connect SFTP/&FTP..."), this,
                           &MainWindow::connectSftpFtp);
    m_cloudMenu->addSeparator();

    // Every saved connection (cloud drive, shared folder, or SFTP/FTP) is
    // listed the same way, e.g. "BaiduPan-dx", "SharedFolder-nas",
    // "SFTP-server1", each with Open / Reconnect / Edit / Delete.
    const auto &drives = m_cloudDrives.drives();
    if (drives.isEmpty()) {
        QAction *none = m_cloudMenu->addAction(TR("(no connected cloud drives)"));
        none->setEnabled(false);
        return;
    }
    m_cloudMenu->addSeparator();
    for (int i = 0; i < drives.size(); ++i) {
        QMenu *item = m_cloudMenu->addMenu(drives[i].displayName());
        const int idx = i;
        item->addAction(TR("Open"), this, [this, idx]() { openSavedCloudDrive(idx); });
        item->addAction(TR("Reconnect"), this, [this, idx]() { reconnectSavedCloudDrive(idx); });
        item->addAction(TR("Edit..."), this, [this, idx]() { editSavedCloudDrive(idx); });
        item->addSeparator();
        item->addAction(TR("Delete"), this, [this, idx]() { deleteSavedCloudDrive(idx); });
    }
}

// Maps a stored backend id to the CloudConnectDialog mode whose combo box
// offers it, so editSavedCloudDrive() reopens the dialog in the right mode.
static CloudConnectDialog::Mode modeForBackend(int backend)
{
    switch (backend) {
    case CB_SmbShare: return CloudConnectDialog::ModeShare;
    case CB_Sftp:
    case CB_Ftp:      return CloudConnectDialog::ModeFtp;
    default:          return CloudConnectDialog::ModeCloud;
    }
}

// ------------------------------------------------------------------
// Editor tab helpers
// ------------------------------------------------------------------
EditorTab *MainWindow::currentEditorTab() const
{
    return qobject_cast<EditorTab *>(m_tabs->currentWidget());
}

EditorTab *MainWindow::addEditorTab()
{
    EditorTab *tab = new EditorTab(this);
    tab->applyTheme(m_themeManager.current());
    tab->editor()->setShowLineNumbers(m_actLineNumbers->isChecked());
    // Apply the current Word Wrap state so a newly opened file wraps
    // immediately instead of requiring a manual toggle off/on.
    tab->editor()->setLineWrapMode(m_actWordWrap->isChecked()
                                       ? QPlainTextEdit::WidgetWidth
                                       : QPlainTextEdit::NoWrap);

    int idx = m_tabs->addTab(tab, tab->fileName());
    m_tabs->setCurrentIndex(idx);

    connect(tab, &EditorTab::modificationChanged, this, [this, tab](bool) {
        int i = m_tabs->indexOf(tab);
        if (i >= 0) {
            QString title = tab->fileName();
            if (tab->isModified())
                title += " *";
            m_tabs->setTabText(i, title);
        }
        updateWindowTitle();
    });
    connect(tab, &EditorTab::cursorInfoChanged, this, &MainWindow::updateStatusBar);

    // Column (block) edit mode indicator in the status bar.
    connect(tab->editor(), &CodeEditor::columnModeChanged, this,
            [this, tab](bool active) {
        // Only reflect the state of the currently visible tab.
        if (m_tabs->currentWidget() != tab)
            return;
        if (active) {
            m_colLabel->setText(TR("COLUMN"));
            statusBar()->showMessage(
                TR("Column (block) edit mode — type/Delete/Ctrl+C/Ctrl+V across "
                   "lines, Esc to exit"), 4000);
        } else {
            m_colLabel->setText("");
        }
    });

    return tab;
}

bool MainWindow::maybeSaveTab(EditorTab *tab)
{
    if (!tab || !tab->isModified())
        return true;

    QMessageBox::StandardButton ret = QMessageBox::warning(
        this, TR("Terminal++"),
        TR("The document \"%1\" has unsaved changes.\nDo you want to save?")
            .arg(tab->fileName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (ret == QMessageBox::Save) {
        m_tabs->setCurrentWidget(tab);
        return saveFile();
    }
    return ret != QMessageBox::Cancel;
}

// ------------------------------------------------------------------
// File operations
// ------------------------------------------------------------------
void MainWindow::newFile()
{
    addEditorTab();
    updateStatusBar();
}

EditorTab *MainWindow::openFile(const QString &path)
{
    if (path.isEmpty())
        return nullptr;

    // If already open, focus it.
    for (int i = 0; i < m_tabs->count(); ++i) {
        EditorTab *t = qobject_cast<EditorTab *>(m_tabs->widget(i));
        if (t && t->filePath() == path) {
            m_tabs->setCurrentIndex(i);
            return t;
        }
    }

    EditorTab *tab = addEditorTab();
    if (!tab->loadFile(path)) {
        QMessageBox::warning(this, TR("Open"), TR("Cannot open file:\n%1").arg(path));
        return nullptr;
    }
    tab->applyTheme(m_themeManager.current());
    int i = m_tabs->indexOf(tab);
    m_tabs->setTabText(i, tab->fileName());
    m_tabs->setTabToolTip(i, tab->filePath());
    addRecentFile(tab->filePath());   // remember in File > Open Recent
    updateWindowTitle();
    updateStatusBar();
    return tab;
}

// --------------------------------------------------------------------------
// Recent files (File > Open Recent), capped at 20, persisted in QSettings.
// --------------------------------------------------------------------------
QStringList MainWindow::recentFiles() const
{
    QSettings s;
    return s.value(QStringLiteral("recent/files")).toStringList();
}

void MainWindow::addRecentFile(const QString &path)
{
    if (path.isEmpty())
        return;
    QSettings s;
    QStringList list = s.value(QStringLiteral("recent/files")).toStringList();
    list.removeAll(path);           // de-dup
    list.prepend(path);             // most-recent first
    while (list.size() > 20)        // cap at 20
        list.removeLast();
    s.setValue(QStringLiteral("recent/files"), list);
    rebuildRecentMenu();
}

void MainWindow::rebuildRecentMenu()
{
    if (!m_recentMenu)
        return;
    m_recentMenu->clear();
    const QStringList list = recentFiles();
    if (list.isEmpty()) {
        QAction *none = m_recentMenu->addAction(TR("(no recent files)"));
        none->setEnabled(false);
        return;
    }
    for (const QString &p : list) {
        QAction *a = m_recentMenu->addAction(p);
        a->setData(p);
        connect(a, &QAction::triggered, this, &MainWindow::openRecentFile);
    }
    m_recentMenu->addSeparator();
    m_recentMenu->addAction(TR("Clear Recent Files"), this, &MainWindow::clearRecentFiles);
}

void MainWindow::openRecentFile()
{
    if (auto *a = qobject_cast<QAction *>(sender())) {
        const QString path = a->data().toString();
        if (!path.isEmpty())
            openFile(path);
    }
}

void MainWindow::clearRecentFiles()
{
    QSettings s;
    s.remove(QStringLiteral("recent/files"));
    rebuildRecentMenu();
}

void MainWindow::openFileDialog()
{
    QStringList files = QFileDialog::getOpenFileNames(this, TR("Open Files"));
    for (const QString &f : files)
        openFile(f);
}

void MainWindow::openHexEditorDialog()
{
    const QString f = QFileDialog::getOpenFileName(this, TR("Open as Hex"));
    if (!f.isEmpty())
        openHexEditor(f);
}

// Open a file in an editable hex-editor tab (view / edit bytes / save).
void MainWindow::openHexEditor(const QString &path)
{
    auto *tab = new HexEditorTab(this);
    tab->applyTheme(m_themeManager.current());
    if (!tab->loadFile(path)) {
        delete tab;
        QMessageBox::warning(this, TR("Open"), TR("Cannot open file:\n%1").arg(path));
        return;
    }
    connect(tab, &HexEditorTab::statusMessage, this, [this](const QString &m) {
        statusBar()->showMessage(m, 4000);
    });
    connect(tab, &HexEditorTab::modificationChanged, this, [this, tab](bool mod) {
        int i = m_tabs->indexOf(tab);
        if (i >= 0) {
            QString t = TR("Hex: %1").arg(QFileInfo(tab->filePath()).fileName());
            m_tabs->setTabText(i, mod ? t + " *" : t);
        }
    });
    int idx = m_tabs->addTab(tab, TR("Hex: %1").arg(QFileInfo(path).fileName()));
    m_tabs->setTabToolTip(idx, path);
    m_tabs->setCurrentIndex(idx);
}

void MainWindow::closeInitialEmptyTab()
{
    // The first tab is the auto-created empty document. If it is still an
    // untitled, unmodified editor and there is more than one tab, drop it so
    // opening a file from the command line doesn't leave a blank tab behind.
    if (m_tabs->count() < 2)
        return;
    EditorTab *first = qobject_cast<EditorTab *>(m_tabs->widget(0));
    if (first && first->isUntitled() && !first->isModified()) {
        m_tabs->removeTab(0);
        first->deleteLater();
    }
}

bool MainWindow::saveFile()
{
    // Hex-editor tab: save the edited bytes back to disk.
    if (auto *hex = qobject_cast<HexEditorTab *>(m_tabs->currentWidget())) {
        if (hex->save()) {
            statusBar()->showMessage(TR("Saved %1").arg(hex->filePath()), 3000);
            return true;
        }
        QMessageBox::warning(this, TR("Save"), TR("Cannot write file."));
        return false;
    }
    EditorTab *tab = currentEditorTab();
    if (!tab)
        return false;
    if (tab->isUntitled())
        return saveFileAs();
    if (!tab->save()) {
        QMessageBox::warning(this, TR("Save"), TR("Cannot write file."));
        return false;
    }
    statusBar()->showMessage(TR("Saved %1").arg(tab->filePath()), 3000);
    updateWindowTitle();
    return true;
}

bool MainWindow::saveFileAs()
{
    // Hex-editor tab: choose a new path and save the edited bytes.
    if (auto *hex = qobject_cast<HexEditorTab *>(m_tabs->currentWidget())) {
        QString path = QFileDialog::getSaveFileName(this, TR("Save As"),
                                                    hex->filePath());
        if (path.isEmpty())
            return false;
        if (!hex->saveAs(path)) {
            QMessageBox::warning(this, TR("Save"), TR("Cannot write file."));
            return false;
        }
        int i = m_tabs->indexOf(hex);
        m_tabs->setTabText(i, TR("Hex: %1").arg(QFileInfo(path).fileName()));
        m_tabs->setTabToolTip(i, path);
        return true;
    }
    EditorTab *tab = currentEditorTab();
    if (!tab)
        return false;
    QString path = QFileDialog::getSaveFileName(this, TR("Save As"), tab->fileName());
    if (path.isEmpty())
        return false;
    if (!tab->saveFile(path)) {
        QMessageBox::warning(this, TR("Save"), TR("Cannot write file."));
        return false;
    }
    int i = m_tabs->indexOf(tab);
    m_tabs->setTabText(i, tab->fileName());
    m_tabs->setTabToolTip(i, tab->filePath());
    statusBar()->showMessage(TR("Saved %1").arg(tab->filePath()), 3000);
    updateWindowTitle();
    updateStatusBar();
    return true;
}

bool MainWindow::saveAll()
{
    bool allOk = true;
    for (int i = 0; i < m_tabs->count(); ++i) {
        EditorTab *t = qobject_cast<EditorTab *>(m_tabs->widget(i));
        if (t && t->isModified()) {
            m_tabs->setCurrentIndex(i);
            if (!saveFile())
                allOk = false;
        }
    }
    return allOk;
}

void MainWindow::printFile()
{
    EditorTab *tab = currentEditorTab();
    if (!tab)
        return;
    QPrinter printer;
    QPrintDialog dlg(&printer, this);
    if (dlg.exec() == QDialog::Accepted)
        tab->editor()->print(&printer);
}

void MainWindow::closeCurrentTab()
{
    onTabCloseRequested(m_tabs->currentIndex());
}

// Close a single tab honouring unsaved-save prompts. Returns true if the tab
// was actually closed (false if the user cancelled a save prompt).
bool MainWindow::closeTabAt(int index)
{
    QWidget *w = m_tabs->widget(index);
    if (!w)
        return false;

    if (EditorTab *tab = qobject_cast<EditorTab *>(w)) {
        if (!maybeSaveTab(tab))
            return false;
    } else if (auto *hex = qobject_cast<HexEditorTab *>(w)) {
        if (hex->isModified()) {
            auto ret = QMessageBox::warning(this, TR("Terminal++"),
                TR("The document \"%1\" has unsaved changes.\nDo you want to save?")
                    .arg(QFileInfo(hex->filePath()).fileName()),
                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
            if (ret == QMessageBox::Cancel) return false;
            if (ret == QMessageBox::Save) hex->save();
        }
    } else if (SshTab *s = qobject_cast<SshTab *>(w)) {
        s->disconnectSession();
    } else if (SerialTab *s = qobject_cast<SerialTab *>(w)) {
        s->closePort();
    }

    m_tabs->removeTab(index);
    w->deleteLater();
    return true;
}

void MainWindow::onTabCloseRequested(int index)
{
    // Closing the last tab no longer re-creates an empty document — the window
    // simply becomes empty.
    closeTabAt(index);
    updateWindowTitle();
    updateStatusBar();
}

// --- Tab-bar context menu -------------------------------------------------
void MainWindow::closeOtherTabs(int keepIndex)
{
    // Remember the kept widget; close everything else (right-to-left so indices
    // stay valid). Stop if a save prompt is cancelled.
    QWidget *keep = m_tabs->widget(keepIndex);
    for (int i = m_tabs->count() - 1; i >= 0; --i) {
        if (m_tabs->widget(i) == keep)
            continue;
        if (!closeTabAt(i))
            break;
    }
    updateWindowTitle();
    updateStatusBar();
}

void MainWindow::closeTabsToRight(int index)
{
    for (int i = m_tabs->count() - 1; i > index; --i)
        if (!closeTabAt(i))
            break;
    updateWindowTitle();
    updateStatusBar();
}

void MainWindow::closeTabsToLeft(int index)
{
    // Close indices [0 .. index-1]; go right-to-left within that range so the
    // remaining indices don't shift under us.
    for (int i = index - 1; i >= 0; --i)
        if (!closeTabAt(i))
            break;
    updateWindowTitle();
    updateStatusBar();
}

void MainWindow::onTabContextMenu(const QPoint &pos)
{
    const int index = m_tabs->tabBar()->tabAt(pos);
    if (index < 0)
        return;
    QMenu menu(this);
    menu.addAction(TR("Close"), this, [this, index]() {
        closeTabAt(index); updateWindowTitle(); updateStatusBar();
    });
    QAction *others = menu.addAction(TR("Close Others"), this,
                                     [this, index]() { closeOtherTabs(index); });
    QAction *right  = menu.addAction(TR("Close to the Right"), this,
                                     [this, index]() { closeTabsToRight(index); });
    QAction *left   = menu.addAction(TR("Close to the Left"), this,
                                     [this, index]() { closeTabsToLeft(index); });
    others->setEnabled(m_tabs->count() > 1);
    right->setEnabled(index < m_tabs->count() - 1);
    left->setEnabled(index > 0);
    menu.exec(m_tabs->tabBar()->mapToGlobal(pos));
}

// ------------------------------------------------------------------
// Edit operations
// ------------------------------------------------------------------
void MainWindow::undo()       { if (EditorTab *t = currentEditorTab()) t->editor()->undo(); }
void MainWindow::redo()       { if (EditorTab *t = currentEditorTab()) t->editor()->redo(); }
void MainWindow::cut()        { if (EditorTab *t = currentEditorTab()) t->editor()->cut(); }
void MainWindow::copy()       { if (EditorTab *t = currentEditorTab()) t->editor()->copy(); }
void MainWindow::paste()      { if (EditorTab *t = currentEditorTab()) t->editor()->paste(); }
void MainWindow::selectAll()  { if (EditorTab *t = currentEditorTab()) t->editor()->selectAll(); }

void MainWindow::showFind()
{
    EditorTab *t = currentEditorTab();
    if (!t)
        return;
    m_findDialog->setEditor(t->editor());
    m_findDialog->showFind();
}

void MainWindow::showReplace()
{
    EditorTab *t = currentEditorTab();
    if (!t)
        return;
    m_findDialog->setEditor(t->editor());
    m_findDialog->showReplace();
}

void MainWindow::gotoLine()
{
    EditorTab *t = currentEditorTab();
    if (!t)
        return;
    CodeEditor *ed = t->editor();
    int maxLine = ed->document()->blockCount();
    int cur = ed->textCursor().blockNumber() + 1;
    GotoDialog dlg(maxLine, cur, this);
    if (dlg.exec() == QDialog::Accepted) {
        QTextCursor c(ed->document()->findBlockByNumber(dlg.line() - 1));
        ed->setTextCursor(c);
        ed->centerCursor();
        ed->setFocus();
    }
}

// ------------------------------------------------------------------
// View operations
// ------------------------------------------------------------------
void MainWindow::toggleLineNumbers(bool on)
{
    for (int i = 0; i < m_tabs->count(); ++i)
        if (EditorTab *t = qobject_cast<EditorTab *>(m_tabs->widget(i)))
            t->editor()->setShowLineNumbers(on);
}

void MainWindow::toggleWordWrap(bool on)
{
    for (int i = 0; i < m_tabs->count(); ++i)
        if (EditorTab *t = qobject_cast<EditorTab *>(m_tabs->widget(i)))
            t->editor()->setLineWrapMode(on ? QPlainTextEdit::WidgetWidth
                                            : QPlainTextEdit::NoWrap);
}

// Adjust the terminal (SSH/Serial) font point size by `delta`, clamped.
static void zoomTerminalFont(TerminalWidget *term, int delta)
{
    QFont f = term->terminalFont();
    int pt = f.pointSize();
    if (pt <= 0) pt = 11;
    pt = qBound(6, pt + delta, 48);
    f.setPointSize(pt);
    term->setTerminalFont(f);
}

void MainWindow::zoomIn()
{
    QWidget *w = m_tabs->currentWidget();
    if (EditorTab *t = qobject_cast<EditorTab *>(w)) t->editor()->zoomIn(1);
    else if (SshTab *s = qobject_cast<SshTab *>(w)) zoomTerminalFont(s->terminal(), +1);
    else if (SerialTab *s = qobject_cast<SerialTab *>(w)) zoomTerminalFont(s->terminal(), +1);
    else if (ImageCompareTab *i = qobject_cast<ImageCompareTab *>(w)) i->zoomIn();
}

void MainWindow::zoomOut()
{
    QWidget *w = m_tabs->currentWidget();
    if (EditorTab *t = qobject_cast<EditorTab *>(w)) t->editor()->zoomOut(1);
    else if (SshTab *s = qobject_cast<SshTab *>(w)) zoomTerminalFont(s->terminal(), -1);
    else if (SerialTab *s = qobject_cast<SerialTab *>(w)) zoomTerminalFont(s->terminal(), -1);
    else if (ImageCompareTab *i = qobject_cast<ImageCompareTab *>(w)) i->zoomOut();
}

void MainWindow::zoomReset()
{
    QWidget *w = m_tabs->currentWidget();
    if (EditorTab *t = qobject_cast<EditorTab *>(w)) {
        QFont f("Consolas", 11);
        f.setStyleHint(QFont::Monospace);
        t->editor()->setFont(f);
    } else if (SshTab *s = qobject_cast<SshTab *>(w)) {
        QFont f("Consolas", 11); f.setStyleHint(QFont::Monospace);
        s->terminal()->setTerminalFont(f);
    } else if (SerialTab *s = qobject_cast<SerialTab *>(w)) {
        QFont f("Consolas", 11); f.setStyleHint(QFont::Monospace);
        s->terminal()->setTerminalFont(f);
    } else if (ImageCompareTab *i = qobject_cast<ImageCompareTab *>(w)) {
        i->zoomFit();
    }
}

void MainWindow::applyThemeByName(const QString &name)
{
    m_themeManager.setCurrent(name);
    const EditorTheme theme = m_themeManager.current();

    // Apply a matching palette to the whole application so menus, dialogs, the
    // tab bar and status bar share the look — this is what makes SSH/Serial
    // tabs (and every other window) consistent with the editor theme.
    qApp->setPalette(paletteFromTheme(theme));

    // Propagate to every open tab, including SSH and Serial terminals.
    for (int i = 0; i < m_tabs->count(); ++i) {
        QWidget *w = m_tabs->widget(i);
        if (EditorTab *t = qobject_cast<EditorTab *>(w))
            t->applyTheme(theme);
        else if (SshTab *s = qobject_cast<SshTab *>(w))
            s->applyTheme(theme);
        else if (SerialTab *s = qobject_cast<SerialTab *>(w))
            s->applyTheme(theme);
    }

    // Persist the chosen theme so it is restored on next launch.
    QSettings st;
    st.setValue(QStringLiteral("ui/theme"), name);

    statusBar()->showMessage(TR("Theme: %1").arg(name), 3000);
}

// ------------------------------------------------------------------
// Language / encoding / EOL
// ------------------------------------------------------------------
void MainWindow::setLanguage(int lang)
{
    EditorTab *t = currentEditorTab();
    if (!t)
        return;
    t->highlighter()->setLanguage(static_cast<SyntaxHighlighter::Language>(lang));
    updateStatusBar();
}

void MainWindow::setEncoding(const QString &enc)
{
    EditorTab *t = currentEditorTab();
    if (!t)
        return;
    // Picking an encoding must actually re-interpret the bytes on disk,
    // otherwise a mis-detected file (e.g. GBK Chinese text) stays garbled and
    // only the status-bar label changes. "System" is kept as-is so the local
    // ANSI codepage (CP936/GBK on Chinese Windows) is used — mapping it to
    // UTF-8 was what made Chinese .txt files unreadable.
    if (!t->filePath().isEmpty() && !t->isModified()) {
        if (t->loadFile(t->filePath(), enc)) {
            t->setEncoding(enc);
            updateStatusBar();
            statusBar()->showMessage(TR("Reopened with encoding: %1").arg(enc), 3000);
            return;
        }
    }
    // Unsaved/untitled document: just record the encoding for the next save.
    t->setEncoding(enc);
    updateStatusBar();
}

void MainWindow::setEol(int eol)
{
    EditorTab *t = currentEditorTab();
    if (!t)
        return;
    t->setEol(static_cast<EditorTab::Eol>(eol));
    updateStatusBar();
}

// ------------------------------------------------------------------
// SSH
// ------------------------------------------------------------------
static void convertSerial(const SerialSessionConfig &c, SerialConfig &out)
{
    out.portName = c.portName;
    out.baudRate = c.baudRate;
    out.dataBits = static_cast<QSerialPort::DataBits>(c.dataBits);
    out.parity = (c.parity == "Even")  ? QSerialPort::EvenParity :
                 (c.parity == "Odd")   ? QSerialPort::OddParity  :
                 (c.parity == "Mark")  ? QSerialPort::MarkParity :
                 (c.parity == "Space") ? QSerialPort::SpaceParity: QSerialPort::NoParity;
    out.stopBits = (c.stopBits == "2")   ? QSerialPort::TwoStop :
                   (c.stopBits == "1.5") ? QSerialPort::OneAndHalfStop : QSerialPort::OneStop;
    out.flow = (c.flow == "RTS-CTS")  ? QSerialPort::HardwareControl :
               (c.flow == "XON-XOFF") ? QSerialPort::SoftwareControl : QSerialPort::NoFlowControl;
    out.lineEnding = c.lineEnding;
    out.localEcho = c.localEcho;
}

static void convertSerialBack(const SerialConfig &c, SerialSessionConfig &out)
{
    out.portName = c.portName;
    out.baudRate = c.baudRate;
    out.dataBits = static_cast<int>(c.dataBits);
    out.parity = (c.parity == QSerialPort::EvenParity)  ? "Even"  :
                 (c.parity == QSerialPort::OddParity)   ? "Odd"   :
                 (c.parity == QSerialPort::MarkParity)  ? "Mark"  :
                 (c.parity == QSerialPort::SpaceParity) ? "Space" : "None";
    out.stopBits = (c.stopBits == QSerialPort::TwoStop)         ? "2"   :
                   (c.stopBits == QSerialPort::OneAndHalfStop)  ? "1.5" : "1";
    out.flow = (c.flow == QSerialPort::HardwareControl) ? "RTS-CTS"  :
               (c.flow == QSerialPort::SoftwareControl) ? "XON-XOFF" : "None";
    out.lineEnding = c.lineEnding;
    out.localEcho = c.localEcho;
}

void MainWindow::newSshSession()
{
    SessionDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    SshSessionConfig cfg = dlg.config();
    if (cfg.host.isEmpty()) {
        QMessageBox::warning(this, TR("SSH"), TR("Host is required."));
        return;
    }

    m_sessions->add(cfg);          // persist
    rebuildSessionsMenu();         // refresh Terminal > Sessions
    openSavedSshSession(m_sessions->sessions().size() - 1);
}

void MainWindow::openSavedSshSession(int index)
{
    const auto &list = m_sessions->sessions();
    if (index < 0 || index >= list.size())
        return;
    SshSessionConfig cfg = list.at(index);

    SshTab *tab = new SshTab(cfg, this);
    tab->applyTheme(m_themeManager.current());   // match the app theme
    int idx = m_tabs->addTab(tab, cfg.displayName());
    m_tabs->setCurrentIndex(idx);

    connect(tab, &SshTab::titleChanged, this, [this, tab](const QString &title) {
        int i = m_tabs->indexOf(tab);
        if (i >= 0 && !title.isEmpty())
            m_tabs->setTabText(i, title);
    });
    connect(tab, &SshTab::connectionClosed, this, [this, tab]() {
        int i = m_tabs->indexOf(tab);
        if (i >= 0)
            m_tabs->setTabText(i, m_tabs->tabText(i) + TR(" [closed]"));
    });

    tab->connectSession();
    tab->terminal()->setFocus();
}

void MainWindow::deleteSavedSshSession(int index)
{
    const auto &list = m_sessions->sessions();
    if (index < 0 || index >= list.size())
        return;
    if (QMessageBox::question(this, TR("Delete Session"),
            TR("Delete SSH session \"%1\"?").arg(list.at(index).displayName()))
        != QMessageBox::Yes)
        return;
    m_sessions->remove(index);
    rebuildSessionsMenu();
}

// Kept for API compatibility (Ctrl+Shift+R).
void MainWindow::connectSelectedSession()
{
    if (!m_sessions->sessions().isEmpty())
        openSavedSshSession(0);
}

void MainWindow::manageSessions()
{
    // Management is now done inline via the Terminal > Sessions submenu.
    rebuildSessionsMenu();
}

void MainWindow::reconnectCurrent()
{
    if (SshTab *s = qobject_cast<SshTab *>(m_tabs->currentWidget()))
        s->reconnect();
}

// ------------------------------------------------------------------
// Serial
// ------------------------------------------------------------------
void MainWindow::newSerialSession()
{
    SerialDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    SerialConfig cfg = dlg.config();
    if (cfg.portName.isEmpty()) {
        QMessageBox::warning(this, TR("Serial"), TR("No serial port selected."));
        return;
    }

    // Persist a serial session entry.
    SerialSessionConfig sc;
    convertSerialBack(cfg, sc);
    m_sessions->addSerial(sc);
    rebuildSessionsMenu();
    openSavedSerialSession(m_sessions->serialSessions().size() - 1);
}

void MainWindow::openSavedSerialSession(int index)
{
    const auto &list = m_sessions->serialSessions();
    if (index < 0 || index >= list.size())
        return;

    SerialConfig cfg;
    convertSerial(list.at(index), cfg);

    SerialTab *tab = new SerialTab(cfg, this);
    tab->applyTheme(m_themeManager.current());   // match the app theme
    QString title = QString("%1 @ %2").arg(cfg.portName).arg(cfg.baudRate);
    int idx = m_tabs->addTab(tab, title);
    m_tabs->setCurrentIndex(idx);

    connect(tab, &SerialTab::connectionClosed, this, [this, tab]() {
        int i = m_tabs->indexOf(tab);
        if (i >= 0)
            m_tabs->setTabText(i, m_tabs->tabText(i) + TR(" [closed]"));
    });

    tab->openPort();
    tab->terminal()->setFocus();
}

void MainWindow::deleteSavedSerialSession(int index)
{
    const auto &list = m_sessions->serialSessions();
    if (index < 0 || index >= list.size())
        return;
    if (QMessageBox::question(this, TR("Delete Session"),
            TR("Delete serial session \"%1\"?").arg(list.at(index).displayName()))
        != QMessageBox::Yes)
        return;
    m_sessions->removeSerial(index);
    rebuildSessionsMenu();
}

// ------------------------------------------------------------------
// Compare
// ------------------------------------------------------------------
void MainWindow::newComparison()
{
    CompareDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const QString l = dlg.leftPath();
    const QString r = dlg.rightPath();
    switch (dlg.type()) {
    case CompareDialog::TextCompare:   openTextCompare(l, r); break;
    case CompareDialog::FolderCompare: openFolderCompare(l, r); break;
    case CompareDialog::HexCompare:    openHexCompare(l, r); break;
    case CompareDialog::ImageCompare:  openImageCompare(l, r); break;
    }
}

void MainWindow::openTextCompare(const QString &left, const QString &right)
{
    if (left.isEmpty() || right.isEmpty())
        return;
    auto *tab = new TextCompareTab(this);
    tab->applyTheme(m_themeManager.current());
    tab->loadFiles(left, right);
    connect(tab, &TextCompareTab::statusMessage, this, [this](const QString &m) {
        statusBar()->showMessage(m, 4000);
    });
    connect(tab, &TextCompareTab::requestCompareAs, this,
            [this](int type, const QString &l, const QString &r) {
        switch (type) {
        case 2: openHexCompare(l, r); break;
        case 3: openImageCompare(l, r); break;
        default: openTextCompare(l, r); break;
        }
    });
    int idx = m_tabs->addTab(tab, TR("Text: %1 ⟷ %2")
                                      .arg(QFileInfo(left).fileName(),
                                           QFileInfo(right).fileName()));
    m_tabs->setCurrentIndex(idx);
}

void MainWindow::openFolderCompare(const QString &left, const QString &right)
{
    if (left.isEmpty() || right.isEmpty())
        return;
    auto *tab = new FolderCompareTab(this);
    tab->applyTheme(m_themeManager.current());
    tab->setFolders(left, right);
    connect(tab, &FolderCompareTab::statusMessage, this, [this](const QString &m) {
        statusBar()->showMessage(m, 4000);
    });
    // Double-clicking a differing file pair dispatches by content type:
    //   image extensions  -> Picture Compare
    //   text extensions   -> Text Compare
    //   everything else   -> Hex Compare
    connect(tab, &FolderCompareTab::compareFiles, this,
            [this](const QString &lf, const QString &rf) {
        const QString ext = QFileInfo(lf).suffix().toLower();
        static const QStringList imgExt = {
            "png","jpg","jpeg","bmp","gif","webp","ico","svg","tif","tiff"};
        static const QStringList textExt = {
            "txt","c","cpp","cc","cxx","h","hpp","hh","cs","java","py","js","ts",
            "json","xml","html","htm","css","sql","sh","bash","ini","cfg","conf",
            "md","log","yml","yaml","pro","pri","cmake","gradle","bat","ps1",
            "rb","go","rs","php","pl","lua","kt","swift","m","mm","vue","toml"};
        if (imgExt.contains(ext))
            openImageCompare(lf, rf);
        else if (textExt.contains(ext))
            openTextCompare(lf, rf);
        else
            openHexCompare(lf, rf);
    });
    int idx = m_tabs->addTab(tab, TR("Folders: %1 ⟷ %2")
                                      .arg(QFileInfo(left).fileName(),
                                           QFileInfo(right).fileName()));
    m_tabs->setCurrentIndex(idx);
}

void MainWindow::openHexCompare(const QString &left, const QString &right)
{
    if (left.isEmpty() || right.isEmpty())
        return;
    auto *tab = new HexCompareTab(this);
    tab->applyTheme(m_themeManager.current());
    tab->loadFiles(left, right);
    connect(tab, &HexCompareTab::statusMessage, this, [this](const QString &m) {
        statusBar()->showMessage(m, 4000);
    });
    connect(tab, &HexCompareTab::requestCompareAs, this,
            [this](int type, const QString &l, const QString &r) {
        switch (type) {
        case 0: openTextCompare(l, r); break;
        case 3: openImageCompare(l, r); break;
        default: openHexCompare(l, r); break;
        }
    });
    int idx = m_tabs->addTab(tab, TR("Hex: %1 ⟷ %2")
                                      .arg(QFileInfo(left).fileName(),
                                           QFileInfo(right).fileName()));
    m_tabs->setCurrentIndex(idx);
}

void MainWindow::openImageCompare(const QString &left, const QString &right)
{
    if (left.isEmpty() || right.isEmpty())
        return;
    auto *tab = new ImageCompareTab(this);
    tab->applyTheme(m_themeManager.current());
    tab->loadFiles(left, right);
    connect(tab, &ImageCompareTab::statusMessage, this, [this](const QString &m) {
        statusBar()->showMessage(m, 4000);
    });
    connect(tab, &ImageCompareTab::requestCompareAs, this,
            [this](int type, const QString &l, const QString &r) {
        switch (type) {
        case 0: openTextCompare(l, r); break;
        case 2: openHexCompare(l, r); break;
        default: openImageCompare(l, r); break;
        }
    });
    int idx = m_tabs->addTab(tab, TR("Picture: %1 ⟷ %2")
                                      .arg(QFileInfo(left).fileName(),
                                           QFileInfo(right).fileName()));
    m_tabs->setCurrentIndex(idx);
}

// ------------------------------------------------------------------
// Cloud drives
// ------------------------------------------------------------------
// Add a new cloud drive: ask for the provider + a drive name + credentials,
// remember it (so it appears in the Cloud menu as "BaiduPan-<name>") and open
// it right away.
void MainWindow::connectCloudDrive()
{
    CloudConnectDialog dlg(CloudConnectDialog::ModeCloud, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const CloudDriveConfig cfg = dlg.config();
    // Validate by building a provider once before storing the configuration.
    CloudProvider *probe = createCloudProvider(cfg, nullptr);
    if (!probe) {
        QMessageBox::warning(this, TR("Cloud"),
                             TR("Missing required connection details."));
        return;
    }
    delete probe;   // the real provider is created by openCloudTab()

    m_cloudDrives.add(cfg);     // persist (tokens encrypted via SecretStore)
    rebuildCloudMenu();         // show it under Connect > Connect Cloud Drive...
    openSavedCloudDrive(m_cloudDrives.count() - 1);
}

// Connect a Windows/Samba shared folder (SMB1/CIFS, SMB2, SMB3 — any dialect
// the server offers; negotiated transparently by the OS once given a UNC
// path). Saved and listed in the Connect menu exactly like a cloud drive.
void MainWindow::connectSharedFolder()
{
    CloudConnectDialog dlg(CloudConnectDialog::ModeShare, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const CloudDriveConfig cfg = dlg.config();
    CloudProvider *probe = createCloudProvider(cfg, nullptr);
    if (!probe) {
        QMessageBox::warning(this, TR("Connect Shared Folder"),
                             TR("Missing required connection details."));
        return;
    }
    delete probe;

    m_cloudDrives.add(cfg);
    rebuildCloudMenu();
    openSavedCloudDrive(m_cloudDrives.count() - 1);
}

// Connect an SFTP (native libssh2) or plain FTP server. Saved and listed in
// the Connect menu exactly like a cloud drive / shared folder.
void MainWindow::connectSftpFtp()
{
    CloudConnectDialog dlg(CloudConnectDialog::ModeFtp, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const CloudDriveConfig cfg = dlg.config();
    CloudProvider *probe = createCloudProvider(cfg, nullptr);
    if (!probe) {
        QMessageBox::warning(this, TR("Connect SFTP/FTP"),
                             TR("Missing required connection details."));
        return;
    }
    delete probe;

    m_cloudDrives.add(cfg);
    rebuildCloudMenu();
    openSavedCloudDrive(m_cloudDrives.count() - 1);
}

// Create (or focus) a browser tab for one drive configuration.
CloudBrowserTab *MainWindow::openCloudTab(const CloudDriveConfig &cfg)
{
    applyCloudProxy(cfg);       // honour the drive's proxy settings

    CloudProvider *provider = createCloudProvider(cfg, nullptr);
    if (!provider) {
        QMessageBox::warning(this, TR("Cloud"),
                             TR("Missing required connection details."));
        return nullptr;
    }

    const QString label = cfg.displayName();
    auto *tab = new CloudBrowserTab(provider, label, this);
    tab->applyTheme(m_themeManager.current());
    connect(tab, &CloudBrowserTab::statusMessage, this, [this](const QString &m) {
        statusBar()->showMessage(m, 4000);
    });
    // A text file downloaded from the cloud opens in an internal editor tab.
    connect(tab, &CloudBrowserTab::openTextFile, this, [this](const QString &lp) {
        openFile(lp);
    });
    // An image file downloaded from the cloud opens in a picture view (compared
    // against itself so the single-image viewer with zoom/pan is reused).
    connect(tab, &CloudBrowserTab::openImageFileCompareNone, this,
            [this](const QString &lp) { openImageCompare(lp, lp); });

    int idx = m_tabs->addTab(tab, TR("☁ %1").arg(label));
    m_tabs->setCurrentIndex(idx);
    // Switching to this cloud tab reveals the Transfers dock via
    // onCurrentTabChanged(); it is hidden again on every other tab type.
    return tab;
}

// Close any already-open tab for this drive label.
bool MainWindow::closeCloudTab(const QString &label)
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *c = qobject_cast<CloudBrowserTab *>(m_tabs->widget(i));
        if (c && c->title() == label) {
            QWidget *w = m_tabs->widget(i);
            m_tabs->removeTab(i);
            w->deleteLater();
            return true;
        }
    }
    return false;
}

// Open: focus the drive's tab if it is already open, otherwise create it.
void MainWindow::openSavedCloudDrive(int index, bool forceNew)
{
    const auto &drives = m_cloudDrives.drives();
    if (index < 0 || index >= drives.size())
        return;
    const CloudDriveConfig cfg = drives.at(index);

    if (!forceNew) {
        for (int i = 0; i < m_tabs->count(); ++i) {
            auto *c = qobject_cast<CloudBrowserTab *>(m_tabs->widget(i));
            if (c && c->title() == cfg.displayName()) {
                m_tabs->setCurrentIndex(i);   // already open -> just focus it
                return;
            }
        }
    }
    openCloudTab(cfg);
}

// Reconnect: drop the existing tab (and its provider/session) and open a fresh
// one using the stored credentials.
void MainWindow::reconnectSavedCloudDrive(int index)
{
    const auto &drives = m_cloudDrives.drives();
    if (index < 0 || index >= drives.size())
        return;
    const CloudDriveConfig cfg = drives.at(index);
    closeCloudTab(cfg.displayName());
    if (openCloudTab(cfg))
        statusBar()->showMessage(TR("Reconnected %1").arg(cfg.displayName()), 3000);
}

// Edit: reopen the connect dialog pre-filled with the stored values so the
// name / token / proxy can be changed, then refresh the menu and any open tab.
void MainWindow::editSavedCloudDrive(int index)
{
    const auto &drives = m_cloudDrives.drives();
    if (index < 0 || index >= drives.size())
        return;
    const CloudDriveConfig oldCfg = drives.at(index);

    CloudConnectDialog dlg(modeForBackend(oldCfg.backend), this);
    dlg.setConfig(oldCfg);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const CloudDriveConfig newCfg = dlg.config();
    CloudProvider *probe = createCloudProvider(newCfg, nullptr);
    if (!probe) {
        QMessageBox::warning(this, TR("Cloud"),
                             TR("Missing required connection details."));
        return;
    }
    delete probe;

    m_cloudDrives.update(index, newCfg);
    rebuildCloudMenu();

    // If the drive was open, reopen it so the new settings take effect.
    if (closeCloudTab(oldCfg.displayName()))
        openCloudTab(newCfg);
    statusBar()->showMessage(TR("Updated %1").arg(newCfg.displayName()), 3000);
}

// Delete: confirm, close its tab, then forget the stored configuration.
void MainWindow::deleteSavedCloudDrive(int index)
{
    const auto &drives = m_cloudDrives.drives();
    if (index < 0 || index >= drives.size())
        return;
    const CloudDriveConfig cfg = drives.at(index);
    if (QMessageBox::question(this, TR("Delete Cloud Drive"),
            TR("Delete cloud drive \"%1\"?").arg(cfg.displayName()))
        != QMessageBox::Yes)
        return;
    closeCloudTab(cfg.displayName());
    m_cloudDrives.remove(index);
    rebuildCloudMenu();
    statusBar()->showMessage(TR("Deleted %1").arg(cfg.displayName()), 3000);
}

// ------------------------------------------------------------------
// Clipboard history / auto-save / UI language
// ------------------------------------------------------------------
void MainWindow::showClipboardHistory()
{
    ClipHistoryDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        const QString text = dlg.selectedText();
        if (text.isEmpty())
            return;
        // Paste into the active editor; terminal tabs receive it via clipboard.
        if (EditorTab *t = currentEditorTab()) {
            t->editor()->insertPlainText(text);
            t->editor()->setFocus();
        }
    }
}

void MainWindow::toggleAutoSave(bool on)
{
    m_autoSaveEnabled = on;
    if (on)
        m_autoSaveTimer->start(m_autoSaveIntervalSec * 1000);
    else
        m_autoSaveTimer->stop();

    QSettings s;
    s.setValue(QStringLiteral("editor/autoSave"), on);

    statusBar()->showMessage(
        on ? TR("Auto-save enabled (every %1s)").arg(m_autoSaveIntervalSec)
           : TR("Auto-save disabled"), 3000);
}

void MainWindow::onAutoSaveTimeout()
{
    // Silently save every modified, already-named editor tab. Untitled tabs are
    // skipped (nothing to save to yet) to avoid popping dialogs unexpectedly.
    int saved = 0;
    for (int i = 0; i < m_tabs->count(); ++i) {
        EditorTab *t = qobject_cast<EditorTab *>(m_tabs->widget(i));
        if (t && t->isModified() && !t->isUntitled()) {
            if (t->save())
                ++saved;
        }
    }
    if (saved > 0) {
        updateWindowTitle();
        statusBar()->showMessage(TR("Auto-saved %1 file(s)").arg(saved), 2000);
    }
}

void MainWindow::changeUiLanguage(const QString &code)
{
    I18n::instance()->setLanguage(code);   // triggers retranslateUi() via signal
    statusBar()->showMessage(
        TR("Language changed. Some elements update immediately; reopen dialogs to fully refresh."),
        5000);
}

// ------------------------------------------------------------------
// Rebuild all menu text for the current UI language.
// ------------------------------------------------------------------
void MainWindow::retranslateUi()
{
    // Easiest robust approach: clear the menu bar and rebuild it. Action
    // connections are recreated by createMenus(); the previous actions are
    // owned by the old menus and deleted with them.
    menuBar()->clear();

    // Reset menu pointers that will be recreated.
    m_sessionsMenu = nullptr;
    m_recentMenu = nullptr;
    m_fileMenu = m_editMenu = m_viewMenu = nullptr;
    m_langSyntaxMenu = m_encMenu = m_termMenu = nullptr;
    m_compareMenu = nullptr;
    m_cloudMenu = nullptr;
    m_uiLangMenu = m_helpMenu = nullptr;
    m_actAutoSave = nullptr;

    createMenus();

    // Restore the auto-save checkbox state in the freshly built menu.
    if (m_actAutoSave) {
        m_actAutoSave->blockSignals(true);
        m_actAutoSave->setChecked(m_autoSaveEnabled);
        m_actAutoSave->blockSignals(false);
    }

    updateStatusBar();
}

// ------------------------------------------------------------------
// Restore persisted preferences (auto-save state).
// ------------------------------------------------------------------
void MainWindow::loadPreferences()
{
    QSettings s;
    m_autoSaveIntervalSec = qMax(5, s.value(QStringLiteral("editor/autoSaveIntervalSec"), 30).toInt());
    const bool autoSave = s.value(QStringLiteral("editor/autoSave"), false).toBool();
    if (m_actAutoSave) {
        m_actAutoSave->setChecked(autoSave);  // triggers toggleAutoSave()
    } else {
        m_autoSaveEnabled = autoSave;
        if (autoSave)
            m_autoSaveTimer->start(m_autoSaveIntervalSec * 1000);
    }

    // Restore the saved theme and apply it application-wide (palette + tabs).
    const QString theme = s.value(QStringLiteral("ui/theme"),
                                  m_themeManager.currentName()).toString();
    if (m_themeManager.themeNames().contains(theme))
        applyThemeByName(theme);
}

// ------------------------------------------------------------------
// Misc
// ------------------------------------------------------------------
void MainWindow::about()
{
    const QString body =
        "<h3>Terminal++ 1.4</h3>"
        "<p>" + TR("A unified Qt 6 application combining:") + "</p>"
        "<ul>"
        "<li><b>" + TR("Code editor") + "</b> &mdash; " +
            TR("multi-tab editor with syntax highlighting, themes, find/replace, "
               "encodings, column/block edit, clipboard history, auto-save, "
               "a built-in hex viewer/editor, and a tab-bar context menu "
               "(close / close others / close to the left or right).") + "</li>"
        "<li><b>" + TR("SSH terminal") + "</b> &mdash; " +
            TR("native libssh2 SSH sessions with tmux support and VT100/256-color "
               "rendering; passwords encrypted with Windows DPAPI.") + "</li>"
        "<li><b>" + TR("Serial / UART") + "</b> &mdash; " +
            TR("configurable serial console sharing the same ANSI engine.") + "</li>"
        "<li><b>" + TR("File Compare") + "</b> &mdash; " +
            TR("Side-by-side text, folder, hex and picture comparison.") + "</li>"
        "<li><b>" + TR("Cloud Drives") + "</b> &mdash; " +
            TR("Baidu Pan and WebDAV browsing, upload/download (files and "
               "folders), rename, delete, and streaming playback, with a "
               "Transfers panel showing live progress and history.") + "</li>"
        "</ul>"
        "<p>" + TR("Multi-language UI (9 languages), VS Code-like themes applied "
                   "across all tabs.") + "</p>"
        "<p>" + TR("Built with MinGW + Qt 6 + libssh2.") + "</p>";
    QMessageBox::about(this, TR("About Terminal++"), body);
}

void MainWindow::onCurrentTabChanged(int)
{
    updateWindowTitle();
    updateStatusBar();

    // Give keyboard focus to terminal tabs so typing works immediately.
    QWidget *w = m_tabs->currentWidget();
    if (SshTab *s = qobject_cast<SshTab *>(w))
        s->terminal()->setFocus();
    else if (SerialTab *s = qobject_cast<SerialTab *>(w))
        s->terminal()->setFocus();

    // The Transfers panel is only relevant to cloud-drive browsing, so only
    // show its dock while a cloud tab is active; hide it for every other tab
    // type (editor/SSH/Serial/compare/hex) without losing its contents.
    if (m_transferDock)
        m_transferDock->setVisible(qobject_cast<CloudBrowserTab *>(w) != nullptr);
}

void MainWindow::updateStatusBar()
{
    EditorTab *t = currentEditorTab();
    if (!t) {
        // Terminal tab selected.
        m_posLabel->setText("");
        m_langLabel->setText(TR("Terminal"));
        m_encLabel->setText("UTF-8");
        m_eolLabel->setText("");
        return;
    }

    QTextCursor c = t->editor()->textCursor();
    int line = c.blockNumber() + 1;
    int col = c.columnNumber() + 1;
    m_posLabel->setText(TR("Ln %1, Col %2").arg(line).arg(col));
    m_langLabel->setText(SyntaxHighlighter::languageName(t->highlighter()->language()));
    m_encLabel->setText(t->encoding());
    m_eolLabel->setText(t->eolName());

    // Reflect the active editor's column (block) edit state.
    if (m_colLabel)
        m_colLabel->setText(t->editor()->hasColumnSelection() ? TR("COLUMN") : QString());
}

void MainWindow::updateWindowTitle()
{
    EditorTab *t = currentEditorTab();
    if (t) {
        QString name = t->isUntitled() ? TR("new") : t->filePath();
        if (t->isModified())
            name += " *";
        setWindowTitle(QString("%1 - Terminal++").arg(name));
    } else {
        setWindowTitle("Terminal++");
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_tabs->tabBar() && event->type() == QEvent::MouseButtonDblClick) {
        auto *me = static_cast<QMouseEvent *>(event);
        // Only react when the double-click lands on empty tab-bar space (not
        // on an existing tab), so double-clicking a tab itself still behaves
        // normally (e.g. inline rename elsewhere is unaffected).
        if (m_tabs->tabBar()->tabAt(me->pos()) == -1) {
            newFile();
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Prompt to save modified editor tabs.
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (EditorTab *t = qobject_cast<EditorTab *>(m_tabs->widget(i))) {
            if (t->isModified()) {
                m_tabs->setCurrentIndex(i);
                if (!maybeSaveTab(t)) {
                    event->ignore();
                    return;
                }
            }
        }
    }

    // Close all terminal sessions.
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (SshTab *s = qobject_cast<SshTab *>(m_tabs->widget(i)))
            s->disconnectSession();
        else if (SerialTab *s = qobject_cast<SerialTab *>(m_tabs->widget(i)))
            s->closePort();
    }

    event->accept();
}