#include "companion/SmileyPainter.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRadialGradient>
#include <QtMath>

namespace SmileyPainter {

namespace {
const QColor kInk(0x3B, 0x26, 0x14);
const QColor kMouth(0x7A, 0x2E, 0x1E);
const QColor kTongue(0xF2, 0x7B, 0x7B);

qreal lerp(qreal a, qreal b, qreal t)
{
    return a + (b - a) * t;
}
} // namespace

Face approach(const Face &c, const Face &t, qreal rate)
{
    Face f;
    f.smile = lerp(c.smile, t.smile, rate);
    f.eyeOpen = lerp(c.eyeOpen, t.eyeOpen, qMin<qreal>(1.0, rate * 2.5)); // blinks are fast
    f.worry = lerp(c.worry, t.worry, rate);
    f.lookX = lerp(c.lookX, t.lookX, rate);
    f.lookY = lerp(c.lookY, t.lookY, rate);
    f.squash = lerp(c.squash, t.squash, rate);
    f.mouthOpen = lerp(c.mouthOpen, t.mouthOpen, qMin<qreal>(1.0, rate * 3.0)); // lips move fast
    f.browRaise = lerp(c.browRaise, t.browRaise, rate);
    return f;
}

void paint(QPainter &p, const QRectF &rect, const Face &face)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);

    // Squash and stretch around the bottom centre
    const qreal sx = 1.0 + 0.10 * face.squash;
    const qreal sy = 1.0 - 0.10 * face.squash;
    const QRectF r(rect.center().x() - rect.width() * sx / 2, rect.bottom() - rect.height() * sy,
                   rect.width() * sx, rect.height() * sy);
    const qreal w = r.width();
    const qreal h = r.height();
    const qreal cx = r.center().x();
    auto at = [&](qreal fx, qreal fy) { return QPointF(r.left() + w * fx, r.top() + h * fy); };

    // Face: warm, clean gradient, light from top-left; no outline or dark rim
    QRadialGradient fill(at(0.36, 0.30), w * 0.85);
    fill.setColorAt(0.0, QColor(0xFF, 0xEC, 0x99));
    fill.setColorAt(0.6, QColor(0xFF, 0xD5, 0x45));
    fill.setColorAt(1.0, QColor(0xFF, 0xC8, 0x2A));
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawEllipse(r.adjusted(w * 0.01, h * 0.01, -w * 0.01, -h * 0.01));

    // Gloss highlight
    QRadialGradient gloss(at(0.34, 0.22), w * 0.28);
    gloss.setColorAt(0.0, QColor(255, 255, 255, 120));
    gloss.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(gloss);
    {
        // Keep the highlight inside the face so it never smudges the background
        p.save();
        QPainterPath faceClip;
        faceClip.addEllipse(r.adjusted(w * 0.01, h * 0.01, -w * 0.01, -h * 0.01));
        p.setClipPath(faceClip, Qt::IntersectClip);
        p.drawEllipse(at(0.34, 0.22), w * 0.28, h * 0.18);
        p.restore();
    }

    // Cheeks (blush grows with the smile)
    const int blush = qBound(0, int(110 * face.smile), 110);
    if (blush > 0) {
        for (qreal fx : {0.22, 0.78}) {
            QRadialGradient cheek(at(fx, 0.60), w * 0.10);
            cheek.setColorAt(0.0, QColor(255, 110, 110, blush));
            cheek.setColorAt(1.0, QColor(255, 110, 110, 0));
            p.setBrush(cheek);
            p.drawEllipse(at(fx, 0.60), w * 0.11, h * 0.08);
        }
    }

    // Eyes
    const QPointF look(face.lookX * w * 0.035, face.lookY * h * 0.03);
    const qreal eyeRx = w * 0.058;
    const qreal eyeRy = h * 0.082;
    const qreal open = qBound<qreal>(0.0, face.eyeOpen, 1.0);
    for (qreal fx : {0.36, 0.64}) {
        const QPointF centre = at(fx, 0.42) + look;
        if (open < 0.22) {
            // Closed: a soft downward arc (blink)
            QPainterPath lid;
            lid.moveTo(centre + QPointF(-eyeRx, 0));
            lid.quadTo(centre + QPointF(0, eyeRy * 0.45), centre + QPointF(eyeRx, 0));
            p.setPen(QPen(kInk, w * 0.032, Qt::SolidLine, Qt::RoundCap));
            p.setBrush(Qt::NoBrush);
            p.drawPath(lid);
            continue;
        }
        p.setPen(Qt::NoPen);
        p.setBrush(kInk);
        p.drawEllipse(centre, eyeRx, eyeRy * open);
        p.setBrush(QColor(255, 255, 255, 230));
        p.drawEllipse(centre + QPointF(eyeRx * 0.35, -eyeRy * 0.38 * open), eyeRx * 0.36, eyeRx * 0.36 * open);
        p.setBrush(QColor(255, 255, 255, 120));
        p.drawEllipse(centre + QPointF(-eyeRx * 0.3, eyeRy * 0.35 * open), eyeRx * 0.16, eyeRx * 0.16 * open);
    }

    // Raised brows: curious / asking a question
    if (face.browRaise > 0.04 && face.worry <= 0.04) {
        QColor brow = kInk;
        brow.setAlphaF(qMin<qreal>(1.0, face.browRaise * 1.4));
        p.setPen(QPen(brow, w * 0.026, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        for (qreal fx : {0.36, 0.64}) {
            const QPointF c = at(fx, 0.27 - 0.04 * face.browRaise);
            QPainterPath b;
            b.moveTo(c + QPointF(-w * 0.07, h * 0.015));
            b.quadTo(c + QPointF(0, -h * 0.035), c + QPointF(w * 0.07, h * 0.015));
            p.drawPath(b);
        }
    }

    // Eyebrows (only when worried)
    if (face.worry > 0.04) {
        QColor brow = kInk;
        brow.setAlphaF(qMin<qreal>(1.0, face.worry * 1.4));
        p.setPen(QPen(brow, w * 0.028, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        for (int side : {-1, 1}) {
            const qreal outerX = cx + side * w * 0.20;
            const qreal innerX = cx + side * w * 0.08;
            const qreal baseY = r.top() + h * 0.27;
            QPainterPath b;
            b.moveTo(outerX, baseY);
            b.quadTo((outerX + innerX) / 2, baseY - h * 0.03, innerX, baseY - h * 0.07 * face.worry);
            p.drawPath(b);
        }
    }

    // Mouth: a smooth curve whose depth follows `smile`; opens when smiling big
    const qreal s = qBound<qreal>(-1.0, face.smile, 1.0);
    const qreal mouthY = r.top() + h * 0.66;
    const qreal halfW = w * (0.13 + 0.07 * qMax<qreal>(0.0, s));
    const QPointF left(cx - halfW, mouthY - h * 0.02 * s);
    const QPointF right(cx + halfW, mouthY - h * 0.02 * s);
    const qreal depth = h * 0.17 * s;

    const qreal talk = qBound<qreal>(0.0, face.mouthOpen, 1.0);
    if (talk > 0.04 && s <= 0.55) {
        // Talking: an open mouth whose height follows the voice
        QPainterPath mouth;
        const qreal top = mouthY - h * 0.02 * s - h * 0.012 * talk;
        const qreal bottom = mouthY + h * (0.03 + 0.15 * talk) + depth * 0.4;
        const qreal half = halfW * (0.75 + 0.15 * (1.0 - talk));
        mouth.moveTo(cx - half, mouthY);
        mouth.cubicTo(cx - half * 0.5, top, cx + half * 0.5, top, cx + half, mouthY);
        mouth.cubicTo(cx + half * 0.6, bottom, cx - half * 0.6, bottom, cx - half, mouthY);
        p.setPen(QPen(kInk, w * 0.028, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(kMouth);
        p.drawPath(mouth);
        p.save();
        p.setClipPath(mouth);
        p.setPen(Qt::NoPen);
        p.setBrush(kTongue);
        p.drawEllipse(QPointF(cx, bottom), half * 0.55, h * 0.06);
        p.restore();
    } else if (s > 0.55) {
        const qreal openness = qMax((s - 0.55) / 0.45, talk); // 0..1
        QPainterPath mouth;
        mouth.moveTo(left);
        mouth.cubicTo(left + QPointF(halfW * 0.5, h * 0.03 * openness), right + QPointF(-halfW * 0.5, h * 0.03 * openness),
                      right);
        mouth.cubicTo(right + QPointF(-halfW * 0.2, depth * 1.05), left + QPointF(halfW * 0.2, depth * 1.05), left);
        p.setPen(QPen(kInk, w * 0.03, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(kMouth);
        p.drawPath(mouth);

        p.save();
        p.setClipPath(mouth);
        p.setPen(Qt::NoPen);
        p.setBrush(kTongue);
        p.drawEllipse(QPointF(cx, mouthY + depth * 0.95), halfW * 0.62, h * 0.07);
        p.restore();
    } else {
        QPainterPath mouth;
        mouth.moveTo(left);
        mouth.cubicTo(left + QPointF(halfW * 0.45, depth), right + QPointF(-halfW * 0.45, depth), right);
        p.setPen(QPen(kInk, w * 0.036, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        p.drawPath(mouth);
    }

    p.restore();
}

QIcon appIcon()
{
    QIcon icon;
    Face face;
    face.smile = 0.8;
    for (int size : {16, 24, 32, 48, 64, 128, 256}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        paint(painter, QRectF(0, 0, size, size), face);
        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

} // namespace SmileyPainter
