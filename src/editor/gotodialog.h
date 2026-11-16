#ifndef GOTODIALOG_H
#define GOTODIALOG_H

#include <QDialog>

class QSpinBox;

// Go-to-line dialog.
class GotoDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GotoDialog(int maxLine, int current, QWidget *parent = nullptr);
    int line() const;

private:
    QSpinBox *m_spin = nullptr;
};

#endif // GOTODIALOG_H