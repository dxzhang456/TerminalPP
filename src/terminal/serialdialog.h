#ifndef SERIALDIALOG_H
#define SERIALDIALOG_H

#include <QDialog>
#include "serialsession.h"

class QComboBox;
class QCheckBox;

// PuTTY-like serial configuration dialog.
class SerialDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SerialDialog(QWidget *parent = nullptr);

    SerialConfig config() const;
    void setConfig(const SerialConfig &cfg);

private slots:
    void refreshPorts();

private:
    QComboBox *m_port = nullptr;
    QComboBox *m_baud = nullptr;
    QComboBox *m_dataBits = nullptr;
    QComboBox *m_parity = nullptr;
    QComboBox *m_stopBits = nullptr;
    QComboBox *m_flow = nullptr;
    QComboBox *m_lineEnding = nullptr;
    QCheckBox *m_localEcho = nullptr;
};

#endif // SERIALDIALOG_H