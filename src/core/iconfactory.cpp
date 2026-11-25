#include "iconfactory.h"

#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>

namespace IconFactory {

// Keyed by the English source string used in each toolbar; aliases for common
// variants (e.g. "Copy → " vs "Copy →") are handled by trimming.
QIcon makeToolIcon(const QString &rawName, const QColor &fg)
{
    const int S = 24;
    QImage img(S, S, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    QPen pen(fg);
    pen.setWidthF(1.8);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    const QRectF R(2, 2, S - 4, S - 4);
    const QString name = rawName.trimmed();

    auto fillArrow = [&](const QPolygonF &poly) {
        p.setBrush(fg); p.drawPolygon(poly); p.setBrush(Qt::NoBrush);
    };
    auto drawText = [&](const QString &txt, int ptSize, bool bold = true) {
        QFont f = p.font();
        f.setPointSize(ptSize);
        f.setBold(bold);
        p.setFont(f);
        p.drawText(R, Qt::AlignCenter, txt);
    };

    // ---------------- PDF / common ---------------------------------------
    if (name == "Save") {
        QPainterPath d;
        d.moveTo(3, 3); d.lineTo(17, 3); d.lineTo(21, 7);
        d.lineTo(21, 21); d.lineTo(3, 21); d.closeSubpath();
        p.drawPath(d);
        p.drawRect(QRectF(7, 3, 10, 6));
        p.drawRect(QRectF(7, 14, 10, 7));
    } else if (name == "Undo") {
        QPainterPath a;
        a.moveTo(19, 13); a.arcTo(5, 6, 14, 14, 0, 180);
        p.drawPath(a);
        QPolygonF arrow; arrow << QPointF(5,13) << QPointF(9,10) << QPointF(9,16);
        fillArrow(arrow);
    } else if (name == "Redo") {
        QPainterPath a;
        a.moveTo(5, 13); a.arcTo(5, 6, 14, 14, 180, -180);
        p.drawPath(a);
        QPolygonF arrow; arrow << QPointF(19,13) << QPointF(15,10) << QPointF(15,16);
        fillArrow(arrow);
    } else if (name == "Actual Size" || name == "1:1") {
        drawText(QStringLiteral("1:1"), 9);
    } else if (name == "Fit Width") {
        p.drawLine(4, 12, 20, 12);
        QPolygonF l; l << QPointF(4,12)  << QPointF(7,9)  << QPointF(7,15);
        QPolygonF r; r << QPointF(20,12) << QPointF(17,9) << QPointF(17,15);
        fillArrow(l); fillArrow(r);
    } else if (name == "Fit Page" || name == "Fit") {
        p.drawRect(QRectF(5, 4, 14, 16));
        QPolygonF u; u << QPointF(12,8)  << QPointF(9,11)  << QPointF(15,11);
        QPolygonF d; d << QPointF(12,16) << QPointF(9,13)  << QPointF(15,13);
        fillArrow(u); fillArrow(d);
    } else if (name == "Zoom In") {
        p.drawEllipse(QPointF(10, 10), 6, 6);
        p.drawLine(14, 14, 20, 20);
        p.drawLine(7, 10, 13, 10);
        p.drawLine(10, 7, 10, 13);
    } else if (name == "Zoom Out") {
        p.drawEllipse(QPointF(10, 10), 6, 6);
        p.drawLine(14, 14, 20, 20);
        p.drawLine(7, 10, 13, 10);
    } else if (name == "Copy Text") {
        p.drawRect(QRectF(5, 5, 11, 13));
        p.drawRect(QRectF(8, 8, 11, 13));
    } else if (name == "Select All Text") {
        QPen dash(fg, 1.2, Qt::DashLine);
        p.setPen(dash);
        p.drawRect(QRectF(3, 3, 18, 18));
        p.setPen(pen);
        drawText(QStringLiteral("A"), 10);
    } else if (name == "Copy Page Text") {
        p.drawRect(QRectF(5, 3, 14, 18));
        p.drawLine(8, 8, 16, 8);
        p.drawLine(8, 12, 16, 12);
        p.drawLine(8, 16, 13, 16);
    } else if (name == "Select") {
        QPolygonF a;
        a << QPointF(5, 4) << QPointF(5, 18) << QPointF(9, 14)
          << QPointF(12, 20) << QPointF(14, 19) << QPointF(11, 13)
          << QPointF(16, 13);
        fillArrow(a);
    } else if (name == "Highlight") {
        QPolygonF m;
        m << QPointF(4, 15) << QPointF(13, 6) << QPointF(18, 11)
          << QPointF(9, 20) << QPointF(4, 20);
        p.setBrush(QColor(255, 230, 90));
        p.drawPolygon(m);
        p.setBrush(Qt::NoBrush);
        p.drawLine(3, 22, 21, 22);
    } else if (name == "Underline") {
        drawText(QStringLiteral("U"), 12);
        p.drawLine(5, 20, 19, 20);
    } else if (name == "Strike") {
        drawText(QStringLiteral("S"), 12);
        p.drawLine(3, 12, 21, 12);
    } else if (name == "Text Box") {
        p.drawRect(QRectF(3, 6, 18, 12));
        drawText(QStringLiteral("T"), 9);
    } else if (name == "Sticky Note") {
        QPolygonF note;
        note << QPointF(4, 4) << QPointF(16, 4) << QPointF(20, 8)
             << QPointF(20, 20) << QPointF(4, 20);
        p.setBrush(QColor(255, 230, 120));
        p.drawPolygon(note);
        p.setBrush(Qt::NoBrush);
        p.drawLine(16, 4, 16, 8); p.drawLine(16, 8, 20, 8);
    } else if (name == "Pen") {
        QPainterPath d;
        d.moveTo(4, 20); d.lineTo(8, 20); d.lineTo(20, 8);
        d.lineTo(16, 4); d.lineTo(4, 16); d.closeSubpath();
        p.drawPath(d);
        p.drawLine(14, 6, 18, 10);
    } else if (name == "Stamp") {
        p.drawRect(QRectF(5, 14, 14, 6));
        p.drawLine(3, 20, 21, 20);
        QPainterPath d;
        d.moveTo(9, 14); d.lineTo(9, 10);
        d.lineTo(7, 7);  d.lineTo(11, 4); d.lineTo(15, 7);
        d.lineTo(13, 10); d.lineTo(13, 14);
        p.drawPath(d);
    } else if (name == "Color..." || name == "Color") {
        for (int i = 0; i < 6; ++i) {
            const double a = i * 60.0;
            QColor c = QColor::fromHsv(int(a), 230, 230);
            QPainterPath wedge;
            wedge.moveTo(12, 12);
            wedge.arcTo(2, 2, 20, 20, a, 60);
            wedge.closeSubpath();
            p.setBrush(c);
            p.setPen(Qt::NoPen);
            p.drawPath(wedge);
        }
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(12, 12), 10, 10);
    } else if (name == "Merge") {
        p.drawRect(QRectF(3, 4, 7, 16));
        p.drawRect(QRectF(14, 4, 7, 16));
        p.drawLine(10, 12, 14, 12);
        QPolygonF a; a << QPointF(14,12) << QPointF(11,10) << QPointF(11,14);
        fillArrow(a);
    } else if (name == "Split") {
        p.drawRect(QRectF(8, 4, 8, 16));
        QPolygonF l; l << QPointF(3,12)  << QPointF(7,9)  << QPointF(7,15);
        QPolygonF r; r << QPointF(21,12) << QPointF(17,9) << QPointF(17,15);
        fillArrow(l); fillArrow(r);
    } else if (name == "Delete Page") {
        p.drawRect(QRectF(5, 3, 14, 18));
        p.drawLine(8, 8, 16, 16);
        p.drawLine(16, 8, 8, 16);
    } else if (name == "Insert") {
        p.drawRect(QRectF(5, 3, 14, 18));
        p.drawLine(12, 7, 12, 17);
        p.drawLine(7, 12, 17, 12);
    } else if (name == "Extract") {
        p.drawRect(QRectF(4, 3, 13, 15));
        p.drawRect(QRectF(8, 7, 13, 15));
    } else if (name == "Rotate Left") {
        QPainterPath a;
        a.moveTo(19, 12); a.arcTo(5, 5, 14, 14, 0, 270);
        p.drawPath(a);
        QPolygonF arrow; arrow << QPointF(5,12) << QPointF(8,9) << QPointF(8,15);
        fillArrow(arrow);
    } else if (name == "Rotate Right") {
        QPainterPath a;
        a.moveTo(5, 12); a.arcTo(5, 5, 14, 14, 180, -270);
        p.drawPath(a);
        QPolygonF arrow; arrow << QPointF(19,12) << QPointF(16,9) << QPointF(16,15);
        fillArrow(arrow);
    } else if (name == "Move Up") {
        p.drawLine(12, 20, 12, 6);
        QPolygonF a; a << QPointF(12,4) << QPointF(7,10) << QPointF(17,10);
        fillArrow(a);
    } else if (name == "Move Down") {
        p.drawLine(12, 4, 12, 18);
        QPolygonF a; a << QPointF(12,20) << QPointF(7,14) << QPointF(17,14);
        fillArrow(a);
    } else if (name == "Sign (draw)") {
        QPainterPath d;
        d.moveTo(3, 18); d.cubicTo(7, 10, 12, 20, 16, 8);
        p.drawPath(d);
        p.drawLine(3, 21, 21, 21);
    } else if (name == "Sign (image)") {
        p.drawRect(QRectF(3, 5, 18, 14));
        p.drawLine(3, 15, 9, 10);
        p.drawLine(9, 10, 14, 15);
        p.drawLine(14, 15, 18, 12);
        p.setBrush(fg);
        p.drawEllipse(QPointF(16, 8), 1.5, 1.5);
        p.setBrush(Qt::NoBrush);
    } else if (name == "To Images") {
        p.drawRect(QRectF(3, 5, 12, 14));
        p.drawLine(15, 12, 20, 12);
        QPolygonF a; a << QPointF(20,12) << QPointF(17,9) << QPointF(17,15);
        fillArrow(a);
    } else if (name == "To Word") {
        p.drawRect(QRectF(3, 3, 18, 18));
        drawText(QStringLiteral("W"), 10);
    } else if (name == "Compress") {
        p.drawLine(4, 6, 20, 6);
        p.drawLine(4, 18, 20, 18);
        QPolygonF a; a << QPointF(12,12) << QPointF(7,8)  << QPointF(17,8);
        QPolygonF b; b << QPointF(12,12) << QPointF(7,16) << QPointF(17,16);
        fillArrow(a); fillArrow(b);
    } else if (name == "Text WM") {
        p.save();
        p.translate(12, 12);
        p.rotate(-30);
        drawText(QStringLiteral("T"), 11);
        p.restore();
    } else if (name == "Image WM") {
        p.save();
        p.translate(12, 12);
        p.rotate(-30);
        p.drawRect(QRectF(-8, -5, 16, 10));
        p.drawLine(-8, 5, -2, -1);
        p.drawLine(-2, -1, 3, 4);
        p.drawEllipse(QPointF(4, -2), 1.5, 1.5);
        p.restore();
    } else if (name == "Password") {
        p.drawEllipse(QPointF(12, 10), 3, 3);
        p.drawLine(12, 13, 12, 20);
        p.drawLine(12, 16, 16, 16);
        p.drawLine(12, 19, 15, 19);
    } else if (name == "Fill Form") {
        p.drawRect(QRectF(4, 5, 16, 14));
        p.drawLine(7, 9, 17, 9);
        p.drawLine(7, 13, 17, 13);
        p.drawLine(7, 17, 13, 17);
    }
    // ---------------- Image editor --------------------------------------
    else if (name == "Brush") {
        QPainterPath d;
        d.moveTo(4, 20); d.lineTo(8, 20); d.lineTo(14, 14); d.lineTo(10, 10);
        d.closeSubpath();
        p.drawPath(d);
        p.drawLine(14, 14, 20, 8);
    } else if (name == "Eraser") {
        QPainterPath d;
        d.moveTo(5, 18); d.lineTo(14, 9); d.lineTo(20, 15); d.lineTo(11, 20);
        d.closeSubpath();
        p.drawPath(d);
    } else if (name == "Line") {
        p.drawLine(5, 19, 19, 5);
    } else if (name == "Rect") {
        p.drawRect(QRectF(5, 7, 14, 10));
    } else if (name == "Ellipse") {
        p.drawEllipse(QPointF(12, 12), 7, 5);
    } else if (name == "Arrow") {
        p.drawLine(4, 20, 18, 6);
        QPolygonF a; a << QPointF(19,5) << QPointF(14,6) << QPointF(18,10);
        fillArrow(a);
    } else if (name == "Text") {
        drawText(QStringLiteral("T"), 14);
    } else if (name == "Crop") {
        p.drawLine(7, 3, 7, 18);
        p.drawLine(3, 7, 18, 7);
        p.drawLine(17, 6, 17, 21);
        p.drawLine(6, 17, 21, 17);
    } else if (name == "Mosaic") {
        for (int gy = 0; gy < 4; ++gy)
            for (int gx = 0; gx < 4; ++gx)
                p.drawRect(QRectF(4 + gx * 4, 4 + gy * 4, 4, 4));
    } else if (name == "Magic Wand") {
        p.drawLine(5, 20, 14, 11);
        p.drawLine(14, 11, 19, 6);
        QPolygonF star;
        star << QPointF(19, 3) << QPointF(21, 6) << QPointF(19, 9)
             << QPointF(17, 6);
        p.drawPolygon(star);
    } else if (name == "Rotate Left"  /* duplicate guard */) {
        // handled above
    } else if (name == "Flip H") {
        p.drawLine(12, 4, 12, 20);
        QPolygonF l; l << QPointF(3, 12) << QPointF(10, 6) << QPointF(10, 18);
        QPolygonF r; r << QPointF(21, 12) << QPointF(14, 6) << QPointF(14, 18);
        p.drawPolygon(l);
        p.setBrush(fg); p.drawPolygon(r); p.setBrush(Qt::NoBrush);
    } else if (name == "Flip V") {
        p.drawLine(4, 12, 20, 12);
        QPolygonF u; u << QPointF(12, 3) << QPointF(6, 10) << QPointF(18, 10);
        QPolygonF d; d << QPointF(12, 21) << QPointF(6, 14) << QPointF(18, 14);
        p.drawPolygon(u);
        p.setBrush(fg); p.drawPolygon(d); p.setBrush(Qt::NoBrush);
    } else if (name == "Free Rotate..." || name == "Free Rotate") {
        QPainterPath a;
        a.moveTo(19, 12); a.arcTo(5, 5, 14, 14, 0, 300);
        p.drawPath(a);
        QPolygonF arrow; arrow << QPointF(5,12) << QPointF(8,9) << QPointF(8,15);
        fillArrow(arrow);
        drawText(QStringLiteral("°"), 8, false);
    } else if (name == "Straighten..." || name == "Straighten") {
        p.drawLine(4, 18, 20, 10);
        p.drawLine(4, 20, 20, 20);
    } else if (name == "Resize..." || name == "Resize") {
        p.drawLine(4, 4, 20, 20);
        QPolygonF a; a << QPointF(4,4) << QPointF(10,4) << QPointF(4,10);
        QPolygonF b; b << QPointF(20,20) << QPointF(14,20) << QPointF(20,14);
        fillArrow(a); fillArrow(b);
    } else if (name == "Scale..." || name == "Scale") {
        p.drawRect(QRectF(5, 5, 14, 14));
        drawText(QStringLiteral("%"), 10);
    } else if (name == "DPI..." || name == "DPI") {
        drawText(QStringLiteral("DPI"), 7);
    } else if (name == "Expand Canvas..." || name == "Expand Canvas") {
        QPen dash(fg, 1.2, Qt::DashLine);
        p.setPen(dash);
        p.drawRect(QRectF(3, 3, 18, 18));
        p.setPen(pen);
        p.drawRect(QRectF(7, 7, 10, 10));
    } else if (name == "Auto Cutout") {
        QPen dash(fg, 1.2, Qt::DashLine);
        p.setPen(dash);
        p.drawEllipse(QPointF(12, 12), 8, 6);
        p.setPen(pen);
        drawText(QStringLiteral("✂"), 9);
    } else if (name == "Text Watermark...") {
        p.save();
        p.translate(12, 12); p.rotate(-30);
        drawText(QStringLiteral("T"), 11);
        p.restore();
    } else if (name == "Image Watermark...") {
        p.save();
        p.translate(12, 12); p.rotate(-30);
        p.drawRect(QRectF(-8, -5, 16, 10));
        p.drawLine(-8, 5, -2, -1);
        p.drawLine(-2, -1, 3, 4);
        p.restore();
    } else if (name == "Export As...") {
        p.drawRect(QRectF(4, 4, 12, 16));
        QPolygonF a; a << QPointF(20,12) << QPointF(14,8) << QPointF(14,16);
        fillArrow(a);
    } else if (name == "Compress & Export...") {
        p.drawRect(QRectF(4, 4, 12, 16));
        QPolygonF a; a << QPointF(20,12) << QPointF(14,8) << QPointF(14,16);
        fillArrow(a);
        drawText(QStringLiteral("z"), 7, false);
    }
    // ---------------- Compare tabs --------------------------------------
    else if (name == "◀ Prev Diff") {
        QPolygonF a; a << QPointF(5,12) << QPointF(11,6) << QPointF(11,18);
        fillArrow(a);
        p.drawLine(13, 8, 19, 8);
        p.drawLine(13, 12, 19, 12);
        p.drawLine(13, 16, 19, 16);
    } else if (name == "Next Diff ▶") {
        p.drawLine(5, 8, 11, 8);
        p.drawLine(5, 12, 11, 12);
        p.drawLine(5, 16, 11, 16);
        QPolygonF a; a << QPointF(19,12) << QPointF(13,6) << QPointF(13,18);
        fillArrow(a);
    } else if (name == "Copy →" || name == "Copy →  ") {
        p.drawRect(QRectF(3, 6, 8, 12));
        QPolygonF a; a << QPointF(20,12) << QPointF(14,8) << QPointF(14,16);
        fillArrow(a);
    } else if (name == "← Copy") {
        p.drawRect(QRectF(13, 6, 8, 12));
        QPolygonF a; a << QPointF(4,12) << QPointF(10,8) << QPointF(10,16);
        fillArrow(a);
    } else if (name == "Swap") {
        p.drawLine(5, 9, 19, 9);
        p.drawLine(5, 15, 19, 15);
        QPolygonF a; a << QPointF(19,9)  << QPointF(16,6)  << QPointF(16,12);
        QPolygonF b; b << QPointF(5,15)  << QPointF(8,12)  << QPointF(8,18);
        fillArrow(a); fillArrow(b);
    } else if (name == "Reload" || name == "Refresh") {
        QPainterPath a;
        a.moveTo(19, 12); a.arcTo(5, 5, 14, 14, 0, 300);
        p.drawPath(a);
        QPolygonF arrow; arrow << QPointF(19,12) << QPointF(16,9) << QPointF(16,15);
        fillArrow(arrow);
    } else if (name == "As Hex") {
        drawText(QStringLiteral("Hex"), 7);
    } else if (name == "As Text") {
        drawText(QStringLiteral("Txt"), 7);
    } else if (name == "As Picture") {
        p.drawRect(QRectF(4, 6, 16, 12));
        p.drawLine(4, 15, 10, 10);
        p.drawLine(10, 10, 14, 14);
        p.drawEllipse(QPointF(15, 10), 1.5, 1.5);
    } else if (name == "Save L") {
        drawText(QStringLiteral("S\u2190"), 8);   // S + left arrow
    } else if (name == "Save R") {
        drawText(QStringLiteral("S\u2192"), 8);   // S + right arrow
    } else if (name == "Ignore Case") {
        drawText(QStringLiteral("Aa"), 10);
    } else if (name == "Ignore Whitespace") {
        drawText(QStringLiteral("⍽"), 10);
    } else if (name == "Differences Only") {
        // Funnel shape.
        QPolygonF funnel;
        funnel << QPointF(4, 5) << QPointF(20, 5) << QPointF(14, 13)
               << QPointF(14, 19) << QPointF(10, 19) << QPointF(10, 13);
        p.drawPolygon(funnel);
    } else if (name == "Difference Overlay") {
        p.drawRect(QRectF(4, 4, 12, 12));
        p.drawRect(QRectF(8, 8, 12, 12));
    } else if (name == "First Page") {
        p.drawLine(6, 5, 6, 19);
        QPolygonF a; a << QPointF(8,12) << QPointF(16,6) << QPointF(16,18);
        fillArrow(a);
    } else if (name == "Previous Page") {
        QPolygonF a; a << QPointF(8,12) << QPointF(16,6) << QPointF(16,18);
        fillArrow(a);
    } else if (name == "Next Page") {
        QPolygonF a; a << QPointF(16,12) << QPointF(8,6) << QPointF(8,18);
        fillArrow(a);
    } else if (name == "Last Page") {
        p.drawLine(18, 5, 18, 19);
        QPolygonF a; a << QPointF(16,12) << QPointF(8,6) << QPointF(8,18);
        fillArrow(a);
    }
    // ---------------- Cloud browser & transfer panel --------------------
    else if (name == "Back") {
        QPainterPath arc;
        arc.moveTo(18, 14); arc.arcTo(5, 6, 14, 14, 0, 180);
        p.drawPath(arc);
        QPolygonF a; a << QPointF(4,13) << QPointF(8,10) << QPointF(8,16);
        fillArrow(a);
    } else if (name == "Up") {
        p.drawLine(12, 20, 12, 7);
        QPolygonF a; a << QPointF(12,4) << QPointF(6,10) << QPointF(18,10);
        fillArrow(a);
    } else if (name == "Upload Files...") {
        p.drawLine(12, 4, 12, 16);
        QPolygonF a; a << QPointF(12,2) << QPointF(6,8) << QPointF(18,8);
        fillArrow(a);
        p.drawLine(4, 20, 20, 20);
    } else if (name == "Upload Folder...") {
        p.drawRect(QRectF(3, 8, 18, 12));
        p.drawLine(3, 8, 9, 8); p.drawLine(9, 8, 11, 5); p.drawLine(11, 5, 14, 5);
        QPolygonF a; a << QPointF(12,10) << QPointF(8,14) << QPointF(16,14);
        fillArrow(a);
        p.drawLine(12, 14, 12, 18);
    } else if (name == "New Folder...") {
        p.drawRect(QRectF(3, 8, 18, 12));
        p.drawLine(3, 8, 9, 8); p.drawLine(9, 8, 11, 5); p.drawLine(11, 5, 14, 5);
        p.drawLine(12, 11, 12, 17);
        p.drawLine(9, 14, 15, 14);
    } else if (name == "Open Folder") {
        p.drawRect(QRectF(3, 8, 18, 12));
        p.drawLine(3, 8, 9, 8); p.drawLine(9, 8, 11, 5); p.drawLine(11, 5, 14, 5);
    } else if (name == "Open") {
        // A document icon with a folded corner — "open this item".
        QPainterPath doc;
        doc.moveTo(5, 3); doc.lineTo(15, 3); doc.lineTo(19, 7);
        doc.lineTo(19, 21); doc.lineTo(5, 21); doc.closeSubpath();
        p.drawPath(doc);
        p.drawLine(15, 3, 15, 7); p.drawLine(15, 7, 19, 7);
        p.drawLine(8, 12, 16, 12);
        p.drawLine(8, 16, 16, 16);
    } else if (name == "Rename...") {
        QFont f = p.font(); f.setBold(true); f.setPointSize(11); p.setFont(f);
        p.drawText(R, Qt::AlignCenter, QStringLiteral("abc"));
        p.drawLine(5, 20, 19, 20);
    } else if (name == "Download" || name == "Download..." ||
               name == "Download Folder...") {
        p.drawLine(12, 4, 12, 16);
        QPolygonF a; a << QPointF(12,18) << QPointF(6,12) << QPointF(18,12);
        fillArrow(a);
        p.drawLine(4, 20, 20, 20);
    } else if (name == "Upload") {
        p.drawLine(12, 4, 12, 16);
        QPolygonF a; a << QPointF(12,2) << QPointF(6,8) << QPointF(18,8);
        fillArrow(a);
        p.drawLine(4, 20, 20, 20);
    } else if (name == "Stream (play while downloading)") {
        QPolygonF play; play << QPointF(7,5) << QPointF(19,12) << QPointF(7,19);
        fillArrow(play);
    } else if (name == "Open With...") {
        p.drawRect(QRectF(4, 6, 16, 12));
        p.drawLine(4, 10, 20, 10);
        drawText(QStringLiteral("…"), 10);
    } else if (name == "Open as Text") {
        p.drawRect(QRectF(5, 3, 14, 18));
        p.drawLine(8, 8, 16, 8);
        p.drawLine(8, 12, 16, 12);
        p.drawLine(8, 16, 13, 16);
    } else if (name == "Open (system default)") {
        p.drawRect(QRectF(5, 4, 14, 14));
        p.drawLine(5, 4, 19, 18);
        p.drawLine(19, 4, 5, 18);
    } else if (name == "Cancel") {
        p.drawEllipse(QPointF(12, 12), 8, 8);
        p.drawLine(7, 7, 17, 17);
        p.drawLine(17, 7, 7, 17);
    } else if (name == "Retry") {
        QPainterPath arc;
        arc.moveTo(19, 12); arc.arcTo(5, 5, 14, 14, 0, 300);
        p.drawPath(arc);
        QPolygonF a; a << QPointF(19,12) << QPointF(16,9) << QPointF(16,15);
        fillArrow(a);
    } else if (name == "Remove") {
        p.drawRect(QRectF(6, 7, 12, 14));
        p.drawLine(4, 6, 20, 6);
        p.drawLine(10, 3, 14, 3);
        p.drawLine(10, 10, 10, 18);
        p.drawLine(14, 10, 14, 18);
    } else if (name == "Clear Finished") {
        p.drawEllipse(QPointF(12, 12), 8, 8);
        p.drawLine(8, 12, 11, 15);
        p.drawLine(11, 15, 16, 9);
    }
    else {
        // Unknown name -> return a blank icon so Qt falls back to the label.
        return QIcon();
    }

    p.end();
    return QIcon(QPixmap::fromImage(img));
}

} // namespace IconFactory