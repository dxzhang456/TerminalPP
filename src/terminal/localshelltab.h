#ifndef LOCALSHELLTAB_H
#define LOCALSHELLTAB_H

#include <QWidget>
#include "editor/thememanager.h"

class TerminalWidget;
class QProcess;

// A local shell terminal tab: runs cmd.exe or powershell.exe as a child
// process and wires its stdout/stderr to the shared TerminalWidget (the same
// ANSI/VT100 renderer used by the SSH and serial tabs), and the terminal's
// typed input back to the process's stdin.
//
// This gives a fully functional interactive local console inside Terminal++
// without any extra dependency — QProcess + the existing terminal engine.
class LocalShellTab : public QWidget
{
    Q_OBJECT
public:
    enum Shell { Cmd, PowerShell };

    explicit LocalShellTab(Shell shell, QWidget *parent = nullptr);
    ~LocalShellTab() override;

    Shell shell() const { return m_shell; }
    TerminalWidget *terminal() const { return m_terminal; }

    bool start();        // launch the shell process
    void stop();         // terminate the shell process
    bool isRunning() const;

    // Apply the shared editor theme so local-shell tabs match the app look.
    void applyTheme(const EditorTheme &theme);

signals:
    void connectionClosed();   // the shell process exited

private:
    Shell m_shell = Cmd;
    TerminalWidget *m_terminal = nullptr;
    QProcess *m_proc = nullptr;
};

#endif // LOCALSHELLTAB_H