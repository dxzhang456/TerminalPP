#include "serialdialog.h"
#include "core/i18n.h"
static inline QString TRc(const char *s){ return I18n::instance()->tr(QString::fromUtf8(s)); }

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QDialogButtonBox>

SerialDialog::SerialDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(TRc("Serial / UART Configuration"));

    m_port = new QComboBox(this);
    m_port->setEditable(true);

    m_baud = new QComboBox(this);
    m_baud->setEditable(true);
    for (int b : {300, 1200, 2400, 4800, 9600, 19200, 38400, 57600,
                  115200, 230400, 460800, 921600})
        m_baud->addItem(QString::number(b), b);
    m_baud->setCurrentText("115200");

    m_dataBits = new QComboBox(this);
    m_dataBits->addItem("5", QSerialPort::Data5);
    m_dataBits->addItem("6", QSerialPort::Data6);
    m_dataBits->addItem("7", QSerialPort::Data7);
    m_dataBits->addItem("8", QSerialPort::Data8);
    m_dataBits->setCurrentText("8");

    m_parity = new QComboBox(this);
    m_parity->addItem(TRc("None"),  QSerialPort::NoParity);
    m_parity->addItem(TRc("Even"),  QSerialPort::EvenParity);
    m_parity->addItem(TRc("Odd"),   QSerialPort::OddParity);
    m_parity->addItem(TRc("Mark"),  QSerialPort::MarkParity);
    m_parity->addItem(TRc("Space"), QSerialPort::SpaceParity);

    m_stopBits = new QComboBox(this);
    m_stopBits->addItem("1",   QSerialPort::OneStop);
    m_stopBits->addItem("1.5", QSerialPort::OneAndHalfStop);
    m_stopBits->addItem("2",   QSerialPort::TwoStop);

    m_flow = new QComboBox(this);
    m_flow->addItem(TRc("None"),         QSerialPort::NoFlowControl);
    m_flow->addItem(TRc("RTS/CTS"),      QSerialPort::HardwareControl);
    m_flow->addItem(TRc("XON/XOFF"),     QSerialPort::SoftwareControl);

    m_lineEnding = new QComboBox(this);
    m_lineEnding->addItems({"CR", "LF", "CRLF", "None"});

    m_localEcho = new QCheckBox(TRc("Local echo"), this);

    QFormLayout *form = new QFormLayout;
    QHBoxLayout *portLayout = new QHBoxLayout;
    portLayout->addWidget(m_port);
    QPushButton *refresh = new QPushButton(TRc("Refresh"), this);
    connect(refresh, &QPushButton::clicked, this, &SerialDialog::refreshPorts);
    portLayout->addWidget(refresh);
    form->addRow(TRc("Serial line:"), portLayout);
    form->addRow(TRc("Speed (baud):"), m_baud);
    form->addRow(TRc("Data bits:"), m_dataBits);
    form->addRow(TRc("Parity:"), m_parity);
    form->addRow(TRc("Stop bits:"), m_stopBits);
    form->addRow(TRc("Flow control:"), m_flow);
    form->addRow(TRc("Enter sends:"), m_lineEnding);
    form->addRow(QString(), m_localEcho);

    QDialogButtonBox *box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout *main = new QVBoxLayout(this);
    main->addLayout(form);
    main->addWidget(box);

    refreshPorts();
    resize(380, 320);
}

void SerialDialog::refreshPorts()
{
    QString current = m_port->currentText();
    m_port->clear();
    m_port->addItems(SerialSession::availablePorts());
    if (!current.isEmpty())
        m_port->setCurrentText(current);
}

SerialConfig SerialDialog::config() const
{
    SerialConfig c;
    c.portName = m_port->currentText().trimmed();
    c.baudRate = m_baud->currentText().toInt();
    c.dataBits = static_cast<QSerialPort::DataBits>(m_dataBits->currentData().toInt());
    c.parity   = static_cast<QSerialPort::Parity>(m_parity->currentData().toInt());
    c.stopBits = static_cast<QSerialPort::StopBits>(m_stopBits->currentData().toInt());
    c.flow     = static_cast<QSerialPort::FlowControl>(m_flow->currentData().toInt());
    c.lineEnding = m_lineEnding->currentText();
    c.localEcho = m_localEcho->isChecked();
    return c;
}

void SerialDialog::setConfig(const SerialConfig &cfg)
{
    m_port->setCurrentText(cfg.portName);
    m_baud->setCurrentText(QString::number(cfg.baudRate));
    m_dataBits->setCurrentIndex(m_dataBits->findData(cfg.dataBits));
    m_parity->setCurrentIndex(m_parity->findData(cfg.parity));
    m_stopBits->setCurrentIndex(m_stopBits->findData(cfg.stopBits));
    m_flow->setCurrentIndex(m_flow->findData(cfg.flow));
    m_lineEnding->setCurrentText(cfg.lineEnding);
    m_localEcho->setChecked(cfg.localEcho);
}