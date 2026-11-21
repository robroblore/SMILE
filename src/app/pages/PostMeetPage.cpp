#include "app/pages/PostMeetPage.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMap>
#include <QScrollArea>
#include <QVBoxLayout>

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

PostMeetPage::PostMeetPage(QWidget *parent)
    : QWidget(parent)
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
    m_reportLayout->addWidget(timing);
    m_reportLayout->addWidget(listCard(tr("What went well"), wentWell, tr("Nothing recorded yet."), c.success));
    m_reportLayout->addWidget(listCard(tr("To improve"), toImprove, tr("Nothing to flag. Well done!"), c.critical));
    m_reportLayout->addWidget(listCard(tr("Next steps"), nextSteps, QString(), c.accent));
    QLabel *note = Ui::label(tr("Detailed feedback on posture, expression and speech will appear here once the AI "
                                "modules are connected."),
                             "secondary");
    note->setWordWrap(true);
    m_reportLayout->addWidget(note);

    m_empty->hide();
    m_report->show();
}
