#include "gotodialog.h"
#include "core/i18n.h"
static inline QString TRc(const char *s){ return I18n::instance()->tr(QString::fromUtf8(s)); }

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QDialogButtonBox>

GotoDialog::GotoDialog(int maxLine, int current, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(TRc("Go To Line"));

    QVBoxLayout *l = new QVBoxLayout(this);
    l->addWidget(new QLabel(TRc("Line number (1 - %1):").arg(maxLine), this));

    m_spin = new QSpinBox(this);
    m_spin->setRange(1, qMax(1, maxLine));
    m_spin->setValue(qBound(1, current, maxLine));
    l->addWidget(m_spin);

    QDialogButtonBox *box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    l->addWidget(box);

    m_spin->setFocus();
    m_spin->selectAll();
}

int GotoDialog::line() const
{
    return m_spin->value();
}