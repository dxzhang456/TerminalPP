#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QString>
#include <QList>

// One saved SSH session configuration (MuxTerm's session model).
struct SshSessionConfig {
    QString name;
    QString host;
    int     port = 22;
    QString username;
    QString password;      // optional (not recommended to store plaintext)
    QString keyFile;       // optional private key path
    QString tmuxName;      // optional tmux session name
    bool    useTmux = false;
    QString extraArgs;     // additional ssh arguments

    QString displayName() const {
        if (!name.isEmpty())
            return name;
        if (!username.isEmpty())
            return QString("%1@%2").arg(username, host);
        return host;
    }
};

// Persists SSH session configs to sessions.json, like MuxTerm.
class SessionManager
{
public:
    SessionManager();

    const QList<SshSessionConfig> &sessions() const { return m_sessions; }
    void add(const SshSessionConfig &cfg);
    void update(int index, const SshSessionConfig &cfg);
    void remove(int index);

    bool load();
    bool save() const;

    QString storagePath() const { return m_path; }

private:
    QList<SshSessionConfig> m_sessions;
    QString m_path;
};

#endif // SESSIONMANAGER_H