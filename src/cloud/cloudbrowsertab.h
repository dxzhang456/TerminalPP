#ifndef CLOUDBROWSERTAB_H
#define CLOUDBROWSERTAB_H

#include <QWidget>
#include <QVector>
#include <QQueue>
#include <memory>
#include "cloudprovider.h"
#include "editor/thememanager.h"

class QDirIterator;

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QLineEdit;
class QToolBar;

// Browses one connected cloud drive in a tree and acts on its files:
//   * navigate folders (double-click / back / up / refresh)
//   * Download a file to a chosen local path
//   * Open internally: text -> editor tab, image -> image view
//   * Open With...: download to temp, launch an external program the user picks
//   * Stream video: hand a resolved URL to the system/default player
//   * non text/image files are downloaded to a temp folder before opening
class CloudBrowserTab : public QWidget
{
    Q_OBJECT
public:
    // Takes ownership of `provider`.
    explicit CloudBrowserTab(CloudProvider *provider, const QString &title,
                             QWidget *parent = nullptr);
    // Declared (and defined in the .cpp) so the std::unique_ptr<QDirIterator>
    // member is destroyed where QDirIterator is a complete type.
    ~CloudBrowserTab() override;

    void applyTheme(const EditorTheme &theme);
    QString title() const { return m_title; }
    // Exposes the provider so the host (MainWindow) can wire a "save back to
    // remote" action on text files opened from this tab. Not owned by caller.
    CloudProvider *provider() const { return m_provider; }

signals:
    void statusMessage(const QString &msg);
    // Ask the host (MainWindow) to open a downloaded local file internally.
    // `remotePath` + `driveLabel` let the host remember where the file came
    // from, so Save can offer to push changes back to this same connection.
    // MainWindow will use libmagic to determine whether to open in EditorTab,
    // ImageEditorTab, or HexEditorTab.
    void openDownloadedFile(const QString &localPath, const QString &remotePath,
                            const QString &driveLabel);
    // Kept for backward compatibility
    void openTextFile(const QString &localPath, const QString &remotePath,
                      const QString &driveLabel);

public slots:
    void refresh();
    void goUp();
    void goBack();
    void uploadFiles();     // pick one or more files and upload to the current dir
    void uploadFolder();    // pick a folder and upload it (recreating subdirs)
    void makeFolder();      // create a new remote folder in the current dir

private slots:
    void onItemActivated(QTreeWidgetItem *item, int column);
    void onContextMenu(const QPoint &pos);
    void onListed(const QString &path, const QVector<CloudEntry> &entries);
    void onError(const QString &message);

private:
    enum Kind { Text, Image, Video, Audio, Other };
    static Kind classify(const QString &name);
    // Sniff a downloaded buffer: true when it looks like human-readable text.
    // Lets files with an unknown/exotic extension still open in the editor.
    static bool looksLikeText(const QByteArray &sample);

    void navigateTo(const QString &path);
    QString tempDownloadPath(const QString &name) const;

    // Context-menu actions.
    void actionDownload(const CloudEntry &e);
    void actionOpenInternal(const CloudEntry &e);     // text/image
    void actionOpenAsText(const CloudEntry &e);       // force internal editor
    void actionOpenWith(const CloudEntry &e);         // external picker
    void actionStream(const CloudEntry &e);           // video/audio -> player
    void actionOpenDefault(const CloudEntry &e);      // OS default app
    void actionDelete(const CloudEntry &e);           // delete, with confirmation
    void actionRename(const CloudEntry &e);           // rename (file or folder)

    // Download `e` to a local path, then run `after(localPath)`.
    void downloadThen(const CloudEntry &e, const QString &localPath,
                      std::function<void(const QString &)> after);

    // Register a download in the TransferManager and wire the provider's
    // progress/finish/error signals to it, then start the download.
    void trackedDownload(const CloudEntry &e, const QString &localPath);
    // Register and start an upload of one local file into a remote directory.
    void trackedUpload(const QString &localPath, const QString &remoteDir);
    // Recursively upload a local directory tree, creating remote sub-folders.
    void uploadDirectory(const QString &localDir, const QString &remoteParent);

    // --- Streaming, sequential folder upload (memory-light) -----------------
    // Instead of enumerating the whole tree and queuing every file at once,
    // the tree is walked lazily with a QDirIterator and ONE file is uploaded
    // at a time; the next file is pulled from the iterator only when the
    // previous upload finishes. This keeps memory flat regardless of how many
    // files / how deep the folder is.
    // Up to this many files upload/download concurrently; the folder tree is
    // still walked lazily (one directory entry / listing pulled at a time),
    // but several files can be in flight at once for throughput.
    static constexpr int kMaxConcurrent = 10;

    struct FolderUpload {
        QString localRoot;     // chosen local folder
        QString remoteRoot;    // its remote destination (already made)
        std::unique_ptr<QDirIterator> it;   // lazy walker
        int done = 0;
        int inFlight = 0;      // files currently uploading (<= kMaxConcurrent)
        bool exhausted = false;// iterator drained
        bool active = false;
    };
    std::unique_ptr<FolderUpload> m_folderUp;
    void startFolderUpload(const QString &localRoot, const QString &remoteRoot);
    void pumpFolderUpload();            // upload the next streamed file
    // Map a local absolute path under localRoot to its remote parent dir.
    QString remoteParentFor(const QString &localAbsPath) const;
    // Streaming sequential folder download (symmetric to the upload above).
    struct FolderDownload {
        QString remoteRoot;    // chosen remote folder
        QString localRoot;     // local destination
        QQueue<CloudEntry> dirs;    // remote dirs still to list
        QQueue<QPair<QString,QString>> files;  // (remotePath, localPath) queue
        int done = 0;
        int inFlight = 0;      // files currently downloading (<= kMaxConcurrent)
        bool active = false;
        bool listing = false;
    };
    std::unique_ptr<FolderDownload> m_folderDn;
    void startFolderDownload(const CloudEntry &dir, const QString &localRoot);
    void pumpFolderDownload();

    CloudProvider *m_provider = nullptr;
    QString m_title;
    QString m_currentPath;
    QVector<QString> m_history;      // back stack
    QVector<CloudEntry> m_entries;   // current listing

    QTreeWidget *m_tree = nullptr;
    QLineEdit *m_pathEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
};

// Forward declaration used by the transfer wiring.
class TransferManager;

#endif // CLOUDBROWSERTAB_H