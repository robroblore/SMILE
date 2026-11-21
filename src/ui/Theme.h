#pragma once

#include <QColor>
#include <QString>

class QApplication;
class QWidget;

// Windows 11 (Fluent) inspired look, applied the same way on every platform.
// Follows the system light/dark setting and updates live when it changes.
namespace Theme {

struct Colors {
    bool dark = false;
    QColor background;     // window / mica-like base
    QColor card;           // raised surfaces
    QColor cardBorder;
    QColor flyout;         // menus, bubble
    QColor text;
    QColor textSecondary;
    QColor textDisabled;
    QColor accent;
    QColor accentHover;
    QColor accentPressed;
    QColor onAccent;       // text on accent
    QColor control;        // buttons, inputs
    QColor controlHover;
    QColor controlPressed;
    QColor controlBorder;
    QColor controlBorderBottom;
    QColor subtleHover;    // transparent buttons, nav items
    QColor subtlePressed;
    QColor track;          // progress / toggle off
    QColor success;
    QColor caution;
    QColor critical;
    QColor orange;         // "needs work" rating
};

const Colors &colors();

// Sets style, palette, font and stylesheet. Call once after QApplication exists.
void apply(QApplication &app);

// Re-polish a widget after changing a dynamic property used by the stylesheet.
void repolish(QWidget *widget);

// "#RRGGBB" or "rgba(r,g,b,a)" for use in stylesheets/HTML
QString css(const QColor &color);

// Green / orange / red for a rating (see core/Rating.h)
QColor ratingColor(int rating); // 0 none, 1 good, 2 needs work, 3 poor

} // namespace Theme
