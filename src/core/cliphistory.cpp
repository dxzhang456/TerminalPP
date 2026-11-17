#include "cliphistory.h"

#include <QApplication>
#include <QClipboard>
#include <QSettings>

ClipHistory *ClipHistory::instance()
{
    static ClipHistory inst;
    return &inst;
}

ClipHistory::ClipHistory(QObject *parent) : QObject(parent)
{
}

void ClipHistory::initialize()
{
    if (m_initialized)
        return;
    m_initialized = true;

    load();

    if (QClipboard *cb = QApplication::clipboard()) {
        connect(cb, &QClipboard::dataChanged,
                this, &ClipHistory::onClipboardChanged);
        // Seed with whatever is currently on the clipboard.
        onClipboardChanged();
    }
}

void ClipHistory::setMaxEntries(int n)
{
    m_max = qMax(1, n);
    while (m_entries.size() > m_max)
        m_entries.removeLast();
    save();
    emit historyChanged();
}

void ClipHistory::clear()
{
    m_entries.clear();
    save();
    emit historyChanged();
}

void ClipHistory::onClipboardChanged()
{
    QClipboard *cb = QApplication::clipboard();
    if (!cb)
        return;
    const QString text = cb->text();
    if (text.isEmpty())
        return;

    // Move-to-front semantics: remove any existing duplicate, prepend newest.
    m_entries.removeAll(text);
    m_entries.prepend(text);
    while (m_entries.size() > m_max)
        m_entries.removeLast();

    save();
    emit historyChanged();
}

void ClipHistory::load()
{
    QSettings s;
    m_max = s.value(QStringLiteral("clipboard/maxEntries"), 50).toInt();
    m_max = qMax(1, m_max);
    m_entries = s.value(QStringLiteral("clipboard/history")).toStringList();
    while (m_entries.size() > m_max)
        m_entries.removeLast();
}

void ClipHistory::save() const
{
    QSettings s;
    s.setValue(QStringLiteral("clipboard/maxEntries"), m_max);
    s.setValue(QStringLiteral("clipboard/history"), m_entries);
}