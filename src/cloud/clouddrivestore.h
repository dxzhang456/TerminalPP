#ifndef CLOUDDRIVESTORE_H
#define CLOUDDRIVESTORE_H

#include <QString>
#include <QVector>

class CloudProvider;
class QObject;

// Backend ids. The values MUST stay in sync with CloudConnectDialog::Backend
// (same order) because the two are converted with a plain int cast.
enum CloudBackend {
    CB_OneDrive = 0,
    CB_GoogleDrive,
    CB_Dropbox,
    CB_BaiduPan,
    CB_WebDAV,
    CB_SmbShare,   // Windows / Samba shared folder (any SMB1/2/3 server)
    CB_Sftp,       // SFTP over SSH (libssh2)
    CB_Ftp         // plain FTP
};

// Everything needed to re-create a cloud-drive connection without asking the
// user again: the user-chosen drive name, the backend, its credentials and the
// optional proxy settings.
struct CloudDriveConfig
{
    QString name;          // user-given drive name, e.g. "work" or "照片"
    int     backend = CB_OneDrive;

    QString clientId;      // registered app client_id / API key
    QString secretKey;     // Baidu SecretKey (app secret)
    QString token;         // OAuth access token
    QString extra;         // WebDAV base URL, or SMB UNC path (\\server\share)
    QString user;          // WebDAV / SMB / SFTP / FTP username
    QString pass;          // WebDAV / SMB / SFTP / FTP password

    // SFTP / FTP connection details.
    QString host;           // server hostname or IP
    int     port = 0;       // 0 => provider default (22 for SFTP, 21 for FTP)
    QString keyPath;        // SFTP: optional private key file (empty = password auth)

    int     proxyType = 0; // 0 = None, 1 = HTTP, 2 = SOCKS5
    QString proxyHost;
    int     proxyPort = 0;
    QString proxyUser;
    QString proxyPass;

    // Short backend identifier used in the menu, e.g. "BaiduPan".
    QString backendName() const;

    // Menu/tab label: "BaiduPan-<name>" (falls back to just the backend name
    // when the user left the name empty).
    QString displayName() const;
};

// Persists the list of connected cloud drives to clouddrives.json in the
// application data folder. Tokens / passwords / secret keys are encrypted with
// SecretStore (Windows DPAPI) so they never touch the disk as plain text.
class CloudDriveStore
{
public:
    CloudDriveStore();

    const QVector<CloudDriveConfig> &drives() const { return m_drives; }
    int count() const { return m_drives.size(); }

    void add(const CloudDriveConfig &cfg);
    void update(int index, const CloudDriveConfig &cfg);
    void remove(int index);

    void load();
    void save() const;

private:
    QString filePath() const;
    QVector<CloudDriveConfig> m_drives;
};

// Build a ready-to-use provider from a stored configuration (caller owns it).
// Returns nullptr when required fields are missing (e.g. WebDAV without URL).
CloudProvider *createCloudProvider(const CloudDriveConfig &cfg, QObject *parent);

// Apply a configuration's proxy settings application-wide.
void applyCloudProxy(const CloudDriveConfig &cfg);

#endif // CLOUDDRIVESTORE_H