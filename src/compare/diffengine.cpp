#include "diffengine.h"

#include <QHash>

namespace Diff {

QString normalize(const QString &line, const Options &opts)
{
    QString s = line;
    if (opts.ignoreWhitespace) {
        // Collapse runs of whitespace to a single space and trim ends.
        s = s.simplified();
    }
    if (opts.ignoreCase)
        s = s.toLower();
    return s;
}

// ---------------------------------------------------------------------------
// Myers O(ND) diff producing an edit script, then converted to aligned rows.
// Operates on normalized-line hash keys for speed and option handling.
// ---------------------------------------------------------------------------
QVector<Row> compute(const QStringList &left, const QStringList &right,
                     const Options &opts)
{
    const int n = left.size();
    const int m = right.size();

    // Precompute normalized keys.
    QVector<QString> a(n), b(m);
    for (int i = 0; i < n; ++i) a[i] = normalize(left[i], opts);
    for (int j = 0; j < m; ++j) b[j] = normalize(right[j], opts);

    // Trivial cases.
    QVector<Row> rows;
    if (n == 0 && m == 0)
        return rows;

    // Myers shortest edit script. We record the full V arrays per d to
    // backtrack the path.
    const int maxD = n + m;
    const int offset = maxD;
    QVector<QVector<int>> trace;
    trace.reserve(maxD + 1);

    QVector<int> v(2 * maxD + 1, 0);
    bool found = false;
    int finalD = 0;

    for (int d = 0; d <= maxD && !found; ++d) {
        for (int k = -d; k <= d; k += 2) {
            int x;
            if (k == -d || (k != d && v[offset + k - 1] < v[offset + k + 1]))
                x = v[offset + k + 1];          // down (insertion)
            else
                x = v[offset + k - 1] + 1;      // right (deletion)
            int y = x - k;
            while (x < n && y < m && a[x] == b[y]) {
                ++x; ++y;
            }
            v[offset + k] = x;
            if (x >= n && y >= m) {
                found = true;
                finalD = d;
                break;
            }
        }
        trace.append(v);
        if (found)
            break;
    }

    // Backtrack to build the edit path (as a list of (x,y) snake endpoints).
    struct Move { int kind; int li; int ri; }; // kind: 0=equal 1=del 2=ins
    QVector<Move> movesRev;

    int x = n, y = m;
    for (int d = finalD; d > 0; --d) {
        const QVector<int> &vv = trace[d];
        int k = x - y;
        int prevK;
        if (k == -d || (k != d && vv[offset + k - 1] < vv[offset + k + 1]))
            prevK = k + 1;   // came from insertion
        else
            prevK = k - 1;   // came from deletion
        int prevX = trace[d - 1][offset + prevK];
        int prevY = prevX - prevK;

        // Snake (diagonal equals) between (prevX/prevY advanced) and (x,y).
        while (x > prevX && y > prevY) {
            movesRev.append({0, x - 1, y - 1});
            --x; --y;
        }
        if (d > 0) {
            if (x == prevX) {
                // insertion (down): right line y-1
                movesRev.append({2, -1, y - 1});
                --y;
            } else {
                // deletion (right): left line x-1
                movesRev.append({1, x - 1, -1});
                --x;
            }
        }
    }
    // Any remaining initial snake.
    while (x > 0 && y > 0) {
        movesRev.append({0, x - 1, y - 1});
        --x; --y;
    }
    while (x > 0) { movesRev.append({1, x - 1, -1}); --x; }
    while (y > 0) { movesRev.append({2, -1, y - 1}); --y; }

    // movesRev is in reverse order; iterate forward.
    // Merge adjacent del+ins into Changed rows (BC shows them paired).
    QVector<Move> moves;
    moves.reserve(movesRev.size());
    for (int i = movesRev.size() - 1; i >= 0; --i)
        moves.append(movesRev[i]);

    int i = 0;
    while (i < moves.size()) {
        const Move &mv = moves[i];
        if (mv.kind == 0) {
            Row r; r.kind = Equal; r.left = mv.li; r.right = mv.ri;
            rows.append(r);
            ++i;
        } else {
            // Gather a run of deletions and a run of insertions, pair them up
            // as Changed rows, leftover as pure Removed / Inserted.
            QVector<int> dels, inss;
            while (i < moves.size() && moves[i].kind == 1) { dels.append(moves[i].li); ++i; }
            while (i < moves.size() && moves[i].kind == 2) { inss.append(moves[i].ri); ++i; }
            int p = 0;
            int paired = qMin(dels.size(), inss.size());
            for (; p < paired; ++p) {
                Row r; r.kind = Changed; r.left = dels[p]; r.right = inss[p];
                rows.append(r);
            }
            for (int q = p; q < dels.size(); ++q) {
                Row r; r.kind = Removed; r.left = dels[q]; r.right = -1;
                rows.append(r);
            }
            for (int q = p; q < inss.size(); ++q) {
                Row r; r.kind = Inserted; r.left = -1; r.right = inss[q];
                rows.append(r);
            }
        }
    }

    return rows;
}

// ---------------------------------------------------------------------------
// Char-level inline diff for a changed row: find common prefix/suffix and mark
// the differing middle on each side. (Simple, fast, and visually close to BC.)
// ---------------------------------------------------------------------------
void inlineDiff(const QString &leftRaw, const QString &rightRaw,
                QVector<InlineSpan> &leftSpans, QVector<InlineSpan> &rightSpans,
                const Options &opts)
{
    leftSpans.clear();
    rightSpans.clear();

    QString l = leftRaw;
    QString r = rightRaw;
    if (opts.ignoreCase) { l = l.toLower(); r = r.toLower(); }

    const int ln = l.size();
    const int rn = r.size();

    int pre = 0;
    while (pre < ln && pre < rn && l[pre] == r[pre])
        ++pre;

    int suf = 0;
    while (suf < (ln - pre) && suf < (rn - pre) &&
           l[ln - 1 - suf] == r[rn - 1 - suf])
        ++suf;

    int lMidLen = ln - pre - suf;
    int rMidLen = rn - pre - suf;

    if (lMidLen > 0)
        leftSpans.append({pre, lMidLen});
    if (rMidLen > 0)
        rightSpans.append({pre, rMidLen});
}

} // namespace Diff