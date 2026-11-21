#include "companion/CompanionWindow.h"

#include <QContextMenuEvent>
#include <QEasingCurve>
#include <QGuiApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QScreen>
#include <QSettings>
#include <QWindow>
#include <QtMath>

#include "core/AppSettings.h"
#include "core/TimeFormat.h"
#include "ui/Theme.h"

namespace {
constexpr int kWidth = 380;
constexpr int kHeight = 210;
constexpr int kDebugWidth = 460;   // room for the "why" line in debug mode
constexpr int kDebugHeight = 330;
constexpr int kTimerW = 222;   // section timer card
constexpr int kTimerH = 120;
constexpr int kTimerExtra = 50; // extra height when the timer row is taller than the face
constexpr int kFace = 76;
constexpr int kMargin = 16;   // room for shadows
constexpr int kPadX = 16;
constexpr int kPadY = 12;
constexpr int kStaleTipMs = 30000; // drop a waiting tip after this long
constexpr qreal kBubbleStartProgress = 0.3;
const char *kPosKey = "companion/anchor"; // bottom-right corner, so resizing keeps it in place

QColor accentFor(const Tip &tip)
{
    const Theme::Colors &c = Theme::colors();
    switch (tip.severity) {
    case TipSeverity::Positive:
        return c.success;
    case TipSeverity::Warning:
        return c.dark ? QColor(0xFF, 0xB9, 0x00) : QColor(0xD8, 0x6A, 0x00);
    case TipSeverity::Info:
        break;
    }
    return c.accent;
}

qreal clamp01(qreal v)
{
    return qBound<qreal>(0.0, v, 1.0);
}

QFont headerFont(const QFont &base)
{
    QFont f = base;
    f.setPointSizeF(base.pointSizeF() * 0.85);
    f.setWeight(QFont::DemiBold);
    return f;
}

QFont bodyFont(const QFont &base)
{
    QFont f = base;
    f.setPointSizeF(base.pointSizeF() * 1.08);
    return f;
}

QFont reasonFont(const QFont &base)
{
    QFont f = base;
    f.setPointSizeF(base.pointSizeF() * 0.85);
    f.setItalic(true);
    return f;
}
} // namespace

CompanionWindow::CompanionWindow(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                          | Qt::WindowDoesNotAcceptFocus)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_MacAlwaysShowToolWindow); // stay visible while the meeting app is focused
    setFixedSize(kWidth, kHeight);
    setWindowTitle(tr("SMILE"));
    setMouseTracking(true);

    m_appear.setStartValue(0.0);
    m_appear.setEndValue(1.0);
    connect(&m_appear, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        if (m_state == State::Entering && m_sessionActive && m_livePresentationPending
            && m_current && value.toReal() > kBubbleStartProgress) {
            m_livePresentationPending = false;
            emit liveTipPresented(*m_current);
        }
    });
    connect(&m_appear, &QVariantAnimation::finished, this, &CompanionWindow::onAppearFinished);

    m_frame.setInterval(16);
    connect(&m_frame, &QTimer::timeout, this, &CompanionWindow::onFrame);

    m_hold.setSingleShot(true);
    connect(&m_hold, &QTimer::timeout, this, &CompanionWindow::startLeaving);

    m_pendingTimer.setSingleShot(true);
    connect(&m_pendingTimer, &QTimer::timeout, this, [this] {
        if (m_pending && m_pendingAge.elapsed() < kStaleTipMs && m_state == State::Hidden) {
            const Tip tip = *m_pending;
            m_pending.reset();
            popUp(tip, false);
        } else {
            m_pending.reset();
        }
    });

    m_blinkTimer.setSingleShot(true);
    connect(&m_blinkTimer, &QTimer::timeout, this, [this] {
        m_face.eyeOpen = 0.0;
        m_target.eyeOpen = 0.0;
        QTimer::singleShot(110, this, [this] { m_target.eyeOpen = 1.0; });
        scheduleBlink();
    });

    // After glancing at its bubble, the companion looks back at the user.
    m_lookTimer.setSingleShot(true);
    connect(&m_lookTimer, &QTimer::timeout, this, [this] {
        m_target.lookX = 0.0;
        m_target.lookY = 0.0;
    });
}

// ----------------------------------------------------------------- public API

void CompanionWindow::showTip(const Tip &tip)
{
    if (m_snoozedUntil.isForever() || !m_snoozedUntil.hasExpired())
        return;

    const bool tooSoon = m_sinceHidden.isValid() && m_sinceHidden.elapsed() < m_gapMs;
    if (m_state != State::Hidden || tooSoon) {
        // Keep only the newest tip; older ones are no longer relevant.
        m_pending = tip;
        m_pendingAge.restart();
        if (m_state == State::Hidden)
            m_pendingTimer.start(int(m_gapMs - m_sinceHidden.elapsed()));
        return;
    }
    popUp(tip, false);
}

void CompanionWindow::previewTip(const Tip &tip)
{
    if (m_state == State::Hidden || m_state == State::Leaving) {
        m_appear.stop();
        m_state = State::Hidden;
        popUp(tip, true);
    } else {
        m_livePresentationPending = false;
        setCurrentTip(tip);
        m_hold.start();
    }
}

void CompanionWindow::dismiss()
{
    if (m_state == State::Entering || m_state == State::Shown)
        startLeaving();
}

void CompanionWindow::setSessionActive(bool active)
{
    m_sessionActive = active;
    if (!active) {
        setTimerVisible(false);
        m_pending.reset();
        m_pendingTimer.stop();
        m_sinceHidden.invalidate();
        m_snoozedUntil = QDeadlineTimer(0);
        dismiss();
    }
}

void CompanionWindow::resetPosition()
{
    QSettings().remove(QLatin1String(kPosKey));
    if (isVisible())
        placeOnScreen();
}

void CompanionWindow::setTimerVisible(bool visible)
{
    if (visible == m_docked)
        return;
    m_docked = visible;
    updateLayout();
    if (visible) {
        placeOnScreen();
        m_clock.restart();
        show();
        m_frame.start(33);
        scheduleBlink();
    } else if (m_state == State::Hidden) {
        m_frame.stop();
        hide();
    }
    updateMask();
    update();
}

void CompanionWindow::setTimerInfo(const SectionTimerInfo &info)
{
    m_timer = info;
    if (m_docked)
        update();
}

QRectF CompanionWindow::timerRect() const
{
    const QRectF face = faceRect();
    return QRectF(face.left() - 12 - kTimerW, height() - kMargin - kTimerH, kTimerW, kTimerH);
}

void CompanionWindow::updateLayout()
{
    const bool debug = m_showReason;
    QSize wanted = debug ? QSize(kDebugWidth, kDebugHeight) : QSize(kWidth, kHeight);
    if (m_docked)
        wanted.rheight() += kTimerExtra;
    if (size() != wanted) {
        const QPoint anchor = geometry().bottomRight();
        setFixedSize(wanted);
        if (isVisible())
            move(anchor - QPoint(wanted.width() - 1, wanted.height() - 1));
    }
}

void CompanionWindow::updateMask()
{
    // Only the drawn parts take mouse clicks; the transparent rest of the
    // window lets clicks through to Zoom, notes, etc.
    QRegion region(faceRect().adjusted(-6, -14, 6, 10).toAlignedRect());
    if (m_docked)
        region += timerRect().adjusted(-6, -6, 6, 10).toAlignedRect();
    if (m_current && m_state != State::Hidden)
        region += m_bubble.adjusted(-10, -10, 10, 16).toAlignedRect();
    setMask(region);
}

// ------------------------------------------------------------- show / hide

void CompanionWindow::popUp(const Tip &tip, bool preview)
{
    setCurrentTip(tip);

    // Land a little squashed and surprised, then settle into the tip's mood.
    m_face = m_target;
    m_face.squash = 0.8;
    m_face.smile = qMin<qreal>(m_target.smile, 0.3);
    m_face.lookX = -0.7;
    m_face.lookY = -0.6;
    m_target.squash = 0.0;
    m_target.lookX = -0.7;
    m_target.lookY = -0.6;
    m_lookTimer.start(1400);

    if (!m_docked) {
        placeOnScreen();
        m_clock.restart();
    }
    m_state = State::Entering;
    updateMask();
    show(); // no raise()/activate: SMILE must never take focus from the meeting

    m_appear.stop();
    m_appear.setDirection(QAbstractAnimation::Forward);
    m_appear.setDuration(520);
    m_appear.setEasingCurve(QEasingCurve::Linear);
    m_livePresentationPending = !preview;
    m_appear.start();
    m_frame.start(16);
    scheduleBlink();
}

void CompanionWindow::setCurrentTip(const Tip &tip)
{
    m_current = tip;
    m_target = faceFor(tip);
    m_target.lookX = m_face.lookX;
    m_target.lookY = m_face.lookY;

    // Size the window (bigger in debug mode, to show why) and the bubble to its text
    const bool debug = AppSettings::instance().debugMode() && !tip.reason.isEmpty();
    m_showReason = debug;
    updateLayout();
    const int maxBubble = width() - 2 * kMargin;
    const QFontMetrics header(headerFont(font()));
    const QFontMetrics body(bodyFont(font()));
    const int maxText = maxBubble - 2 * kPadX;
    const QRect textRect = body.boundingRect(QRect(0, 0, maxText, 1000), Qt::TextWordWrap, tip.text);
    const int headerWidth = header.horizontalAdvance(categoryName(tip.category)) + 16;
    int reasonHeight = 0;
    int reasonWidth = 0;
    if (debug) {
        const QRect r = QFontMetrics(reasonFont(font())).boundingRect(QRect(0, 0, maxText, 1000), Qt::TextWordWrap,
                                                                      tr("Why: ") + tip.reason);
        reasonHeight = r.height() + 8;
        reasonWidth = r.width();
    }
    const int w = qBound(170, qMax(qMax(textRect.width(), headerWidth), reasonWidth) + 2 * kPadX, maxBubble);
    const int bottomRow = m_docked ? qMax(kFace, kTimerH) : kFace;
    const int maxH = height() - bottomRow - 3 * kMargin;
    const int h = qMin(maxH, header.height() + 6 + textRect.height() + reasonHeight + 2 * kPadY);

    const QRectF face = faceRect();
    const qreal right = width() - kMargin;
    const qreal bottom = (m_docked ? qMin(face.top(), timerRect().top()) : face.top()) - 10;
    m_bubble = QRectF(right - w, bottom - h, w, h);
    updateMask();

    m_hold.setInterval(qMin(4500 + 45 * int(tip.text.size()) + (debug ? 30 * int(tip.reason.size()) : 0), 15000));
    update();
}

void CompanionWindow::startLeaving()
{
    if (m_state == State::Hidden || m_state == State::Leaving)
        return;
    m_hold.stop();
    m_livePresentationPending = false;
    m_state = State::Leaving;
    m_appear.stop();
    m_appear.setDirection(QAbstractAnimation::Backward);
    m_appear.setDuration(260);
    m_appear.start();
}

void CompanionWindow::onAppearFinished()
{
    if (m_state == State::Entering) {
        m_state = State::Shown;
        if (!m_hover)
            m_hold.start();
    } else if (m_state == State::Leaving) {
        m_state = State::Hidden;
        m_current.reset();
        updateMask();
        if (m_docked) {
            m_frame.start(33); // stay visible with the timer, calmer frame rate
        } else {
            m_frame.stop();
            m_blinkTimer.stop();
            hide();
        }
        m_sinceHidden.restart();
        if (m_pending)
            m_pendingTimer.start(m_gapMs);
    }
}

void CompanionWindow::placeOnScreen()
{
    m_placing = true;
    const QPoint anchor = QSettings().value(QLatin1String(kPosKey)).toPoint();
    QScreen *screen = anchor.isNull() ? nullptr : QGuiApplication::screenAt(anchor - QPoint(width() / 2, height() / 2));
    if (screen) {
        move(anchor - QPoint(width() - 1, height() - 1));
    } else if (QScreen *primary = QGuiApplication::primaryScreen()) {
        const QRect area = primary->availableGeometry();
        move(area.right() - width() - 8, area.bottom() - height() - 8);
    }
    m_placing = false;
}

void CompanionWindow::scheduleBlink()
{
    m_blinkTimer.start(QRandomGenerator::global()->bounded(2200, 5200));
}

SmileyPainter::Face CompanionWindow::faceFor(const Tip &tip) const
{
    SmileyPainter::Face f;
    if (tip.category == TipCategory::Expression && tip.severity != TipSeverity::Warning) {
        f.smile = 1.0; // show, don't tell: a big smile to encourage one back
        return f;
    }
    switch (tip.severity) {
    case TipSeverity::Positive:
        f.smile = 0.95;
        break;
    case TipSeverity::Info:
        f.smile = 0.4;
        f.worry = 0.15;
        break;
    case TipSeverity::Warning:
        f.smile = 0.0;
        f.worry = 0.85;
        break;
    }
    return f;
}

QRectF CompanionWindow::faceRect() const
{
    return QRectF(width() - kMargin - kFace, height() - kMargin - kFace, kFace, kFace);
}

// ------------------------------------------------------------------ painting

void CompanionWindow::onFrame()
{
    m_face = SmileyPainter::approach(m_face, m_target, 0.14);
    update();
}

void CompanionWindow::paintEvent(QPaintEvent *)
{
    if (!m_current && !m_docked)
        return;

    const Theme::Colors &c = Theme::colors();
    qreal t = m_appear.currentValue().isValid() ? m_appear.currentValue().toReal() : 1.0;
    if (!m_current)
        t = 0.0; // docked, no tip: no bubble
    const qreal faceT = m_docked ? 1.0 : QEasingCurve(QEasingCurve::OutBack).valueForProgress(clamp01(t / 0.7));
    const qreal bubbleT = QEasingCurve(QEasingCurve::OutCubic).valueForProgress(
        clamp01((t - kBubbleStartProgress) / (1.0 - kBubbleStartProgress)));
    const qreal seconds = m_clock.isValid() ? m_clock.elapsed() / 1000.0 : 0.0;
    const qreal bob = std::sin(seconds * 2.4) * 2.0;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (m_docked)
        paintTimer(p);

    // ---- Face, popping up from its bottom centre
    const QRectF face = faceRect();
    const qreal scale = qMax<qreal>(0.0, faceT);
    p.setOpacity(m_docked ? 1.0 : clamp01(t * 2.0));

    // Contact shadow
    QRadialGradient shadow(QPointF(face.center().x(), face.bottom() + 2), face.width() * 0.45);
    shadow.setColorAt(0.0, QColor(0, 0, 0, 70));
    shadow.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(shadow);
    p.drawEllipse(QPointF(face.center().x(), face.bottom() + 2), face.width() * 0.42 * scale, 6 * scale);

    p.save();
    p.translate(face.center().x(), face.bottom() + bob * scale);
    p.scale(scale, scale);
    SmileyPainter::paint(p, QRectF(-face.width() / 2, -face.height(), face.width(), face.height()), m_face);
    p.restore();

    // ---- Bubble
    if (bubbleT <= 0.0 || !m_current)
        return;
    p.setOpacity(bubbleT);
    p.save();
    p.translate(0, (1.0 - bubbleT) * 10);

    const QRectF b = m_bubble;
    QPainterPath shape;
    shape.addRoundedRect(b, 10, 10);
    QPainterPath tail;
    const qreal tx = face.center().x() - 6;
    tail.moveTo(tx - 12, b.bottom() - 1);
    tail.quadTo(tx, b.bottom() + 4, tx + 4, b.bottom() + 11);
    tail.quadTo(tx + 3, b.bottom() + 3, tx + 10, b.bottom() - 1);
    tail.closeSubpath();
    shape = shape.united(tail);

    // Soft layered shadow
    p.setPen(Qt::NoPen);
    for (int i = 8; i >= 1; --i) {
        p.setBrush(QColor(0, 0, 0, c.dark ? 9 : 4));
        p.drawRoundedRect(b.adjusted(-i, -i + 4, i, i + 4), 10 + i, 10 + i);
    }

    p.setPen(QPen(c.dark ? QColor(255, 255, 255, 22) : QColor(0, 0, 0, 22), 1));
    p.setBrush(c.flyout);
    p.drawPath(shape);

    // Header: coloured dot + category
    const QColor accent = accentFor(*m_current);
    const QFont hf = headerFont(font());
    const QFontMetrics hfm(hf);
    const QRectF inner = b.adjusted(kPadX, kPadY, -kPadX, -kPadY);
    p.setPen(Qt::NoPen);
    p.setBrush(accent);
    p.drawEllipse(QPointF(inner.left() + 4, inner.top() + hfm.height() / 2.0), 4, 4);
    p.setFont(hf);
    p.setPen(c.textSecondary);
    QString header = categoryName(m_current->category);
    if (m_current->source == QLatin1String("demo"))
        header += tr(" · demo");
    p.drawText(QRectF(inner.left() + 14, inner.top(), inner.width() - 14, hfm.height()),
               Qt::AlignLeft | Qt::AlignVCenter, header);

    // Body
    p.setFont(bodyFont(font()));
    p.setPen(c.text);
    const QRectF bodyRect = inner.adjusted(0, hfm.height() + 6, 0, 0);
    QRectF used;
    p.drawText(bodyRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, m_current->text, &used);
    if (m_showReason) {
        p.setFont(reasonFont(font()));
        p.setPen(c.textSecondary);
        p.drawText(bodyRect.adjusted(0, used.height() + 8, 0, 0), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                   tr("Why: ") + m_current->reason);
    }
    p.restore();
}

void CompanionWindow::paintTimer(QPainter &p)
{
    const Theme::Colors &c = Theme::colors();
    const QRectF r = timerRect();
    p.save();
    p.setPen(Qt::NoPen);
    for (int i = 6; i >= 1; --i) {
        p.setBrush(QColor(0, 0, 0, c.dark ? 9 : 4));
        p.drawRoundedRect(r.adjusted(-i, -i + 3, i, i + 3), 10 + i, 10 + i);
    }
    p.setPen(QPen(c.dark ? QColor(255, 255, 255, 22) : QColor(0, 0, 0, 22), 1));
    p.setBrush(c.flyout);
    p.drawRoundedRect(r, 10, 10);

    const QRectF in = r.adjusted(12, 9, -12, -10);
    QFont small = font();
    small.setPointSizeF(font().pointSizeF() * 0.8);
    QFont bold = small;
    bold.setWeight(QFont::DemiBold);
    const QFontMetricsF fm(small);
    qreal y = in.top();

    // SMILE · status
    p.setFont(bold);
    p.setPen(c.text);
    p.drawText(QRectF(in.left(), y, in.width(), fm.height()), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("SMILE"));
    p.setFont(small);
    p.setPen(c.textSecondary);
    const qreal smileW = QFontMetricsF(bold).horizontalAdvance(QStringLiteral("SMILE  "));
    p.setPen(c.success);
    p.setBrush(c.success);
    p.drawEllipse(QPointF(in.left() + smileW + 3, y + fm.height() / 2), 3, 3);
    p.setPen(c.textSecondary);
    p.drawText(QRectF(in.left() + smileW + 10, y, in.width() - smileW - 10, fm.height()), Qt::AlignLeft | Qt::AlignVCenter,
               fm.elidedText(m_timer.status, Qt::ElideRight, in.width() - smileW - 10));
    y += fm.height() + 4;

    // Status pill
    QString pill;
    QColor tone = c.success;
    const bool over = m_timer.targetSeconds > 0 && m_timer.elapsedSeconds > m_timer.targetSeconds;
    if (over) {
        pill = tr("Over by %1").arg(formatDuration(m_timer.elapsedSeconds - m_timer.targetSeconds));
        tone = m_timer.elapsedSeconds > m_timer.targetSeconds * 1.3 ? c.critical : c.orange;
    } else if (m_timer.scheduleDeltaSeconds > 15) {
        pill = tr("Behind by %1").arg(formatDuration(m_timer.scheduleDeltaSeconds));
        tone = c.orange;
    } else if (m_timer.scheduleDeltaSeconds < -15) {
        pill = tr("Ahead by %1").arg(formatDuration(-m_timer.scheduleDeltaSeconds));
        tone = c.accent;
    } else {
        pill = tr("On track");
    }
    p.setFont(bold);
    const qreal pw = QFontMetricsF(bold).horizontalAdvance(pill) + 16;
    QColor bg = tone;
    bg.setAlpha(c.dark ? 55 : 35);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    const QRectF pillRect(in.left(), y, pw, fm.height() + 4);
    p.drawRoundedRect(pillRect, pillRect.height() / 2, pillRect.height() / 2);
    p.setPen(tone);
    p.drawText(pillRect, Qt::AlignCenter, pill);
    y += pillRect.height() + 5;

    // Section
    p.setFont(small);
    p.setPen(c.textSecondary);
    const QString section = m_timer.count > 1 ? tr("Section %1/%2: %3").arg(m_timer.index + 1).arg(m_timer.count).arg(m_timer.section)
                                              : m_timer.section;
    p.drawText(QRectF(in.left(), y, in.width(), fm.height()), Qt::AlignLeft | Qt::AlignVCenter,
               fm.elidedText(section, Qt::ElideRight, in.width()));
    y += fm.height() + 2;

    // Big time + target
    QFont big = font();
    big.setPointSizeF(font().pointSizeF() * 1.7);
    big.setWeight(QFont::Bold);
    p.setFont(big);
    p.setPen(over ? tone : c.text);
    const QRectF timeRect(in.left(), y, in.width() * 0.55, in.bottom() - y - 5);
    p.drawText(timeRect, Qt::AlignLeft | Qt::AlignVCenter, formatDuration(m_timer.elapsedSeconds));
    p.setFont(small);
    p.setPen(c.textSecondary);
    if (m_timer.targetSeconds > 0)
        p.drawText(QRectF(in.left() + in.width() * 0.5, y, in.width() * 0.5, timeRect.height()),
                   Qt::AlignRight | Qt::AlignVCenter, tr("Target %1").arg(formatDuration(m_timer.targetSeconds)));

    // Progress line
    if (m_timer.targetSeconds > 0) {
        const QRectF track(in.left(), in.bottom() - 3, in.width(), 3);
        p.setPen(Qt::NoPen);
        p.setBrush(c.track);
        p.drawRoundedRect(track, 1.5, 1.5);
        p.setBrush(over ? tone : c.accent);
        const qreal frac = qMin(1.0, double(m_timer.elapsedSeconds) / m_timer.targetSeconds);
        p.drawRoundedRect(QRectF(track.left(), track.top(), track.width() * frac, track.height()), 1.5, 1.5);
    }
    p.restore();
}

// --------------------------------------------------------------- interaction

void CompanionWindow::enterEvent(QEnterEvent *)
{
    m_hover = true;
    m_hold.stop(); // stay while the user is reading
    m_target.lookX = -0.2;
    m_target.lookY = 0.3;
}

void CompanionWindow::leaveEvent(QEvent *)
{
    m_hover = false;
    m_target.lookX = 0.0;
    m_target.lookY = 0.0;
    if (m_state == State::Shown)
        m_hold.start(2000);
}

void CompanionWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_pressed = true;
    m_dragged = false;
    m_pressPos = event->globalPosition().toPoint();
}

void CompanionWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_pressed || m_dragged)
        return;
    if ((event->globalPosition().toPoint() - m_pressPos).manhattanLength() < 6)
        return;
    m_dragged = true;
    m_pressed = false;
    if (windowHandle())
        windowHandle()->startSystemMove(); // works on Wayland, X11, Windows and macOS
}

void CompanionWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const bool click = m_pressed && !m_dragged;
    m_pressed = false;
    if (click && m_current)
        dismiss();
}

void CompanionWindow::mouseDoubleClickEvent(QMouseEvent *)
{
    emit openMainWindowRequested();
    dismiss();
}

void CompanionWindow::contextMenuEvent(QContextMenuEvent *event)
{
    m_hold.stop();
    QMenu menu(this);
    menu.addAction(tr("Dismiss"), this, &CompanionWindow::dismiss)->setEnabled(m_current.has_value());
    menu.addAction(tr("Snooze tips for 10 minutes"), this, [this] {
        m_snoozedUntil = QDeadlineTimer(10 * 60 * 1000);
        emit liveTipsSnoozed();
        m_pending.reset();
        dismiss();
    });
    menu.addSeparator();
    QAction *next = menu.addAction(tr("Next point"), this, &CompanionWindow::nextPointRequested);
    QAction *end = menu.addAction(tr("End session"), this, &CompanionWindow::endSessionRequested);
    next->setEnabled(m_sessionActive);
    end->setEnabled(m_sessionActive);
    menu.addSeparator();
    menu.addAction(tr("Open SMILE"), this, &CompanionWindow::openMainWindowRequested);
    if (AppSettings::instance().debugMode())
        menu.addAction(tr("Open debug view"), this, &CompanionWindow::debugViewRequested);
    menu.exec(event->globalPos());
    if (m_state == State::Shown && !m_hover)
        m_hold.start(2000);
}

void CompanionWindow::moveEvent(QMoveEvent *event)
{
    QWidget::moveEvent(event);
    if (!m_placing && isVisible())
        QSettings().setValue(QLatin1String(kPosKey), geometry().bottomRight());
}
