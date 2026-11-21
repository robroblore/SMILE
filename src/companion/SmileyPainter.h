#pragma once

#include <QIcon>
#include <QRectF>

class QPainter;

// Draws the SMILE character. Used by the companion window and the app icon,
// so no image files are needed.
namespace SmileyPainter {

enum class Mood {
    Happy,
    Neutral,
    Concerned,
};

void paint(QPainter &painter, const QRectF &rect, Mood mood, bool eyesClosed = false);
QIcon appIcon();

} // namespace SmileyPainter
