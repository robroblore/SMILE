#include "app/pages/HomePage.h"

#include <QAbstractButton>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QScrollArea>
#include <QTime>
#include <QVBoxLayout>

#include "app/SessionHistory.h"
#include "core/AppSettings.h"
#include "core/Rating.h"
#include "core/TimeFormat.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

// Scores of the last sessions as a small line chart
class Sparkline : public QWidget
{
public:
    explicit Sparkline(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(160, 56);
    }
    void setValues(const QList<int> &values)
    {
        m_values = values;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const Theme::Colors &c = Theme::colors();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(4, 6, -4, -6);
        if (m_values.size() < 2) {
            p.setPen(c.textSecondary);
            p.drawText(rect(), Qt::AlignCenter, HomePage::tr("Your scores will appear here"));
            return;
        }
        auto pt = [&](int i) {
            const qreal x = r.left() + r.width() * i / qreal(m_values.size() - 1);
            const qreal y = r.bottom() - r.height() * qBound(0, m_values[i], 100) / 100.0;
            return QPointF(x, y);
        };
        QPainterPath line;
        line.moveTo(pt(0));
        for (int i = 1; i < m_values.size(); ++i)
            line.lineTo(pt(i));
        QPainterPath area = line;
        area.lineTo(r.right(), r.bottom());
        area.lineTo(r.left(), r.bottom());
        area.closeSubpath();
        QColor fill = c.accent;
        fill.setAlpha(c.dark ? 50 : 30);
        p.fillPath(area, fill);
        p.setPen(QPen(c.accent, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(line);
        const QPointF last = pt(m_values.size() - 1);
        p.setPen(Qt::NoPen);
        p.setBrush(c.accent);
        p.drawEllipse(last, 4, 4);
    }

private:
    QList<int> m_values;
};

namespace {
// A big, friendly button: coloured icon disc, title and one short line
class Tile : public QAbstractButton
{
public:
    Tile(Icons::Icon icon, const QColor &tone, const QString &title, const QString &subtitle, QWidget *parent = nullptr)
        : QAbstractButton(parent)
        , m_icon(icon)
        , m_tone(tone)
        , m_subtitle(subtitle)
    {
        setText(title);
        setCursor(Qt::PointingHandCursor);
        setMinimumSize(200, 150);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        setAttribute(Qt::WA_Hover);
        setAccessibleName(title);
        setAccessibleDescription(subtitle);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const Theme::Colors &c = Theme::colors();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
        const bool hover = underMouse();
        p.setPen(QPen(hover ? m_tone : c.cardBorder, hover ? 2 : 1));
        p.setBrush(isDown() ? c.subtlePressed : c.card);
        p.drawRoundedRect(r, 14, 14);

        // Icon disc
        const qreal d = 54;
        const QRectF disc(r.left() + 20, r.top() + 20, d, d);
        QColor bg = m_tone;
        bg.setAlpha(c.dark ? 60 : 38);
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawEllipse(disc);
        Icons::paint(p, disc.adjusted(14, 14, -14, -14), m_icon, m_tone);

        QFont f = font();
        f.setPointSizeF(f.pointSizeF() * 1.35);
        f.setWeight(QFont::DemiBold);
        p.setFont(f);
        p.setPen(c.text);
        const QRectF titleRect(r.left() + 20, disc.bottom() + 12, r.width() - 40, QFontMetricsF(f).height() + 2);
        p.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, text());
        p.setFont(font());
        p.setPen(c.textSecondary);
        p.drawText(QRectF(r.left() + 20, titleRect.bottom() + 2, r.width() - 40, r.bottom() - titleRect.bottom() - 12),
                   Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, m_subtitle);

        // Arrow on hover
        if (hover)
            Icons::paint(p, QRectF(r.right() - 34, r.top() + 22, 16, 16), Icons::Icon::Next, m_tone);
    }
    void enterEvent(QEnterEvent *e) override
    {
        update();
        QAbstractButton::enterEvent(e);
    }
    void leaveEvent(QEvent *e) override
    {
        update();
        QAbstractButton::leaveEvent(e);
    }

private:
    Icons::Icon m_icon;
    QColor m_tone;
    QString m_subtitle;
};

QLabel *stat(const QString &value)
{
    QLabel *l = Ui::label(value, "bigValue");
    return l;
}

QWidget *statBlock(QLabel *value, const QString &caption)
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    v->addWidget(Ui::label(caption.toUpper(), "caption"));
    v->addWidget(value);
    return w;
}
} // namespace

HomePage::HomePage(QWidget *parent)
    : QWidget(parent)
    , m_greeting(Ui::label(QString(), "pageTitle"))
    , m_level(Ui::label(QString(), "cardTitle"))
    , m_xp(new QProgressBar)
    , m_xpText(Ui::label(QString(), "caption"))
    , m_streak(stat(QString()))
    , m_best(stat(QString()))
    , m_last(stat(QString()))
    , m_spark(new Sparkline)
    , m_badges(new QGridLayout)
    , m_recent(new QVBoxLayout)
{
    const Theme::Colors &c = Theme::colors();

    // ---- Greeting
    auto *hello = new QHBoxLayout;
    hello->setSpacing(14);
    hello->addWidget(new SmileyBadge(48, 0.9));
    auto *helloText = new QVBoxLayout;
    helloText->setSpacing(0);
    helloText->addWidget(m_greeting);
    helloText->addWidget(Ui::label(tr("What would you like to do?"), "secondary"));
    hello->addLayout(helloText, 1);

    // ---- The four things you do
    auto *tiles = new QGridLayout;
    tiles->setSpacing(12);
    auto *train = new Tile(Icons::Icon::Training, c.accent, tr("Practise"), tr("Rehearse your talk or a meeting with SMILE."));
    auto *check = new Tile(Icons::Icon::PreMeet, QColor(0x8E, 0x5B, 0xE8), tr("Check my setup"),
                           tr("Camera, light and sound in one minute."));
    auto *meet = new Tile(Icons::Icon::Meet, c.success, tr("Start a meeting"), tr("Quiet tips during the real call."));
    auto *results = new Tile(Icons::Icon::PostMeet, c.orange, tr("My results"), tr("Your last score and what to work on."));
    tiles->addWidget(train, 0, 0);
    tiles->addWidget(check, 0, 1);
    tiles->addWidget(meet, 1, 0);
    tiles->addWidget(results, 1, 1);
    connect(train, &QAbstractButton::clicked, this, &HomePage::openTraining);
    connect(check, &QAbstractButton::clicked, this, &HomePage::openSetupCheck);
    connect(meet, &QAbstractButton::clicked, this, &HomePage::openMeet);
    connect(results, &QAbstractButton::clicked, this, &HomePage::openResults);

    // ---- Progress
    auto *progress = new Card(tr("Your progress"));
    auto *levelRow = new QHBoxLayout;
    levelRow->addWidget(m_level);
    levelRow->addStretch();
    levelRow->addWidget(m_xpText);
    progress->body()->addLayout(levelRow);
    m_xp->setTextVisible(false);
    m_xp->setFixedHeight(8);
    progress->body()->addWidget(m_xp);
    progress->body()->addSpacing(6);
    auto *stats = new QHBoxLayout;
    stats->setSpacing(28);
    stats->addWidget(statBlock(m_streak, tr("Day streak")));
    stats->addWidget(statBlock(m_last, tr("Last score")));
    stats->addWidget(statBlock(m_best, tr("Best")));
    stats->addWidget(m_spark, 1);
    progress->body()->addLayout(stats);

    auto *badgeCard = new Card(tr("Badges"));
    m_badges->setHorizontalSpacing(8);
    m_badges->setVerticalSpacing(8);
    badgeCard->body()->addLayout(m_badges);

    auto *recentCard = new Card(tr("Recent sessions"));
    m_recent->setSpacing(4);
    recentCard->body()->addLayout(m_recent);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(36, 28, 36, 28);
    layout->setSpacing(14);
    layout->addLayout(hello);
    layout->addLayout(tiles);
    layout->addWidget(progress);
    layout->addWidget(badgeCard);
    layout->addWidget(recentCard);
    layout->addStretch();
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(Ui::scrollPage(content));

    connect(&SessionHistory::instance(), &SessionHistory::changed, this, &HomePage::refresh);
    connect(&AppSettings::instance(), &AppSettings::changed, this, &HomePage::refresh); // theme
    refresh();
}

void HomePage::refresh()
{
    const Theme::Colors &c = Theme::colors();
    const SessionHistory &h = SessionHistory::instance();
    const QList<HistoryEntry> &entries = h.entries();

    const int hour = QTime::currentTime().hour();
    m_greeting->setText(hour < 12 ? tr("Good morning") : hour < 18 ? tr("Good afternoon") : tr("Good evening"));

    m_xp->setStyleSheet(QStringLiteral("QProgressBar { background: %1; border: none; border-radius: 4px; }"
                                       "QProgressBar::chunk { background: %2; border-radius: 4px; }")
                            .arg(Theme::css(c.track), Theme::css(c.accent)));
    const int level = h.level();
    const int xp = h.xp();
    const int from = h.xpForLevel(level), to = h.xpForLevel(level + 1);
    m_level->setText(tr("Level %1").arg(level));
    m_xp->setRange(0, to - from);
    m_xp->setValue(xp - from);
    m_xpText->setText(tr("%1 / %2 XP").arg(xp - from).arg(to - from));

    const int streak = h.streakDays();
    m_streak->setText(QString::number(streak));
    const int best = h.bestScore();
    m_best->setText(best >= 0 ? QString::number(best) : QStringLiteral("–"));
    const int last = entries.isEmpty() ? -1 : entries.last().score;
    m_last->setText(last >= 0 ? QString::number(last) : QStringLiteral("–"));
    if (last >= 0)
        m_last->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::css(Theme::ratingColor(last >= 75 ? int(Rating::Good) : last >= 50 ? int(Rating::NeedsWork) : int(Rating::Poor)))));
    QList<int> values;
    for (int i = qMax(0, int(entries.size()) - 12); i < entries.size(); ++i)
        if (entries[i].score >= 0)
            values << entries[i].score;
    m_spark->setValues(values);

    // Badges
    while (QLayoutItem *item = m_badges->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    const QList<SessionHistory::Badge> badges = h.badges();
    for (int i = 0; i < badges.size(); ++i) {
        const auto &b = badges[i];
        QLabel *chip = Ui::label(QStringLiteral("%1  %2").arg(b.earned ? QStringLiteral("★") : QStringLiteral("☆"), b.title));
        chip->setToolTip(b.hint);
        QColor bg = b.earned ? c.accent : c.track;
        bg.setAlpha(b.earned ? (c.dark ? 70 : 40) : 120);
        chip->setStyleSheet(QStringLiteral("background: %1; color: %2; border-radius: 14px; padding: 6px 12px; font-weight: 600;")
                                .arg(Theme::css(bg), Theme::css(b.earned ? c.text : c.textSecondary)));
        m_badges->addWidget(chip, i / 4, i % 4);
    }

    // Recent sessions
    while (QLayoutItem *item = m_recent->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    if (entries.isEmpty())
        m_recent->addWidget(Ui::label(tr("Nothing yet. Tap Practise to start."), "secondary"));
    for (int i = int(entries.size()) - 1, shown = 0; i >= 0 && shown < 5; --i, ++shown) {
        const HistoryEntry &e = entries[i];
        auto *row = new QWidget;
        auto *rl = new QHBoxLayout(row);
        rl->setContentsMargins(4, 2, 4, 2);
        rl->addWidget(Ui::label(e.at.toString(QStringLiteral("d MMM, HH:mm")), "secondary"));
        rl->addSpacing(12);
        rl->addWidget(Ui::label(SessionHistory::modeName(e.mode)));
        rl->addWidget(Ui::label(formatDuration(e.seconds), "secondary"));
        rl->addStretch();
        QLabel *score = Ui::label(e.score >= 0 ? QString::number(e.score) : QStringLiteral("–"));
        score->setStyleSheet(QStringLiteral("font-weight: 700; color: %1;")
                                 .arg(Theme::css(e.score >= 75 ? c.success : e.score >= 50 ? c.orange : e.score >= 0 ? c.critical : c.textSecondary)));
        rl->addWidget(score);
        m_recent->addWidget(row);
    }
}
