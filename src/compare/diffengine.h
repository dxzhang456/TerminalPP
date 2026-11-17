#ifndef DIFFENGINE_H
#define DIFFENGINE_H

#include <QString>
#include <QStringList>
#include <QVector>

// A line-based diff engine (Myers' O(ND) algorithm, LCS backtracking) plus a
// char-level intra-line diff, replicating the core of Beyond Compare's text
// comparison. Produces an aligned list of rows where each side is either a real
// line index or a gap (-1), classified as Equal / Changed / Inserted / Removed.
namespace Diff {

enum RowKind {
    Equal,      // both sides identical
    Changed,    // both sides present but differ
    Inserted,   // only on the right (added)
    Removed     // only on the left (deleted)
};

struct Row {
    RowKind kind = Equal;
    int left  = -1;   // index into the left line list, or -1 for a gap
    int right = -1;   // index into the right line list, or -1 for a gap
};

// Options controlling how lines are compared.
struct Options {
    bool ignoreCase = false;
    bool ignoreWhitespace = false;    // trim + collapse internal whitespace
    bool ignoreLineEndings = true;    // always true in practice (we split lines)
};

// A single inline (character-level) span inside a changed line.
struct InlineSpan {
    int start = 0;   // start column (chars)
    int length = 0;  // length in chars
};

// Compute the aligned diff of two line lists.
QVector<Row> compute(const QStringList &left, const QStringList &right,
                     const Options &opts = Options());

// For a Changed row, compute the differing character spans on each side.
void inlineDiff(const QString &left, const QString &right,
                QVector<InlineSpan> &leftSpans, QVector<InlineSpan> &rightSpans,
                const Options &opts = Options());

// Normalize a line according to options (used internally + by callers).
QString normalize(const QString &line, const Options &opts);

} // namespace Diff

#endif // DIFFENGINE_H