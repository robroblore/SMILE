#include "ui/Widgets.h"

#include <QEnterEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QVBoxLayout>

#include "companion/SmileyPainter.h"
#include "ui/Theme.h"

// ---------------------------------------------------------------- ToggleSwitch

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_anim.setDuration(140);
    m_anim.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_anim, &QVariantAnimation::valueChanged, this, [this] { update(); });
    connect(this, &QAbstractButton::toggled, this, [this](bool on) {
        m_anim.stop();
        m_anim.setStartValue(m_anim.currentValue().isValid() ? m_anim.currentValue() : QVariant(on ? 0.0 : 1.0));
        m_anim.setEndValue(on ? 1.0 : 0.0);
        m_anim.start();
    });
}

QSize ToggleSwitch::sizeHint() const
{
    return {40, 20};
}

void ToggleSwitch::enterEvent(QEnterEvent *)
{
    m_hover = true;
    update();
}

void ToggleSwitch::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    const Theme::Colors &c = Theme::colors();
    const qreal t = m_anim.state() == QAbstractAnimation::Running ? m_anim.currentValue().toReal()
                                                                   : (isChecked() ? 1.0 : 0.0);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF track = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal r = track.height() / 2;
    if (isChecked()) {
        p.setPen(Qt::NoPen);
        p.setBrush(isEnabled() ? (m_hover ? c.accentHover : c.accent) : c.track);
    } else {
        p.setPen(QPen(isEnabled() ? c.textSecondary : c.textDisabled, 1));
        p.setBrush(m_hover ? c.subtleHover : Qt::transparent);
    }
    p.drawRoundedRect(track, r, r);

    const qreal knob = (m_hover || isDown()) ? 14 : 12;
    const qreal left = track.left() + r;
    const qreal right = track.right() - r;
    const QPointF centre(left + (right - left) * t, track.center().y());
    p.setPen(Qt::NoPen);
    p.setBrush(isChecked() ? c.onAccent : (isEnabled() ? c.textSecondary : c.textDisabled));
    p.drawEllipse(centre, knob / 2, knob / 2);
}

// ----------------------------------------------------------------- CheckCircle

CheckCircle::CheckCircle(QWidget *parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QSize CheckCircle::sizeHint() const
{
    return {22, 22};
}

void CheckCircle::paintEvent(QPaintEvent *)
{
    const Theme::Colors &c = Theme::colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);

    if (isChecked()) {
        p.setPen(Qt::NoPen);
        p.setBrush(c.accent);
        p.drawEllipse(r);
        QPainterPath tick;
        tick.moveTo(r.left() + r.width() * 0.28, r.top() + r.height() * 0.52);
        tick.lineTo(r.left() + r.width() * 0.44, r.top() + r.height() * 0.68);
        tick.lineTo(r.left() + r.width() * 0.73, r.top() + r.height() * 0.36);
        p.setPen(QPen(c.onAccent, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(tick);
    } else {
        p.setPen(QPen(c.textSecondary, 1.2));
        p.setBrush(underMouse() ? c.subtleHover : Qt::transparent);
        p.drawEllipse(r);
    }
}

// ------------------------------------------------------------------- NavButton

NavButton::NavButton(Icons::Icon icon, const QString &text, QWidget *parent)
    : QAbstractButton(parent)
    , m_icon(icon)
{
    setText(text);
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QSize NavButton::sizeHint() const
{
    return {200, 38};
}

void NavButton::enterEvent(QEnterEvent *)
{
    m_hover = true;
    update();
}

void NavButton::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}

void NavButton::paintEvent(QPaintEvent *)
{
    const Theme::Colors &c = Theme::colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF bg = QRectF(rect()).adjusted(4, 2, -4, -2);
    if (isChecked() || m_hover) {
        p.setPen(Qt::NoPen);
        p.setBrush(isDown() ? c.subtlePressed : c.subtleHover);
        p.drawRoundedRect(bg, 5, 5);
    }
    if (isChecked()) {
        p.setBrush(c.accent);
        p.drawRoundedRect(QRectF(bg.left(), bg.center().y() - 8, 3, 16), 1.5, 1.5);
    }

    Icons::paint(p, QRectF(bg.left() + 14, bg.center().y() - 8, 16, 16), m_icon, c.text);

    QFont f = font();
    if (isChecked())
        f.setWeight(QFont::DemiBold);
    p.setFont(f);
    p.setPen(c.text);
    p.drawText(bg.adjusted(44, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, text());
}

// ----------------------------------------------------------------- SmileyBadge

SmileyBadge::SmileyBadge(int size, qreal smile, QWidget *parent)
    : QWidget(parent)
    , m_smile(smile)
{
    setFixedSize(size, size);
}

void SmileyBadge::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    SmileyPainter::Face face;
    face.smile = m_smile;
    SmileyPainter::paint(p, QRectF(rect()).adjusted(1, 1, -1, -1), face);
}

// --------------------------------------------------------------- CameraPreview

CameraPreview::CameraPreview(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(240, 160);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void CameraPreview::setFrame(const QImage &frame)
{
    m_frame = frame;
    update();
}

void CameraPreview::setMessage(const QString &message)
{
    m_message = message;
    if (!message.isEmpty())
        m_frame = QImage();
    update();
}

void CameraPreview::paintEvent(QPaintEvent *)
{
    const Theme::Colors &c = Theme::colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath clip;
    clip.addRoundedRect(r, 8, 8);
    p.setClipPath(clip);
    p.fillRect(r, c.dark ? QColor(0x1A, 0x1A, 0x1A) : QColor(0xE9, 0xE9, 0xE9));

    if (!m_frame.isNull()) {
        // Aspect-fill
        const QSizeF img = m_frame.size();
        const qreal scale = qMax(r.width() / img.width(), r.height() / img.height());
        const QSizeF drawn = img * scale;
        const QRectF target(r.center().x() - drawn.width() / 2, r.center().y() - drawn.height() / 2, drawn.width(),
                            drawn.height());
        p.drawImage(target, m_frame);
    } else {
        Icons::paint(p, QRectF(r.center().x() - 14, r.center().y() - 30, 28, 28), Icons::Icon::Camera,
                     c.textSecondary);
        p.setPen(c.textSecondary);
        p.drawText(r.adjusted(16, 10, -16, 0), Qt::AlignCenter | Qt::TextWordWrap,
                   m_message.isEmpty() ? tr("Camera is off") : m_message);
    }
    p.setClipping(false);
    p.setPen(QPen(c.cardBorder, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r, 8, 8);
}

// ------------------------------------------------------------------ LevelMeter

LevelMeter::LevelMeter(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(6);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void LevelMeter::setLevel(qreal level)
{
    m_level = qBound<qreal>(0.0, level, 1.0);
    update();
}

void LevelMeter::setSpeaking(bool speaking)
{
    m_speaking = speaking;
    update();
}

void LevelMeter::paintEvent(QPaintEvent *)
{
    const Theme::Colors &c = Theme::colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    const QRectF r = QRectF(rect());
    p.setBrush(c.track);
    p.drawRoundedRect(r, 3, 3);
    p.setBrush(m_speaking ? c.success : c.accent);
    p.drawRoundedRect(QRectF(r.left(), r.top(), r.width() * m_level, r.height()), 3, 3);
}

// ------------------------------------------------------------------------ Card

Card::Card(QWidget *parent)
    : QFrame(parent)
    , m_body(new QVBoxLayout(this))
{
    setObjectName(QStringLiteral("card"));
    m_body->setContentsMargins(20, 18, 20, 18);
    m_body->setSpacing(10);
}

Card::Card(const QString &title, QWidget *parent)
    : Card(parent)
{
    m_body->addWidget(Ui::label(title, "cardTitle", this));
    m_body->addSpacing(2);
}

// ------------------------------------------------------------------- OptionRow

OptionRow::OptionRow(const QString &title, const QString &description, QAbstractButton *control,
                     bool controlOnLeft, QWidget *parent)
    : QWidget(parent)
    , m_control(control)
{
    setCursor(Qt::PointingHandCursor);
    auto *text = new QVBoxLayout;
    text->setSpacing(1);
    text->addWidget(Ui::label(title, nullptr, this));
    if (!description.isEmpty()) {
        QLabel *desc = Ui::label(description, "secondary", this);
        QFont f = desc->font();
        f.setPointSizeF(f.pointSizeF() * 0.92);
        desc->setFont(f);
        desc->setWordWrap(true);
        text->addWidget(desc);
    }

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(10, 8, 10, 8);
    row->setSpacing(14);
    control->setParent(this);
    if (controlOnLeft) {
        row->addWidget(control, 0, Qt::AlignVCenter);
        row->addLayout(text, 1);
    } else {
        row->addLayout(text, 1);
        row->addWidget(control, 0, Qt::AlignVCenter);
    }
}

void OptionRow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()) && m_control->isEnabled())
        m_control->toggle();
}

void OptionRow::enterEvent(QEnterEvent *)
{
    m_hover = true;
    update();
}

void OptionRow::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}

void OptionRow::paintEvent(QPaintEvent *)
{
    if (!m_hover)
        return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(Theme::colors().subtleHover);
    p.drawRoundedRect(QRectF(rect()), 6, 6);
}

// -------------------------------------------------------------------- helpers

namespace Ui {

QLabel *label(const QString &text, const char *objectName, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    if (objectName)
        l->setObjectName(QLatin1String(objectName));
    return l;
}

QWidget *pageHeader(const QString &title, const QString &subtitle, QWidget *parent)
{
    auto *w = new QWidget(parent);
    auto *layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 8);
    layout->setSpacing(4);
    layout->addWidget(label(title, "pageTitle", w));
    if (!subtitle.isEmpty()) {
        QLabel *sub = label(subtitle, "pageSubtitle", w);
        sub->setWordWrap(true);
        layout->addWidget(sub);
    }
    return w;
}

QScrollArea *scrollPage(QWidget *content)
{
    content->setObjectName(QStringLiteral("page"));
    content->setMaximumWidth(1000);
    auto *area = new QScrollArea;
    area->setObjectName(QStringLiteral("page"));
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    area->setWidget(content);
    return area;
}

QFrame *divider(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setFixedHeight(1);
    line->setStyleSheet(QStringLiteral("background: %1;").arg(Theme::css(Theme::colors().cardBorder)));
    return line;
}

} // namespace Ui
