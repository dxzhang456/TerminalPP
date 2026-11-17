#ifndef COMPAREDIALOG_H
#define COMPAREDIALOG_H

#include <QDialog>
#include <QString>

class QLineEdit;
class QComboBox;

// A small launcher dialog that collects two paths (files or folders) and the
// desired comparison type, mirroring a diff tool New Session chooser.
class CompareDialog : public QDialog
{
    Q_OBJECT
public:
    enum Type { TextCompare, FolderCompare, HexCompare, ImageCompare };

    explicit CompareDialog(QWidget *parent = nullptr);

    // Preselect a comparison type in the dropdown (used by the Compare menu
    // entries so "Folder Compare..." opens with Folder already chosen).
    void setType(Type t);

    Type type() const;
    QString leftPath() const;
    QString rightPath() const;

private slots:
    void browseLeft();
    void browseRight();
    void onTypeChanged(int index);

private:
    QComboBox *m_typeCombo = nullptr;
    QLineEdit *m_leftEdit = nullptr;
    QLineEdit *m_rightEdit = nullptr;
};

#endif // COMPAREDIALOG_H