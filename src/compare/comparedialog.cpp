#include "comparedialog.h"
#include "core/i18n.h"

#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QFileDialog>

static QString TRc(const char *s) { return I18n::instance()->tr(QString::fromUtf8(s)); }

CompareDialog::CompareDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(TRc("New Comparison"));
    resize(560, 200);

    auto *grid = new QGridLayout();

    grid->addWidget(new QLabel(TRc("Type:"), this), 0, 0);
    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem(TRc("Text Compare"), int(TextCompare));
    m_typeCombo->addItem(TRc("Folder Compare"), int(FolderCompare));
    m_typeCombo->addItem(TRc("Hex Compare"), int(HexCompare));
    m_typeCombo->addItem(TRc("Picture Compare"), int(ImageCompare));
    connect(m_typeCombo, &QComboBox::currentIndexChanged,
            this, &CompareDialog::onTypeChanged);
    grid->addWidget(m_typeCombo, 0, 1, 1, 2);

    grid->addWidget(new QLabel(TRc("Left:"), this), 1, 0);
    m_leftEdit = new QLineEdit(this);
    grid->addWidget(m_leftEdit, 1, 1);
    auto *lb = new QPushButton(TRc("Browse..."), this);
    connect(lb, &QPushButton::clicked, this, &CompareDialog::browseLeft);
    grid->addWidget(lb, 1, 2);

    grid->addWidget(new QLabel(TRc("Right:"), this), 2, 0);
    m_rightEdit = new QLineEdit(this);
    grid->addWidget(m_rightEdit, 2, 1);
    auto *rb = new QPushButton(TRc("Browse..."), this);
    connect(rb, &QPushButton::clicked, this, &CompareDialog::browseRight);
    grid->addWidget(rb, 2, 2);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *v = new QVBoxLayout(this);
    v->addLayout(grid);
    v->addStretch(1);
    v->addWidget(buttons);
}

void CompareDialog::setType(Type t)
{
    int idx = m_typeCombo->findData(int(t));
    if (idx >= 0)
        m_typeCombo->setCurrentIndex(idx);
}

CompareDialog::Type CompareDialog::type() const
{
    return static_cast<Type>(m_typeCombo->currentData().toInt());
}

QString CompareDialog::leftPath() const { return m_leftEdit->text(); }
QString CompareDialog::rightPath() const { return m_rightEdit->text(); }

void CompareDialog::browseLeft()
{
    QString p;
    if (type() == FolderCompare)
        p = QFileDialog::getExistingDirectory(this, TRc("Select Left Folder"),
                                              m_leftEdit->text());
    else
        p = QFileDialog::getOpenFileName(this, TRc("Select Left File"),
                                         m_leftEdit->text());
    if (!p.isEmpty())
        m_leftEdit->setText(p);
}

void CompareDialog::browseRight()
{
    QString p;
    if (type() == FolderCompare)
        p = QFileDialog::getExistingDirectory(this, TRc("Select Right Folder"),
                                              m_rightEdit->text());
    else
        p = QFileDialog::getOpenFileName(this, TRc("Select Right File"),
                                         m_rightEdit->text());
    if (!p.isEmpty())
        m_rightEdit->setText(p);
}

void CompareDialog::onTypeChanged(int)
{
    // Nothing to do beyond affecting the browse dialogs; kept for clarity.
}