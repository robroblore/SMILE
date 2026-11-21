#include "companion/CompanionWindow.h"

#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QWindow>

namespace {
constexpr int kWidth = 340;
constexpr int kHeight = 230;
constexpr int kFaceSize = 88;
constexpr int kMargin = 10;
constexpr int kMaxQueuedTips = 3;
constexpr int kBubblePadX = 14;
constexpr int kBubblePadY = 10;

QColor accentFor(TipSeverity severity)
{
    switch (severity) {
    case TipSeverity::Positive:
        return QColor(0x2E, 0x9E, 0x5B);
    case TipSeverity::Warning:
        return QColor(0xD9, 0x6A, 0x1E);
    case TipSeverity::Info:
        break;
    }
    return QColor(0x3B, 0x7D, 0xD8);
}
} // namespace

CompanionWindow::CompanionWindow(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                          | Qt::WindowDoesNotAcceptFocus)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_MacAlwaysShowToolWindow); // stay visible while another app is focused
    setFixedSize(kWidth, kHeight);
    setWindowTitle(tr("SMILE companion"));
    setToolTip(tr("Drag to move. Click to dismiss a tip. Double-click to open SMILE."));

    m_tipTimer.setSingleShot(true);
    connect(&m_tipTimer, &QTimer::timeout, this, &CompanionWindow::dismissTip);

    // A blink every few seconds makes the companion feel alive.
    m_blinkTimer.setInterval(4200);
    connect(&m_blinkTimer, &QTimer::timeout, this, [this] {
        m_eyesClosed = true;
        update();
        QTimer::singleShot(160, this, [this] {
            m_eyesClosed = false;
            update();
        });
    });
    m_blinkTimer.start();
}

void CompanionWindow::moveToDefaultPosition()
{
    const QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    const QRect area = screen->availableGeometry();
    move(area.right() - width() - 24, area.bottom() - height() - 24);
}

void CompanionWindow::showTip(const Tip &tip)
{
    m_queue.enqueue(tip);
    while (m_queue.size() > kMaxQueuedTips)
        m_queue.dequeue(); // drop stale tips rather than lag behind
    if (!m_current)
        showNextTip();
}

void CompanionWindow::dismissTip()
{
    m_current.reset();
    m_tipTimer.stop();
    showNextTip();
    update();
}

void CompanionWindow::setSessionActive(bool active)
{
    m_sessionActive = active;
    if (!active) {
        m_queue.clear();
        m_current.reset();
        m_tipTimer.stop();
    }
    update();
}

void CompanionWindow::showNextTip()
{
    if (m_queue.isEmpty())
        return;
    m_current = m_queue.dequeue();
    // Long enough to read, short enough not to linger.
    m_tipTimer.start(6000 + 45 * static_cast<int>(m_current->text.size()));
    update();
}

QRectF CompanionWindow::faceRect() const
{
    return QRectF(width() - kFaceSize - kMargin, height() - kFaceSize - kMargin, kFaceSize, kFaceSize);
}

QFont CompanionWindow::headerFont() const
{
    QFont f = font();
    f.setPointSizeF(f.pointSizeF() * 0.8);
    f.setBold(true);
    f.setLetterSpacing(QFont::PercentageSpacing, 108);
    return f;
}

QFont CompanionWindow::bodyFont() const
{
    QFont f = font();
    f.setPointSizeF(f.pointSizeF() * 1.1);
    return f;
}

QRectF CompanionWindow::bubbleRect() const
{
    // The bubble sits just above the face and grows upwards to fit the text.
    const qreal maxHeight = height() - kFaceSize - 3 * kMargin;
    const qreal width = this->width() - 2 * kMargin;
    qreal needed = 0;
    if (m_current) {
        const int textWidth = int(width) - 2 * kBubblePadX;
        const QRect body = QFontMetrics(bodyFont()).boundingRect(QRect(0, 0, textWidth, 10000),
                                                                 Qt::TextWordWrap, m_current->text);
        needed = QFontMetrics(headerFont()).height() + 4 + body.height() + 2 * kBubblePadY;
    }
    const qreal h = qBound<qreal>(56, needed, maxHeight);
    return QRectF(kMargin, kMargin + maxHeight - h, width, h);
}

SmileyPainter::Mood CompanionWindow::currentMood() const
{
    if (!m_current)
        return m_sessionActive ? SmileyPainter::Mood::Happy : SmileyPainter::Mood::Neutral;
    switch (m_current->severity) {
    case TipSeverity::Warning:
        return SmileyPainter::Mood::Concerned;
    case TipSeverity::Info:
        return SmileyPainter::Mood::Neutral;
    case TipSeverity::Positive:
        break;
    }
    return SmileyPainter::Mood::Happy;
}

void CompanionWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (m_current) {
        const QRectF bubble = bubbleRect();
        const QColor accent = accentFor(m_current->severity);

        // Bubble with a tail pointing down at the face
        QPainterPath path;
        path.addRoundedRect(bubble, 14, 14);
        const QRectF face = faceRect();
        QPainterPath tail;
        tail.moveTo(face.center().x() - 26, bubble.bottom() - 1);
        tail.lineTo(face.center().x() - 4, face.top() + 6);
        tail.lineTo(face.center().x() - 4, bubble.bottom() - 1);
        tail.closeSubpath();
        path = path.united(tail);

        p.setPen(QPen(accent, 2));
        p.setBrush(QColor(255, 255, 255, 245));
        p.drawPath(path);

        // Header: category (and "demo" for placeholder tips)
        QString header = categoryName(m_current->category).toUpper();
        if (m_current->source == QLatin1String("demo"))
            header += tr("  ·  DEMO");
        p.setFont(headerFont());
        p.setPen(accent);
        const QRectF inner = bubble.adjusted(kBubblePadX, kBubblePadY, -kBubblePadX, -kBubblePadY);
        p.drawText(inner, Qt::AlignLeft | Qt::AlignTop, header);

        p.setFont(bodyFont());
        p.setPen(QColor(0x22, 0x22, 0x22));
        const int headerHeight = QFontMetrics(headerFont()).height() + 4;
        p.drawText(inner.adjusted(0, headerHeight, 0, 0), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                   m_current->text);
    }

    if (!m_sessionActive && !m_current)
        p.setOpacity(0.85);
    SmileyPainter::paint(p, faceRect(), currentMood(), m_eyesClosed);
}

void CompanionWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_pressed = true;
    m_dragging = false;
    m_pressPos = event->globalPosition().toPoint();
    m_dragOffset = m_pressPos - frameGeometry().topLeft();
}

void CompanionWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_pressed || m_dragging)
        return;
    const QPoint global = event->globalPosition().toPoint();
    if ((global - m_pressPos).manhattanLength() < 6)
        return;

    m_dragging = true;
    // Let the window system move us (needed on Wayland); fall back to manual moves.
    if (windowHandle() && windowHandle()->startSystemMove()) {
        m_pressed = false;
        return;
    }
    move(global - m_dragOffset);
}

void CompanionWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const bool wasClick = m_pressed && !m_dragging;
    m_pressed = false;
    m_dragging = false;
    if (wasClick && m_current)
        dismissTip();
}

void CompanionWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (faceRect().contains(event->position()))
        emit openMainWindowRequested();
}

void CompanionWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QAction *dismiss = menu.addAction(tr("Dismiss tip"), this, &CompanionWindow::dismissTip);
    dismiss->setEnabled(m_current.has_value());
    menu.addAction(tr("Open SMILE"), this, &CompanionWindow::openMainWindowRequested);
    menu.addSeparator();
    menu.addAction(tr("Hide companion"), this, &QWidget::hide);
    menu.exec(event->globalPos());
}

void CompanionWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    emit visibilityChanged(true);
}

void CompanionWindow::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    emit visibilityChanged(false);
}
