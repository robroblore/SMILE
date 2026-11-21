#include "companion/SmileyPainter.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace SmileyPainter {

namespace {
const QColor kFace(0xFF, 0xD2, 0x3F);
const QColor kInk(0x33, 0x33, 0x33);

QRectF sub(const QRectF &r, qreal x, qreal y, qreal w, qreal h)
{
    return QRectF(r.left() + r.width() * x, r.top() + r.height() * y, r.width() * w, r.height() * h);
}
} // namespace

void paint(QPainter &painter, const QRectF &rect, Mood mood, bool eyesClosed)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    const qreal stroke = qMax<qreal>(1.0, rect.width() * 0.045);
    const QRectF face = rect.adjusted(stroke / 2, stroke / 2, -stroke / 2, -stroke / 2);

    painter.setPen(QPen(kInk, stroke));
    painter.setBrush(kFace);
    painter.drawEllipse(face);

    // Eyes
    QPen ink(kInk, stroke, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(ink);
    painter.setBrush(kInk);
    for (qreal x : {0.33, 0.67}) {
        const QPointF centre(face.left() + face.width() * x, face.top() + face.height() * 0.40);
        const qreal r = face.width() * 0.06;
        if (eyesClosed)
            painter.drawLine(centre - QPointF(r, 0), centre + QPointF(r, 0));
        else
            painter.drawEllipse(centre, r * 0.8, r);
    }

    // Worried eyebrows (inner ends raised) when concerned
    if (mood == Mood::Concerned) {
        painter.drawLine(QPointF(face.left() + face.width() * 0.24, face.top() + face.height() * 0.29),
                         QPointF(face.left() + face.width() * 0.40, face.top() + face.height() * 0.24));
        painter.drawLine(QPointF(face.left() + face.width() * 0.76, face.top() + face.height() * 0.29),
                         QPointF(face.left() + face.width() * 0.60, face.top() + face.height() * 0.24));
    }

    // Mouth (Qt angles: 0 = 3 o'clock, counter-clockwise, in 1/16 degree)
    painter.setBrush(Qt::NoBrush);
    switch (mood) {
    case Mood::Happy:
        painter.drawArc(sub(face, 0.24, 0.32, 0.52, 0.44), 200 * 16, 140 * 16);
        break;
    case Mood::Neutral:
        painter.drawArc(sub(face, 0.32, 0.46, 0.36, 0.24), 215 * 16, 110 * 16);
        break;
    case Mood::Concerned:
        // A small, slightly open "o" mouth: attentive rather than cross
        painter.drawEllipse(QPointF(face.center().x(), face.top() + face.height() * 0.70),
                            face.width() * 0.07, face.height() * 0.05);
        break;
    }

    painter.restore();
}

QIcon appIcon()
{
    QIcon icon;
    for (int size : {16, 24, 32, 48, 64, 128, 256}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        paint(painter, QRectF(0, 0, size, size), Mood::Happy);
        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

} // namespace SmileyPainter
