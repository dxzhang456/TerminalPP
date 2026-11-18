#ifndef CLOUDCONNECTDIALOG_H
#define CLOUDCONNECTDIALOG_H

#include <QDialog>
#include <QString>

#include "clouddrivestore.h"

class QComboBox;
class QLineEdit;
class QLabel;
class QPushButton;
class CloudProvider;

// Dialog to pick a cloud backend and supply its credentials, then build a
// ready-to-use CloudProvider instance.
class CloudConnectDialog : public QDialog
{
    Q_OBJECT
public:
    enum Backend { OneDrive, GoogleDrive, Dropbox, BaiduPan, WebDAV };

    explicit CloudConnectDialog(QWidget *parent = nullptr);

    // Create a provider from the entered values (caller owns it). Returns
    // nullptr if required fields are missing.
    CloudProvider *createProvider(QObject *parent) const;

    // Menu/tab label for the entered drive, e.g. "BaiduPan-work".
    QString displayTitle() const;

    // The user-entered drive name (may be empty).
    QString driveName() const;

    // Read all entered values as a storable configuration.
    CloudDriveConfig config() const;

    // Pre-fill every field from a stored configuration (used when editing or
    // reconnecting an already-saved drive).
    void setConfig(const CloudDriveConfig &cfg);

    // Apply the dialog's proxy settings to the whole application
    // (QNetworkProxy::setApplicationProxy). Also called from a loaded config.
    void applyProxyToApplication() const;

private slots:
    void onBackendChanged(int index);
    void onLoginInBrowser();   // open provider OAuth page to obtain a token
    void onProxyTypeChanged(int index);

private:
    QComboBox *m_backend = nullptr;
    QLineEdit *m_name = nullptr;        // user-chosen drive name
    QLineEdit *m_clientId = nullptr;    // your registered app's client_id / API key
    QLineEdit *m_secretKey = nullptr;   // Baidu SecretKey (app secret)
    QLineEdit *m_token = nullptr;       // OAuth access token (REST backends)
    QLineEdit *m_extra = nullptr;       // Aliyun drive_id, or WebDAV URL
    QLineEdit *m_user = nullptr;        // WebDAV user
    QLineEdit *m_pass = nullptr;        // WebDAV password
    QLabel *m_nameLabel = nullptr;
    QLabel *m_clientIdLabel = nullptr;
    QLabel *m_secretKeyLabel = nullptr;
    QLabel *m_tokenLabel = nullptr;
    QLabel *m_extraLabel = nullptr;
    QLabel *m_userLabel = nullptr;
    QLabel *m_passLabel = nullptr;
    QLabel *m_hint = nullptr;
    QPushButton *m_loginBtn = nullptr;  // "Login in Browser" (OAuth)

    // Proxy controls.
    QComboBox *m_proxyType = nullptr;   // None / HTTP / SOCKS5
    QLineEdit *m_proxyHost = nullptr;
    QLineEdit *m_proxyPort = nullptr;
    QLineEdit *m_proxyUser = nullptr;
    QLineEdit *m_proxyPass = nullptr;
};

#endif // CLOUDCONNECTDIALOG_H