#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QString>
#include <QList>

// One saved SSH session configuration.
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

// One saved serial/UART session (primitive fields, independent of QtSerialPort
// so the manager has no Qt SerialPort dependency).
struct SerialSessionConfig {
    QString name;
    QString portName;              // e.g. COM3
    int     baudRate = 115200;
    int     dataBits = 8;          // 5/6/7/8
    QString parity = "None";       // None/Even/Odd/Mark/Space
    QString stopBits = "1";        // 1/1.5/2
    QString flow = "None";         // None/RTS-CTS/XON-XOFF
    QString lineEnding = "CR";     // CR/LF/CRLF/None
    bool    localEcho = false;

    QString displayName() const {
        if (!name.isEmpty())
            return name;
        return QString("%1 @ %2").arg(portName).arg(baudRate);
    }
};

// Persists SSH + serial session configs to sessions.json.
class SessionManager
{
public:
    SessionManager();

    // --- SSH ---
    const QList<SshSessionConfig> &sessions() const { return m_sessions; }
    void add(const SshSessionConfig &cfg);
    void update(int index, const SshSessionConfig &cfg);
    void remove(int index);

    // --- Serial ---
    const QList<SerialSessionConfig> &serialSessions() const { return m_serial; }
    void addSerial(const SerialSessionConfig &cfg);
    void updateSerial(int index, const SerialSessionConfig &cfg);
    void removeSerial(int index);

    bool load();
    bool save() const;

    QString storagePath() const { return m_path; }

private:
    QList<SshSessionConfig>    m_sessions;
    QList<SerialSessionConfig> m_serial;
    QString m_path;
};

#endif // SESSIONMANAGER_H