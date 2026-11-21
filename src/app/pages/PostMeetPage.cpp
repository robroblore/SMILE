#include "app/pages/PostMeetPage.h"

#include <QGridLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMap>
#include <QScrollArea>
#include <QVBoxLayout>

#include "ai/AiClient.h"
#include "core/TimeFormat.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

namespace {

Card *statCard(const QString &caption, const QString &value, const QString &detail)
{
    auto *card = new Card;
    card->body()->setSpacing(2);
    card->body()->addWidget(Ui::label(caption.toUpper(), "caption"));
    card->body()->addWidget(Ui::label(value, "bigValue"));
    QLabel *d = Ui::label(detail, "secondary");
    d->setWordWrap(true);
    card->body()->addWidget(d);
    return card;
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
        QLabel *text = Ui::label(item);
        text->setWordWrap(true);
        auto *dot = new QVBoxLayout; // keep the dot level with the first line of text
        dot->setContentsMargins(0, 6, 0, 0);
        dot->addWidget(bullet);
        dot->addStretch();
        row->addLayout(dot);
        row->addWidget(text, 1);
        card->body()->addLayout(row);
    }
    return card;
}

} // namespace

PostMeetPage::PostMeetPage(AiClient *ai, QWidget *parent)
    : QWidget(parent)
    , m_ai(ai)
    , m_empty(new Card)
    , m_report(new QWidget)
    , m_reportLayout(new QVBoxLayout(m_report))
{
    // Empty state
    auto *emptyCard = static_cast<Card *>(m_empty);
    emptyCard->body()->setContentsMargins(24, 40, 24, 40);
    emptyCard->body()->addWidget(new SmileyBadge(64, 0.5), 0, Qt::AlignHCenter);
    emptyCard->body()->addSpacing(8);
    QLabel *emptyTitle = Ui::label(tr("No report yet"), "cardTitle");
    emptyTitle->setAlignment(Qt::AlignHCenter);
    emptyCard->body()->addWidget(emptyTitle);
    QLabel *emptyText = Ui::label(tr("Finish a rehearsal or a meeting and your feedback will appear here."),
                                  "secondary");
    emptyText->setAlignment(Qt::AlignHCenter);
    emptyText->setWordWrap(true);
    emptyCard->body()->addWidget(emptyText);

    m_reportLayout->setContentsMargins(0, 0, 0, 0);
    m_reportLayout->setSpacing(12);
    m_report->hide();

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(36, 28, 36, 28);
    layout->setSpacing(12);
    layout->addWidget(Ui::pageHeader(tr("Post-Meet"), tr("What went well, what to work on, and what to do next.")));
    layout->addWidget(m_empty);
    layout->addWidget(m_report);
    layout->addStretch();

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(Ui::scrollPage(content));
}

void PostMeetPage::showReport(const SessionReport &report)
{
    // Clear the previous report
    while (QLayoutItem *item = m_reportLayout->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    const Theme::Colors &c = Theme::colors();
    const QString kind = report.mode == SessionMode::Training ? tr("Rehearsal") : tr("Meeting");

    int totalTarget = 0;
    int timedPoints = 0;
    int onTime = 0;
    QStringList wentWell;
    QStringList toImprove;
    QStringList nextSteps;

    // Timing card
    auto *timing = new Card(tr("Timing per point"));
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(10);
    grid->setColumnStretch(0, 1);
    grid->addWidget(Ui::label(tr("POINT"), "caption"), 0, 0);
    grid->addWidget(Ui::label(tr("TARGET"), "caption"), 0, 1, Qt::AlignRight);
    grid->addWidget(Ui::label(tr("ACTUAL"), "caption"), 0, 2, Qt::AlignRight);
    grid->addWidget(Ui::label(QString(), "caption"), 0, 3);

    int row = 1;
    for (const AgendaPoint &point : report.points) {
        totalTarget += point.targetSeconds;
        const bool timed = point.targetSeconds > 0;
        const int diff = point.actualSeconds - point.targetSeconds;

        QString status;
        QColor statusColor = c.textSecondary;
        if (!point.reached) {
            status = tr("Not reached");
            statusColor = c.critical;
            toImprove << tr("You didn't reach \"%1\".").arg(point.title);
            nextSteps << tr("Leave time for \"%1\" by tightening the earlier points.").arg(point.title);
        } else if (timed) {
            ++timedPoints;
            if (diff > 0) {
                status = tr("%1 over").arg(formatDuration(diff));
                statusColor = c.critical;
                toImprove << tr("\"%1\" ran %2 over.").arg(point.title, formatDuration(diff));
                nextSteps << tr("Trim \"%1\" by about %2, or plan more time for it.").arg(point.title, formatDuration(diff));
            } else {
                ++onTime;
                status = tr("On time");
                statusColor = c.success;
                wentWell << tr("\"%1\" finished on time.").arg(point.title);
            }
        }

        grid->addWidget(Ui::label(point.title), row, 0);
        grid->addWidget(Ui::label(timed ? formatDuration(point.targetSeconds) : tr("–"), "secondary"), row, 1,
                        Qt::AlignRight);
        grid->addWidget(Ui::label(point.reached ? formatDuration(point.actualSeconds) : tr("–")), row, 2, Qt::AlignRight);
        QLabel *chip = Ui::label(status);
        chip->setStyleSheet(QStringLiteral("color: %1; font-weight: 600;").arg(Theme::css(statusColor)));
        grid->addWidget(chip, row, 3, Qt::AlignRight);
        ++row;
    }
    timing->body()->addLayout(grid);

    // Tips logged by the AI modules (timing is covered above)
    QMap<QString, int> warnings;
    int tipCount = 0;
    for (const Tip &tip : report.tips) {
        if (tip.category == TipCategory::Timing)
            continue;
        ++tipCount;
        if (tip.severity == TipSeverity::Positive)
            wentWell << tip.text;
        else if (tip.severity == TipSeverity::Warning)
            ++warnings[categoryName(tip.category)];
    }
    for (auto it = warnings.cbegin(); it != warnings.cend(); ++it)
        toImprove << tr("%1: flagged %n time(s).", nullptr, it.value()).arg(it.key());
    wentWell.removeDuplicates();
    if (nextSteps.isEmpty())
        nextSteps << tr("Rehearse once more to make your timing second nature.");

    // Summary stats
    auto *stats = new QWidget;
    auto *statsRow = new QHBoxLayout(stats);
    statsRow->setContentsMargins(0, 0, 0, 0);
    statsRow->setSpacing(12);
    statsRow->addWidget(statCard(tr("Total time"), formatDuration(report.totalSeconds),
                                 totalTarget > 0 ? tr("Target %1").arg(formatDuration(totalTarget)) : tr("No target")));
    statsRow->addWidget(statCard(tr("On time"),
                                 timedPoints > 0 ? QStringLiteral("%1 / %2").arg(onTime).arg(timedPoints) : tr("–"),
                                 tr("Points within their target")));
    statsRow->addWidget(statCard(tr("Tips"), QString::number(tipCount), tr("Posture, expression and speech")));

    const QString started = QLocale().toString(report.startedAt, QLocale::ShortFormat);
    QLabel *heading = Ui::label(tr("%1 · %2").arg(kind, started), "cardTitle");

    m_reportLayout->addWidget(heading);
    m_reportLayout->addWidget(stats);

    // AI coach feedback (filled in asynchronously)
    if (m_ai->isConfigured()) {
        auto *coach = new Card;
        auto *coachTitle = new QHBoxLayout;
        coachTitle->setSpacing(8);
        QLabel *icon = Ui::label(QString());
        icon->setPixmap(Icons::icon(Icons::Icon::Sparkle, c.accent, 18).pixmap(18, 18));
        coachTitle->addWidget(icon);
        coachTitle->addWidget(Ui::label(tr("Your coach's feedback"), "cardTitle"));
        coachTitle->addStretch();
        coach->body()->addLayout(coachTitle);
        m_coach = Ui::label(tr("Writing your feedback…"), "secondary");
        m_coach->setWordWrap(true);
        m_coach->setTextInteractionFlags(Qt::TextSelectableByMouse);
        coach->body()->addWidget(m_coach);
        m_reportLayout->addWidget(coach);
    } else {
        m_coach = nullptr;
    }

    // Camera and voice statistics from the sensor modules
    auto tileRow = [](const QList<Card *> &cards) {
        auto *row = new QWidget;
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(12);
        for (Card *card : cards)
            h->addWidget(card);
        return row;
    };
    const QVariantMap vision = report.sources.value(QStringLiteral("vision")).toMap();
    if (!vision.isEmpty()) {
        m_reportLayout->addWidget(Ui::label(tr("On camera"), "cardTitle"));
        m_reportLayout->addWidget(tileRow({
            statCard(tr("Smiling"), tr("%1%").arg(vision.value(QStringLiteral("smilingPct")).toInt()),
                     tr("Of snapshots with a natural smile")),
            statCard(tr("Eye contact"), tr("%1%").arg(vision.value(QStringLiteral("eyeContactPct")).toInt()),
                     tr("Looking at the camera")),
            statCard(tr("Upright"), tr("%1%").arg(vision.value(QStringLiteral("uprightPct")).toInt()),
                     tr("Good posture")),
        }));
    }
    const QVariantMap speech = report.sources.value(QStringLiteral("speech")).toMap();
    if (!speech.isEmpty()) {
        const int wpm = speech.value(QStringLiteral("wordsPerMinute")).toInt();
        m_reportLayout->addWidget(Ui::label(tr("Your voice"), "cardTitle"));
        m_reportLayout->addWidget(tileRow({
            statCard(tr("Talk time"), formatDuration(speech.value(QStringLiteral("speechSeconds")).toInt()),
                     tr("Longest without a pause: %1")
                         .arg(formatDuration(speech.value(QStringLiteral("longestMonologueSeconds")).toInt()))),
            statCard(tr("Filler words"), QString::number(speech.value(QStringLiteral("fillerTotal")).toInt()),
                     tr("%1 per minute").arg(speech.value(QStringLiteral("fillersPerMinute")).toDouble())),
            statCard(tr("Pace"), wpm > 0 ? tr("%1 wpm").arg(wpm) : tr("–"),
                     wpm == 0 ? tr("Needs an AI key") : (wpm > 170 ? tr("A bit fast") : (wpm < 110 ? tr("Unhurried") : tr("Comfortable")))),
        }));
    }

    m_reportLayout->addWidget(timing);
    m_reportLayout->addWidget(listCard(tr("What went well"), wentWell, tr("Nothing recorded yet."), c.success));
    m_reportLayout->addWidget(listCard(tr("To improve"), toImprove, tr("Nothing to flag. Well done!"), c.critical));
    m_reportLayout->addWidget(listCard(tr("Next steps"), nextSteps, QString(), c.accent));
    if (!m_ai->isConfigured()) {
        QLabel *note = Ui::label(tr("Add an AI key in Settings to get personal written feedback, plus smile, "
                                    "posture and filler-word analysis."),
                                 "secondary");
        note->setWordWrap(true);
        m_reportLayout->addWidget(note);
    }

    m_empty->hide();
    m_report->show();

    if (m_coach)
        requestCoachFeedback(report);
}

QString PostMeetPage::summarizeForAi(const SessionReport &report)
{
    QString s;
    s += report.mode == SessionMode::Training ? QStringLiteral("Session: rehearsal\n") : QStringLiteral("Session: real meeting\n");
    s += QStringLiteral("Total time: %1\n\nAgenda (target / actual):\n").arg(formatDuration(report.totalSeconds));
    for (const AgendaPoint &p : report.points) {
        s += QStringLiteral("- %1: %2 / %3\n")
                 .arg(p.title, p.targetSeconds > 0 ? formatDuration(p.targetSeconds) : QStringLiteral("no limit"),
                      p.reached ? formatDuration(p.actualSeconds) : QStringLiteral("not reached"));
    }
    const QVariantMap vision = report.sources.value(QStringLiteral("vision")).toMap();
    if (!vision.isEmpty()) {
        s += QStringLiteral("\nCamera (%1 snapshots): smiling %2%, eye contact %3%, upright %4%. Postures: %5. Expressions: %6.\n")
                 .arg(vision.value(QStringLiteral("snapshots")).toInt())
                 .arg(vision.value(QStringLiteral("smilingPct")).toInt())
                 .arg(vision.value(QStringLiteral("eyeContactPct")).toInt())
                 .arg(vision.value(QStringLiteral("uprightPct")).toInt())
                 .arg(QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(vision.value(QStringLiteral("postures")).toMap())).toJson(QJsonDocument::Compact)),
                      QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(vision.value(QStringLiteral("expressions")).toMap())).toJson(QJsonDocument::Compact)));
    }
    const QVariantMap speech = report.sources.value(QStringLiteral("speech")).toMap();
    if (!speech.isEmpty()) {
        s += QStringLiteral("\nVoice: talk time %1, longest stretch without pausing %2, filler words %3 (%4/min) %5, pace %6 words/min.\n")
                 .arg(formatDuration(speech.value(QStringLiteral("speechSeconds")).toInt()),
                      formatDuration(speech.value(QStringLiteral("longestMonologueSeconds")).toInt()))
                 .arg(speech.value(QStringLiteral("fillerTotal")).toInt())
                 .arg(speech.value(QStringLiteral("fillersPerMinute")).toDouble())
                 .arg(QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(speech.value(QStringLiteral("fillers")).toMap())).toJson(QJsonDocument::Compact)))
                 .arg(speech.value(QStringLiteral("wordsPerMinute")).toInt());
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

void PostMeetPage::requestCoachFeedback(const SessionReport &report)
{
    const int id = ++m_reportId;
    const QString system = QStringLiteral(
        "You are SMILE, a warm, direct presentation coach for professionals preparing for high-stakes online "
        "meetings and job interviews. Using only the data provided, write short, specific feedback in Markdown "
        "with exactly three sections: '**What went well**', '**What to work on**' and '**Try this next time**'. "
        "Two or three bullets each, under 170 words in total. Refer to the agenda points and, if there is a "
        "transcript, quote short phrases from it. A comfortable speaking pace is 130-160 words per minute; more "
        "than 2 filler words per minute is worth working on. Never invent data that isn't given.");
    m_ai->chat(system, summarizeForAi(report), 1200, [this, id](const QString &text, const QString &error) {
        if (id != m_reportId || !m_coach)
            return; // a newer report replaced this one
        if (!error.isEmpty()) {
            m_coach->setTextFormat(Qt::PlainText);
            m_coach->setText(tr("Couldn't get feedback: %1").arg(error));
            return;
        }
        m_coach->setObjectName(QString());
        Theme::repolish(m_coach);
        m_coach->setTextFormat(Qt::MarkdownText);
        m_coach->setText(text);
    });
}
