#include "finddialog.h"
#include "core/i18n.h"
static inline QString TRc(const char *s){ return I18n::instance()->tr(QString::fromUtf8(s)); }
#include "codeeditor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QTabWidget>
#include <QGroupBox>
#include <QTextCursor>
#include <QRegularExpression>
#include <QMessageBox>

FindDialog::FindDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(TRc("Find / Replace"));
    setModal(false);

    m_tabs = new QTabWidget(this);

    m_caseCheck = new QCheckBox(TRc("Match case"), this);
    m_wholeWordCheck = new QCheckBox(TRc("Whole word only"), this);
    m_regexCheck = new QCheckBox(TRc("Regular expression"), this);
    m_wrapCheck = new QCheckBox(TRc("Wrap around"), this);
    m_wrapCheck->setChecked(true);

    m_findEdit = new QLineEdit(this);
    m_replaceEdit = new QLineEdit(this);

    // ---- Find tab ----
    QWidget *findTab = new QWidget(this);
    {
        QVBoxLayout *l = new QVBoxLayout(findTab);
        QFormLayout *form = new QFormLayout;
        form->addRow(TRc("Find what:"), m_findEdit);
        l->addLayout(form);

        QHBoxLayout *btns = new QHBoxLayout;
        QPushButton *next = new QPushButton(TRc("Find Next"), this);
        QPushButton *prev = new QPushButton(TRc("Find Previous"), this);
        QPushButton *count = new QPushButton(TRc("Count"), this);
        btns->addWidget(next);
        btns->addWidget(prev);
        btns->addWidget(count);
        l->addLayout(btns);

        connect(next, &QPushButton::clicked, this, &FindDialog::findNext);
        connect(prev, &QPushButton::clicked, this, &FindDialog::findPrev);
        connect(count, &QPushButton::clicked, this, &FindDialog::countAll);
    }

    // ---- Replace tab ----
    QWidget *replaceTab = new QWidget(this);
    {
        QVBoxLayout *l = new QVBoxLayout(replaceTab);
        QFormLayout *form = new QFormLayout;
        form->addRow(TRc("Replace with:"), m_replaceEdit);
        l->addLayout(form);

        QHBoxLayout *btns = new QHBoxLayout;
        QPushButton *rep = new QPushButton(TRc("Replace"), this);
        QPushButton *repAll = new QPushButton(TRc("Replace All"), this);
        btns->addWidget(rep);
        btns->addWidget(repAll);
        l->addLayout(btns);

        connect(rep, &QPushButton::clicked, this, &FindDialog::replaceOne);
        connect(repAll, &QPushButton::clicked, this, &FindDialog::replaceAll);
    }

    m_tabs->addTab(findTab, TRc("Find"));
    m_tabs->addTab(replaceTab, TRc("Replace"));

    // ---- Options group ----
    QGroupBox *opts = new QGroupBox(TRc("Options"), this);
    QVBoxLayout *optsLayout = new QVBoxLayout(opts);
    optsLayout->addWidget(m_caseCheck);
    optsLayout->addWidget(m_wholeWordCheck);
    optsLayout->addWidget(m_regexCheck);
    optsLayout->addWidget(m_wrapCheck);

    QVBoxLayout *main = new QVBoxLayout(this);
    main->addWidget(m_tabs);
    main->addWidget(opts);

    QPushButton *closeBtn = new QPushButton(TRc("Close"), this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::hide);
    main->addWidget(closeBtn);

    resize(380, 320);
}

void FindDialog::setEditor(CodeEditor *editor)
{
    m_editor = editor;
}

void FindDialog::showFind()
{
    m_tabs->setCurrentIndex(0);
    show();
    raise();
    activateWindow();
    m_findEdit->setFocus();
    m_findEdit->selectAll();
}

void FindDialog::showReplace()
{
    m_tabs->setCurrentIndex(1);
    show();
    raise();
    activateWindow();
    m_findEdit->setFocus();
    m_findEdit->selectAll();
}

QTextDocument::FindFlags FindDialog::flags() const
{
    QTextDocument::FindFlags f;
    if (m_caseCheck->isChecked())
        f |= QTextDocument::FindCaseSensitively;
    if (m_wholeWordCheck->isChecked())
        f |= QTextDocument::FindWholeWords;
    return f;
}

bool FindDialog::doFind(bool backward)
{
    if (!m_editor)
        return false;
    const QString pattern = m_findEdit->text();
    if (pattern.isEmpty())
        return false;

    QTextDocument::FindFlags f = flags();
    if (backward)
        f |= QTextDocument::FindBackward;

    bool found;
    if (m_regexCheck->isChecked()) {
        QRegularExpression::PatternOptions opt = QRegularExpression::NoPatternOption;
        if (!m_caseCheck->isChecked())
            opt |= QRegularExpression::CaseInsensitiveOption;
        QRegularExpression re(pattern, opt);
        if (!re.isValid()) {
            QMessageBox::warning(this, TRc("Find"), TRc("Invalid regular expression."));
            return false;
        }
        found = m_editor->find(re, f);
        if (!found && m_wrapCheck->isChecked()) {
            QTextCursor c = m_editor->textCursor();
            c.movePosition(backward ? QTextCursor::End : QTextCursor::Start);
            m_editor->setTextCursor(c);
            found = m_editor->find(re, f);
        }
    } else {
        found = m_editor->find(pattern, f);
        if (!found && m_wrapCheck->isChecked()) {
            QTextCursor c = m_editor->textCursor();
            c.movePosition(backward ? QTextCursor::End : QTextCursor::Start);
            m_editor->setTextCursor(c);
            found = m_editor->find(pattern, f);
        }
    }

    if (!found)
        emit statusMessage(TRc("Cannot find \"%1\"").arg(pattern));
    return found;
}

void FindDialog::findNext()
{
    doFind(false);
}

void FindDialog::findPrev()
{
    doFind(true);
}

void FindDialog::replaceOne()
{
    if (!m_editor)
        return;
    QTextCursor c = m_editor->textCursor();
    if (c.hasSelection() &&
        c.selectedText().compare(m_findEdit->text(),
                                 m_caseCheck->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive) == 0) {
        c.insertText(m_replaceEdit->text());
    }
    doFind(false);
}

void FindDialog::replaceAll()
{
    if (!m_editor)
        return;
    const QString pattern = m_findEdit->text();
    if (pattern.isEmpty())
        return;

    int count = 0;
    QTextCursor cursor = m_editor->textCursor();
    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::Start);
    m_editor->setTextCursor(cursor);

    while (doFind(false)) {
        QTextCursor c = m_editor->textCursor();
        if (c.hasSelection()) {
            c.insertText(m_replaceEdit->text());
            count++;
        } else {
            break;
        }
        if (count > 1000000) break; // safety
    }
    cursor.endEditBlock();
    emit statusMessage(TRc("Replaced %1 occurrence(s)").arg(count));
}

void FindDialog::countAll()
{
    if (!m_editor)
        return;
    const QString pattern = m_findEdit->text();
    if (pattern.isEmpty())
        return;

    const QString doc = m_editor->toPlainText();
    int count = 0;
    if (m_regexCheck->isChecked()) {
        QRegularExpression::PatternOptions opt = QRegularExpression::NoPatternOption;
        if (!m_caseCheck->isChecked())
            opt |= QRegularExpression::CaseInsensitiveOption;
        QRegularExpression re(pattern, opt);
        auto it = re.globalMatch(doc);
        while (it.hasNext()) { it.next(); count++; }
    } else {
        Qt::CaseSensitivity cs = m_caseCheck->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
        int from = 0;
        while ((from = doc.indexOf(pattern, from, cs)) != -1) {
            count++;
            from += pattern.length();
        }
    }
    emit statusMessage(TRc("Count: %1 occurrence(s)").arg(count));
    QMessageBox::information(this, TRc("Count"), TRc("%1 occurrence(s) found.").arg(count));
}