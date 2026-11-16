#ifndef SERIALSESSION_H
#define SERIALSESSION_H

#include <QObject>
#include <QSerialPort>

// Serial/UART configuration, replicating PuTTY's serial options.
struct SerialConfig {
    QString portName;                                   // e.g. COM3
    qint32  baudRate = 115200;
    QSerialPort::DataBits dataBits = QSerialPort::Data8;
    QSerialPort::Parity   parity   = QSerialPort::NoParity;
    QSerialPort::StopBits stopBits = QSerialPort::OneStop;
    QSerialPort::FlowControl flow  = QSerialPort::NoFlowControl;
    bool localEcho = false;
    QString lineEnding = "CR";                          // CR, LF, CRLF, None
};

// Wraps QSerialPort for the PuTTY-like UART terminal.
class SerialSession : public QObject
{
    Q_OBJECT
public:
    explicit SerialSession(QObject *parent = nullptr);
    ~SerialSession() override;

    void setConfig(const SerialConfig &cfg) { m_config = cfg; }
    const SerialConfig &config() const { return m_config; }

    bool open();
    void close();
    bool isOpen() const;

    static QStringList availablePorts();

public slots:
    void sendData(const QByteArray &data);

signals:
    void dataReceived(const QByteArray &data);
    void opened();
    void closed();
    void errorText(const QString &text);

private slots:
    void onReadyRead();
    void onError(QSerialPort::SerialPortError err);

private:
    SerialConfig m_config;
    QSerialPort *m_port = nullptr;
};

#endif // SERIALSESSION_H