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
#include "ui/Theme.h"

namespace {
constexpr int kWidth = 380;
constexpr int kHeight = 210;
constexpr int kDebugWidth = 460;   // room for the "why" line in debug mode
constexpr int kDebugHeight = 330;
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

    placeOnScreen();
    m_state = State::Entering;
    m_clock.restart();
    show();
    raise();

    m_appear.stop();
    m_appear.setDirection(QAbstractAnimation::Forward);
    m_appear.setDuration(520);
    m_appear.setEasingCurve(QEasingCurve::Linear);
    m_livePresentationPending = !preview;
    m_appear.start();
    m_frame.start();
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
    const QSize wanted = debug ? QSize(kDebugWidth, kDebugHeight) : QSize(kWidth, kHeight);
    if (size() != wanted) {
        const QPoint anchor = geometry().bottomRight();
        setFixedSize(wanted);
        if (isVisible())
            move(anchor - QPoint(wanted.width() - 1, wanted.height() - 1));
    }
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
    const int maxH = height() - kFace - 3 * kMargin;
    const int h = qMin(maxH, header.height() + 6 + textRect.height() + reasonHeight + 2 * kPadY);

    const QRectF face = faceRect();
    const qreal right = width() - kMargin;
    const qreal bottom = face.top() - 10;
    m_bubble = QRectF(right - w, bottom - h, w, h);

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
        m_frame.stop();
        m_blinkTimer.stop();
        m_current.reset();
        hide();
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
    if (!m_current)
        return;

    const Theme::Colors &c = Theme::colors();
    const qreal t = m_appear.currentValue().isValid() ? m_appear.currentValue().toReal() : 1.0;
    const qreal faceT = QEasingCurve(QEasingCurve::OutBack).valueForProgress(clamp01(t / 0.7));
    const qreal bubbleT = QEasingCurve(QEasingCurve::OutCubic).valueForProgress(
        clamp01((t - kBubbleStartProgress) / (1.0 - kBubbleStartProgress)));
    const qreal seconds = m_clock.isValid() ? m_clock.elapsed() / 1000.0 : 0.0;
    const qreal bob = std::sin(seconds * 2.4) * 2.0;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // ---- Face, popping up from its bottom centre
    const QRectF face = faceRect();
    const qreal scale = qMax<qreal>(0.0, faceT);
    p.setOpacity(clamp01(t * 2.0));

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
    if (bubbleT <= 0.0)
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
    if (click)
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
    menu.addAction(tr("Dismiss"), this, &CompanionWindow::dismiss);
    menu.addAction(tr("Snooze tips for 10 minutes"), this, [this] {
        m_snoozedUntil = QDeadlineTimer(10 * 60 * 1000);
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
