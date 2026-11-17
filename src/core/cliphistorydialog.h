#ifndef CLIPHISTORYDIALOG_H
#define CLIPHISTORYDIALOG_H

#include <QDialog>

class QListWidget;

// A small picker that lists recent clipboard entries. Double-clicking (or
// selecting + "Paste") returns the chosen text via selectedText(), and also
// puts it back on the system clipboard so a normal paste uses it.
class ClipHistoryDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ClipHistoryDialog(QWidget *parent = nullptr);

    // The text the user chose, or empty if cancelled.
    QString selectedText() const { return m_selected; }

private slots:
    void refresh();
    void acceptCurrent();
    void clearHistory();

private:
    QListWidget *m_list = nullptr;
    QString m_selected;
};

#endif // CLIPHISTORYDIALOG_H