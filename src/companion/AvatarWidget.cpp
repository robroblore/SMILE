#include "companion/AvatarWidget.h"

#include <QPainter>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <cmath>

#include "ui/Theme.h"

AvatarWidget::AvatarWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(200, 200);
    m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, &AvatarWidget::tick);
    m_clock.start();
    m_timer.start();
}

void AvatarWidget::setState(State state)
{
    m_state = state;
    update();
}

void AvatarWidget::setMood(const QString &mood)
{
    m_mood = mood;
}

SmileyPainter::Face AvatarWidget::target() const
{
    SmileyPainter::Face f;
    // Mood
    if (m_mood == QLatin1String("impressed")) {
        f.smile = 0.85;
        f.browRaise = 0.5;
    } else if (m_mood == QLatin1String("skeptical")) {
        f.smile = 0.1;
        f.worry = 0.35;
        f.lookX = 0.15;
    } else if (m_mood == QLatin1String("confused")) {
        f.smile = 0.15;
        f.worry = 0.6;
    } else if (m_mood == QLatin1String("concerned")) {
        f.smile = 0.05;
        f.worry = 0.75;
    } else if (m_mood == QLatin1String("neutral")) {
        f.smile = 0.35;
    } else { // friendly
        f.smile = 0.6;
    }

    const qreal t = m_clock.elapsed() / 1000.0;
    switch (m_state) {
    case State::Listening:
        // Attentive: looks at you, small nods while you talk
        f.lookY = m_userSpeaking ? 0.1 + 0.12 * std::sin(t * 3.2) : 0.0;
        f.browRaise = qMax(f.browRaise, 0.15);
        break;
    case State::Thinking:
        f.lookX = 0.55;
        f.lookY = -0.6;
        f.browRaise = qMax(f.browRaise, 0.35);
        f.smile = qMin<qreal>(f.smile, 0.35);
        break;
    case State::Speaking:
        f.mouthOpen = m_level ? qBound<qreal>(0.0, m_level() * 1.1, 1.0) : 0.0;
        f.smile = qMin<qreal>(f.smile, 0.5);
        break;
    case State::Idle:
        break;
    }
    return f;
}

void AvatarWidget::tick()
{
    SmileyPainter::Face t = target();
    const qint64 now = m_clock.elapsed();
    if (now >= m_nextBlink) {
        m_blinkUntil = now + 120;
        m_nextBlink = now + QRandomGenerator::global()->bounded(2200, 5200);
    }
    if (now < m_blinkUntil)
        t.eyeOpen = 0.0;
    m_face = SmileyPainter::approach(m_face, t, 0.16);
    if (now < m_blinkUntil)
        m_face.eyeOpen = 0.0;
    update();
}

void AvatarWidget::paintEvent(QPaintEvent *)
{
    const Theme::Colors &c = Theme::colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const qreal side = qMin(width(), height()) * 0.9;
    const QPointF centre(width() / 2.0, height() / 2.0);

    // Soft backdrop disc; it pulses gently while speaking
    const qreal t = m_clock.elapsed() / 1000.0;
    const qreal speakPulse = m_state == State::Speaking ? m_face.mouthOpen * 0.06 : 0.0;
    QRadialGradient halo(centre, side * (0.55 + speakPulse));
    QColor a = c.accent;
    a.setAlpha(c.dark ? 50 : 34);
    QColor b = c.accent;
    b.setAlpha(0);
    halo.setColorAt(0.0, a);
    halo.setColorAt(1.0, b);
    p.setPen(Qt::NoPen);
    p.setBrush(halo);
    p.drawEllipse(centre, side * (0.55 + speakPulse), side * (0.55 + speakPulse));

    // Face, bobbing very slightly
    const qreal faceSize = side * 0.72;
    const qreal bob = std::sin(t * 1.8) * side * 0.006;
    const QRectF faceRect(centre.x() - faceSize / 2, centre.y() - faceSize / 2 + bob, faceSize, faceSize);
    SmileyPainter::paint(p, faceRect, m_face);

    // Thinking dots
    if (m_state == State::Thinking) {
        for (int i = 0; i < 3; ++i) {
            const qreal phase = std::fmod(t * 2.0 + i * 0.33, 1.0);
            QColor dot = c.accent;
            dot.setAlphaF(0.35 + 0.65 * std::sin(phase * 3.14159265));
            p.setBrush(dot);
            p.drawEllipse(QPointF(faceRect.right() - faceSize * 0.05 + i * faceSize * 0.11, faceRect.top() + faceSize * 0.05),
                          faceSize * 0.035, faceSize * 0.035);
        }
    }
}
