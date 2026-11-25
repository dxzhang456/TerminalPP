#include "cloudbrowsertab.h"
#include "streamproxy.h"
#include "transfermanager.h"
#include "core/i18n.h"

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include "core/iconfactory.h"
#include <QAction>
#include <QLabel>
#include <QLineEdit>
#include <QHeaderView>
#include <QMenu>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <QDirIterator>
#include <QQueue>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QStringConverter>

static QString TRc(const char *s) { return I18n::instance()->tr(QString::fromUtf8(s)); }

namespace {
enum Column { ColName = 0, ColSize, ColModified, ColCount };
const int RoleIndex = Qt::UserRole + 1;   // index into m_entries

QString humanSize(qint64 bytes, bool isDir)
{
    if (isDir) return QString();
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}
} // namespace

CloudBrowserTab::CloudBrowserTab(CloudProvider *provider, const QString &title,
                                 QWidget *parent)
    : QWidget(parent), m_provider(provider), m_title(title)
{
    m_provider->setParent(this);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *tb = new QToolBar(this);
    // Toolbar uses auto-generated monochrome vector icons; the original label
    // is kept as the tooltip and follows UI language changes live.
    tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
    tb->setIconSize(QSize(20, 20));
    auto addIconAction = [&](const char *src, auto slot) -> QAction * {
        QIcon icon = IconFactory::makeToolIcon(QString::fromUtf8(src));
        QAction *a = icon.isNull() ? tb->addAction(TRc(src), this, slot)
                                   : tb->addAction(icon, TRc(src), this, slot);
        a->setToolTip(TRc(src));
        return a;
    };
    addIconAction("Back", &CloudBrowserTab::goBack);
    addIconAction("Up", &CloudBrowserTab::goUp);
    addIconAction("Refresh", &CloudBrowserTab::refresh);
    tb->addSeparator();
    addIconAction("Upload Files...", &CloudBrowserTab::uploadFiles);
    addIconAction("Upload Folder...", &CloudBrowserTab::uploadFolder);
    addIconAction("New Folder...", &CloudBrowserTab::makeFolder);
    tb->addSeparator();
    tb->addWidget(new QLabel(TRc(" Path: "), this));
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setReadOnly(true);
    tb->addWidget(m_pathEdit);
    outer->addWidget(tb);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(ColCount);
    m_tree->setHeaderLabels({TRc("Name"), TRc("Size"), TRc("Modified")});
    m_tree->setRootIsDecorated(false);
    m_tree->setAlternatingRowColors(true);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->header()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    connect(m_tree, &QTreeWidget::itemActivated, this, &CloudBrowserTab::onItemActivated);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &CloudBrowserTab::onItemActivated);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &CloudBrowserTab::onContextMenu);
    outer->addWidget(m_tree, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setContentsMargins(6, 2, 6, 2);
    outer->addWidget(m_statusLabel);

    connect(m_provider, &CloudProvider::listed, this, &CloudBrowserTab::onListed);
    connect(m_provider, &CloudProvider::errorOccurred, this, &CloudBrowserTab::onError);
    connect(m_provider, &CloudProvider::downloadProgress, this,
            [this](qint64 r, qint64 t) {
        if (t > 0)
            emit statusMessage(TRc("Downloading… %1 / %2")
                .arg(humanSize(r, false), humanSize(t, false)));
    });

    // When the user changes maxTasks on the transfer panel, immediately pump
    // in-flight folder transfers so newly allowed task slots are filled at once.
    connect(TransferManager::instance(), &TransferManager::maxTasksChanged,
            this, [this](int) {
        if (m_folderUp && m_folderUp->active)
            pumpFolderUpload();
        if (m_folderDn && m_folderDn->active)
            pumpFolderDownload();
        if (m_folderDel && m_folderDel->active)
            pumpFolderDelete();
    });

    navigateTo(m_provider->rootPath());
}

// Out-of-line so the std::unique_ptr<QDirIterator> (and FolderUpload/Download)
// members are destroyed here, where QDirIterator is a complete type.
CloudBrowserTab::~CloudBrowserTab() = default;

// ---- classification by extension ----
CloudBrowserTab::Kind CloudBrowserTab::classify(const QString &name)
{
    const QString ext = QFileInfo(name).suffix().toLower();
    // Source code / markup / config / script extensions that are plain text.
    static const QStringList text = {
        // plain text & docs
        "txt","text","md","markdown","rst","adoc","asciidoc","log","nfo","me",
        "csv","tsv","tab","diff","patch","po","pot","srt","vtt","ass","lrc",
        // config
        "ini","cfg","conf","config","properties","env","toml","yaml","yml",
        "json","json5","jsonc","xml","xsd","xsl","xslt","plist","reg","desktop",
        "editorconfig","gitignore","gitattributes","gitmodules","npmrc","babelrc",
        "eslintrc","prettierrc","dockerignore","dockerfile","makefile","mk",
        "cmake","gradle","pro","pri","pri","qrc","ui","spec","manifest",
        // C / C++ / Obj-C
        "c","cc","cpp","cxx","c++","h","hh","hpp","hxx","h++","inl","ipp",
        "m","mm","def","i","s","asm","inc",
        // other languages
        "cs","java","kt","kts","scala","groovy","py","pyw","pyi","rb","rake",
        "pl","pm","php","phtml","lua","tcl","r","jl","swift","go","rs","dart",
        "js","jsx","mjs","cjs","ts","tsx","vue","svelte","coffee",
        "sh","bash","zsh","fish","ksh","csh","bat","cmd","ps1","psm1","psd1",
        "sql","ddl","dml","pgsql","mysql","graphql","gql","proto","thrift",
        "f","f90","for","pas","pp","bas","vb","vbs","ada","adb","ads","d",
        "erl","hrl","ex","exs","hs","lhs","ml","mli","clj","cljs","lisp","el",
        "nim","zig","v","sv","svh","vhd","vhdl","verilog",
        // web / styling
        "html","htm","xhtml","shtml","css","scss","sass","less","styl",
        "jsp","asp","aspx","erb","haml","pug","jade","twig","hbs","mustache",
        // build / misc
        "am","ac","in","m4","spec","service","unit","rules","map","lst","sym",
        "ld","lds","dts","dtsi","kconfig","defconfig","gyp","gypi","bzl","bazel" };
    static const QStringList image = {
        "png","jpg","jpeg","bmp","gif","webp","ico","svg","tif","tiff" };
    static const QStringList video = {
        "mp4","mkv","avi","mov","wmv","flv","webm","m4v","mpg","mpeg","ts","3gp" };
    static const QStringList audio = {
        "mp3","flac","wav","aac","ogg","m4a","wma" };
    // ".ts" is ambiguous (TypeScript vs MPEG transport stream); the text list
    // is consulted first, so treat it as source code — use "Open With..." for
    // the rare transport-stream case.
    if (text.contains(ext)) return Text;
    if (image.contains(ext)) return Image;
    if (video.contains(ext)) return Video;
    if (audio.contains(ext)) return Audio;

    // No/unknown extension: well-known extension-less text files (Makefile,
    // Dockerfile, LICENSE, README, CHANGELOG, .bashrc, ...) are still text.
    const QString base = QFileInfo(name).fileName().toLower();
    static const QStringList namedText = {
        "makefile","gnumakefile","dockerfile","license","licence","copying",
        "readme","changelog","changes","authors","contributors","notice",
        "install","news","todo","version","cmakelists.txt","kconfig",
        ".bashrc",".bash_profile",".zshrc",".profile",".gitconfig",".vimrc" };
    if (namedText.contains(base))
        return Text;

    // Anything else is decided by sniffing the bytes after download
    // (see looksLikeText()), so a text file with an exotic extension still
    // opens in the internal editor instead of being handed to the OS.
    return Other;
}

// Heuristic: does this buffer look like human-readable text rather than binary?
// Used for files whose extension we do not recognise, so that e.g. a cloud file
// named "notes.bak" or with no extension still opens in the editor (correctly
// decoded) instead of being passed to an unrelated OS application.
bool CloudBrowserTab::looksLikeText(const QByteArray &sample)
{
    if (sample.isEmpty())
        return true;                      // empty file: harmless as text
    // A BOM is a definitive text marker.
    if (QStringConverter::encodingForData(sample).has_value())
        return true;
    int suspicious = 0;
    for (int i = 0; i < sample.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(sample.at(i));
        if (c == 0)
            return false;                 // NUL byte: treat as binary
        // Control characters other than tab/LF/CR/FF/ESC are suspicious.
        if (c < 0x20 && c != '\t' && c != '\n' && c != '\r' && c != 0x0c && c != 0x1b)
            ++suspicious;
    }
    // Allow a tiny amount of noise (< 1%) before declaring the file binary.
    return suspicious * 100 < sample.size();
}

QString CloudBrowserTab::tempDownloadPath(const QString &name) const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                  + "/TerminalPP-cloud";
    QDir().mkpath(dir);
    return dir + "/" + name;
}

void CloudBrowserTab::navigateTo(const QString &path)
{
    m_currentPath = path;
    m_pathEdit->setText(path);
    emit statusMessage(TRc("Loading…"));
    m_provider->list(path);
}

void CloudBrowserTab::refresh()  { navigateTo(m_currentPath); }

void CloudBrowserTab::goBack()
{
    if (m_history.isEmpty())
        return;
    QString prev = m_history.takeLast();
    // navigateTo pushes onto history, so temporarily bypass.
    m_currentPath = prev;
    m_pathEdit->setText(prev);
    m_provider->list(prev);
}

void CloudBrowserTab::goUp()
{
    // Only meaningful for path-based providers (slash-separated).
    if (!m_currentPath.contains('/') || m_currentPath == "/")
        return;
    QString parent = m_currentPath;
    if (parent.endsWith('/')) parent.chop(1);
    int idx = parent.lastIndexOf('/');
    parent = (idx <= 0) ? "/" : parent.left(idx);
    m_history.push_back(m_currentPath);
    navigateTo(parent);
}

void CloudBrowserTab::onListed(const QString &path, const QVector<CloudEntry> &entries)
{
    // Ignore internal background directory listings (such as FolderDownload or
    // FolderDelete expanding child subtrees) so the browser view never moves
    // or displays the contents of folders being recursively deleted/downloaded.
    if (!path.isEmpty() && !m_currentPath.isEmpty() && path != m_currentPath)
        return;

    m_entries = entries;
    m_tree->clear();

    // Sort: folders first, then by name.
    std::sort(m_entries.begin(), m_entries.end(), [](const CloudEntry &a, const CloudEntry &b) {
        if (a.isDir != b.isDir) return a.isDir;
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });

    for (int i = 0; i < m_entries.size(); ++i) {
        const CloudEntry &e = m_entries[i];
        auto *item = new QTreeWidgetItem(m_tree);
        item->setText(ColName, (e.isDir ? "📁 " : "📄 ") + e.name);
        item->setText(ColSize, humanSize(e.size, e.isDir));
        if (e.modified.isValid())
            item->setText(ColModified, e.modified.toString("yyyy-MM-dd HH:mm"));
        item->setData(ColName, RoleIndex, i);
    }
    emit statusMessage(TRc("%1 item(s)").arg(m_entries.size()));
}

void CloudBrowserTab::onError(const QString &message)
{
    emit statusMessage(TRc("Error: %1").arg(message));
}

void CloudBrowserTab::onItemActivated(QTreeWidgetItem *item, int)
{
    if (!item) return;
    int idx = item->data(ColName, RoleIndex).toInt();
    if (idx < 0 || idx >= m_entries.size()) return;
    const CloudEntry &e = m_entries[idx];
    if (e.isDir) {
        m_history.push_back(m_currentPath);
        navigateTo(e.path);
    } else {
        actionOpenInternal(e);   // default action: open internally / temp+default
    }
}

void CloudBrowserTab::onContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    if (!item) return;
    int idx = item->data(ColName, RoleIndex).toInt();
    if (idx < 0 || idx >= m_entries.size()) return;
    const CloudEntry e = m_entries[idx];

    QMenu menu(this);
    if (e.isDir) {
        menu.addAction(TRc("Open Folder"), this, [this, e]() {
            m_history.push_back(m_currentPath);
            navigateTo(e.path);
        });
        menu.addAction(TRc("Download Folder..."), this, [this, e]() {
            // Streaming recursive folder download (memory-light): pick a local
            // destination directory, then walk the remote tree one listing at a
            // time and download one file at a time.
            const QString local = QFileDialog::getExistingDirectory(
                this, TRc("Select Folder to Upload"));
            if (!local.isEmpty())
                startFolderDownload(e, local + "/" + e.name);
        });
        menu.addSeparator();
        menu.addAction(TRc("Rename..."), this, [this, e]() { actionRename(e); });
        menu.addAction(TRc("Delete"), this, [this, e]() { actionDelete(e); });
    } else {
        const Kind k = classify(e.name);
        // Open (internal) for text/image.
        if (k == Text || k == Image)
            menu.addAction(TRc("Open"), this, [this, e]() { actionOpenInternal(e); });
        // Stream for video/audio.
        if (k == Video || k == Audio)
            menu.addAction(TRc("Stream (play while downloading)"), this,
                           [this, e]() { actionStream(e); });
        // Unknown type: offer both "Open" (sniffs the content and opens text
        // in the editor) and an explicit "Open as Text".
        if (k == Other)
            menu.addAction(TRc("Open"), this, [this, e]() { actionOpenInternal(e); });
        // Force-open any file in the internal editor (correctly decoded), handy
        // for text files with an exotic extension.
        if (k != Image && k != Video && k != Audio)
            menu.addAction(TRc("Open as Text"), this,
                           [this, e]() { actionOpenAsText(e); });
        menu.addSeparator();
        // Open With (external program picker) — download to temp first.
        menu.addAction(TRc("Open With..."), this, [this, e]() { actionOpenWith(e); });
        // OS default app — download to temp first.
        menu.addAction(TRc("Open (system default)"), this,
                       [this, e]() { actionOpenDefault(e); });
        menu.addSeparator();
        menu.addAction(TRc("Download..."), this, [this, e]() { actionDownload(e); });
        menu.addSeparator();
        menu.addAction(TRc("Rename..."), this, [this, e]() { actionRename(e); });
        menu.addAction(TRc("Delete"), this, [this, e]() { actionDelete(e); });
    }
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

// ---- actions ----
// Download `e` to `localPath`, then run `after(localPath)`. The download is
// registered in the TransferManager so it ALWAYS shows up in the Transfers
// panel with live progress and can be canceled/removed — this covers temp
// downloads triggered by Open / Open With / Open-default and the stream
// fall-back (e.g. an mp4 that cannot be streamed is downloaded first), which
// previously ran "invisibly" without a panel entry.
void CloudBrowserTab::downloadThen(const CloudEntry &e, const QString &localPath,
                                   std::function<void(const QString &)> after)
{
    emit statusMessage(TRc("Downloading %1…").arg(e.name));

    TransferManager *tm = TransferManager::instance();
    const int id = tm->begin(TransferItem::Download, m_title, e.name,
                             e.path, localPath, m_provider, e.size);

    auto *pConn = new QMetaObject::Connection();
    auto *fConn = new QMetaObject::Connection();
    auto *eConn = new QMetaObject::Connection();
    auto cleanup = [pConn, fConn, eConn]() {
        QObject::disconnect(*pConn); QObject::disconnect(*fConn);
        QObject::disconnect(*eConn);
        delete pConn; delete fConn; delete eConn;
    };

    *pConn = connect(m_provider, &CloudProvider::downloadProgress, this,
        [tm, id](qint64 r, qint64 t) { tm->updateProgress(id, r, t); });
    *fConn = connect(m_provider, &CloudProvider::downloadFinished, this,
        [this, tm, id, cleanup, after, localPath](const QString &, const QString &lp) {
            if (lp != localPath) return;
            tm->markCompleted(id);
            cleanup();
            emit statusMessage(TRc("Downloaded to %1").arg(lp));
            if (after) after(lp);
        });
    *eConn = connect(m_provider, &CloudProvider::errorOccurred, this,
        [tm, id, cleanup](const QString &msg) {
            tm->markFailed(id, msg);
            cleanup();
        });

    tm->markRunning(id);
    m_provider->download(e.path, localPath);
}

void CloudBrowserTab::actionDownload(const CloudEntry &e)
{
    QString local = QFileDialog::getSaveFileName(this, TRc("Save As"), e.name);
    if (local.isEmpty()) return;
    // A user-chosen download is a first-class transfer: register it so it shows
    // up in the Transfers panel with progress and can be canceled/retried.
    trackedDownload(e, local);
}

// Register a download in the TransferManager, wire the provider's progress /
// finish / error signals to update it, then start it. Signals are matched by
// local path so concurrent transfers don't cross-talk.
void CloudBrowserTab::trackedDownload(const CloudEntry &e, const QString &localPath)
{
    TransferManager *tm = TransferManager::instance();
    const int id = tm->begin(TransferItem::Download, m_title, e.name,
                             e.path, localPath, m_provider, e.size);

    auto *pConn = new QMetaObject::Connection();
    auto *fConn = new QMetaObject::Connection();
    auto *eConn = new QMetaObject::Connection();
    auto cleanup = [pConn, fConn, eConn]() {
        QObject::disconnect(*pConn); QObject::disconnect(*fConn);
        QObject::disconnect(*eConn);
        delete pConn; delete fConn; delete eConn;
    };

    *pConn = connect(m_provider, &CloudProvider::downloadProgress, this,
        [tm, id](qint64 r, qint64 t) { tm->updateProgress(id, r, t); });
    *fConn = connect(m_provider, &CloudProvider::downloadFinished, this,
        [tm, id, localPath, cleanup](const QString &, const QString &lp) {
            if (lp != localPath) return;
            tm->markCompleted(id);
            cleanup();
        });
    *eConn = connect(m_provider, &CloudProvider::errorOccurred, this,
        [tm, id, cleanup](const QString &msg) {
            tm->markFailed(id, msg);
            cleanup();
        });

    tm->markRunning(id);
    m_provider->download(e.path, localPath);
}

// ---- uploads ----
void CloudBrowserTab::uploadFiles()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, TRc("Select Files to Upload"));
    for (const QString &f : files)
        trackedUpload(f, m_currentPath);
    if (!files.isEmpty())
        emit statusMessage(TRc("Uploading %1 file(s)…").arg(files.size()));
}

void CloudBrowserTab::uploadFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, TRc("Select Folder to Upload"));
    if (dir.isEmpty())
        return;
    if (m_folderUp && m_folderUp->active) {
        emit statusMessage(TRc("A folder upload is already in progress."));
        return;
    }
    // Recreate the chosen folder itself under the current remote directory.
    const QString folderName = QFileInfo(dir).fileName();
    emit statusMessage(TRc("Uploading folder %1…").arg(folderName));
    m_provider->makeDir(m_currentPath, folderName);
    QString childRemote = m_currentPath;
    if (!childRemote.endsWith('/')) childRemote += '/';
    childRemote += folderName;
    // Collapse accidental double slashes (root "/" + "/name" -> "//name"),
    // which some providers (e.g. Baidu xpan) reject as an invalid path.
    while (childRemote.contains("//"))
        childRemote.replace("//", "/");
    startFolderUpload(dir, childRemote);
}

// Kept for API compatibility: a one-shot recursive upload is no longer used
// (replaced by the streaming startFolderUpload/pumpFolderUpload below), but
// the declaration remains, so provide a thin wrapper that starts a streaming
// upload rooted at the given local/remote pair.
void CloudBrowserTab::uploadDirectory(const QString &localDir, const QString &remoteParent)
{
    startFolderUpload(localDir, remoteParent);
}

// ---- streaming, sequential folder upload (memory-light) ----
// Walks the local tree lazily with a QDirIterator and uploads ONE file at a
// time. The whole file list is never materialised in memory; each next file
// is pulled from the iterator only after the previous upload finishes.
void CloudBrowserTab::startFolderUpload(const QString &localRoot,
                                        const QString &remoteRoot)
{
    m_folderUp = std::make_unique<FolderUpload>();
    m_folderUp->localRoot = QDir(localRoot).absolutePath();
    m_folderUp->remoteRoot = remoteRoot;
    m_folderUp->it = std::make_unique<QDirIterator>(
        localRoot,
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);
    m_folderUp->active = true;
    pumpFolderUpload();
}

// Map a local absolute path (a file inside localRoot) to the remote directory
// it should be uploaded into, preserving the sub-folder structure.
QString CloudBrowserTab::remoteParentFor(const QString &localAbsPath) const
{
    if (!m_folderUp)
        return m_currentPath;
    const QString root = m_folderUp->localRoot;
    QString rel = QDir(root).relativeFilePath(QFileInfo(localAbsPath).absolutePath());
    QString remote = m_folderUp->remoteRoot;
    if (rel != "." && !rel.isEmpty()) {
        if (!remote.endsWith('/')) remote += '/';
        remote += rel;
    }
    while (remote.contains("//"))
        remote.replace("//", "/");
    return remote;
}

void CloudBrowserTab::pumpFolderUpload()
{
    if (!m_folderUp || !m_folderUp->active)
        return;

    const int maxConcurrent = TransferManager::instance()->maxTasks();

    // Keep up to maxConcurrent uploads in flight. The tree is still walked
    // lazily: each loop iteration pulls at most one more entry from the
    // iterator, starts a file upload (or creates a sub-folder) and loops until
    // either the iterator is drained or the concurrency limit is reached. When
    // an upload finishes it calls pumpFolderUpload() again to refill the slot.
    while (m_folderUp->inFlight < maxConcurrent && m_folderUp->it->hasNext()) {
        const QString path = m_folderUp->it->next();
        const QFileInfo fi = m_folderUp->it->fileInfo();
        if (fi.isDir()) {
            // Ensure the remote sub-folder exists (fire-and-forget; providers
            // tolerate "already exists"). remoteParentFor() maps this dir's
            // local path to the remote dir that should CONTAIN it.
            const QString remoteDirParent = remoteParentFor(path);
            m_provider->makeDir(remoteDirParent, fi.fileName());
            continue;   // directories don't consume a concurrency slot
        }
        // A file: register + start one upload; several may run concurrently.
        const QString remoteDir = remoteParentFor(path);
        const QString localPath = path;
        emit statusMessage(TRc("Uploading %1…").arg(fi.fileName()));

        TransferManager *tm = TransferManager::instance();
        const int id = tm->begin(TransferItem::Upload, m_title, fi.fileName(),
                                 remoteDir, localPath, m_provider, fi.size());

        auto *pConn = new QMetaObject::Connection();
        auto *fConn = new QMetaObject::Connection();
        auto *eConn = new QMetaObject::Connection();
        auto cleanup = [pConn, fConn, eConn]() {
            QObject::disconnect(*pConn); QObject::disconnect(*fConn);
            QObject::disconnect(*eConn);
            delete pConn; delete fConn; delete eConn;
        };
        *pConn = connect(m_provider, &CloudProvider::uploadProgress, this,
            [tm, id, localPath](const QString &lp, qint64 s, qint64 t) {
                if (lp == localPath) tm->updateProgress(id, s, t);
            });
        *fConn = connect(m_provider, &CloudProvider::uploadFinished, this,
            [this, tm, id, localPath, cleanup](const QString &lp, const QString &) {
                if (lp != localPath) return;
                tm->markCompleted(id);
                cleanup();
                if (m_folderUp) { --m_folderUp->inFlight; ++m_folderUp->done; }
                pumpFolderUpload();   // refill the freed slot (streamed)
            });
        *eConn = connect(m_provider, &CloudProvider::errorOccurred, this,
            [this, tm, id, localPath, cleanup](const QString &msg) {
                // Only react to an error for THIS file (progress/finished are
                // matched by localPath; errorOccurred has no path, so guard
                // against double-decrement by disconnecting on first hit).
                Q_UNUSED(localPath);
                tm->markFailed(id, msg);
                cleanup();
                if (m_folderUp) --m_folderUp->inFlight;
                pumpFolderUpload();
            });

        ++m_folderUp->inFlight;
        tm->markRunning(id);
        m_provider->upload(localPath, remoteDir);
    }

    if (!m_folderUp->it->hasNext())
        m_folderUp->exhausted = true;

    // Finished only when the iterator is drained AND no uploads remain.
    if (m_folderUp->exhausted && m_folderUp->inFlight == 0) {
        const int n = m_folderUp->done;
        m_folderUp->active = false;
        m_folderUp.reset();
        emit statusMessage(TRc("Uploaded %1 file(s).").arg(n));
        refresh();
    }
}

// ---- streaming, sequential folder download (memory-light) ----
// Lists the remote tree one directory at a time and downloads one file at a
// time. The full file list is never held in memory at once — directories are
// expanded lazily as earlier ones finish.
void CloudBrowserTab::startFolderDownload(const CloudEntry &dir, const QString &localRoot)
{
    if (m_folderDn && m_folderDn->active) {
        emit statusMessage(TRc("A folder download is already in progress."));
        return;
    }
    QDir().mkpath(localRoot);
    m_folderDn = std::make_unique<FolderDownload>();
    m_folderDn->remoteRoot = dir.path;
    m_folderDn->localRoot = QDir(localRoot).absolutePath();
    m_folderDn->active = true;
    CloudEntry root = dir;        // seed the dir queue with the chosen folder
    m_folderDn->dirs.enqueue(root);
    pumpFolderDownload();
}

void CloudBrowserTab::pumpFolderDownload()
{
    if (!m_folderDn || !m_folderDn->active)
        return;

    const int maxConcurrent = TransferManager::instance()->maxTasks();

    // 1) Start downloads from the queued files, up to maxConcurrent at once.
    while (m_folderDn->inFlight < maxConcurrent && !m_folderDn->files.isEmpty()) {
        const auto pair = m_folderDn->files.dequeue();
        const QString remotePath = pair.first;
        const QString localPath = pair.second;
        QDir().mkpath(QFileInfo(localPath).absolutePath());
        emit statusMessage(TRc("Downloading %1…").arg(QFileInfo(localPath).fileName()));

        TransferManager *tm = TransferManager::instance();
        const int id = tm->begin(TransferItem::Download, m_title,
                                 QFileInfo(localPath).fileName(),
                                 remotePath, localPath, m_provider, 0);
        auto *pConn = new QMetaObject::Connection();
        auto *fConn = new QMetaObject::Connection();
        auto *eConn = new QMetaObject::Connection();
        auto cleanup = [pConn, fConn, eConn]() {
            QObject::disconnect(*pConn); QObject::disconnect(*fConn);
            QObject::disconnect(*eConn);
            delete pConn; delete fConn; delete eConn;
        };
        *pConn = connect(m_provider, &CloudProvider::downloadProgress, this,
            [tm, id](qint64 r, qint64 t) { tm->updateProgress(id, r, t); });
        *fConn = connect(m_provider, &CloudProvider::downloadFinished, this,
            [this, tm, id, localPath, cleanup](const QString &, const QString &lp) {
                if (lp != localPath) return;
                tm->markCompleted(id);
                cleanup();
                if (m_folderDn) { --m_folderDn->inFlight; ++m_folderDn->done; }
                pumpFolderDownload();
            });
        *eConn = connect(m_provider, &CloudProvider::errorOccurred, this,
            [this, tm, id, cleanup](const QString &msg) {
                tm->markFailed(id, msg);
                cleanup();
                if (m_folderDn) --m_folderDn->inFlight;
                pumpFolderDownload();
            });
        ++m_folderDn->inFlight;
        tm->markRunning(id);
        m_provider->download(remotePath, localPath);
    }

    // If files are still queued but the concurrency limit is reached, wait for
    // an in-flight download to finish (its handler re-pumps).
    if (!m_folderDn->files.isEmpty())
        return;

    // 2) Files queue empty: expand the next remote directory (lazy listing).
    //    (Only one listing at a time; it refills the files queue on return.)
    if (!m_folderDn->dirs.isEmpty() && !m_folderDn->listing) {
        const CloudEntry d = m_folderDn->dirs.dequeue();
        m_folderDn->listing = true;

        auto *lConn = new QMetaObject::Connection();
        auto *eConn = new QMetaObject::Connection();
        auto cleanup = [lConn, eConn]() {
            QObject::disconnect(*lConn); QObject::disconnect(*eConn);
            delete lConn; delete eConn;
        };
        // Map this remote dir to its local counterpart.
        const QString relRemote = d.path.mid(m_folderDn->remoteRoot.length());
        QString localDir = m_folderDn->localRoot + relRemote;
        while (localDir.contains("//")) localDir.replace("//", "/");
        QDir().mkpath(localDir);

        *lConn = connect(m_provider, &CloudProvider::listed, this,
            [this, cleanup, localDir](const QString &, const QVector<CloudEntry> &entries) {
                cleanup();
                m_folderDn->listing = false;
                for (const CloudEntry &e : entries) {
                    if (e.isDir) {
                        m_folderDn->dirs.enqueue(e);   // expand later (lazy)
                    } else {
                        QString lp = localDir;
                        if (!lp.endsWith('/')) lp += '/';
                        lp += e.name;
                        m_folderDn->files.enqueue({ e.path, lp });
                    }
                }
                pumpFolderDownload();
            });
        *eConn = connect(m_provider, &CloudProvider::errorOccurred, this,
            [this, cleanup](const QString &msg) {
                cleanup();
                m_folderDn->listing = false;
                emit statusMessage(TRc("Error: %1").arg(msg));
                pumpFolderDownload();
            });
        m_provider->list(d.path);
        return;
    }

    // 3) Finished only when nothing is listing, no dirs remain to expand, no
    // files are queued, and no downloads are still in flight.
    if (m_folderDn->listing || m_folderDn->inFlight > 0
        || !m_folderDn->files.isEmpty() || !m_folderDn->dirs.isEmpty())
        return;
    const int n = m_folderDn->done;
    m_folderDn->active = false;
    m_folderDn.reset();
    emit statusMessage(TRc("Downloaded %1 file(s).").arg(n));
}

// Register an upload in the TransferManager and wire the provider signals.
void CloudBrowserTab::trackedUpload(const QString &localPath, const QString &remoteDir)
{
    QFileInfo fi(localPath);
    TransferManager *tm = TransferManager::instance();
    const int id = tm->begin(TransferItem::Upload, m_title, fi.fileName(),
                             remoteDir, localPath, m_provider, fi.size());

    auto *pConn = new QMetaObject::Connection();
    auto *fConn = new QMetaObject::Connection();
    auto *eConn = new QMetaObject::Connection();
    auto cleanup = [pConn, fConn, eConn]() {
        QObject::disconnect(*pConn); QObject::disconnect(*fConn);
        QObject::disconnect(*eConn);
        delete pConn; delete fConn; delete eConn;
    };

    *pConn = connect(m_provider, &CloudProvider::uploadProgress, this,
        [tm, id, localPath](const QString &lp, qint64 s, qint64 t) {
            if (lp == localPath) tm->updateProgress(id, s, t);
        });
    *fConn = connect(m_provider, &CloudProvider::uploadFinished, this,
        [this, tm, id, localPath, cleanup](const QString &lp, const QString &) {
            if (lp != localPath) return;
            tm->markCompleted(id);
            cleanup();
            refresh();   // show the newly uploaded file
        });
    *eConn = connect(m_provider, &CloudProvider::errorOccurred, this,
        [tm, id, cleanup](const QString &msg) {
            tm->markFailed(id, msg);
            cleanup();
        });

    tm->markRunning(id);
    m_provider->upload(localPath, remoteDir);
}

void CloudBrowserTab::makeFolder()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, TRc("New Folder"), TRc("Folder name:"),
        QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    auto *conn = new QMetaObject::Connection();
    *conn = connect(m_provider, &CloudProvider::operationFinished, this,
        [this, conn]() {
            disconnect(*conn);
            delete conn;
            refresh();
        });
    m_provider->makeDir(m_currentPath, name.trimmed());
}

void CloudBrowserTab::actionOpenInternal(const CloudEntry &e)
{
    const Kind k = classify(e.name);
    if (k == Video || k == Audio) {
        actionStream(e);
        return;
    }
    const QString local = tempDownloadPath(e.name);
    const QString remotePath = e.path;
    downloadThen(e, local, [this, remotePath](const QString &lp) {
        emit openDownloadedFile(lp, remotePath, m_title);
    });
}

// Force-open a file in the internal editor regardless of its extension.
void CloudBrowserTab::actionOpenAsText(const CloudEntry &e)
{
    const QString local = tempDownloadPath(e.name);
    const QString remotePath = e.path;
    downloadThen(e, local, [this, remotePath](const QString &lp) {
        emit openDownloadedFile(lp, remotePath, m_title);
    });
}

void CloudBrowserTab::actionOpenWith(const CloudEntry &e)
{
    // Pick an external program, then download to temp and launch it with the file.
    const QString program = QFileDialog::getOpenFileName(
        this, TRc("Choose Program to Open With"), QString(),
        TRc("Programs (*.exe);;All Files (*)"));
    if (program.isEmpty()) return;

    const QString local = tempDownloadPath(e.name);
    downloadThen(e, local, [program](const QString &lp) {
        QProcess::startDetached(program, { lp });
    });
}

void CloudBrowserTab::actionStream(const CloudEntry &e)
{
    // Resolve a temporary direct URL and hand it to the system default handler
    // (media player). This is "边下边播" — the player streams from the URL.
    //
    // Some providers only serve that URL when specific headers are present
    // (Baidu Pan demands "User-Agent: pan.baidu.com" and otherwise answers
    // error_code 31326 "user is not authorized"; Google Drive needs the bearer
    // token). A player/browser cannot send custom headers, so when the provider
    // reports such requirements we republish the URL through the local
    // StreamProxy and give the player that header-free localhost URL instead.
    auto *conn = new QMetaObject::Connection();
    *conn = connect(m_provider, &CloudProvider::streamUrlReady, this,
        [this, conn, e](const QString &, const QString &url) {
            disconnect(*conn);
            delete conn;
            QString playUrl = url;
            const auto headers = m_provider->streamHeaders();
            if (!headers.isEmpty()) {
                const QString local =
                    StreamProxy::instance()->publish(url, headers, e.name);
                if (local.isEmpty()) {
                    // Relay unavailable -> fall back to download-then-play.
                    emit statusMessage(
                        TRc("Local stream relay unavailable; downloading first…"));
                    const QString tmp = tempDownloadPath(e.name);
                    downloadThen(e, tmp, [](const QString &lp) {
                        QDesktopServices::openUrl(QUrl::fromLocalFile(lp));
                    });
                    return;
                }
                playUrl = local;
            }
            emit statusMessage(TRc("Streaming via system player…"));
            QDesktopServices::openUrl(QUrl(playUrl));
        });
    auto *econn = new QMetaObject::Connection();
    *econn = connect(m_provider, &CloudProvider::errorOccurred, this,
        [this, econn, e](const QString &msg) {
            disconnect(*econn);
            delete econn;
            // Streaming unsupported -> fall back to download-to-temp then play.
            emit statusMessage(TRc("Streaming unavailable (%1); downloading first…").arg(msg));
            const QString local = tempDownloadPath(e.name);
            downloadThen(e, local, [](const QString &lp) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(lp));
            });
        });
    m_provider->resolveStreamUrl(e.path);
}

void CloudBrowserTab::actionOpenDefault(const CloudEntry &e)
{
    const QString local = tempDownloadPath(e.name);
    downloadThen(e, local, [](const QString &lp) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(lp));
    });
}

// Delete a file or folder after explicit user confirmation. The confirmation
// text and resulting status message are both run through TRc() so they are
// shown in whichever UI language is currently selected.
void CloudBrowserTab::actionDelete(const CloudEntry &e)
{
    const QString question = e.isDir
        ? TRc("Delete folder \"%1\" and all its contents?").arg(e.name)
        : TRc("Delete \"%1\"?").arg(e.name);
    if (QMessageBox::question(this, TRc("Delete"), question,
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes)
        return;

    if (e.isDir) {
        startFolderDelete(e);
    } else {
        trackedDelete(e.path, false, e.name);
    }
}

// Track a single delete operation in TransferManager and TransferPanel.
void CloudBrowserTab::trackedDelete(const QString &remotePath, bool isDir, const QString &name)
{
    const QString displayName = name.isEmpty() ? QFileInfo(remotePath).fileName() : name;
    emit statusMessage(TRc("Deleting %1…").arg(displayName));

    TransferManager *tm = TransferManager::instance();
    const int id = tm->begin(TransferItem::Delete, m_title, displayName,
                             remotePath, QString(), m_provider, 0);

    auto *okConn = new QMetaObject::Connection();
    auto *errConn = new QMetaObject::Connection();
    auto cleanup = [okConn, errConn]() {
        QObject::disconnect(*okConn);
        QObject::disconnect(*errConn);
        delete okConn; delete errConn;
    };
    *okConn = connect(m_provider, &CloudProvider::operationFinished, this,
        [this, tm, id, displayName, cleanup]() {
            tm->markCompleted(id);
            cleanup();
            emit statusMessage(TRc("Deleted \"%1\".").arg(displayName));
            refresh();
        });
    *errConn = connect(m_provider, &CloudProvider::errorOccurred, this,
        [this, tm, id, cleanup](const QString &msg) {
            tm->markFailed(id, msg);
            cleanup();
            emit statusMessage(TRc("Delete failed: %1").arg(msg));
        });

    tm->markRunning(id);
    m_provider->remove(remotePath);
}

// ---- streaming, lazy folder delete (memory-light) ----
// Lists remote directories on demand, deleting files concurrently up to
// maxTasks() and recording each in TransferManager/TransferPanel. Emptied
// subdirectories and the root folder are removed bottom-up in dirsToDelete.
void CloudBrowserTab::startFolderDelete(const CloudEntry &dir)
{
    if (m_folderDel && m_folderDel->active) {
        emit statusMessage(TRc("A folder deletion is already in progress."));
        return;
    }
    m_folderDel = std::make_unique<FolderDelete>();
    m_folderDel->remoteRoot = dir.path;
    m_folderDel->active = true;
    m_folderDel->dirsToDelete.push_back(dir.path); // will be rmdir'd at the very end
    CloudEntry root = dir;
    m_folderDel->dirsToList.enqueue(root);
    emit statusMessage(TRc("Deleting %1…").arg(dir.name));
    pumpFolderDelete();
}

void CloudBrowserTab::pumpFolderDelete()
{
    if (!m_folderDel || !m_folderDel->active)
        return;

    const int maxConcurrent = TransferManager::instance()->maxTasks();

    // 1) Delete queued files up to maxConcurrent at once
    while (m_folderDel->inFlight < maxConcurrent && !m_folderDel->filesToDelete.isEmpty()) {
        const QString remoteFilePath = m_folderDel->filesToDelete.dequeue();
        const QString fileName = QFileInfo(remoteFilePath).fileName();
        emit statusMessage(TRc("Deleting %1…").arg(fileName));

        TransferManager *tm = TransferManager::instance();
        const int id = tm->begin(TransferItem::Delete, m_title, fileName,
                                 remoteFilePath, QString(), m_provider, 0);

        auto *okConn = new QMetaObject::Connection();
        auto *errConn = new QMetaObject::Connection();
        auto cleanup = [okConn, errConn]() {
            QObject::disconnect(*okConn);
            QObject::disconnect(*errConn);
            delete okConn; delete errConn;
        };

        *okConn = connect(m_provider, &CloudProvider::operationFinished, this,
            [this, tm, id, cleanup]() {
                tm->markCompleted(id);
                cleanup();
                if (m_folderDel) { --m_folderDel->inFlight; ++m_folderDel->done; }
                pumpFolderDelete();
            });
        *errConn = connect(m_provider, &CloudProvider::errorOccurred, this,
            [this, tm, id, cleanup](const QString &msg) {
                tm->markFailed(id, msg);
                cleanup();
                if (m_folderDel) --m_folderDel->inFlight;
                pumpFolderDelete();
            });

        ++m_folderDel->inFlight;
        tm->markRunning(id);
        m_provider->remove(remoteFilePath);
    }

    // If files are still queued but concurrency slot is full, wait for completion
    if (!m_folderDel->filesToDelete.isEmpty())
        return;

    // 2) Expand next directory lazily if more dirs remain to list
    if (!m_folderDel->dirsToList.isEmpty() && !m_folderDel->listing) {
        const CloudEntry d = m_folderDel->dirsToList.dequeue();
        m_folderDel->listing = true;

        auto *lConn = new QMetaObject::Connection();
        auto *eConn = new QMetaObject::Connection();
        auto cleanup = [lConn, eConn]() {
            QObject::disconnect(*lConn);
            QObject::disconnect(*eConn);
            delete lConn; delete eConn;
        };

        *lConn = connect(m_provider, &CloudProvider::listed, this,
            [this, cleanup](const QString &, const QVector<CloudEntry> &entries) {
                cleanup();
                m_folderDel->listing = false;
                for (const CloudEntry &e : entries) {
                    if (e.isDir) {
                        m_folderDel->dirsToList.enqueue(e);
                        // Record for later bottom-up rmdir
                        m_folderDel->dirsToDelete.push_back(e.path);
                    } else {
                        m_folderDel->filesToDelete.enqueue(e.path);
                    }
                }
                pumpFolderDelete();
            });
        *eConn = connect(m_provider, &CloudProvider::errorOccurred, this,
            [this, cleanup](const QString &msg) {
                cleanup();
                m_folderDel->listing = false;
                emit statusMessage(TRc("Error: %1").arg(msg));
                pumpFolderDelete();
            });

        m_provider->list(d.path);
        return;
    }

    // 3) If listing or files are in flight, wait
    if (m_folderDel->listing || m_folderDel->inFlight > 0)
        return;

    // 4) All files have been deleted. Now remove directories bottom-up
    if (!m_folderDel->dirsToDelete.isEmpty()) {
        const QString dirPath = m_folderDel->dirsToDelete.takeLast();
        const QString dirName = QFileInfo(dirPath).fileName();

        TransferManager *tm = TransferManager::instance();
        const int id = tm->begin(TransferItem::Delete, m_title, dirName,
                                 dirPath, QString(), m_provider, 0);

        auto *okConn = new QMetaObject::Connection();
        auto *errConn = new QMetaObject::Connection();
        auto cleanup = [okConn, errConn]() {
            QObject::disconnect(*okConn);
            QObject::disconnect(*errConn);
            delete okConn; delete errConn;
        };

        *okConn = connect(m_provider, &CloudProvider::operationFinished, this,
            [this, tm, id, cleanup]() {
                tm->markCompleted(id);
                cleanup();
                pumpFolderDelete(); // proceed to next parent dir
            });
        *errConn = connect(m_provider, &CloudProvider::errorOccurred, this,
            [this, tm, id, cleanup](const QString &msg) {
                tm->markFailed(id, msg);
                cleanup();
                pumpFolderDelete();
            });

        tm->markRunning(id);
        m_provider->remove(dirPath);
        return;
    }

    // 5) Finished: all files deleted and all directories removed
    const int totalDeleted = m_folderDel->done;
    m_folderDel->active = false;
    m_folderDel.reset();
    emit statusMessage(TRc("Deleted %1 file(s).").arg(totalDeleted));
    refresh();
}

// Rename a file or folder. Prompts for the new name (pre-filled with the
// current one) and reports success/failure via the (multilingual) status bar.
void CloudBrowserTab::actionRename(const CloudEntry &e)
{
    bool ok = false;
    const QString newName = QInputDialog::getText(
        this, TRc("Rename"), TRc("New name:"),
        QLineEdit::Normal, e.name, &ok);
    if (!ok)
        return;
    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty() || trimmed == e.name)
        return;

    auto *okConn = new QMetaObject::Connection();
    auto *errConn = new QMetaObject::Connection();
    auto cleanup = [okConn, errConn]() {
        QObject::disconnect(*okConn);
        QObject::disconnect(*errConn);
        delete okConn; delete errConn;
    };
    *okConn = connect(m_provider, &CloudProvider::operationFinished, this,
        [this, e, trimmed, cleanup]() {
            cleanup();
            emit statusMessage(TRc("Renamed \"%1\" to \"%2\".").arg(e.name, trimmed));
            refresh();
        });
    *errConn = connect(m_provider, &CloudProvider::errorOccurred, this,
        [this, cleanup](const QString &msg) {
            cleanup();
            emit statusMessage(TRc("Rename failed: %1").arg(msg));
        });
    m_provider->rename(e.path, trimmed);
}

void CloudBrowserTab::applyTheme(const EditorTheme &t)
{
    QString qss = QString("QTreeWidget{background:%1;color:%2;}")
                      .arg(t.background.name(), t.foreground.name());
    m_tree->setStyleSheet(qss);
}
