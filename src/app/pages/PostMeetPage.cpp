#include "app/pages/PostMeetPage.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QScrollArea>
#include <QVBoxLayout>

#include "ai/AiClient.h"
#include "core/Plural.h"
#include "core/TimeFormat.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

namespace {

// Circular score with the rating colour
class ScoreRing : public QWidget
{
public:
    ScoreRing(int score, const QColor &colour, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_score(score)
        , m_colour(colour)
    {
        setFixedSize(112, 112);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const Theme::Colors &c = Theme::colors();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(8, 8, -8, -8);
        p.setPen(QPen(c.track, 9, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(r, 0, 360 * 16);
        p.setPen(QPen(m_colour, 9, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(r, 90 * 16, -int(360 * 16 * qBound(0, m_score, 100) / 100.0));
        QFont f = font();
        f.setPointSizeF(f.pointSizeF() * 2.2);
        f.setWeight(QFont::Bold);
        p.setFont(f);
        p.setPen(c.text);
        p.drawText(r, Qt::AlignCenter, QString::number(m_score));
    }

private:
    int m_score;
    QColor m_colour;
};

QLabel *pill(Rating rating)
{
    const QColor colour = Theme::ratingColor(int(rating));
    QColor bg = colour;
    bg.setAlpha(Theme::colors().dark ? 50 : 30);
    QLabel *l = Ui::label(Ratings::name(rating));
    l->setStyleSheet(QStringLiteral("background: %1; color: %2; border-radius: 9px; padding: 2px 10px; font-weight: 600;")
                         .arg(Theme::css(bg), Theme::css(colour)));
    l->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return l;
}

QWidget *metricTile(const RatedMetric &m)
{
    const QColor colour = Theme::ratingColor(int(m.rating));
    auto *tile = new Card;
    tile->body()->setContentsMargins(18, 14, 18, 16);
    tile->body()->setSpacing(4);

    auto *bar = new QFrame;
    bar->setFixedHeight(4);
    bar->setStyleSheet(QStringLiteral("background: %1; border-radius: 2px;").arg(Theme::css(colour)));
    tile->body()->addWidget(bar);
    tile->body()->addSpacing(6);

    auto *head = new QHBoxLayout;
    head->addWidget(Ui::label(m.label.toUpper(), "caption"));
    head->addStretch();
    head->addWidget(pill(m.rating));
    tile->body()->addLayout(head);

    QLabel *value = Ui::label(m.value);
    QFont f = value->font();
    f.setPointSizeF(f.pointSizeF() * 2.3);
    f.setWeight(QFont::Bold);
    value->setFont(f);
    value->setStyleSheet(QStringLiteral("color: %1;").arg(
        Theme::css(m.rating == Rating::None ? Theme::colors().textSecondary : colour)));
    tile->body()->addWidget(value);

    QLabel *measured = Ui::label(m.measured, "secondary");
    measured->setWordWrap(true);
    QFont small = measured->font();
    small.setPointSizeF(small.pointSizeF() * 0.92);
    measured->setFont(small);
    tile->body()->addWidget(measured);
    QLabel *why = Ui::label(m.why);
    why->setWordWrap(true);
    tile->body()->addWidget(why);
    tile->body()->addStretch();
    return tile;
}

Card *listCard(const QString &title, const QStringList &items, const QString &emptyText, const QColor &dot)
{
    auto *card = new Card(title);
    card->body()->setSpacing(8);
    if (items.isEmpty()) {
        card->body()->addWidget(Ui::label(emptyText, "secondary"));
        return card;
    }
    for (const QString &item : items) {
        auto *row = new QHBoxLayout;
        row->setSpacing(10);
        QLabel *bullet = Ui::label(QString());
        bullet->setFixedSize(8, 8);
        bullet->setStyleSheet(QStringLiteral("background: %1; border-radius: 4px;").arg(Theme::css(dot)));
        auto *dotBox = new QVBoxLayout;
        dotBox->setContentsMargins(0, 6, 0, 0);
        dotBox->addWidget(bullet);
        dotBox->addStretch();
        QLabel *text = Ui::label(item);
        text->setWordWrap(true);
        row->addLayout(dotBox);
        row->addWidget(text, 1);
        card->body()->addLayout(row);
    }
    return card;
}

QString pct(int v)
{
    return v < 0 ? QStringLiteral("–") : QStringLiteral("%1%").arg(v);
}

} // namespace

// ------------------------------------------------------------------ metrics

QList<RatedMetric> PostMeetPage::metrics(const SessionReport &report, RatedMetric *overall)
{
    QList<RatedMetric> list;
    const QVariantMap vision = report.sources.value(QStringLiteral("vision")).toMap();
    const QVariantMap speech = report.sources.value(QStringLiteral("speech")).toMap();
    const bool heardSpeech = !speech.value(QStringLiteral("transcript")).toString().trimmed().isEmpty();

    // Key points covered
    {
        RatedMetric m;
        m.label = tr("Key points");
        int total = 0, covered = 0, unreachedSections = 0;
        for (const AgendaPoint &p : report.points) {
            total += int(p.keyPoints.size());
            for (int k = 0; k < p.keyPoints.size(); ++k)
                covered += p.keyPointCovered.value(k) ? 1 : 0;
            if (!p.reached && !p.keyPoints.isEmpty())
                ++unreachedSections;
        }
        m.measured = tr("Key points from your sections that SMILE heard you say.");
        if (total == 0) {
            m.value = tr("–");
            m.why = tr("No key points set. Add them to your sections on the Training page.");
        } else if (!heardSpeech) {
            m.value = tr("–");
            m.why = tr("Needs the microphone and an AI key to hear what you said.");
        } else {
            const double share = double(covered) / total;
            m.value = QStringLiteral("%1 / %2").arg(covered).arg(total);
            m.rating = Ratings::higherIsBetter(share, 0.9, 0.6);
            if (unreachedSections > 0 && m.rating == Rating::Good)
                m.rating = Rating::NeedsWork;
            m.why = m.rating == Rating::Good ? tr("You got your message across.")
                    : unreachedSections > 0
                        ? plural(unreachedSections, tr("%1 section never reached, so their points were missed."), tr("%1 sections never reached, so their points were missed."))
                        : tr("%1 key points weren't mentioned; aim for at least 90%.").arg(total - covered);
        }
        list << m;
    }

    // Timing
    {
        RatedMetric m;
        m.label = tr("Timing");
        int timed = 0, onTime = 0, unreached = 0, target = 0;
        for (const AgendaPoint &p : report.points) {
            target += p.targetSeconds;
            if (!p.reached) {
                ++unreached;
                continue;
            }
            if (p.targetSeconds > 0) {
                ++timed;
                onTime += p.actualSeconds <= p.targetSeconds * 1.1 ? 1 : 0;
            }
        }
        m.measured = tr("Sections finished within 10% of their target time.");
        if (timed + unreached == 0) {
            m.value = formatDuration(report.totalSeconds);
            m.why = tr("No target times set.");
        } else {
            const double share = double(onTime) / (timed + unreached);
            m.value = QStringLiteral("%1 / %2").arg(onTime).arg(timed + unreached);
            m.rating = Ratings::higherIsBetter(share, 0.8, 0.5);
            m.why = unreached > 0 ? plural(unreached, tr("Ran out of time: %1 section not reached."), tr("Ran out of time: %1 sections not reached."))
                    : m.rating == Rating::Good
                        ? tr("Total %1 against a target of %2.").arg(formatDuration(report.totalSeconds), formatDuration(target))
                        : tr("Several sections ran long. Total %1, target %2.")
                              .arg(formatDuration(report.totalSeconds), formatDuration(target));
        }
        list << m;
    }

    auto visionMetric = [&](const QString &label, const char *key, double good, double poor, const QString &measured,
                            const QString &goodWhy, const QString &badWhy) {
        RatedMetric m;
        m.label = label;
        m.measured = measured;
        const int v = vision.value(QLatin1String(key), -1).toInt();
        m.value = pct(v);
        if (v < 0) {
            m.why = tr("Needs the camera.");
        } else {
            m.rating = Ratings::higherIsBetter(v, good, poor);
            m.why = m.rating == Rating::Good ? goodWhy : badWhy.arg(int(good));
        }
        return m;
    };
    list << visionMetric(tr("Smiling"), "smilingPct", 35, 15,
                         tr("Share of time a natural smile was seen (cloud AI, local tracking in between)."),
                         tr("Warm and approachable."), tr("Smile more often; aim for at least %1% of the time."));
    list << visionMetric(tr("Eye contact"), "eyeContactPct", 70, 45,
                         tr("Share of time your face was turned towards the camera."),
                         tr("You kept good eye contact."), tr("Look into the camera more; aim for %1%."));
    list << visionMetric(tr("Engagement"), "engagementPct", 70, 50,
                         tr("Combines eye contact, smiling and staying in frame."),
                         tr("You came across as engaged."), tr("Facial engagement was low; aim for %1%."));

    // Speaking pace
    {
        RatedMetric m;
        m.label = tr("Speaking pace");
        const int wpm = speech.value(QStringLiteral("wordsPerMinute"), 0).toInt();
        m.measured = tr("Words per minute (%1).").arg(speech.value(QStringLiteral("paceSource"), tr("not measured")).toString());
        if (wpm <= 0) {
            m.value = tr("–");
            m.why = tr("Needs the microphone.");
        } else {
            m.value = tr("%1 wpm").arg(wpm);
            m.rating = Ratings::within(wpm, 130, 165, 110, 185);
            m.why = m.rating == Rating::Good ? tr("A comfortable pace (130–165).")
                    : wpm > 165               ? tr("Too fast; slow down to 130–165 wpm.")
                                              : tr("A little slow; aim for 130–165 wpm.");
        }
        list << m;
    }

    // Filler words
    {
        RatedMetric m;
        m.label = tr("Filler words");
        const double perMinute = speech.value(QStringLiteral("fillersPerMinute"), -1.0).toDouble();
        const int total = speech.value(QStringLiteral("fillerTotal"), 0).toInt();
        m.measured = tr("\"Um\", \"uh\", \"you know\"… per minute of speech (Whisper).");
        if (perMinute < 0) {
            m.value = tr("–");
            m.why = tr("Needs the microphone and an AI key.");
        } else {
            m.value = tr("%1/min").arg(perMinute, 0, 'f', 1);
            m.rating = Ratings::lowerIsBetter(perMinute, 2.0, 5.0);
            m.why = m.rating == Rating::Good ? plural(total, tr("%1 filler word in total. Clean delivery."), tr("%1 filler words in total. Clean delivery."))
                                             : plural(total, tr("%1 filler word; replace it with a short pause."), tr("%1 filler words; replace them with short pauses."));
        }
        list << m;
    }

    list << visionMetric(tr("Posture"), "uprightPct", 80, 60, tr("Share of cloud checks where you sat upright."),
                         tr("Upright and confident."), tr("Sit up straight more; aim for %1%."));

    // Overall: weighted average of what was measured
    if (overall) {
        double sum = 0, weight = 0;
        bool objectiveMissed = false;
        for (int i = 0; i < list.size(); ++i) {
            const RatedMetric &m = list[i];
            if (m.rating == Rating::None)
                continue;
            const double w = i == 0 ? 2.0 : (i == 1 ? 1.5 : 1.0);
            sum += Ratings::points(m.rating) * w;
            weight += w;
            if (i <= 1 && m.rating == Rating::Poor)
                objectiveMissed = true;
        }
        RatedMetric o;
        o.label = tr("Overall");
        o.measured = tr("Weighted average of everything measured; key points and timing count most.");
        if (weight == 0) {
            o.value = QStringLiteral("0");
            o.why = tr("Not enough was measured for an overall score.");
        } else {
            const int score = int(qRound(sum / weight));
            o.value = QString::number(score);
            o.rating = Ratings::higherIsBetter(score, 80, 55);
            if (objectiveMissed && o.rating == Rating::Good)
                o.rating = Rating::NeedsWork;
            int good = 0, needs = 0, poor = 0;
            for (const RatedMetric &m : list) {
                good += m.rating == Rating::Good;
                needs += m.rating == Rating::NeedsWork;
                poor += m.rating == Rating::Poor;
            }
            o.why = tr("%1 good, %2 need work, %3 poor%4.")
                        .arg(good)
                        .arg(needs)
                        .arg(poor)
                        .arg(objectiveMissed ? tr("; an important objective was missed") : QString());
        }
        *overall = o;
    }
    return list;
}

// ------------------------------------------------------------------- page

PostMeetPage::PostMeetPage(AiClient *ai, QWidget *parent)
    : QWidget(parent)
    , m_ai(ai)
    , m_empty(new Card)
    , m_reportWidget(new QWidget)
    , m_reportLayout(new QVBoxLayout(m_reportWidget))
{
    auto *emptyCard = static_cast<Card *>(m_empty);
    emptyCard->body()->setContentsMargins(24, 40, 24, 40);
    emptyCard->body()->addWidget(new SmileyBadge(64, 0.5), 0, Qt::AlignHCenter);
    emptyCard->body()->addSpacing(8);
    QLabel *emptyTitle = Ui::label(tr("No report yet"), "cardTitle");
    emptyTitle->setAlignment(Qt::AlignHCenter);
    emptyCard->body()->addWidget(emptyTitle);
    QLabel *emptyText = Ui::label(tr("Finish a rehearsal or a meeting and your feedback will appear here."), "secondary");
    emptyText->setAlignment(Qt::AlignHCenter);
    emptyText->setWordWrap(true);
    emptyCard->body()->addWidget(emptyText);

    m_reportLayout->setContentsMargins(0, 0, 0, 0);
    m_reportLayout->setSpacing(12);
    m_reportWidget->hide();

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(36, 28, 36, 28);
    layout->setSpacing(12);
    layout->addWidget(Ui::pageHeader(tr("Post-Meet"), tr("How it went, what to work on, and what to do next.")));
    layout->addWidget(m_empty);
    layout->addWidget(m_reportWidget);
    layout->addStretch();

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(Ui::scrollPage(content));
}

void PostMeetPage::showReport(const SessionReport &report)
{
    m_report = report;
    m_coverageFromAi = false;
    m_coachText.clear();
    m_coachError.clear();
    m_coachPending = m_ai->isConfigured();
    ++m_reportId;
    rebuild();
    if (m_ai->isConfigured()) {
        requestCoverageJudgement();
        requestCoachFeedback();
    }
}

void PostMeetPage::rebuild()
{
    while (QLayoutItem *item = m_reportLayout->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    const SessionReport &report = m_report;
    const Theme::Colors &c = Theme::colors();

    // ---- Dashboard (top, most prominent)
    RatedMetric overall;
    const QList<RatedMetric> list = metrics(report, &overall);

    const QString kind = report.mode == SessionMode::Training ? tr("Rehearsal") : tr("Meeting");
    const QString started = QLocale().toString(report.startedAt, QLocale::ShortFormat);

    auto *hero = new Card;
    auto *heroRow = new QHBoxLayout;
    heroRow->setSpacing(20);
    heroRow->addWidget(new ScoreRing(overall.value.toInt(), Theme::ratingColor(int(overall.rating))));
    auto *heroText = new QVBoxLayout;
    heroText->setSpacing(4);
    heroText->addWidget(Ui::label(tr("%1 · %2 · %3").arg(kind, started, formatDuration(report.totalSeconds)).toUpper(),
                                  "caption"));
    auto *heroTitle = new QHBoxLayout;
    heroTitle->addWidget(Ui::label(tr("Overall performance"), "bigValue"));
    heroTitle->addSpacing(8);
    heroTitle->addWidget(pill(overall.rating), 0, Qt::AlignVCenter);
    heroTitle->addStretch();
    heroText->addLayout(heroTitle);
    QLabel *heroWhy = Ui::label(overall.why);
    heroWhy->setWordWrap(true);
    heroText->addWidget(heroWhy);
    QLabel *legend = Ui::label(QStringLiteral("<span style='color:%1'>●</span> %2 &nbsp;&nbsp; "
                                              "<span style='color:%3'>●</span> %4 &nbsp;&nbsp; "
                                              "<span style='color:%5'>●</span> %6")
                                   .arg(Theme::css(c.success), tr("achieved"), Theme::css(c.orange), tr("needs work"),
                                        Theme::css(c.critical), tr("poor or missed")),
                               "secondary");
    legend->setTextFormat(Qt::RichText);
    heroText->addWidget(legend);
    heroRow->addLayout(heroText, 1);
    hero->body()->addLayout(heroRow);
    m_reportLayout->addWidget(hero);

    auto *gridHost = new QWidget;
    auto *grid = new QGridLayout(gridHost);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);
    for (int i = 0; i < list.size(); ++i)
        grid->addWidget(metricTile(list[i]), i / 4, i % 4);
    for (int col = 0; col < 4; ++col)
        grid->setColumnStretch(col, 1);
    m_reportLayout->addWidget(gridHost);

    // ---- Coach feedback
    if (m_ai->isConfigured()) {
        auto *coach = new Card;
        auto *coachTitle = new QHBoxLayout;
        QLabel *icon = Ui::label(QString());
        icon->setPixmap(Icons::icon(Icons::Icon::Sparkle, c.accent, 18).pixmap(18, 18));
        coachTitle->addWidget(icon);
        coachTitle->addWidget(Ui::label(tr("Your coach's feedback"), "cardTitle"));
        coachTitle->addStretch();
        coach->body()->addLayout(coachTitle);
        QLabel *text = Ui::label(QString(), m_coachText.isEmpty() ? "secondary" : nullptr);
        text->setWordWrap(true);
        text->setTextInteractionFlags(Qt::TextSelectableByMouse);
        if (!m_coachText.isEmpty()) {
            text->setTextFormat(Qt::MarkdownText);
            text->setText(m_coachText);
        } else {
            text->setText(m_coachError.isEmpty() ? tr("Writing your feedback…") : tr("Couldn't get feedback: %1").arg(m_coachError));
        }
        coach->body()->addWidget(text);
        m_reportLayout->addWidget(coach);
    }

    // ---- Key points per section
    bool anyKeyPoints = false;
    for (const AgendaPoint &p : report.points)
        anyKeyPoints = anyKeyPoints || !p.keyPoints.isEmpty();
    if (anyKeyPoints) {
        auto *kp = new Card(m_coverageFromAi ? tr("Key points (checked by the AI)") : tr("Key points"));
        kp->body()->setSpacing(6);
        for (const AgendaPoint &p : report.points) {
            if (p.keyPoints.isEmpty())
                continue;
            QLabel *section = Ui::label(p.title + (p.reached ? QString() : tr("  (not reached)")));
            QFont f = section->font();
            f.setWeight(QFont::DemiBold);
            section->setFont(f);
            kp->body()->addSpacing(4);
            kp->body()->addWidget(section);
            for (int k = 0; k < p.keyPoints.size(); ++k) {
                const bool ok = p.keyPointCovered.value(k);
                QLabel *line = Ui::label(QStringLiteral("<span style='color:%1; font-weight:700'>%2</span>&nbsp; %3")
                                             .arg(Theme::css(ok ? c.success : c.critical),
                                                  ok ? QStringLiteral("✓") : QStringLiteral("✗"),
                                                  p.keyPoints[k].toHtmlEscaped()));
                line->setTextFormat(Qt::RichText);
                line->setWordWrap(true);
                line->setContentsMargins(12, 0, 0, 0);
                kp->body()->addWidget(line);
            }
        }
        m_reportLayout->addWidget(kp);
    }

    // ---- Timing per section
    QStringList wentWell, toImprove, nextSteps;
    auto *timing = new Card(tr("Timing per section"));
    auto *tg = new QGridLayout;
    tg->setHorizontalSpacing(16);
    tg->setVerticalSpacing(10);
    tg->setColumnStretch(0, 1);
    tg->addWidget(Ui::label(tr("SECTION"), "caption"), 0, 0);
    tg->addWidget(Ui::label(tr("TARGET"), "caption"), 0, 1, Qt::AlignRight);
    tg->addWidget(Ui::label(tr("ACTUAL"), "caption"), 0, 2, Qt::AlignRight);
    int row = 1;
    for (const AgendaPoint &point : report.points) {
        const bool timed = point.targetSeconds > 0;
        const int diff = point.actualSeconds - point.targetSeconds;
        QString status;
        QColor statusColor = c.textSecondary;
        if (!point.reached) {
            status = tr("Not reached");
            statusColor = c.critical;
            nextSteps << tr("Leave time for \"%1\" by tightening the earlier sections.").arg(point.title);
        } else if (timed && diff > point.targetSeconds * 0.1) {
            status = tr("%1 over").arg(formatDuration(diff));
            statusColor = diff > point.targetSeconds * 0.3 ? c.critical : c.orange;
            nextSteps << tr("Trim \"%1\" by about %2, or plan more time for it.").arg(point.title, formatDuration(diff));
        } else if (timed) {
            status = tr("On time");
            statusColor = c.success;
        }
        tg->addWidget(Ui::label(point.title), row, 0);
        tg->addWidget(Ui::label(timed ? formatDuration(point.targetSeconds) : tr("–"), "secondary"), row, 1, Qt::AlignRight);
        tg->addWidget(Ui::label(point.reached ? formatDuration(point.actualSeconds) : tr("–")), row, 2, Qt::AlignRight);
        QLabel *chip = Ui::label(status);
        chip->setStyleSheet(QStringLiteral("color: %1; font-weight: 600;").arg(Theme::css(statusColor)));
        tg->addWidget(chip, row, 3, Qt::AlignRight);
        ++row;
    }
    timing->body()->addLayout(tg);
    m_reportLayout->addWidget(timing);

    for (const RatedMetric &m : list) {
        if (m.rating == Rating::Good)
            wentWell << QStringLiteral("%1: %2").arg(m.label, m.why);
        else if (m.rating != Rating::None)
            toImprove << QStringLiteral("%1 (%2): %3").arg(m.label, m.value, m.why);
    }
    for (const Tip &tip : report.tips)
        if (tip.severity == TipSeverity::Positive && tip.category != TipCategory::Timing)
            wentWell << tip.text;
    wentWell.removeDuplicates();
    if (nextSteps.isEmpty())
        nextSteps << tr("Rehearse once more to make it second nature.");

    m_reportLayout->addWidget(listCard(tr("What went well"), wentWell, tr("Nothing recorded yet."), c.success));
    m_reportLayout->addWidget(listCard(tr("To improve"), toImprove, tr("Nothing to flag. Well done!"), c.critical));
    m_reportLayout->addWidget(listCard(tr("Next steps"), nextSteps, QString(), c.accent));
    if (!m_ai->isConfigured()) {
        QLabel *note = Ui::label(tr("Add an AI key in Settings to get personal written feedback, plus smile, posture, "
                                    "key-point and filler-word analysis."),
                                 "secondary");
        note->setWordWrap(true);
        m_reportLayout->addWidget(note);
    }

    m_empty->hide();
    m_reportWidget->show();
}

QString PostMeetPage::summarizeForAi(const SessionReport &report)
{
    QString s;
    s += report.mode == SessionMode::Training ? QStringLiteral("Session: rehearsal\n") : QStringLiteral("Session: real meeting\n");
    s += QStringLiteral("Total time: %1\n\nSections (target / actual):\n").arg(formatDuration(report.totalSeconds));
    for (const AgendaPoint &p : report.points) {
        s += QStringLiteral("- %1: %2 / %3\n")
                 .arg(p.title, p.targetSeconds > 0 ? formatDuration(p.targetSeconds) : QStringLiteral("no limit"),
                      p.reached ? formatDuration(p.actualSeconds) : QStringLiteral("not reached"));
        for (int k = 0; k < p.keyPoints.size(); ++k)
            s += QStringLiteral("    key point: %1 [%2]\n")
                     .arg(p.keyPoints[k], p.keyPointCovered.value(k) ? QStringLiteral("said") : QStringLiteral("not heard"));
    }
    RatedMetric overall;
    s += QStringLiteral("\nMeasured (rating):\n");
    for (const RatedMetric &m : metrics(report, &overall))
        s += QStringLiteral("- %1: %2 (%3)\n").arg(m.label, m.value, Ratings::name(m.rating));
    const QVariantMap speech = report.sources.value(QStringLiteral("speech")).toMap();
    if (!speech.isEmpty()) {
        s += QStringLiteral("Longest stretch without pausing: %1. Fillers: %2.\n")
                 .arg(formatDuration(speech.value(QStringLiteral("longestMonologueSeconds")).toInt()),
                      QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(speech.value(QStringLiteral("fillers")).toMap()))
                                            .toJson(QJsonDocument::Compact)));
        const QString transcript = speech.value(QStringLiteral("transcript")).toString();
        if (!transcript.isEmpty())
            s += QStringLiteral("\nTranscript (may be partial):\n\"%1\"\n").arg(transcript.left(12000));
    }
    QStringList tips;
    for (const Tip &t : report.tips)
        tips << QStringLiteral("[%1] %2").arg(categoryName(t.category), t.text);
    if (!tips.isEmpty())
        s += QStringLiteral("\nLive tips shown during the session:\n- ") + tips.join(QStringLiteral("\n- ")) + QLatin1Char('\n');
    return s;
}

void PostMeetPage::requestCoachFeedback()
{
    const int id = m_reportId;
    const QString system = QStringLiteral(
        "You are SMILE, a warm, direct presentation coach for professionals preparing for high-stakes online "
        "meetings and job interviews. Using only the data provided, write short, specific feedback in Markdown "
        "with exactly three sections: '**What went well**', '**What to work on**' and '**Try this next time**'. "
        "Two or three bullets each, under 170 words in total. Refer to the sections and key points by name and, if "
        "there is a transcript, quote short phrases from it. A comfortable speaking pace is 130-165 words per "
        "minute; more than 2 filler words per minute is worth working on. Never invent data that isn't given.");
    m_ai->chat(system, summarizeForAi(m_report), 1500, [this, id](const QString &text, const QString &error) {
        if (id != m_reportId)
            return;
        m_coachPending = false;
        m_coachText = text;
        m_coachError = error;
        rebuild();
    });
}

void PostMeetPage::requestCoverageJudgement()
{
    const QString transcript =
        m_report.sources.value(QStringLiteral("speech")).toMap().value(QStringLiteral("transcript")).toString();
    QJsonArray sections;
    for (const AgendaPoint &p : m_report.points) {
        if (p.keyPoints.isEmpty())
            continue;
        sections.append(QJsonObject{{QStringLiteral("title"), p.title},
                                    {QStringLiteral("key_points"), QJsonArray::fromStringList(p.keyPoints)},
                                    {QStringLiteral("heard_during_section"), p.spoken.trimmed()}});
    }
    if (transcript.trimmed().isEmpty() || sections.isEmpty())
        return;

    const int id = m_reportId;
    const QString system = QStringLiteral(
        "You check whether a speaker got their key points across. For each key point decide if the speaker "
        "clearly made that point (same meaning, any wording). Use the text heard during that section first, then "
        "the full transcript. Reply with ONLY JSON: {\"sections\": [{\"title\": \"...\", \"covered\": [true|false, ...]}]} "
        "with one boolean per key point, in order.");
    const QString user = QStringLiteral("Sections:\n%1\n\nFull transcript:\n\"%2\"")
                             .arg(QString::fromUtf8(QJsonDocument(sections).toJson(QJsonDocument::Indented)),
                                  transcript.left(15000));
    m_ai->chat(system, user, 2000, [this, id](const QString &text, const QString &error) {
        if (id != m_reportId || !error.isEmpty())
            return;
        const QJsonArray judged = AiClient::extractJson(text).value(QLatin1String("sections")).toArray();
        if (judged.isEmpty())
            return;
        for (AgendaPoint &p : m_report.points) {
            for (const QJsonValue &v : judged) {
                const QJsonObject o = v.toObject();
                if (o.value(QLatin1String("title")).toString() != p.title)
                    continue;
                const QJsonArray covered = o.value(QLatin1String("covered")).toArray();
                p.keyPointCovered = QList<bool>(p.keyPoints.size(), false);
                for (int k = 0; k < p.keyPoints.size() && k < covered.size(); ++k)
                    p.keyPointCovered[k] = covered[k].toBool();
            }
        }
        m_coverageFromAi = true;
        rebuild();
    });
}
