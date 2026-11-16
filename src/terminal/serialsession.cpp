#include "serialsession.h"
#include <QSerialPortInfo>

SerialSession::SerialSession(QObject *parent)
    : QObject(parent)
{
    m_port = new QSerialPort(this);
    connect(m_port, &QSerialPort::readyRead, this, &SerialSession::onReadyRead);
    connect(m_port, &QSerialPort::errorOccurred, this, &SerialSession::onError);
}

SerialSession::~SerialSession()
{
    close();
}

QStringList SerialSession::availablePorts()
{
    QStringList ports;
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos)
        ports << info.portName();
    return ports;
}

bool SerialSession::isOpen() const
{
    return m_port && m_port->isOpen();
}

bool SerialSession::open()
{
    if (isOpen())
        close();

    m_port->setPortName(m_config.portName);
    m_port->setBaudRate(m_config.baudRate);
    m_port->setDataBits(m_config.dataBits);
    m_port->setParity(m_config.parity);
    m_port->setStopBits(m_config.stopBits);
    m_port->setFlowControl(m_config.flow);

    if (!m_port->open(QIODevice::ReadWrite)) {
        emit errorText(tr("\r\n[Failed to open %1: %2]\r\n")
                           .arg(m_config.portName, m_port->errorString()));
        return false;
    }

    const QString parityStr = (m_config.parity == QSerialPort::NoParity)   ? "N" :
                              (m_config.parity == QSerialPort::EvenParity)  ? "E" :
                              (m_config.parity == QSerialPort::OddParity)   ? "O" :
                              (m_config.parity == QSerialPort::MarkParity)  ? "M" : "S";
    const QString stopStr = (m_config.stopBits == QSerialPort::OneStop) ? "1" :
                            (m_config.stopBits == QSerialPort::TwoStop) ? "2" : "1.5";
    emit errorText(tr("\r\n[Opened %1 @ %2 baud %3%4%5]\r\n")
                       .arg(m_config.portName)
                       .arg(m_config.baudRate)
                       .arg(static_cast<int>(m_config.dataBits))
                       .arg(parityStr)
                       .arg(stopStr));
    emit opened();
    return true;
}

void SerialSession::close()
{
    if (m_port && m_port->isOpen()) {
        m_port->close();
        emit closed();
    }
}

void SerialSession::sendData(const QByteArray &data)
{
    if (!isOpen())
        return;

    QByteArray out = data;

    // Translate Enter (\r from terminal widget) into the configured line ending.
    if (out == "\r") {
        if (m_config.lineEnding == "LF")
            out = "\n";
        else if (m_config.lineEnding == "CRLF")
            out = "\r\n";
        else if (m_config.lineEnding == "None")
            out = "";
        else
            out = "\r";
    }

    m_port->write(out);

    // Local echo (PuTTY option).
    if (m_config.localEcho && !out.isEmpty())
        emit dataReceived(out == "\r\n" ? QByteArray("\r\n") : out);
}

void SerialSession::onReadyRead()
{
    QByteArray data = m_port->readAll();
    if (!data.isEmpty())
        emit dataReceived(data);
}

void SerialSession::onError(QSerialPort::SerialPortError err)
{
    if (err == QSerialPort::NoError)
        return;
    if (err == QSerialPort::ResourceError) {
        emit errorText(tr("\r\n[Serial device removed or disconnected]\r\n"));
        close();
    }
}