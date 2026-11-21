#include "ui/Icons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QtMath>

namespace Icons {

void paint(QPainter &p, const QRectF &rect, Icon icon, const QColor &color)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    // Draw on a 16x16 grid, scaled to the target rect.
    p.translate(rect.topLeft());
    p.scale(rect.width() / 16.0, rect.height() / 16.0);
    QPen pen(color, 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    switch (icon) {
    case Icon::Training:
        p.drawRoundedRect(QRectF(2, 2.5, 12, 8.5), 1.5, 1.5);
        p.drawLine(QPointF(8, 11), QPointF(8, 13.5));
        p.drawLine(QPointF(5.5, 13.5), QPointF(10.5, 13.5));
        p.drawLine(QPointF(5, 8.5), QPointF(7, 6.5));
        p.drawLine(QPointF(7, 6.5), QPointF(8.5, 7.8));
        p.drawLine(QPointF(8.5, 7.8), QPointF(11, 5));
        break;
    case Icon::PreMeet:
        p.drawRoundedRect(QRectF(3, 2.5, 10, 11.5), 1.5, 1.5);
        p.drawLine(QPointF(5.5, 6), QPointF(6.3, 6.8));
        p.drawLine(QPointF(6.3, 6.8), QPointF(7.8, 5.2));
        p.drawLine(QPointF(9, 6), QPointF(11, 6));
        p.drawLine(QPointF(5.5, 10), QPointF(6.3, 10.8));
        p.drawLine(QPointF(6.3, 10.8), QPointF(7.8, 9.2));
        p.drawLine(QPointF(9, 10), QPointF(11, 10));
        break;
    case Icon::Meet: {
        p.drawRoundedRect(QRectF(1.5, 4, 9.5, 8), 1.8, 1.8);
        QPainterPath lens;
        lens.moveTo(11, 7);
        lens.lineTo(14.5, 5);
        lens.lineTo(14.5, 11);
        lens.lineTo(11, 9);
        p.drawPath(lens);
        break;
    }
    case Icon::PostMeet:
        p.drawLine(QPointF(2.5, 13.5), QPointF(13.5, 13.5));
        p.drawRoundedRect(QRectF(3.5, 8, 2.2, 5.5), 0.6, 0.6);
        p.drawRoundedRect(QRectF(6.9, 4.5, 2.2, 9), 0.6, 0.6);
        p.drawRoundedRect(QRectF(10.3, 6.5, 2.2, 7), 0.6, 0.6);
        break;
    case Icon::Info:
        p.drawEllipse(QPointF(8, 8), 6, 6);
        p.drawLine(QPointF(8, 7.3), QPointF(8, 11));
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(8, 5.2), 0.8, 0.8);
        break;
    case Icon::Add:
        p.drawLine(QPointF(8, 3), QPointF(8, 13));
        p.drawLine(QPointF(3, 8), QPointF(13, 8));
        break;
    case Icon::Close:
        p.drawLine(QPointF(4, 4), QPointF(12, 12));
        p.drawLine(QPointF(12, 4), QPointF(4, 12));
        break;
    case Icon::Play: {
        QPainterPath tri;
        tri.moveTo(5, 3.5);
        tri.lineTo(12.5, 8);
        tri.lineTo(5, 12.5);
        tri.closeSubpath();
        p.drawPath(tri);
        break;
    }
    case Icon::Next: {
        QPainterPath tri;
        tri.moveTo(3.5, 3.5);
        tri.lineTo(10, 8);
        tri.lineTo(3.5, 12.5);
        tri.closeSubpath();
        p.drawPath(tri);
        p.drawLine(QPointF(12.5, 3.5), QPointF(12.5, 12.5));
        break;
    }
    case Icon::Stop:
        p.drawRoundedRect(QRectF(4, 4, 8, 8), 1.5, 1.5);
        break;
    case Icon::Eye: {
        QPainterPath eye;
        eye.moveTo(1.5, 8);
        eye.cubicTo(4, 3.5, 12, 3.5, 14.5, 8);
        eye.cubicTo(12, 12.5, 4, 12.5, 1.5, 8);
        p.drawPath(eye);
        p.drawEllipse(QPointF(8, 8), 2.2, 2.2);
        break;
    }
    case Icon::Settings: {
        // Gear: ring with eight teeth
        for (int i = 0; i < 8; ++i) {
            const qreal a = qDegreesToRadians(i * 45.0);
            p.drawLine(QPointF(8 + std::cos(a) * 4.6, 8 + std::sin(a) * 4.6),
                       QPointF(8 + std::cos(a) * 6.6, 8 + std::sin(a) * 6.6));
        }
        p.drawEllipse(QPointF(8, 8), 4.6, 4.6);
        p.drawEllipse(QPointF(8, 8), 1.8, 1.8);
        break;
    }
    case Icon::Camera: {
        p.drawRoundedRect(QRectF(1.5, 4.5, 13, 9), 2, 2);
        QPainterPath bump;
        bump.moveTo(5, 4.5);
        bump.lineTo(6.2, 2.8);
        bump.lineTo(9.8, 2.8);
        bump.lineTo(11, 4.5);
        p.drawPath(bump);
        p.drawEllipse(QPointF(8, 9), 2.6, 2.6);
        break;
    }
    case Icon::Mic:
        p.drawRoundedRect(QRectF(5.75, 1.5, 4.5, 8), 2.25, 2.25);
        p.drawArc(QRectF(3.5, 4, 9, 8), 200 * 16, 140 * 16);
        p.drawLine(QPointF(8, 12), QPointF(8, 14.5));
        p.drawLine(QPointF(5.5, 14.5), QPointF(10.5, 14.5));
        break;
    case Icon::Sparkle: {
        QPainterPath star;
        star.moveTo(7, 1.5);
        star.quadTo(7.6, 6.4, 12.5, 7);
        star.quadTo(7.6, 7.6, 7, 12.5);
        star.quadTo(6.4, 7.6, 1.5, 7);
        star.quadTo(6.4, 6.4, 7, 1.5);
        p.drawPath(star);
        p.drawLine(QPointF(12.5, 11), QPointF(12.5, 14.5));
        p.drawLine(QPointF(10.75, 12.75), QPointF(14.25, 12.75));
        break;
    }
    case Icon::ArrowUp:
        p.drawLine(QPointF(8, 13), QPointF(8, 3));
        p.drawLine(QPointF(4, 7), QPointF(8, 3));
        p.drawLine(QPointF(12, 7), QPointF(8, 3));
        break;
    case Icon::ArrowDown:
        p.drawLine(QPointF(8, 3), QPointF(8, 13));
        p.drawLine(QPointF(4, 9), QPointF(8, 13));
        p.drawLine(QPointF(12, 9), QPointF(8, 13));
        break;
    case Icon::ChevronDown:
        p.drawLine(QPointF(4, 6), QPointF(8, 10));
        p.drawLine(QPointF(12, 6), QPointF(8, 10));
        break;
    case Icon::ChevronRight:
        p.drawLine(QPointF(6, 4), QPointF(10, 8));
        p.drawLine(QPointF(6, 12), QPointF(10, 8));
        break;
    case Icon::Bug:
        p.drawEllipse(QRectF(5, 5, 6, 9));
        p.drawLine(QPointF(8, 7.5), QPointF(8, 14));
        p.drawArc(QRectF(6, 2.5, 4, 4), 0, 180 * 16);
        for (qreal y : {7.0, 9.5, 12.0}) {
            p.drawLine(QPointF(5, y), QPointF(2.5, y - 1));
            p.drawLine(QPointF(11, y), QPointF(13.5, y - 1));
        }
        break;
    case Icon::Text:
        p.drawLine(QPointF(2.5, 4), QPointF(13.5, 4));
        p.drawLine(QPointF(2.5, 7), QPointF(13.5, 7));
        p.drawLine(QPointF(2.5, 10), QPointF(13.5, 10));
        p.drawLine(QPointF(2.5, 13), QPointF(9, 13));
        break;
    case Icon::Bullets:
        for (qreal y : {4.0, 8.0, 12.0}) {
            p.drawLine(QPointF(6, y), QPointF(13.5, y));
            p.save();
            p.setBrush(color);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(3, y), 1.1, 1.1);
            p.restore();
        }
        break;
    }
    p.restore();
}

QIcon icon(Icon which, const QColor &color, int size)
{
    QIcon result;
    for (qreal dpr : {1.0, 2.0}) {
        QPixmap pixmap(QSize(size, size) * dpr);
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        paint(painter, QRectF(0, 0, size, size), which, color);
        painter.end();
        result.addPixmap(pixmap);
    }
    return result;
}

} // namespace Icons
