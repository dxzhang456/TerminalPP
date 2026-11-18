#ifndef CLOUDBROWSERTAB_H
#define CLOUDBROWSERTAB_H

#include <QWidget>
#include <QVector>
#include "cloudprovider.h"
#include "editor/thememanager.h"

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
//   * Open text with VS Code (if `code` is on PATH)
//   * Stream video: hand a resolved URL to the system/default player
//   * non text/image files are downloaded to a temp folder before opening
class CloudBrowserTab : public QWidget
{
    Q_OBJECT
public:
    // Takes ownership of `provider`.
    explicit CloudBrowserTab(CloudProvider *provider, const QString &title,
                             QWidget *parent = nullptr);

    void applyTheme(const EditorTheme &theme);
    QString title() const { return m_title; }

signals:
    void statusMessage(const QString &msg);
    // Ask the host (MainWindow) to open a downloaded local file internally.
    void openTextFile(const QString &localPath);
    void openImageFileCompareNone(const QString &localPath); // open image viewer

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
    void actionOpenWithVSCode(const CloudEntry &e);   // text -> VS Code
    void actionStream(const CloudEntry &e);           // video/audio -> player
    void actionOpenDefault(const CloudEntry &e);      // OS default app

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