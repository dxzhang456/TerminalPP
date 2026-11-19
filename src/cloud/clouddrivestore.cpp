#include "clouddrivestore.h"
#include "restproviders.h"
#include "smbprovider.h"
#include "sftpprovider.h"
#include "ftpprovider.h"
#include "core/secretstore.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QNetworkProxy>

// ---------------------------------------------------------------------------
//  CloudDriveConfig
// ---------------------------------------------------------------------------
QString CloudDriveConfig::backendName() const
{
    switch (backend) {
    case CB_OneDrive:    return QStringLiteral("OneDrive");
    case CB_GoogleDrive: return QStringLiteral("GoogleDrive");
    case CB_Dropbox:     return QStringLiteral("Dropbox");
    case CB_BaiduPan:    return QStringLiteral("BaiduPan");
    case CB_WebDAV:      return QStringLiteral("WebDAV");
    case CB_SmbShare:    return QStringLiteral("SharedFolder");
    case CB_Sftp:        return QStringLiteral("SFTP");
    case CB_Ftp:         return QStringLiteral("FTP");
    }
    return QStringLiteral("Cloud");
}

QString CloudDriveConfig::displayName() const
{
    const QString n = name.trimmed();
    if (n.isEmpty())
        return backendName();
    return backendName() + QLatin1Char('-') + n;
}

// ---------------------------------------------------------------------------
//  CloudDriveStore
// ---------------------------------------------------------------------------
CloudDriveStore::CloudDriveStore()
{
    load();
}

QString CloudDriveStore::filePath() const
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/clouddrives.json");
}

void CloudDriveStore::add(const CloudDriveConfig &cfg)
{
    m_drives.push_back(cfg);
    save();
}

void CloudDriveStore::update(int index, const CloudDriveConfig &cfg)
{
    if (index < 0 || index >= m_drives.size())
        return;
    m_drives[index] = cfg;
    save();
}

void CloudDriveStore::remove(int index)
{
    if (index < 0 || index >= m_drives.size())
        return;
    m_drives.remove(index);
    save();
}

void CloudDriveStore::load()
{
    m_drives.clear();
    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        CloudDriveConfig c;
        c.name      = o.value("name").toString();
        c.backend   = o.value("backend").toInt();
        c.clientId  = o.value("clientId").toString();
        // Secrets are stored encrypted; decrypt() passes legacy plain text
        // through unchanged so older files keep working.
        c.secretKey = SecretStore::decrypt(o.value("secretKey").toString());
        c.token     = SecretStore::decrypt(o.value("token").toString());
        c.extra     = o.value("extra").toString();
        c.user      = o.value("user").toString();
        c.pass      = SecretStore::decrypt(o.value("pass").toString());
        c.host      = o.value("host").toString();
        c.port      = o.value("port").toInt();
        c.keyPath   = o.value("keyPath").toString();
        c.proxyType = o.value("proxyType").toInt();
        c.proxyHost = o.value("proxyHost").toString();
        c.proxyPort = o.value("proxyPort").toInt();
        c.proxyUser = o.value("proxyUser").toString();
        c.proxyPass = SecretStore::decrypt(o.value("proxyPass").toString());
        m_drives.push_back(c);
    }
}

void CloudDriveStore::save() const
{
    QJsonArray arr;
    for (const CloudDriveConfig &c : m_drives) {
        QJsonObject o;
        o["name"]      = c.name;
        o["backend"]   = c.backend;
        o["clientId"]  = c.clientId;
        o["secretKey"] = SecretStore::encrypt(c.secretKey);
        o["token"]     = SecretStore::encrypt(c.token);
        o["extra"]     = c.extra;
        o["user"]      = c.user;
        o["pass"]      = SecretStore::encrypt(c.pass);
        o["host"]      = c.host;
        o["port"]      = c.port;
        o["keyPath"]   = c.keyPath;
        o["proxyType"] = c.proxyType;
        o["proxyHost"] = c.proxyHost;
        o["proxyPort"] = c.proxyPort;
        o["proxyUser"] = c.proxyUser;
        o["proxyPass"] = SecretStore::encrypt(c.proxyPass);
        arr.push_back(o);
    }
    QFile f(filePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

// ---------------------------------------------------------------------------
//  Provider factory + proxy
// ---------------------------------------------------------------------------
CloudProvider *createCloudProvider(const CloudDriveConfig &cfg, QObject *parent)
{
    switch (cfg.backend) {
    case CB_OneDrive: {
        auto *p = new OneDriveProvider(parent);
        p->setAccessToken(cfg.token);
        return p;
    }
    case CB_GoogleDrive: {
        auto *p = new GoogleDriveProvider(parent);
        p->setAccessToken(cfg.token);
        return p;
    }
    case CB_Dropbox: {
        auto *p = new DropboxProvider(parent);
        p->setAccessToken(cfg.token);
        return p;
    }
    case CB_BaiduPan: {
        auto *p = new BaiduPanProvider(parent);
        p->setAccessToken(cfg.token);
        return p;
    }
    case CB_WebDAV: {
        if (cfg.extra.trimmed().isEmpty())
            return nullptr;          // WebDAV requires a base URL
        auto *p = new WebDavProvider(parent);
        p->setEndpoint(cfg.extra.trimmed(), cfg.user, cfg.pass);
        return p;
    }
    case CB_SmbShare: {
        if (cfg.extra.trimmed().isEmpty())
            return nullptr;          // requires a UNC/share path
        auto *p = new SmbShareProvider(parent);
        p->setEndpoint(cfg.extra.trimmed(), cfg.user, cfg.pass);
        return p;
    }
    case CB_Sftp: {
        if (cfg.host.trimmed().isEmpty() || cfg.user.trimmed().isEmpty())
            return nullptr;          // requires host + user
        auto *p = new SftpProvider(parent);
        const int port = cfg.port > 0 ? cfg.port : 22;
        p->setEndpoint(cfg.host.trimmed(), port, cfg.user, cfg.pass, cfg.keyPath);
        return p;
    }
    case CB_Ftp: {
        if (cfg.host.trimmed().isEmpty())
            return nullptr;          // requires host
        auto *p = new FtpProvider(parent);
        const int port = cfg.port > 0 ? cfg.port : 21;
        p->setEndpoint(cfg.host.trimmed(), port, cfg.user, cfg.pass);
        return p;
    }
    }
    return nullptr;
}

void applyCloudProxy(const CloudDriveConfig &cfg)
{
    if (cfg.proxyType == 0) {
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
        return;
    }
    QNetworkProxy proxy;
    proxy.setType(cfg.proxyType == 1 ? QNetworkProxy::HttpProxy
                                     : QNetworkProxy::Socks5Proxy);
    proxy.setHostName(cfg.proxyHost);
    proxy.setPort(static_cast<quint16>(cfg.proxyPort));
    if (!cfg.proxyUser.isEmpty())
        proxy.setUser(cfg.proxyUser);
    if (!cfg.proxyPass.isEmpty())
        proxy.setPassword(cfg.proxyPass);
    QNetworkProxy::setApplicationProxy(proxy);
}