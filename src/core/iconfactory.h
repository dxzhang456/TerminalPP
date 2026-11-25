#ifndef ICONFACTORY_H
#define ICONFACTORY_H

#include <QIcon>
#include <QColor>
#include <QString>

// ---------------------------------------------------------------------------
// IconFactory — procedural toolbar icons.
//
// Every toolbar button in Terminal++ (PDF, image editor, compare tabs, ...)
// requests its icon by *source string name* (e.g. "Save", "Copy →",
// "Difference Overlay") and this factory draws a clean monochrome line
// drawing with QPainter. No PNG/SVG assets are shipped; every icon lives in
// code, so dark/light themes just pass a different foreground color and the
// icons follow.
//
// The source string is the same English key used by the i18n system, so the
// button's visible label (or its tooltip, if the button is set to
// ToolButtonIconOnly) continues to retranslate when the UI language changes.
// ---------------------------------------------------------------------------
namespace IconFactory {

// Build a 24x24 QIcon for `name`. `fg` is the foreground color used for line
// work; components that are inherently colored (yellow highlight, color
// wheel, ...) keep their own colors so the function renders them naturally.
// If `name` is unknown an empty QIcon is returned (the button then falls back
// to showing its text label).
QIcon makeToolIcon(const QString &name, const QColor &fg = QColor(60, 60, 60));

} // namespace IconFactory

#endif // ICONFACTORY_H