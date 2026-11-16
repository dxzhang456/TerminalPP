#ifndef SESSIONDIALOG_H
#define SESSIONDIALOG_H

#include <QDialog>
#include "sessionmanager.h"

class QLineEdit;
class QSpinBox;
class QCheckBox;

// Add / edit an SSH session (MuxTerm's session dialog).
class SessionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SessionDialog(QWidget *parent = nullptr);

    void setConfig(const SshSessionConfig &cfg);
    SshSessionConfig config() const;

private slots:
    void browseKey();

private:
    QLineEdit *m_name = nullptr;
    QLineEdit *m_host = nullptr;
    QSpinBox  *m_port = nullptr;
    QLineEdit *m_user = nullptr;
    QLineEdit *m_pass = nullptr;
    QLineEdit *m_key = nullptr;
    QCheckBox *m_useTmux = nullptr;
    QLineEdit *m_tmuxName = nullptr;
    QLineEdit *m_extra = nullptr;
};

#endif // SESSIONDIALOG_H