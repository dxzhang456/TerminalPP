#include "serialtab.h"
#include "terminalwidget.h"
#include "ansiparser.h"

#include <QVBoxLayout>

SerialTab::SerialTab(const SerialConfig &cfg, QWidget *parent)
    : QWidget(parent), m_config(cfg)
{
    m_terminal = new TerminalWidget(this);
    m_terminal->setColors(QColor(0, 255, 0), QColor(0, 0, 0)); // classic green-on-black

    QVBoxLayout *l = new QVBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    l->addWidget(m_terminal);

    m_session = new SerialSession(this);
    m_session->setConfig(m_config);

    connect(m_terminal, &TerminalWidget::dataEntered,
            m_session, &SerialSession::sendData);

    connect(m_session, &SerialSession::dataReceived, this, [this](const QByteArray &d) {
        m_terminal->parser()->feed(d);
    });
    connect(m_session, &SerialSession::errorText, this, [this](const QString &t) {
        m_terminal->parser()->feed(t.toUtf8());
    });
    connect(m_session, &SerialSession::closed, this, [this]() {
        emit connectionClosed();
    });
}

bool SerialTab::openPort()
{
    bool ok = m_session->open();
    m_terminal->setFocus();
    return ok;
}

void SerialTab::closePort()
{
    m_session->close();
}

bool SerialTab::isOpen() const
{
    return m_session->isOpen();
}