#include "localshelltab.h"
#include "terminalwidget.h"
#include "ansiparser.h"
#include "core/i18n.h"

#include <QVBoxLayout>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QDir>

static inline QString TRl(const char *s) { return I18n::instance()->tr(QString::fromUtf8(s)); }

LocalShellTab::LocalShellTab(Shell shell, QWidget *parent)
    : QWidget(parent), m_shell(shell)
{
    m_terminal = new TerminalWidget(this);
    m_terminal->setColors(QColor(212, 212, 212), QColor(30, 30, 30));

    auto *l = new QVBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    l->addWidget(m_terminal);

    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);   // stdout+stderr

    // Typed input -> process stdin.
    connect(m_terminal, &TerminalWidget::dataEntered, this,
            [this](const QByteArray &d) {
        if (m_proc && m_proc->state() == QProcess::Running)
            m_proc->write(d);
    });

    // Process output -> terminal renderer.
    connect(m_proc, &QProcess::readyReadStandardOutput, this, [this]() {
        m_terminal->parser()->feed(m_proc->readAllStandardOutput());
    });
    connect(m_proc, &QProcess::readyReadStandardError, this, [this]() {
        m_terminal->parser()->feed(m_proc->readAllStandardError());
    });
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) {
        m_terminal->parser()->feed(
            QString("\r\n[%1]\r\n").arg(TRl("process exited (code %1)").arg(code)).toUtf8());
        emit connectionClosed();
    });
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        m_terminal->parser()->feed(
            ("\r\n[" + TRl("failed to start the shell process") + "]\r\n").toUtf8());
    });
}

LocalShellTab::~LocalShellTab()
{
    stop();
}

bool LocalShellTab::start()
{
    if (m_proc->state() != QProcess::NotRunning)
        return true;

    // Start in the user's home directory for a familiar prompt.
    m_proc->setWorkingDirectory(QDir::homePath());

    // Keep ANSI/VT processing on where possible so colored output renders.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("TERM", "xterm-256color");
    m_proc->setProcessEnvironment(env);

#ifdef _WIN32
    if (m_shell == PowerShell) {
        // Prefer Windows PowerShell; fall back to pwsh (PowerShell 7) if present.
        QString ps = QStandardPaths::findExecutable("powershell.exe");
        if (ps.isEmpty())
            ps = QStandardPaths::findExecutable("pwsh.exe");
        if (ps.isEmpty())
            ps = "powershell.exe";
        m_proc->start(ps, { "-NoLogo", "-NoExit" });
    } else {
        QString cmd = QStandardPaths::findExecutable("cmd.exe");
        if (cmd.isEmpty())
            cmd = "cmd.exe";
        m_proc->start(cmd, { "/Q", "/K" });   // /Q echo off, /K keep open
    }
#else
    // On non-Windows, run the user's shell (bash/sh) — "cmd" maps to sh,
    // "PowerShell" to pwsh if available.
    if (m_shell == PowerShell) {
        QString ps = QStandardPaths::findExecutable("pwsh");
        if (ps.isEmpty()) ps = "/bin/bash";
        m_proc->start(ps, {});
    } else {
        QString sh = qEnvironmentVariable("SHELL");
        if (sh.isEmpty()) sh = "/bin/bash";
        m_proc->start(sh, { "-i" });
    }
#endif

    const bool ok = m_proc->waitForStarted(5000);
    if (!ok) {
        m_terminal->parser()->feed(
            ("[" + TRl("failed to start the shell process") + "]\r\n").toUtf8());
    }
    m_terminal->setFocus();
    return ok;
}

void LocalShellTab::stop()
{
    if (!m_proc)
        return;
    if (m_proc->state() != QProcess::NotRunning) {
        m_proc->terminate();
        if (!m_proc->waitForFinished(1500))
            m_proc->kill();
    }
}

bool LocalShellTab::isRunning() const
{
    return m_proc && m_proc->state() == QProcess::Running;
}

void LocalShellTab::applyTheme(const EditorTheme &theme)
{
    if (m_terminal)
        m_terminal->setColors(theme.foreground, theme.background);
}