#include "sessionmanager.h"

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

bool SessionManager::load()
{
    m_sessions.clear();
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly))
        return false;

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isArray())
        return false;

    for (const QJsonValue &v : doc.array()) {
        QJsonObject o = v.toObject();
        SshSessionConfig c;
        c.name = o.value("name").toString();
        c.host = o.value("host").toString();
        c.port = o.value("port").toInt(22);
        c.username = o.value("username").toString();
        c.password = o.value("password").toString();
        c.keyFile = o.value("keyFile").toString();
        c.tmuxName = o.value("tmuxName").toString();
        c.useTmux = o.value("useTmux").toBool(false);
        c.extraArgs = o.value("extraArgs").toString();
        m_sessions.append(c);
    }
    return true;
}

bool SessionManager::save() const
{
    QJsonArray arr;
    for (const SshSessionConfig &c : m_sessions) {
        QJsonObject o;
        o["name"] = c.name;
        o["host"] = c.host;
        o["port"] = c.port;
        o["username"] = c.username;
        o["password"] = c.password;
        o["keyFile"] = c.keyFile;
        o["tmuxName"] = c.tmuxName;
        o["useTmux"] = c.useTmux;
        o["extraArgs"] = c.extraArgs;
        arr.append(o);
    }

    QFile f(m_path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}