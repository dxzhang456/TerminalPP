#ifndef SSHTAB_H
#define SSHTAB_H

#include <QWidget>
#include "sessionmanager.h"
#include "editor/thememanager.h"

class TerminalWidget;
class SshSession;

// A single SSH session tab: a TerminalWidget wired to an SshSession.
// Replicates MuxTerm's terminal_tab.py.
class SshTab : public QWidget
{
    Q_OBJECT
public:
    explicit SshTab(const SshSessionConfig &cfg, QWidget *parent = nullptr);

    void connectSession();
    void disconnectSession();
    void reconnect();
    bool isConnected() const;

    const SshSessionConfig &config() const { return m_config; }
    TerminalWidget *terminal() const { return m_terminal; }

    // Apply the shared editor theme's background/foreground to the terminal so
    // SSH tabs match the rest of the application's look.
    void applyTheme(const EditorTheme &theme);

signals:
    void titleChanged(const QString &title);
    void connectionClosed();

private:
    SshSessionConfig m_config;
    TerminalWidget *m_terminal = nullptr;
    SshSession *m_session = nullptr;
};

#endif // SSHTAB_H