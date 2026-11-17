#include "sessionmanager.h"
#include "core/secretstore.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

SessionManager::SessionManager()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    m_path = dir + "/sessions.json";
    load();
}

// ---------------- SSH ----------------
void SessionManager::add(const SshSessionConfig &cfg)
{
    m_sessions.append(cfg);
    save();
}

void SessionManager::update(int index, const SshSessionConfig &cfg)
{
    if (index >= 0 && index < m_sessions.size()) {
        m_sessions[index] = cfg;
        save();
    }
}

void SessionManager::remove(int index)
{
    if (index >= 0 && index < m_sessions.size()) {
        m_sessions.removeAt(index);
        save();
    }
}

// ---------------- Serial ----------------
void SessionManager::addSerial(const SerialSessionConfig &cfg)
{
    m_serial.append(cfg);
    save();
}

void SessionManager::updateSerial(int index, const SerialSessionConfig &cfg)
{
    if (index >= 0 && index < m_serial.size()) {
        m_serial[index] = cfg;
        save();
    }
}

void SessionManager::removeSerial(int index)
{
    if (index >= 0 && index < m_serial.size()) {
        m_serial.removeAt(index);
        save();
    }
}

// ---------------- Persistence ----------------
bool SessionManager::load()
{
    m_sessions.clear();
    m_serial.clear();

    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly))
        return false;

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    // Backward compatible: old format was a top-level array of SSH sessions.
    QJsonArray sshArr;
    QJsonArray serArr;
    if (doc.isArray()) {
        sshArr = doc.array();
    } else if (doc.isObject()) {
        QJsonObject root = doc.object();
        sshArr = root.value("ssh").toArray();
        serArr = root.value("serial").toArray();
    }

    for (const QJsonValue &v : std::as_const(sshArr)) {
        QJsonObject o = v.toObject();
        SshSessionConfig c;
        c.name = o.value("name").toString();
        c.host = o.value("host").toString();
        c.port = o.value("port").toInt(22);
        c.username = o.value("username").toString();
        // Passwords are stored encrypted (DPAPI on Windows, XOR+Base64 else);
        // decrypt transparently. Legacy plain-text values pass through.
        c.password = SecretStore::decrypt(o.value("password").toString());
        c.keyFile = o.value("keyFile").toString();
        c.tmuxName = o.value("tmuxName").toString();
        c.useTmux = o.value("useTmux").toBool(false);
        c.extraArgs = o.value("extraArgs").toString();
        m_sessions.append(c);
    }

    for (const QJsonValue &v : std::as_const(serArr)) {
        QJsonObject o = v.toObject();
        SerialSessionConfig c;
        c.name = o.value("name").toString();
        c.portName = o.value("portName").toString();
        c.baudRate = o.value("baudRate").toInt(115200);
        c.dataBits = o.value("dataBits").toInt(8);
        c.parity = o.value("parity").toString("None");
        c.stopBits = o.value("stopBits").toString("1");
        c.flow = o.value("flow").toString("None");
        c.lineEnding = o.value("lineEnding").toString("CR");
        c.localEcho = o.value("localEcho").toBool(false);
        m_serial.append(c);
    }
    return true;
}

bool SessionManager::save() const
{
    QJsonArray sshArr;
    for (const SshSessionConfig &c : m_sessions) {
        QJsonObject o;
        o["name"] = c.name;
        o["host"] = c.host;
        o["port"] = c.port;
        o["username"] = c.username;
        // Never write the password in plain text.
        o["password"] = SecretStore::encrypt(c.password);
        o["keyFile"] = c.keyFile;
        o["tmuxName"] = c.tmuxName;
        o["useTmux"] = c.useTmux;
        o["extraArgs"] = c.extraArgs;
        sshArr.append(o);
    }

    QJsonArray serArr;
    for (const SerialSessionConfig &c : m_serial) {
        QJsonObject o;
        o["name"] = c.name;
        o["portName"] = c.portName;
        o["baudRate"] = c.baudRate;
        o["dataBits"] = c.dataBits;
        o["parity"] = c.parity;
        o["stopBits"] = c.stopBits;
        o["flow"] = c.flow;
        o["lineEnding"] = c.lineEnding;
        o["localEcho"] = c.localEcho;
        serArr.append(o);
    }

    QJsonObject root;
    root["ssh"] = sshArr;
    root["serial"] = serArr;

    QFile f(m_path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}