#include "sshtab.h"
#include "terminalwidget.h"
#include "ansiparser.h"
#include "sshsession.h"

#include <QVBoxLayout>

SshTab::SshTab(const SshSessionConfig &cfg, QWidget *parent)
    : QWidget(parent), m_config(cfg)
{
    m_terminal = new TerminalWidget(this);
    m_terminal->setColors(QColor(220, 220, 220), QColor(12, 12, 12));

    QVBoxLayout *l = new QVBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    l->addWidget(m_terminal);

    m_session = new SshSession(this);
    m_session->setConfig(m_config);

    // Wire terminal <-> session.
    connect(m_terminal, &TerminalWidget::dataEntered,
            m_session, &SshSession::sendData);
    connect(m_terminal, &TerminalWidget::sizeChanged,
            m_session, &SshSession::setWindowSize);

    connect(m_session, &SshSession::dataReceived, this, [this](const QByteArray &d) {
        m_terminal->parser()->feed(d);
    });
    connect(m_session, &SshSession::errorText, this, [this](const QString &t) {
        m_terminal->parser()->feed(t.toUtf8());
    });
    connect(m_session, &SshSession::disconnected, this, [this](int) {
        emit connectionClosed();
    });

    // Propagate terminal title changes (OSC) to the tab.
    connect(m_terminal->parser(), &AnsiParser::titleChanged,
            this, &SshTab::titleChanged);
}

void SshTab::connectSession()
{
    m_session->setWindowSize(m_terminal->termRows(), m_terminal->termCols());
    m_session->start();
    m_terminal->setFocus();
}

void SshTab::disconnectSession()
{
    m_session->stop();
}

void SshTab::reconnect()
{
    m_session->stop();
    m_terminal->clearScreen();
    connectSession();
}

bool SshTab::isConnected() const
{
    return m_session->isRunning();
}