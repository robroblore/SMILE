#include "ui/Widgets.h"

#include <QEnterEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
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

void CameraPreview::setFace(const FaceFrame &face, bool looking)
{
    m_face = face;
    m_looking = looking;
    if (m_overlay)
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
        const QSizeF img = m_frame.size();
        const qreal horizontal = r.width() / img.width();
        const qreal vertical = r.height() / img.height();
        const qreal scale = m_showFullFrame ? qMin(horizontal, vertical) : qMax(horizontal, vertical);
        const QSizeF drawn = img * scale;
        const QRectF target(r.center().x() - drawn.width() / 2, r.center().y() - drawn.height() / 2, drawn.width(),
                            drawn.height());
        p.drawImage(target, m_frame);

        auto map = [&target](const QPointF &n) {
            return QPointF(target.left() + n.x() * target.width(), target.top() + n.y() * target.height());
        };
        // Objects flagged by the cloud AI: always shown, they are things to fix
        for (const Marked &m : std::as_const(m_marked)) {
            const QRectF r(map(m.box.topLeft()), map(m.box.bottomRight()));
            p.setPen(QPen(c.critical, 2, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRect(r);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 150));
            const QRectF tag(r.left(), r.top() - 18, QFontMetrics(font()).horizontalAdvance(m.label) + 10, 18);
            p.drawRect(tag);
            p.setPen(Qt::white);
            p.drawText(tag, Qt::AlignCenter, m.label);
        }
        if (m_overlay) {
            if (m_face.found) {
                const QColor tone = m_looking ? QColor(0x4C, 0xD9, 0x64) : QColor(0xFF, 0xA9, 0x4D);
                const QRectF box(map(m_face.box.topLeft()), map(m_face.box.bottomRight()));
                // Corner brackets rather than a full box: less clutter
                p.setPen(QPen(tone, 2.5, Qt::SolidLine, Qt::RoundCap));
                p.setBrush(Qt::NoBrush);
                const qreal k = qMin(box.width(), box.height()) * 0.22;
                const QPointF corners[4] = {box.topLeft(), box.topRight(), box.bottomRight(), box.bottomLeft()};
                const QPointF dirs[4] = {{1, 1}, {-1, 1}, {-1, -1}, {1, -1}};
                for (int i = 0; i < 4; ++i) {
                    p.drawLine(corners[i], corners[i] + QPointF(dirs[i].x() * k, 0));
                    p.drawLine(corners[i], corners[i] + QPointF(0, dirs[i].y() * k));
                }
                // Landmarks: eyes, nose, mouth corners
                const QPointF eyeA = map(m_face.landmarks[0]);
                const QPointF eyeB = map(m_face.landmarks[1]);
                const QPointF nose = map(m_face.landmarks[2]);
                const QPointF mouthA = map(m_face.landmarks[3]);
                const QPointF mouthB = map(m_face.landmarks[4]);
                p.setPen(QPen(QColor(255, 255, 255, 140), 1));
                p.drawLine(eyeA, eyeB);
                p.drawLine(mouthA, mouthB);
                p.drawLine((eyeA + eyeB) / 2, nose);
                const QColor colours[5] = {QColor(0x4C, 0xC2, 0xFF), QColor(0x4C, 0xC2, 0xFF), QColor(0xFF, 0xD2, 0x3C),
                                           QColor(0xFF, 0x6B, 0xC8), QColor(0xFF, 0x6B, 0xC8)};
                const QPointF pts[5] = {eyeA, eyeB, nose, mouthA, mouthB};
                for (int i = 0; i < 5; ++i) {
                    p.setPen(QPen(Qt::black, 1));
                    p.setBrush(colours[i]);
                    p.drawEllipse(pts[i], 4, 4);
                }
                // Head direction arrow from the nose
                const qreal len = QLineF(eyeA, eyeB).length() * 1.5;
                const QPointF tip = nose + QPointF(m_face.yaw * len, (m_face.pitch - 0.6) * len);
                p.setPen(QPen(tone, 2, Qt::SolidLine, Qt::RoundCap));
                p.drawLine(nose, tip);
                // Numbers
                const QString info = QStringLiteral("yaw %1  pitch %2  mouth %3  %4 ms")
                                         .arg(m_face.yaw, 0, 'f', 2)
                                         .arg(m_face.pitch, 0, 'f', 2)
                                         .arg(m_face.mouthRatio, 0, 'f', 2)
                                         .arg(m_face.detectMs, 0, 'f', 0);
                QFont f = font();
                f.setPointSizeF(f.pointSizeF() * 0.85);
                p.setFont(f);
                const QRectF tag(box.left(), box.bottom() + 4, QFontMetrics(f).horizontalAdvance(info) + 12, 20);
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(0, 0, 0, 160));
                p.drawRoundedRect(tag, 4, 4);
                p.setPen(Qt::white);
                p.drawText(tag, Qt::AlignCenter, info);
            } else if (m_face.timestampMs > 0) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(0, 0, 0, 150));
                const QRectF tag(r.left() + 10, r.top() + 10, 150, 22);
                p.drawRoundedRect(tag, 4, 4);
                p.setPen(QColor(0xFF, 0xA9, 0x4D));
                p.drawText(tag, Qt::AlignCenter, tr("No face detected"));
            }
        }
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

// -------------------------------------------------------------------- Expander

namespace {
class ScoreBar : public QWidget
{
public:
    ScoreBar(int score, const QColor &colour, QWidget *parent)
        : QWidget(parent)
        , m_score(score)
        , m_colour(colour)
    {
        setFixedHeight(8);
        setMinimumWidth(80);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(Theme::colors().track);
        p.drawRoundedRect(QRectF(rect()), 4, 4);
        if (m_score >= 0) {
            p.setBrush(m_colour);
            p.drawRoundedRect(QRectF(0, 0, width() * qBound(0, m_score, 100) / 100.0, height()), 4, 4);
        }
    }

private:
    int m_score;
    QColor m_colour;
};
} // namespace

Expander::Expander(const QString &title, const QString &subtitle, int score, const QColor &colour,
                   const QString &valueText, QWidget *parent)
    : QFrame(parent)
    , m_header(new QWidget(this))
    , m_content(new QWidget(this))
    , m_body(new QVBoxLayout(m_content))
    , m_chevron(new QLabel(this))
{
    setObjectName(QStringLiteral("card"));
    Theme::addShadow(this);
    m_header->setCursor(Qt::PointingHandCursor);
    m_header->installEventFilter(this);

    auto *titles = new QVBoxLayout;
    titles->setSpacing(1);
    QLabel *t = Ui::label(title, nullptr, m_header);
    QFont f = t->font();
    f.setPointSizeF(f.pointSizeF() * 1.12);
    f.setWeight(QFont::DemiBold);
    t->setFont(f);
    titles->addWidget(t);
    if (!subtitle.isEmpty())
        titles->addWidget(Ui::label(subtitle, "secondary", m_header));

    QLabel *value = Ui::label(valueText, nullptr, m_header);
    QFont vf = value->font();
    vf.setPointSizeF(vf.pointSizeF() * 1.45);
    vf.setWeight(QFont::Bold);
    value->setFont(vf);
    value->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::css(colour)));
    value->setMinimumWidth(64);
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_chevron->setPixmap(Icons::icon(Icons::Icon::ChevronRight, Theme::colors().textSecondary, 16).pixmap(16, 16));

    auto *h = new QHBoxLayout(m_header);
    h->setContentsMargins(20, 14, 16, 14);
    h->setSpacing(16);
    h->addLayout(titles, 3);
    if (score >= 0 || !valueText.isEmpty()) {
        h->addWidget(new ScoreBar(score, colour, m_header), 2);
        h->addWidget(value);
    } else {
        value->hide();
    }
    h->addWidget(m_chevron);

    m_body->setContentsMargins(20, 0, 20, 16);
    m_body->setSpacing(8);
    m_content->hide();

    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    v->addWidget(m_header);
    v->addWidget(m_content);
}

void Expander::setExpanded(bool expanded)
{
    m_content->setVisible(expanded);
    m_chevron->setPixmap(Icons::icon(expanded ? Icons::Icon::ChevronDown : Icons::Icon::ChevronRight,
                                     Theme::colors().textSecondary, 16)
                             .pixmap(16, 16));
}

bool Expander::isExpanded() const
{
    return m_content->isVisible();
}

bool Expander::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_header && event->type() == QEvent::MouseButtonRelease) {
        setExpanded(!isExpanded());
        return true;
    }
    return QFrame::eventFilter(watched, event);
}

// ------------------------------------------------------------------- ScoreRing

ScoreRing::ScoreRing(int score, const QColor &colour, int size, const QString &caption, QWidget *parent)
    : QWidget(parent)
    , m_score(score)
    , m_colour(colour)
    , m_caption(caption)
{
    setFixedSize(size, size);
}

void ScoreRing::paintEvent(QPaintEvent *)
{
    const Theme::Colors &c = Theme::colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal stroke = width() * 0.075;
    const QRectF r = QRectF(rect()).adjusted(stroke, stroke, -stroke, -stroke);
    p.setPen(QPen(c.track, stroke, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(r, 0, 360 * 16);
    p.setPen(QPen(m_colour, stroke, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(r, 90 * 16, -int(360 * 16 * qBound(0, m_score, 100) / 100.0));

    QFont big = font();
    big.setPixelSize(int(width() * 0.3));
    big.setWeight(QFont::Bold);
    p.setFont(big);
    p.setPen(c.text);
    const QRectF numberRect(r.left(), r.top(), r.width(), r.height() * (m_caption.isEmpty() ? 1.0 : 0.86));
    p.drawText(numberRect, Qt::AlignCenter, m_score >= 0 ? QString::number(m_score) : QStringLiteral("–"));
    if (!m_caption.isEmpty()) {
        QFont small = font();
        small.setPixelSize(int(width() * 0.085));
        p.setFont(small);
        p.setPen(c.textSecondary);
        p.drawText(QRectF(r.left(), r.center().y() + r.height() * 0.16, r.width(), r.height() * 0.2), Qt::AlignHCenter | Qt::AlignTop,
                   m_caption);
    }
}

// ---------------------------------------------------------------- DurationEdit

DurationEdit::DurationEdit(QWidget *parent)
    : QLineEdit(parent)
{
    setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    setToolTip(tr("Type minutes (5), minutes and seconds (5:30), or 90s / 2m 30s."));
    connect(this, &QLineEdit::editingFinished, this, &DurationEdit::commit);
    setText(format(0, m_allowZero));
}

QSize DurationEdit::sizeHint() const
{
    const int w = fontMetrics().horizontalAdvance(QStringLiteral("00:00 min")) + 36;
    return {qMax(110, w), QLineEdit::sizeHint().height()};
}

int DurationEdit::parse(const QString &input, bool *ok)
{
    *ok = true;
    QString t = input.trimmed().toLower();
    t.remove(QStringLiteral("min"));
    t = t.trimmed();
    if (t.isEmpty() || t == QLatin1String("no limit"))
        return 0;
    static const QRegularExpression clock(QStringLiteral("^(\\d+):(\\d{1,2})$"));
    static const QRegularExpression parts(QStringLiteral("(\\d+(?:[.,]\\d+)?)\\s*(h|m|s)?"));
    const auto c = clock.match(t);
    if (c.hasMatch())
        return c.captured(1).toInt() * 60 + c.captured(2).toInt();
    double total = 0;
    int consumed = 0;
    auto it = parts.globalMatch(t);
    while (it.hasNext()) {
        const auto m = it.next();
        const double n = m.captured(1).replace(QLatin1Char(','), QLatin1Char('.')).toDouble();
        const QString unit = m.captured(2);
        total += unit == QLatin1String("h") ? n * 3600 : unit == QLatin1String("s") ? n : n * 60; // bare number = minutes
        consumed += int(m.capturedLength());
    }
    QString rest = t;
    rest.remove(QRegularExpression(QStringLiteral("[\\d.,hms\\s]")));
    if (consumed == 0 || !rest.isEmpty()) {
        *ok = false;
        return 0;
    }
    return int(qRound(total));
}

QString DurationEdit::format(int seconds, bool allowZero)
{
    if (seconds <= 0)
        return allowZero ? tr("No limit") : tr("0 min");
    if (seconds % 60 == 0)
        return tr("%1 min").arg(seconds / 60);
    return tr("%1:%2 min").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

void DurationEdit::setSeconds(int seconds)
{
    m_seconds = qMax(0, seconds);
    setText(format(m_seconds, m_allowZero));
}

void DurationEdit::focusInEvent(QFocusEvent *event)
{
    QLineEdit::focusInEvent(event);
    // Edit the plain value ("5:30"), not the label
    setText(m_seconds % 60 ? QStringLiteral("%1:%2").arg(m_seconds / 60).arg(m_seconds % 60, 2, 10, QLatin1Char('0'))
                           : QString::number(m_seconds / 60));
    selectAll();
}

void DurationEdit::commit()
{
    bool ok = false;
    const int value = parse(text(), &ok);
    if (!ok || (value == 0 && !m_allowZero)) {
        setText(format(m_seconds, m_allowZero)); // keep the old value
        return;
    }
    const bool changed = value != m_seconds;
    m_seconds = qMin(value, 6 * 3600);
    setText(format(m_seconds, m_allowZero));
    if (changed)
        emit secondsChanged(m_seconds);
}

// ------------------------------------------------------------------------ Card

Card::Card(QWidget *parent)
    : QFrame(parent)
    , m_body(new QVBoxLayout(this))
{
    setObjectName(QStringLiteral("card"));
    Theme::addShadow(this);
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

OptionRow::OptionRow(const QString &title, const QString &description, QWidget *control, QWidget *parent)
    : OptionRow(title, description, static_cast<QAbstractButton *>(nullptr), false, parent)
{
    setCursor(Qt::ArrowCursor);
    control->setParent(this);
    static_cast<QHBoxLayout *>(layout())->addWidget(control, 0, Qt::AlignVCenter);
}

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
    if (!control) {
        row->addLayout(text, 1);
    } else if (controlOnLeft) {
        control->setParent(this);
        row->addWidget(control, 0, Qt::AlignVCenter);
        row->addLayout(text, 1);
    } else {
        control->setParent(this);
        row->addLayout(text, 1);
        row->addWidget(control, 0, Qt::AlignVCenter);
    }
}

void OptionRow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()) && m_control && m_control->isEnabled())
        m_control->toggle();
}

void OptionRow::enterEvent(QEnterEvent *)
{
    m_hover = m_control != nullptr;
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
