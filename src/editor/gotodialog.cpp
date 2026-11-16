#include "gotodialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QDialogButtonBox>

GotoDialog::GotoDialog(int maxLine, int current, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Go To Line"));

    QVBoxLayout *l = new QVBoxLayout(this);
    l->addWidget(new QLabel(tr("Line number (1 - %1):").arg(maxLine), this));

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