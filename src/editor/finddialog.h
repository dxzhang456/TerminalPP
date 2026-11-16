#ifndef FINDDIALOG_H
#define FINDDIALOG_H

#include <QDialog>
#include <QTextDocument>

class QLineEdit;
class QCheckBox;
class QTabWidget;
class CodeEditor;

// Find / Replace dialog replicating ndd's search feature.
class FindDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FindDialog(QWidget *parent = nullptr);

    void setEditor(CodeEditor *editor);
    void showFind();
    void showReplace();

signals:
    void statusMessage(const QString &msg);

private slots:
    void findNext();
    void findPrev();
    void replaceOne();
    void replaceAll();
    void countAll();

private:
    bool doFind(bool backward);
    QTextDocument::FindFlags flags() const;

    CodeEditor *m_editor = nullptr;

    QTabWidget *m_tabs = nullptr;
    QLineEdit  *m_findEdit = nullptr;
    QLineEdit  *m_replaceEdit = nullptr;
    QCheckBox  *m_caseCheck = nullptr;
    QCheckBox  *m_wholeWordCheck = nullptr;
    QCheckBox  *m_regexCheck = nullptr;
    QCheckBox  *m_wrapCheck = nullptr;
};

#endif // FINDDIALOG_H