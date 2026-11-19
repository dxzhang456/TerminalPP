#include "cloudconnectdialog.h"
#include "restproviders.h"
#include "smbprovider.h"
#include "sftpprovider.h"
#include "ftpprovider.h"
#include "core/i18n.h"

#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QUrl>
#include <QNetworkProxy>
#include <QSettings>
#include <QMessageBox>
#include <QFileDialog>

static QString TRc(const char *s) { return I18n::instance()->tr(QString::fromUtf8(s)); }

// Built-in registered-app client ids (hardcoded, not shown in the UI).
// Baidu Pan uses this AppKey so the user doesn't need to register their own.
static const QString kBaiduClientId = QStringLiteral("UOnlv2lSJt3ykY5sbVSQTrvK7p886SGi");
// Baidu SecretKey for the app (used for the authorization-code → token exchange
// and refresh). Pre-filled, editable. Replace with your own if needed.
static const QString kBaiduSecretKey = QStringLiteral("PIQ0tncydrB8PMHp5dKYLa2mlUOB3e3H");

CloudConnectDialog::CloudConnectDialog(Mode mode, QWidget *parent)
    : QDialog(parent), m_mode(mode)
{
    setWindowTitle(TRc("Connect to Cloud Drive"));
    resize(600, 480);

    auto *grid = new QGridLayout();

    grid->addWidget(new QLabel(TRc("Provider:"), this), 0, 0);
    m_backend = new QComboBox(this);
    // Only the backends relevant to this dialog's mode are offered.
    switch (m_mode) {
    case ModeCloud:
        m_backend->addItem("Baidu Pan", BaiduPan);
        m_backend->addItem("WebDAV", WebDAV);
        break;
    case ModeShare:
        m_backend->addItem(TRc("Shared Folder (SMB/Samba)"), SmbShare);
        break;
    case ModeFtp:
        m_backend->addItem("SFTP", Sftp);
        m_backend->addItem("FTP", Ftp);
        break;
    }
    grid->addWidget(m_backend, 0, 1);
    // Login-in-browser button: opens the provider's OAuth page so the user can
    // authorize via web / QR / running app and copy back the access token.
    m_loginBtn = new QPushButton(TRc("Login in Browser (get token)"), this);
    connect(m_loginBtn, &QPushButton::clicked, this, &CloudConnectDialog::onLoginInBrowser);
    grid->addWidget(m_loginBtn, 0, 2);

    // User-chosen drive name. Combined with the backend it forms the label
    // shown in the Cloud menu and on the tab, e.g. "BaiduPan-work".
    m_nameLabel = new QLabel(TRc("Drive Name:"), this);
    m_name = new QLineEdit(this);
    m_name->setPlaceholderText(TRc("e.g. work, photos (shown as BaiduPan-work)"));
    grid->addWidget(m_nameLabel, 1, 0);
    grid->addWidget(m_name, 1, 1, 1, 2);

    // Your registered app's client_id / API key (needed for "Login in Browser").
    // Masked: it is an app credential.
    m_clientIdLabel = new QLabel(TRc("Client ID / API Key:"), this);
    m_clientId = new QLineEdit(this);
    m_clientId->setEchoMode(QLineEdit::Password);
    grid->addWidget(m_clientIdLabel, 2, 0);
    grid->addWidget(m_clientId, 2, 1, 1, 2);

    // Baidu Secret Key (app secret) — masked.
    m_secretKeyLabel = new QLabel(TRc("Secret Key:"), this);
    m_secretKey = new QLineEdit(this);
    m_secretKey->setEchoMode(QLineEdit::Password);
    grid->addWidget(m_secretKeyLabel, 3, 0);
    grid->addWidget(m_secretKey, 3, 1, 1, 2);

    // Access token shown in clear text: the user has to paste it from the
    // browser redirect URL and must be able to verify it was pasted correctly.
    m_tokenLabel = new QLabel(TRc("Access Token:"), this);
    m_token = new QLineEdit(this);
    m_token->setEchoMode(QLineEdit::Normal);
    grid->addWidget(m_tokenLabel, 4, 0);
    grid->addWidget(m_token, 4, 1, 1, 2);

    // Host + Port: SFTP / FTP only.
    m_hostLabel = new QLabel(TRc("Host:"), this);
    m_host = new QLineEdit(this);
    m_host->setPlaceholderText(TRc("server address, e.g. 192.168.1.10 or ftp.example.com"));
    grid->addWidget(m_hostLabel, 5, 0);
    grid->addWidget(m_host, 5, 1, 1, 1);
    m_portLabel = new QLabel(TRc("Port:"), this);
    m_port = new QLineEdit(this);
    m_port->setMaximumWidth(80);
    grid->addWidget(m_portLabel, 5, 2);
    grid->addWidget(m_port, 5, 3);

    // "Extra": WebDAV base URL, or SMB UNC share path.
    m_extraLabel = new QLabel(TRc("Drive ID:"), this);
    m_extra = new QLineEdit(this);
    m_extra->setPlaceholderText(TRc("e.g. \\\\server\\share or //server/share"));
    grid->addWidget(m_extraLabel, 6, 0);
    grid->addWidget(m_extra, 6, 1, 1, 3);

    m_userLabel = new QLabel(TRc("Username:"), this);
    m_user = new QLineEdit(this);
    grid->addWidget(m_userLabel, 7, 0);
    grid->addWidget(m_user, 7, 1, 1, 3);

    m_passLabel = new QLabel(TRc("Password:"), this);
    m_pass = new QLineEdit(this);
    m_pass->setEchoMode(QLineEdit::Password);
    grid->addWidget(m_passLabel, 8, 0);
    grid->addWidget(m_pass, 8, 1, 1, 3);

    // SFTP-only: optional private key file (password auth is used when empty).
    m_keyPathLabel = new QLabel(TRc("Private Key (optional):"), this);
    m_keyPath = new QLineEdit(this);
    grid->addWidget(m_keyPathLabel, 9, 0);
    grid->addWidget(m_keyPath, 9, 1, 1, 2);
    m_browseKey = new QPushButton(TRc("Browse..."), this);
    connect(m_browseKey, &QPushButton::clicked, this, [this]() {
        const QString f = QFileDialog::getOpenFileName(this, TRc("Select private key"));
        if (!f.isEmpty())
            m_keyPath->setText(f);
    });
    grid->addWidget(m_browseKey, 9, 3);

    m_hint = new QLabel(this);
    m_hint->setWordWrap(true);
    m_hint->setStyleSheet("color: gray;");

    // ---- Proxy group ----
    auto *proxyBox = new QGroupBox(TRc("Proxy Server (optional)"), this);
    auto *pg = new QGridLayout(proxyBox);
    pg->addWidget(new QLabel(TRc("Type:"), proxyBox), 0, 0);
    m_proxyType = new QComboBox(proxyBox);
    m_proxyType->addItem(TRc("None"));
    m_proxyType->addItem("HTTP");
    m_proxyType->addItem("SOCKS5");
    pg->addWidget(m_proxyType, 0, 1);
    pg->addWidget(new QLabel(TRc("Host:"), proxyBox), 1, 0);
    m_proxyHost = new QLineEdit(proxyBox);
    pg->addWidget(m_proxyHost, 1, 1);
    pg->addWidget(new QLabel(TRc("Port:"), proxyBox), 1, 2);
    m_proxyPort = new QLineEdit(proxyBox);
    m_proxyPort->setMaximumWidth(80);
    pg->addWidget(m_proxyPort, 1, 3);
    pg->addWidget(new QLabel(TRc("Username:"), proxyBox), 2, 0);
    m_proxyUser = new QLineEdit(proxyBox);
    pg->addWidget(m_proxyUser, 2, 1);
    pg->addWidget(new QLabel(TRc("Password:"), proxyBox), 2, 2);
    m_proxyPass = new QLineEdit(proxyBox);
    m_proxyPass->setEchoMode(QLineEdit::Password);
    pg->addWidget(m_proxyPass, 2, 3);
    connect(m_proxyType, &QComboBox::currentIndexChanged,
            this, &CloudConnectDialog::onProxyTypeChanged);
    // Proxy settings only make sense for REST/cloud backends (HTTP traffic);
    // SMB/SFTP/FTP use their own native protocols and ignore the app-wide
    // QNetworkProxy, so hide the group entirely outside ModeCloud to avoid
    // implying it has any effect there.
    proxyBox->setVisible(m_mode == ModeCloud);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        applyProxyToApplication();   // apply before the provider is created/used
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *v = new QVBoxLayout(this);
    v->addLayout(grid);
    v->addWidget(m_hint);
    v->addWidget(proxyBox);
    v->addStretch(1);
    v->addWidget(buttons);

    connect(m_backend, &QComboBox::currentIndexChanged,
            this, &CloudConnectDialog::onBackendChanged);
    onBackendChanged(0);
    onProxyTypeChanged(0);
}

void CloudConnectDialog::onProxyTypeChanged(int)
{
    const bool on = m_proxyType->currentIndex() != 0;  // 0 = None
    m_proxyHost->setEnabled(on);
    m_proxyPort->setEnabled(on);
    m_proxyUser->setEnabled(on);
    m_proxyPass->setEnabled(on);
}

void CloudConnectDialog::applyProxyToApplication() const
{
    if (m_mode != ModeCloud)
        return;   // SMB/SFTP/FTP don't use the app-wide HTTP/SOCKS proxy
    const int t = m_proxyType->currentIndex();
    if (t == 0) {
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
        return;
    }
    QNetworkProxy proxy;
    proxy.setType(t == 1 ? QNetworkProxy::HttpProxy : QNetworkProxy::Socks5Proxy);
    proxy.setHostName(m_proxyHost->text().trimmed());
    proxy.setPort(static_cast<quint16>(m_proxyPort->text().toUInt()));
    if (!m_proxyUser->text().isEmpty())
        proxy.setUser(m_proxyUser->text());
    if (!m_proxyPass->text().isEmpty())
        proxy.setPassword(m_proxyPass->text());
    QNetworkProxy::setApplicationProxy(proxy);
}

void CloudConnectDialog::onLoginInBrowser()
{
    // Open the provider's OAuth authorization page in the default browser so
    // the user can log in via web / QR code / an already-running app. After
    // authorizing they obtain an access token which they paste into the field.
    // (We keep the manual token entry; this is just a convenience shortcut.)
    const int b = m_backend->currentData().toInt();
    if (b == WebDAV) {
        m_hint->setText(TRc("WebDAV uses URL + account credentials, no OAuth login."));
        return;
    }

    // The OAuth endpoints require a real registered-app client_id (Baidu calls
    // it "API Key"). The Client ID field is shown for all REST backends; for
    // Baidu it is pre-filled with a built-in AppKey but remains editable.
    QString cid = m_clientId->text().trimmed();
    if (cid.isEmpty()) {
        if (b == BaiduPan)
            cid = kBaiduClientId;    // fall back to the built-in Baidu AppKey
        else {
            QMessageBox::information(this, TRc("Login in Browser (get token)"),
                TRc("Please enter your registered app's Client ID / API Key first.\n"
                    "Each cloud provider requires you to register a developer app and "
                    "paste its client_id (Baidu: API Key) here; otherwise the login "
                    "page returns \"invalid_client / unknown client id\"."));
            m_clientId->setFocus();
            return;
        }
    }
    // Persist the client id (and Baidu secret key) so it is remembered next time.
    QSettings s;
    s.setValue(QStringLiteral("cloud/clientId/%1").arg(b), cid);
    if (b == BaiduPan)
        s.setValue(QStringLiteral("cloud/secretKey/%1").arg(b), m_secretKey->text().trimmed());

    const QString ec = QString::fromUtf8(QUrl::toPercentEncoding(cid));
    QString url;
    switch (b) {
    case OneDrive:
        url = QString("https://login.microsoftonline.com/common/oauth2/v2.0/authorize"
              "?client_id=%1&response_type=token"
              "&redirect_uri=https://login.microsoftonline.com/common/oauth2/nativeclient"
              "&scope=Files.ReadWrite%20offline_access").arg(ec);
        break;
    case GoogleDrive:
        url = QString("https://accounts.google.com/o/oauth2/v2/auth"
              "?client_id=%1&response_type=token"
              "&redirect_uri=urn:ietf:wg:oauth:2.0:oob"
              "&scope=https://www.googleapis.com/auth/drive").arg(ec);
        break;
    case Dropbox:
        url = QString("https://www.dropbox.com/oauth2/authorize"
              "?client_id=%1&response_type=token"
              "&redirect_uri=https://localhost").arg(ec);
        break;
    case BaiduPan:
        // scope MUST include "netdisk" — that is the scope that grants file
        // listing + download (dlink) access. Without it the filemetas API
        // returns no dlink and downloads fail ("app token lacks download scope").
        url = QString("https://openapi.baidu.com/oauth/2.0/authorize"
              "?client_id=%1&response_type=token"
              "&redirect_uri=oob&scope=basic,netdisk&display=page&qrcode=1").arg(ec);
        break;
    default:
        break;
    }
    QDesktopServices::openUrl(QUrl(url));
    m_hint->setText(TRc("A browser window opened for login (supports web / QR code). "
                        "After authorizing, copy the access_token (and drive_id for "
                        "Aliyun) from the redirect URL and paste it above. The manual "
                        "token field remains available."));
}

void CloudConnectDialog::onBackendChanged(int)
{
    const int b = m_backend->currentData().toInt();
    const bool isWebDav = (b == WebDAV);
    const bool isBaidu  = (b == BaiduPan);
    const bool isSmb    = (b == SmbShare);
    const bool isSftp   = (b == Sftp);
    const bool isFtp    = (b == Ftp);
    const bool isHostBased = isSftp || isFtp;   // needs Host + Port
    const bool isRest = (b == OneDrive || b == GoogleDrive || b == Dropbox || isBaidu);

    // Token row + login button: REST/OAuth backends only.
    m_tokenLabel->setVisible(isRest);
    m_token->setVisible(isRest);
    m_loginBtn->setVisible(isRest);
    // Client ID field: REST backends only.
    m_clientIdLabel->setVisible(isRest);
    m_clientId->setVisible(isRest);
    if (isRest) {
        QSettings s;
        QString saved = s.value(QStringLiteral("cloud/clientId/%1").arg(b)).toString();
        if (saved.isEmpty() && isBaidu)
            saved = kBaiduClientId;
        m_clientId->setText(saved);
    }

    // Secret Key row: Baidu only (its OpenAPI needs AppKey + SecretKey).
    m_secretKeyLabel->setVisible(isBaidu);
    m_secretKey->setVisible(isBaidu);
    if (isBaidu) {
        QSettings s;
        QString sk = s.value(QStringLiteral("cloud/secretKey/%1").arg(b)).toString();
        if (sk.isEmpty())
            sk = kBaiduSecretKey;
        m_secretKey->setText(sk);
    }

    // Host + Port rows: SFTP / FTP only.
    m_hostLabel->setVisible(isHostBased);
    m_host->setVisible(isHostBased);
    m_portLabel->setVisible(isHostBased);
    m_port->setVisible(isHostBased);
    if (isHostBased && m_port->text().isEmpty())
        m_port->setText(isSftp ? "22" : "21");

    // "Extra" row: WebDAV base URL, or SMB UNC path.
    m_extra->setVisible(isWebDav || isSmb);
    m_extraLabel->setVisible(isWebDav || isSmb);
    m_extraLabel->setText(isSmb ? TRc("Share Path (UNC):") : TRc("WebDAV URL:"));

    // Username/password: WebDAV, SMB, SFTP, FTP.
    const bool showUserPass = isWebDav || isSmb || isHostBased;
    m_userLabel->setVisible(showUserPass);
    m_user->setVisible(showUserPass);
    m_passLabel->setVisible(showUserPass);
    m_pass->setVisible(showUserPass);

    // SFTP-only private key row.
    m_keyPathLabel->setVisible(isSftp);
    m_keyPath->setVisible(isSftp);
    m_browseKey->setVisible(isSftp);

    switch (b) {
    case OneDrive:
        m_hint->setText(TRc("Paste a Microsoft Graph OAuth2 access token (Files.ReadWrite scope), "
                            "or click \"Login in Browser\" to obtain one."));
        break;
    case GoogleDrive:
        m_hint->setText(TRc("Paste a Google OAuth2 access token (drive scope), "
                            "or click \"Login in Browser\" to obtain one."));
        break;
    case Dropbox:
        m_hint->setText(TRc("Paste a Dropbox access token, or click \"Login in Browser\"."));
        break;
    case BaiduPan:
        m_hint->setText(TRc("Baidu: fill \"Client ID / API Key\" (AppKey) and \"Secret Key\" "
                            "(SecretKey) from your app's details page. A built-in AppKey is "
                            "pre-filled; enter the matching SecretKey. Click \"Login in Browser\" "
                            "to scan the QR code; after authorizing, copy access_token from the "
                            "redirect URL into the Access Token field, or paste a token directly.\n"
                            "IMPORTANT: your Baidu app must have the download scope enabled. In the "
                            "Baidu developer console open your app, enable the \"netdisk\" / "
                            "\"\xE4\xB8\x8B\xE8\xBD\xBD\" (download) permission, and make sure the "
                            "token is issued with scope=basic,netdisk. Without the download scope "
                            "the dlink API returns no link and downloads fail."));
        break;
    case WebDAV:
        m_hint->setText(TRc("Enter the WebDAV base URL (e.g. https://dav.jianguoyun.com/dav/) "
                            "and your account credentials. Works with Nutstore and self-hosted servers."));
        break;
    case SmbShare:
        m_hint->setText(TRc("Enter a Windows/Samba share path, e.g. \\\\server\\share or "
                            "//server/share. Works against any SMB dialect (SMB1/CIFS, SMB2, "
                            "SMB3) the server offers — Windows negotiates it automatically. "
                            "Username/password are used to authenticate the share if needed."));
        break;
    case Sftp:
        m_hint->setText(TRc("Enter the SFTP server host and port (default 22), your username, "
                            "and either a password or a private key file. Uses the same native "
                            "libssh2 client as the SSH terminal feature — no external sftp.exe."));
        break;
    case Ftp:
        m_hint->setText(TRc("Enter the FTP server host and port (default 21) and your account "
                            "credentials. Uses passive-mode (PASV) data connections."));
        break;
    }
}

CloudProvider *CloudConnectDialog::createProvider(QObject *parent) const
{
    const int b = m_backend->currentData().toInt();
    switch (b) {
    case OneDrive: {
        auto *p = new OneDriveProvider(parent);
        p->setAccessToken(m_token->text().trimmed());
        return p;
    }
    case GoogleDrive: {
        auto *p = new GoogleDriveProvider(parent);
        p->setAccessToken(m_token->text().trimmed());
        return p;
    }
    case Dropbox: {
        auto *p = new DropboxProvider(parent);
        p->setAccessToken(m_token->text().trimmed());
        return p;
    }
    case BaiduPan: {
        auto *p = new BaiduPanProvider(parent);
        p->setAccessToken(m_token->text().trimmed());
        return p;
    }
    case WebDAV: {
        if (m_extra->text().trimmed().isEmpty())
            return nullptr;
        auto *p = new WebDavProvider(parent);
        p->setEndpoint(m_extra->text().trimmed(),
                       m_user->text(), m_pass->text());
        return p;
    }
    case SmbShare: {
        if (m_extra->text().trimmed().isEmpty())
            return nullptr;
        auto *p = new SmbShareProvider(parent);
        p->setEndpoint(m_extra->text().trimmed(), m_user->text(), m_pass->text());
        return p;
    }
    case Sftp: {
        if (m_host->text().trimmed().isEmpty() || m_user->text().trimmed().isEmpty())
            return nullptr;
        auto *p = new SftpProvider(parent);
        const int port = m_port->text().toInt() > 0 ? m_port->text().toInt() : 22;
        p->setEndpoint(m_host->text().trimmed(), port, m_user->text(),
                       m_pass->text(), m_keyPath->text().trimmed());
        return p;
    }
    case Ftp: {
        if (m_host->text().trimmed().isEmpty())
            return nullptr;
        auto *p = new FtpProvider(parent);
        const int port = m_port->text().toInt() > 0 ? m_port->text().toInt() : 21;
        p->setEndpoint(m_host->text().trimmed(), port, m_user->text(), m_pass->text());
        return p;
    }
    }
    return nullptr;
}

QString CloudConnectDialog::driveName() const
{
    return m_name->text().trimmed();
}

// Label used in the Cloud menu and on the tab: "BaiduPan-<name>".
QString CloudConnectDialog::displayTitle() const
{
    return config().displayName();
}

CloudDriveConfig CloudConnectDialog::config() const
{
    CloudDriveConfig c;
    c.name      = m_name->text().trimmed();
    c.backend   = m_backend->currentData().toInt();
    c.clientId  = m_clientId->text().trimmed();
    c.secretKey = m_secretKey->text().trimmed();
    c.token     = m_token->text().trimmed();
    c.extra     = m_extra->text().trimmed();
    c.user      = m_user->text();
    c.pass      = m_pass->text();
    c.host      = m_host->text().trimmed();
    c.port      = m_port->text().toInt();
    c.keyPath   = m_keyPath->text().trimmed();
    c.proxyType = m_proxyType->currentIndex();
    c.proxyHost = m_proxyHost->text().trimmed();
    c.proxyPort = m_proxyPort->text().toInt();
    c.proxyUser = m_proxyUser->text();
    c.proxyPass = m_proxyPass->text();
    return c;
}

void CloudConnectDialog::setConfig(const CloudDriveConfig &cfg)
{
    // Select the backend first so onBackendChanged() has already adjusted
    // field visibility before the values are written in.
    const int idx = m_backend->findData(cfg.backend);
    if (idx >= 0)
        m_backend->setCurrentIndex(idx);

    m_name->setText(cfg.name);
    if (!cfg.clientId.isEmpty())
        m_clientId->setText(cfg.clientId);
    if (!cfg.secretKey.isEmpty())
        m_secretKey->setText(cfg.secretKey);
    m_token->setText(cfg.token);
    m_extra->setText(cfg.extra);
    m_user->setText(cfg.user);
    m_pass->setText(cfg.pass);
    m_host->setText(cfg.host);
    m_port->setText(cfg.port > 0 ? QString::number(cfg.port) : QString());
    m_keyPath->setText(cfg.keyPath);

    m_proxyType->setCurrentIndex(cfg.proxyType);
    m_proxyHost->setText(cfg.proxyHost);
    m_proxyPort->setText(cfg.proxyPort > 0 ? QString::number(cfg.proxyPort)
                                           : QString());
    m_proxyUser->setText(cfg.proxyUser);
    m_proxyPass->setText(cfg.proxyPass);
    onProxyTypeChanged(cfg.proxyType);
}