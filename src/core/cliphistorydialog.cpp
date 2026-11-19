#include "cliphistorydialog.h"
#include "cliphistory.h"
#include "i18n.h"

#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QApplication>
#include <QClipboard>

ClipHistoryDialog::ClipHistoryDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(I18n::instance()->tr(QStringLiteral("Clipboard History")));
    resize(520, 420);

    auto *layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(
        I18n::instance()->tr(QStringLiteral("Clipboard History")), this));

    m_list = new QListWidget(this);
    m_list->setWordWrap(true);
    m_list->setAlternatingRowColors(true);
    layout->addWidget(m_list, 1);

    auto *buttons = new QHBoxLayout();
    auto *pasteBtn = new QPushButton(
        I18n::instance()->tr(QStringLiteral("&Paste")), this);
    // "Copy" copies the selected entry to the system clipboard WITHOUT closing
    // the dialog, so the user can pick an old entry and paste it elsewhere.
    auto *copyBtn = new QPushButton(
        I18n::instance()->tr(QStringLiteral("&Copy")), this);
    auto *clearBtn = new QPushButton(
        I18n::instance()->tr(QStringLiteral("Clear")), this);
    auto *cancelBtn = new QPushButton(
        I18n::instance()->tr(QStringLiteral("Cancel")), this);
    buttons->addWidget(clearBtn);
    buttons->addStretch(1);
    buttons->addWidget(copyBtn);
    buttons->addWidget(pasteBtn);
    buttons->addWidget(cancelBtn);
    layout->addLayout(buttons);

    pasteBtn->setDefault(true);

    connect(m_list, &QListWidget::itemDoubleClicked,
            this, &ClipHistoryDialog::acceptCurrent);
    connect(pasteBtn, &QPushButton::clicked,
            this, &ClipHistoryDialog::acceptCurrent);
    connect(copyBtn, &QPushButton::clicked,
            this, &ClipHistoryDialog::copyCurrent);
    connect(clearBtn, &QPushButton::clicked,
            this, &ClipHistoryDialog::clearHistory);
    connect(cancelBtn, &QPushButton::clicked,
            this, &QDialog::reject);
    connect(ClipHistory::instance(), &ClipHistory::historyChanged,
            this, &ClipHistoryDialog::refresh);

    refresh();
}

void ClipHistoryDialog::refresh()
{
    if (!m_list)
        return;
    m_list->clear();
    const QStringList &entries = ClipHistory::instance()->entries();
    for (const QString &e : entries) {
        // Show a single-line preview (first line, trimmed, capped length).
        QString preview = e;
        int nl = preview.indexOf(QLatin1Char('\n'));
        if (nl >= 0)
            preview = preview.left(nl) + QStringLiteral(" ...");
        preview = preview.trimmed();
        if (preview.size() > 120)
            preview = preview.left(120) + QStringLiteral(" ...");

        auto *item = new QListWidgetItem(preview, m_list);
        // Store the full original text in the item's data role.
        item->setData(Qt::UserRole, e);
        item->setToolTip(e.left(2000));
    }
    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
}

void ClipHistoryDialog::acceptCurrent()
{
    QListWidgetItem *item = m_list ? m_list->currentItem() : nullptr;
    if (!item) {
        reject();
        return;
    }
    m_selected = item->data(Qt::UserRole).toString();
    // Put the chosen entry back on the clipboard so a subsequent paste uses it.
    if (QClipboard *cb = QApplication::clipboard())
        cb->setText(m_selected);
    accept();
}

void ClipHistoryDialog::copyCurrent()
{
    QListWidgetItem *item = m_list ? m_list->currentItem() : nullptr;
    if (!item)
        return;
    const QString text = item->data(Qt::UserRole).toString();
    if (text.isEmpty())
        return;
    // Copy to the system clipboard but keep the dialog open so the user can
    // continue browsing / picking other entries.
    if (QClipboard *cb = QApplication::clipboard())
        cb->setText(text);
}

void ClipHistoryDialog::clearHistory()
{
    ClipHistory::instance()->clear();
    refresh();
}
