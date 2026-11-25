#include "hexcomparetab.h"
#include "core/i18n.h"

static inline QString TRc(const char *s) { return I18n::instance()->tr(QString::fromUtf8(s)); }

#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolBar>
#include "core/iconfactory.h"
#include <QFile>
#include <QFileInfo>
#include <QScrollBar>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QSyntaxHighlighter>

// A syntax highlighter that colors differing byte columns red. We mark the
// differing positions by storing them in a per-block user state is not enough,
// so instead we build the hex text and separately apply extra selections.
//
// Simpler approach: generate the hex dump as plain text, then use a highlighter
// that compares each line's bytes against the sibling. To keep it robust we
// precompute the set of differing global byte offsets and highlight the exact
// hex + ascii columns via QSyntaxHighlighter on a fixed 16-byte-per-row layout.

namespace {
const int kBytesPerRow = 16;

// Column layout of one row:  "OFFSET: HH HH ... HH  |ascii|"
// offset field width:
const int kOffsetChars = 8;    // 8 hex digits
const int kOffsetSep   = 2;    // ": "
int hexColStart() { return kOffsetChars + kOffsetSep; }          // first hex col
int hexColFor(int byteInRow) { return hexColStart() + byteInRow * 3; } // "HH "
int asciiColStart() { return hexColStart() + kBytesPerRow * 3 + 1; }   // after "  "

QString dumpLine(const QByteArray &data, int rowStart)
{
    QString line;
    line += QString("%1: ").arg(rowStart, kOffsetChars, 16, QChar('0')).toUpper();
    QString ascii;
    for (int i = 0; i < kBytesPerRow; ++i) {
        int idx = rowStart + i;
        if (idx < data.size()) {
            unsigned char b = static_cast<unsigned char>(data[idx]);
            line += QString("%1 ").arg(b, 2, 16, QChar('0')).toUpper();
            ascii += (b >= 32 && b < 127) ? QChar(b) : QChar('.');
        } else {
            line += "   ";
            ascii += ' ';
        }
    }
    line += " |" + ascii + "|";
    return line;
}
} // namespace

class HexDiffHighlighter : public QSyntaxHighlighter
{
public:
    HexDiffHighlighter(QTextDocument *doc, const QByteArray *self,
                       const QByteArray *other, const QColor &diffColor)
        : QSyntaxHighlighter(doc), m_self(self), m_other(other),
          m_diff(diffColor) {}

    void setColor(const QColor &c) { m_diff = c; rehighlight(); }

protected:
    void highlightBlock(const QString &text) override
    {
        int row = currentBlock().blockNumber();
        int rowStart = row * kBytesPerRow;

        QTextCharFormat fmt;
        fmt.setBackground(m_diff);
        fmt.setForeground(QColor(255, 255, 255));

        for (int i = 0; i < kBytesPerRow; ++i) {
            int idx = rowStart + i;
            unsigned char sb = idx < m_self->size()
                               ? static_cast<unsigned char>((*m_self)[idx]) : 0;
            unsigned char ob = idx < m_other->size()
                               ? static_cast<unsigned char>((*m_other)[idx]) : 0;
            bool selfHas = idx < m_self->size();
            bool otherHas = idx < m_other->size();
            bool differ = (selfHas != otherHas) || (selfHas && otherHas && sb != ob);
            if (!differ)
                continue;

            // Highlight the 2 hex digits.
            int hc = hexColFor(i);
            if (hc + 2 <= text.size())
                setFormat(hc, 2, fmt);
            // Highlight the ascii char.
            int ac = asciiColStart() + i;
            if (ac < text.size())
                setFormat(ac, 1, fmt);
        }
    }

private:
    const QByteArray *m_self;
    const QByteArray *m_other;
    QColor m_diff;
};

HexCompareTab::HexCompareTab(QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *tb = new QToolBar(this);
    tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
    tb->setIconSize(QSize(20, 20));
    auto addIconAction = [&](const char *src, auto slot) -> QAction * {
        QIcon icon = IconFactory::makeToolIcon(QString::fromUtf8(src));
        QAction *a = icon.isNull() ? tb->addAction(TRc(src), this, slot)
                                   : tb->addAction(icon, TRc(src), this, slot);
        a->setToolTip(TRc(src));
        return a;
    };
    addIconAction("Reload", &HexCompareTab::reload);
    tb->addSeparator();
    {
        QIcon icon = IconFactory::makeToolIcon(QStringLiteral("As Text"));
        QAction *a = icon.isNull() ? tb->addAction(TRc("As Text"))
                                   : tb->addAction(icon, TRc("As Text"));
        a->setToolTip(TRc("As Text"));
        connect(a, &QAction::triggered, this, [this]() {
            if (!m_leftPath.isEmpty() && !m_rightPath.isEmpty())
                emit requestCompareAs(0, m_leftPath, m_rightPath);
        });
    }
    {
        QIcon icon = IconFactory::makeToolIcon(QStringLiteral("As Picture"));
        QAction *a = icon.isNull() ? tb->addAction(TRc("As Picture"))
                                   : tb->addAction(icon, TRc("As Picture"));
        a->setToolTip(TRc("As Picture"));
        connect(a, &QAction::triggered, this, [this]() {
            if (!m_leftPath.isEmpty() && !m_rightPath.isEmpty())
                emit requestCompareAs(3, m_leftPath, m_rightPath);
        });
    }
    outer->addWidget(tb);

    auto *hdr = new QHBoxLayout();
    hdr->setContentsMargins(4, 2, 4, 2);
    m_leftLabel = new QLabel(TRc("(left)"), this);
    m_rightLabel = new QLabel(TRc("(right)"), this);
    m_leftLabel->setStyleSheet("font-weight:bold;");
    m_rightLabel->setStyleSheet("font-weight:bold;");
    hdr->addWidget(m_leftLabel, 1);
    hdr->addWidget(m_rightLabel, 1);
    outer->addLayout(hdr);

    auto *panes = new QHBoxLayout();
    panes->setContentsMargins(0, 0, 0, 0);
    panes->setSpacing(1);
    m_left = new QPlainTextEdit(this);
    m_right = new QPlainTextEdit(this);
    for (QPlainTextEdit *e : {m_left, m_right}) {
        QFont f("Consolas", 10);
        f.setStyleHint(QFont::Monospace);
        e->setFont(f);
        e->setReadOnly(true);
        e->setLineWrapMode(QPlainTextEdit::NoWrap);
    }
    panes->addWidget(m_left, 1);
    panes->addWidget(m_right, 1);
    outer->addLayout(panes, 1);

    m_statsLabel = new QLabel(this);
    m_statsLabel->setContentsMargins(6, 2, 6, 2);
    outer->addWidget(m_statsLabel);

    // Synchronized vertical scrolling.
    connect(m_left->verticalScrollBar(), &QScrollBar::valueChanged, this,
            [this](int v) { if (m_right->verticalScrollBar()->value() != v)
                                m_right->verticalScrollBar()->setValue(v); });
    connect(m_right->verticalScrollBar(), &QScrollBar::valueChanged, this,
            [this](int v) { if (m_left->verticalScrollBar()->value() != v)
                                m_left->verticalScrollBar()->setValue(v); });
}

bool HexCompareTab::loadFiles(const QString &leftPath, const QString &rightPath)
{
    QFile lf(leftPath), rf(rightPath);
    bool okL = lf.open(QIODevice::ReadOnly);
    bool okR = rf.open(QIODevice::ReadOnly);
    m_leftData = okL ? lf.readAll() : QByteArray();
    m_rightData = okR ? rf.readAll() : QByteArray();
    m_leftPath = leftPath;
    m_rightPath = rightPath;
    m_leftLabel->setText(QFileInfo(leftPath).fileName() + "  —  " + leftPath);
    m_rightLabel->setText(QFileInfo(rightPath).fileName() + "  —  " + rightPath);
    rebuild();
    return okL && okR;
}

void HexCompareTab::reload()
{
    if (!m_leftPath.isEmpty() && !m_rightPath.isEmpty())
        loadFiles(m_leftPath, m_rightPath);
}

void HexCompareTab::rebuild()
{
    const int maxLen = qMax(m_leftData.size(), m_rightData.size());
    const int rows = (maxLen + kBytesPerRow - 1) / kBytesPerRow;

    QString lText, rText;
    lText.reserve(rows * 80);
    rText.reserve(rows * 80);
    for (int r = 0; r < rows; ++r) {
        int off = r * kBytesPerRow;
        lText += dumpLine(m_leftData, off);
        rText += dumpLine(m_rightData, off);
        if (r + 1 < rows) { lText += '\n'; rText += '\n'; }
    }
    m_left->setPlainText(lText);
    m_right->setPlainText(rText);

    // Attach (or refresh) the diff highlighters, kept as members.
    delete m_leftHl;
    delete m_rightHl;
    m_leftHl = new HexDiffHighlighter(m_left->document(), &m_leftData, &m_rightData, m_diffColor);
    m_rightHl = new HexDiffHighlighter(m_right->document(), &m_rightData, &m_leftData, m_diffColor);

    // Stats.
    int diffBytes = 0;
    for (int i = 0; i < maxLen; ++i) {
        unsigned char a = i < m_leftData.size() ? (unsigned char)m_leftData[i] : 0;
        unsigned char b = i < m_rightData.size() ? (unsigned char)m_rightData[i] : 0;
        bool ah = i < m_leftData.size(), bh = i < m_rightData.size();
        if ((ah != bh) || (ah && bh && a != b)) ++diffBytes;
    }
    const QString msg = TRc("Left: %1 bytes   Right: %2 bytes   Differing: %3 bytes")
                            .arg(m_leftData.size()).arg(m_rightData.size()).arg(diffBytes);
    m_statsLabel->setText(msg);
    emit statusMessage(msg);
}

void HexCompareTab::applyTheme(const EditorTheme &t)
{
    const bool dark = t.background.lightness() < 128;
    m_diffColor = dark ? QColor(150, 50, 50) : QColor(230, 120, 120);

    QString qss = QString("QPlainTextEdit{background:%1;color:%2;}")
                      .arg(t.background.name(), t.foreground.name());
    m_left->setStyleSheet(qss);
    m_right->setStyleSheet(qss);

    if (m_leftHl)  m_leftHl->setColor(m_diffColor);
    if (m_rightHl) m_rightHl->setColor(m_diffColor);
}
