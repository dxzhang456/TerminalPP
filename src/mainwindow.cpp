#include "mainwindow.h"

#include "editor/editortab.h"
#include "editor/codeeditor.h"
#include "editor/finddialog.h"
#include "editor/gotodialog.h"
#include "terminal/sshtab.h"
#include "terminal/serialtab.h"
#include "terminal/sessiondialog.h"
#include "terminal/serialdialog.h"
#include "terminal/sessionmanager.h"
#include "terminal/terminalwidget.h"

#include <QTabWidget>
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

    m_sessions = new SessionManager();
    m_findDialog = new FindDialog(this);
    connect(m_findDialog, &FindDialog::statusMessage, this, [this](const QString &m) {
        statusBar()->showMessage(m, 4000);
    });

    createActions();
    createMenus();
    createToolBars();
    createStatusBar();
    createSidebar();

    addEditorTab();
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
    m_actLineNumbers = new QAction(tr("Show &Line Numbers"), this);
    m_actLineNumbers->setCheckable(true);
    m_actLineNumbers->setChecked(true);
    connect(m_actLineNumbers, &QAction::toggled, this, &MainWindow::toggleLineNumbers);

    m_actWordWrap = new QAction(tr("&Word Wrap"), this);
    m_actWordWrap->setCheckable(true);
    connect(m_actWordWrap, &QAction::toggled, this, &MainWindow::toggleWordWrap);

    m_actSidebar = new QAction(tr("Show Session &Sidebar"), this);
    m_actSidebar->setCheckable(true);
    m_actSidebar->setChecked(true);
    m_actSidebar->setShortcut(QKeySequence("Ctrl+B"));
    connect(m_actSidebar, &QAction::toggled, this, &MainWindow::toggleSidebar);
}

void MainWindow::createMenus()
{
    // ---------- File ----------
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&New"), QKeySequence::New, this, &MainWindow::newFile);
    fileMenu->addAction(tr("&Open..."), QKeySequence::Open, this, &MainWindow::openFileDialog);
    fileMenu->addAction(tr("&Save"), QKeySequence::Save, this, &MainWindow::saveFile);
    fileMenu->addAction(tr("Save &As..."), QKeySequence::SaveAs, this, &MainWindow::saveFileAs);
    fileMenu->addAction(tr("Save A&ll"), QKeySequence("Ctrl+Shift+S"), this, &MainWindow::saveAll);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Print..."), QKeySequence::Print, this, &MainWindow::printFile);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Close Tab"), QKeySequence("Ctrl+W"), this, &MainWindow::closeCurrentTab);
    fileMenu->addAction(tr("E&xit"), QKeySequence("Ctrl+Q"), this, &MainWindow::close);

    // ---------- Edit ----------
    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(tr("&Undo"), QKeySequence::Undo, this, &MainWindow::undo);
    editMenu->addAction(tr("&Redo"), QKeySequence::Redo, this, &MainWindow::redo);
    editMenu->addSeparator();
    editMenu->addAction(tr("Cu&t"), QKeySequence::Cut, this, &MainWindow::cut);
    editMenu->addAction(tr("&Copy"), QKeySequence::Copy, this, &MainWindow::copy);
    editMenu->addAction(tr("&Paste"), QKeySequence::Paste, this, &MainWindow::paste);
    editMenu->addAction(tr("Select &All"), QKeySequence::SelectAll, this, &MainWindow::selectAll);
    editMenu->addSeparator();
    editMenu->addAction(tr("&Find..."), QKeySequence::Find, this, &MainWindow::showFind);
    editMenu->addAction(tr("&Replace..."), QKeySequence::Replace, this, &MainWindow::showReplace);
    editMenu->addAction(tr("&Go To Line..."), QKeySequence("Ctrl+G"), this, &MainWindow::gotoLine);

    // ---------- View ----------
    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(m_actLineNumbers);
    viewMenu->addAction(m_actWordWrap);
    viewMenu->addSeparator();
    viewMenu->addAction(tr("Zoom &In"), QKeySequence::ZoomIn, this, &MainWindow::zoomIn);
    viewMenu->addAction(tr("Zoom &Out"), QKeySequence::ZoomOut, this, &MainWindow::zoomOut);
    viewMenu->addAction(tr("&Reset Zoom"), QKeySequence("Ctrl+0"), this, &MainWindow::zoomReset);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actSidebar);

    // Themes submenu.
    QMenu *themeMenu = viewMenu->addMenu(tr("&Theme"));
    QActionGroup *themeGroup = new QActionGroup(this);
    for (const QString &name : m_themeManager.themeNames()) {
        QAction *a = themeMenu->addAction(name);
        a->setCheckable(true);
        if (name == m_themeManager.currentName())
            a->setChecked(true);
        themeGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, name]() { applyThemeByName(name); });
    }

    // ---------- Language ----------
    QMenu *langMenu = menuBar()->addMenu(tr("&Language"));
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
        QAction *a = langMenu->addAction(li.name);
        a->setCheckable(true);
        m_langGroup->addAction(a);
        int id = li.id;
        connect(a, &QAction::triggered, this, [this, id]() { setLanguage(id); });
    }

    // ---------- Encoding ----------
    QMenu *encMenu = menuBar()->addMenu(tr("E&ncoding"));
    const QStringList encs = {"UTF-8", "UTF-16LE", "UTF-16BE", "System", "ISO-8859-1", "GB18030"};
    for (const QString &e : encs) {
        QAction *a = encMenu->addAction(e);
        connect(a, &QAction::triggered, this, [this, e]() { setEncoding(e); });
    }
    encMenu->addSeparator();
    QMenu *eolMenu = encMenu->addMenu(tr("Line Ending"));
    eolMenu->addAction(tr("Windows (CRLF)"), this, [this]() { setEol(EditorTab::CRLF); });
    eolMenu->addAction(tr("Unix (LF)"), this, [this]() { setEol(EditorTab::LF); });
    eolMenu->addAction(tr("Mac (CR)"), this, [this]() { setEol(EditorTab::CR); });

    // ---------- Terminal ----------
    QMenu *termMenu = menuBar()->addMenu(tr("&Terminal"));
    termMenu->addAction(tr("New &SSH Session..."), QKeySequence("Ctrl+N"), this, &MainWindow::newSshSession)
        ->setShortcutContext(Qt::ApplicationShortcut);
    termMenu->addAction(tr("&Manage Sessions..."), this, &MainWindow::manageSessions);
    termMenu->addAction(tr("&Reconnect Current"), QKeySequence("Ctrl+Shift+R"), this, &MainWindow::reconnectCurrent);
    termMenu->addSeparator();
    termMenu->addAction(tr("New Serial / &UART Session..."), this, &MainWindow::newSerialSession);

    // ---------- Help ----------
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About Terminal++"), this, &MainWindow::about);
}

void MainWindow::createToolBars()
{
    QToolBar *tb = addToolBar(tr("Main"));
    tb->setMovable(false);
    tb->addAction(tr("New"), this, &MainWindow::newFile);
    tb->addAction(tr("Open"), this, &MainWindow::openFileDialog);
    tb->addAction(tr("Save"), this, &MainWindow::saveFile);
    tb->addSeparator();
    tb->addAction(tr("Find"), this, &MainWindow::showFind);
    tb->addSeparator();
    tb->addAction(tr("SSH"), this, &MainWindow::newSshSession);
    tb->addAction(tr("Serial"), this, &MainWindow::newSerialSession);
}

void MainWindow::createStatusBar()
{
    m_posLabel = new QLabel("Ln 1, Col 1", this);
    m_langLabel = new QLabel("Plain Text", this);
    m_encLabel = new QLabel("UTF-8", this);
    m_eolLabel = new QLabel("Windows (CRLF)", this);

    statusBar()->addPermanentWidget(m_posLabel);
    statusBar()->addPermanentWidget(m_langLabel);
    statusBar()->addPermanentWidget(m_encLabel);
    statusBar()->addPermanentWidget(m_eolLabel);
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::createSidebar()
{
    m_sidebarDock = new QDockWidget(tr("SSH Sessions"), this);
    m_sidebarDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    QWidget *panel = new QWidget(m_sidebarDock);
    QVBoxLayout *l = new QVBoxLayout(panel);
    l->setContentsMargins(4, 4, 4, 4);

    m_sessionList = new QListWidget(panel);
    l->addWidget(m_sessionList);

    QHBoxLayout *btns = new QHBoxLayout;
    QPushButton *addBtn = new QPushButton(tr("+ Add"), panel);
    QPushButton *connBtn = new QPushButton(tr("Connect"), panel);
    btns->addWidget(addBtn);
    btns->addWidget(connBtn);
    l->addLayout(btns);

    connect(addBtn, &QPushButton::clicked, this, &MainWindow::newSshSession);
    connect(connBtn, &QPushButton::clicked, this, &MainWindow::connectSelectedSession);
    connect(m_sessionList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *) { connectSelectedSession(); });

    m_sidebarDock->setWidget(panel);
    addDockWidget(Qt::LeftDockWidgetArea, m_sidebarDock);
    connect(m_sidebarDock, &QDockWidget::visibilityChanged, this, [this](bool v) {
        if (m_actSidebar) m_actSidebar->setChecked(v);
    });

    refreshSessionList();
}

void MainWindow::refreshSessionList()
{
    if (!m_sessionList)
        return;
    m_sessionList->clear();
    for (const SshSessionConfig &c : m_sessions->sessions())
        m_sessionList->addItem(c.displayName());
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

    return tab;
}

bool MainWindow::maybeSaveTab(EditorTab *tab)
{
    if (!tab || !tab->isModified())
        return true;

    QMessageBox::StandardButton ret = QMessageBox::warning(
        this, tr("Terminal++"),
        tr("The document \"%1\" has unsaved changes.\nDo you want to save?")
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
        QMessageBox::warning(this, tr("Open"), tr("Cannot open file:\n%1").arg(path));
        return nullptr;
    }
    tab->applyTheme(m_themeManager.current());
    int i = m_tabs->indexOf(tab);
    m_tabs->setTabText(i, tab->fileName());
    m_tabs->setTabToolTip(i, tab->filePath());
    updateWindowTitle();
    updateStatusBar();
    return tab;
}

void MainWindow::openFileDialog()
{
    QStringList files = QFileDialog::getOpenFileNames(this, tr("Open Files"));
    for (const QString &f : files)
        openFile(f);
}

bool MainWindow::saveFile()
{
    EditorTab *tab = currentEditorTab();
    if (!tab)
        return false;
    if (tab->isUntitled())
        return saveFileAs();
    if (!tab->save()) {
        QMessageBox::warning(this, tr("Save"), tr("Cannot write file."));
        return false;
    }
    statusBar()->showMessage(tr("Saved %1").arg(tab->filePath()), 3000);
    updateWindowTitle();
    return true;
}

bool MainWindow::saveFileAs()
{
    EditorTab *tab = currentEditorTab();
    if (!tab)
        return false;
    QString path = QFileDialog::getSaveFileName(this, tr("Save As"), tab->fileName());
    if (path.isEmpty())
        return false;
    if (!tab->saveFile(path)) {
        QMessageBox::warning(this, tr("Save"), tr("Cannot write file."));
        return false;
    }
    int i = m_tabs->indexOf(tab);
    m_tabs->setTabText(i, tab->fileName());
    m_tabs->setTabToolTip(i, tab->filePath());
    statusBar()->showMessage(tr("Saved %1").arg(tab->filePath()), 3000);
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

void MainWindow::onTabCloseRequested(int index)
{
    QWidget *w = m_tabs->widget(index);
    if (!w)
        return;

    if (EditorTab *tab = qobject_cast<EditorTab *>(w)) {
        if (!maybeSaveTab(tab))
            return;
    } else if (SshTab *s = qobject_cast<SshTab *>(w)) {
        s->disconnectSession();
    } else if (SerialTab *s = qobject_cast<SerialTab *>(w)) {
        s->closePort();
    }

    m_tabs->removeTab(index);
    w->deleteLater();

    if (m_tabs->count() == 0)
        addEditorTab();
    updateWindowTitle();
    updateStatusBar();
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

void MainWindow::zoomIn()  { if (EditorTab *t = currentEditorTab()) t->editor()->zoomIn(1); }
void MainWindow::zoomOut() { if (EditorTab *t = currentEditorTab()) t->editor()->zoomOut(1); }
void MainWindow::zoomReset()
{
    if (EditorTab *t = currentEditorTab()) {
        QFont f("Consolas", 11);
        f.setStyleHint(QFont::Monospace);
        t->editor()->setFont(f);
    }
}

void MainWindow::applyThemeByName(const QString &name)
{
    m_themeManager.setCurrent(name);
    for (int i = 0; i < m_tabs->count(); ++i)
        if (EditorTab *t = qobject_cast<EditorTab *>(m_tabs->widget(i)))
            t->applyTheme(m_themeManager.current());
    statusBar()->showMessage(tr("Theme: %1").arg(name), 3000);
}

void MainWindow::toggleSidebar(bool on)
{
    if (m_sidebarDock)
        m_sidebarDock->setVisible(on);
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
    t->setEncoding(enc == "System" ? "UTF-8" : enc);
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
void MainWindow::newSshSession()
{
    SessionDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    SshSessionConfig cfg = dlg.config();
    if (cfg.host.isEmpty()) {
        QMessageBox::warning(this, tr("SSH"), tr("Host is required."));
        return;
    }

    m_sessions->add(cfg);
    refreshSessionList();

    SshTab *tab = new SshTab(cfg, this);
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
            m_tabs->setTabText(i, m_tabs->tabText(i) + tr(" [closed]"));
    });

    tab->connectSession();
}

void MainWindow::connectSelectedSession()
{
    int row = m_sessionList->currentRow();
    if (row < 0 || row >= m_sessions->sessions().size())
        return;

    SshSessionConfig cfg = m_sessions->sessions().at(row);
    SshTab *tab = new SshTab(cfg, this);
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
            m_tabs->setTabText(i, m_tabs->tabText(i) + tr(" [closed]"));
    });

    tab->connectSession();
}

void MainWindow::manageSessions()
{
    // Simple management: list sessions and allow edit/delete via input.
    QStringList names;
    for (const SshSessionConfig &c : m_sessions->sessions())
        names << c.displayName();

    if (names.isEmpty()) {
        QMessageBox::information(this, tr("Sessions"),
                                 tr("No saved sessions. Add one via New SSH Session."));
        return;
    }

    bool ok = false;
    QString chosen = QInputDialog::getItem(this, tr("Manage Sessions"),
                                           tr("Select a session to edit:"),
                                           names, 0, false, &ok);
    if (!ok)
        return;

    int row = names.indexOf(chosen);
    if (row < 0)
        return;

    SessionDialog dlg(this);
    dlg.setConfig(m_sessions->sessions().at(row));
    if (dlg.exec() == QDialog::Accepted) {
        m_sessions->update(row, dlg.config());
        refreshSessionList();
    }
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
        QMessageBox::warning(this, tr("Serial"), tr("No serial port selected."));
        return;
    }

    SerialTab *tab = new SerialTab(cfg, this);
    QString title = QString("%1 @ %2").arg(cfg.portName).arg(cfg.baudRate);
    int idx = m_tabs->addTab(tab, title);
    m_tabs->setCurrentIndex(idx);

    connect(tab, &SerialTab::connectionClosed, this, [this, tab]() {
        int i = m_tabs->indexOf(tab);
        if (i >= 0)
            m_tabs->setTabText(i, m_tabs->tabText(i) + tr(" [closed]"));
    });

    if (!tab->openPort()) {
        // Keep the tab open so the error message is visible in the terminal.
    }
}

// ------------------------------------------------------------------
// Misc
// ------------------------------------------------------------------
void MainWindow::about()
{
    QMessageBox::about(this, tr("About Terminal++"),
        tr("<h3>Terminal++ 1.0</h3>"
           "<p>A unified Qt6 application combining:</p>"
           "<ul>"
           "<li><b>Code editor</b> &mdash; a Notepad-- (ndd) style editor with "
           "syntax highlighting, themes, find/replace, multi-tab, encodings.</li>"
           "<li><b>SSH terminal</b> &mdash; MuxTerm-style SSH sessions with "
           "tmux auto-attach and VT100/256-color rendering.</li>"
           "<li><b>Serial / UART</b> &mdash; a PuTTY-like serial console.</li>"
           "</ul>"
           "<p>Built with MinGW + Qt 6.</p>"));
}

void MainWindow::onCurrentTabChanged(int)
{
    updateWindowTitle();
    updateStatusBar();
}

void MainWindow::updateStatusBar()
{
    EditorTab *t = currentEditorTab();
    if (!t) {
        // Terminal tab selected.
        m_posLabel->setText("");
        m_langLabel->setText(tr("Terminal"));
        m_encLabel->setText("UTF-8");
        m_eolLabel->setText("");
        return;
    }

    QTextCursor c = t->editor()->textCursor();
    int line = c.blockNumber() + 1;
    int col = c.columnNumber() + 1;
    m_posLabel->setText(tr("Ln %1, Col %2").arg(line).arg(col));
    m_langLabel->setText(SyntaxHighlighter::languageName(t->highlighter()->language()));
    m_encLabel->setText(t->encoding());
    m_eolLabel->setText(t->eolName());
}

void MainWindow::updateWindowTitle()
{
    EditorTab *t = currentEditorTab();
    if (t) {
        QString name = t->isUntitled() ? tr("new") : t->filePath();
        if (t->isModified())
            name += " *";
        setWindowTitle(QString("%1 - Terminal++").arg(name));
    } else {
        setWindowTitle("Terminal++");
    }
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