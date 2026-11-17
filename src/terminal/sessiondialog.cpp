#include "sessiondialog.h"
#include "core/i18n.h"
static inline QString TRc(const char *s){ return I18n::instance()->tr(QString::fromUtf8(s)); }

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>

SessionDialog::SessionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(TRc("SSH Session"));

    m_name = new QLineEdit(this);
    m_host = new QLineEdit(this);
    m_port = new QSpinBox(this);
    m_port->setRange(1, 65535);
    m_port->setValue(22);
    m_user = new QLineEdit(this);
    m_pass = new QLineEdit(this);
    m_pass->setEchoMode(QLineEdit::Password);
    m_key = new QLineEdit(this);
    m_useTmux = new QCheckBox(TRc("Auto-attach tmux session"), this);
    m_tmuxName = new QLineEdit(this);
    m_tmuxName->setPlaceholderText(TRc("tmux session name (default: muxterm)"));
    m_extra = new QLineEdit(this);
    m_extra->setPlaceholderText(TRc("extra ssh arguments (optional)"));

    QFormLayout *form = new QFormLayout;
    form->addRow(TRc("Name:"), m_name);
    form->addRow(TRc("Host:"), m_host);
    form->addRow(TRc("Port:"), m_port);
    form->addRow(TRc("Username:"), m_user);
    form->addRow(TRc("Password:"), m_pass);

    QHBoxLayout *keyLayout = new QHBoxLayout;
    keyLayout->addWidget(m_key);
    QPushButton *browse = new QPushButton(TRc("Browse..."), this);
    connect(browse, &QPushButton::clicked, this, &SessionDialog::browseKey);
    keyLayout->addWidget(browse);
    form->addRow(TRc("Private key:"), keyLayout);

    form->addRow(QString(), m_useTmux);
    form->addRow(TRc("tmux name:"), m_tmuxName);
    form->addRow(TRc("Extra args:"), m_extra);

    QDialogButtonBox *box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout *main = new QVBoxLayout(this);
    main->addLayout(form);
    main->addWidget(box);

    resize(420, 360);
}

void SessionDialog::browseKey()
{
    QString file = QFileDialog::getOpenFileName(this, TRc("Select private key"));
    if (!file.isEmpty())
        m_key->setText(file);
}

void SessionDialog::setConfig(const SshSessionConfig &cfg)
{
    m_name->setText(cfg.name);
    m_host->setText(cfg.host);
    m_port->setValue(cfg.port);
    m_user->setText(cfg.username);
    m_pass->setText(cfg.password);
    m_key->setText(cfg.keyFile);
    m_useTmux->setChecked(cfg.useTmux);
    m_tmuxName->setText(cfg.tmuxName);
    m_extra->setText(cfg.extraArgs);
}

SshSessionConfig SessionDialog::config() const
{
    SshSessionConfig c;
    c.name = m_name->text().trimmed();
    c.host = m_host->text().trimmed();
    c.port = m_port->value();
    c.username = m_user->text().trimmed();
    c.password = m_pass->text();
    c.keyFile = m_key->text().trimmed();
    c.useTmux = m_useTmux->isChecked();
    c.tmuxName = m_tmuxName->text().trimmed();
    c.extraArgs = m_extra->text().trimmed();
    return c;
}