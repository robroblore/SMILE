#pragma once

#include <QIcon>
#include <QRectF>

class QPainter;

// Draws the SMILE character. Every feature is a continuous value, so the
// companion can animate smoothly between expressions instead of switching
// between fixed faces.
namespace SmileyPainter {

struct Face {
    qreal smile = 0.6;   // -1 frown .. 0 flat .. 1 big open smile
    qreal eyeOpen = 1.0; // 0 closed (blink) .. 1 open
    qreal worry = 0.0;   // 0 relaxed .. 1 worried eyebrows
    qreal lookX = 0.0;   // -1 left .. 1 right
    qreal lookY = 0.0;   // -1 up .. 1 down
    qreal squash = 0.0;  // -1 tall .. 1 wide (for bouncy pop-in)
};

// Smoothly move each feature of `current` towards `target` (0 < rate <= 1).
Face approach(const Face &current, const Face &target, qreal rate);

void paint(QPainter &painter, const QRectF &rect, const Face &face);
QIcon appIcon();

} // namespace SmileyPainter
