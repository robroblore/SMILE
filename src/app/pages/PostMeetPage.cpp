#include "app/pages/PostMeetPage.h"

#include <QLabel>
#include <QLocale>
#include <QMap>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "core/TimeFormat.h"

namespace {
QString esc(const QString &s)
{
    return s.toHtmlEscaped();
}

QString bulletList(const QStringList &items, const QString &emptyText)
{
    if (items.isEmpty())
        return QStringLiteral("<p><i>%1</i></p>").arg(esc(emptyText));
    QString html = QStringLiteral("<ul>");
    for (const QString &item : items)
        html += QStringLiteral("<li>%1</li>").arg(esc(item));
    return html + QStringLiteral("</ul>");
}
} // namespace

PostMeetPage::PostMeetPage(QWidget *parent)
    : QWidget(parent)
    , m_view(new QTextBrowser(this))
{
    auto *title = new QLabel(tr("Post-Meet"), this);
    QFont f = title->font();
    f.setPointSizeF(f.pointSizeF() * 1.8);
    f.setBold(true);
    title->setFont(f);

    m_view->setOpenLinks(false);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setHtml(tr("<p><i>Finish a rehearsal or a meeting to see your report here.</i></p>"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->addWidget(title);
    layout->addWidget(m_view, 1);
}

void PostMeetPage::showReport(const SessionReport &report)
{
    const QString kind = report.mode == SessionMode::Training ? tr("Rehearsal") : tr("Meeting");

    int totalTarget = 0;
    QStringList wentWell;
    QStringList toImprove;
    QStringList nextSteps;

    QString rows;
    for (const AgendaPoint &point : report.points) {
        totalTarget += point.targetSeconds;
        const bool timed = point.targetSeconds > 0;
        const int diff = point.actualSeconds - point.targetSeconds;
        QString diffText = QStringLiteral("–");
        if (!point.reached) {
            diffText = tr("not reached");
            toImprove << tr("You didn't reach \"%1\".").arg(point.title);
            nextSteps << tr("Leave time for \"%1\" by tightening the earlier points.").arg(point.title);
        } else if (timed) {
            diffText = diff > 0 ? tr("+%1 over").arg(formatDuration(diff))
                                : tr("%1 under").arg(formatDuration(-diff));
            if (diff > 0) {
                toImprove << tr("\"%1\" ran %2 over its target.").arg(point.title, formatDuration(diff));
                nextSteps << tr("Trim \"%1\" by about %2, or give it more time in your plan.")
                                 .arg(point.title, formatDuration(diff));
            } else if (point.actualSeconds > 0) {
                wentWell << tr("\"%1\" finished within its target.").arg(point.title);
            }
        }
        const bool flagged = !point.reached || (timed && diff > 0);
        const QString colour = flagged ? QStringLiteral("#C0392B") : QStringLiteral("inherit");
        rows += QStringLiteral("<tr><td>%1</td><td align='right'>%2</td><td align='right'>%3</td>"
                               "<td align='right' style='color:%4'>%5</td></tr>")
                    .arg(esc(point.title), timed ? formatDuration(point.targetSeconds) : tr("no limit"),
                         point.reached ? formatDuration(point.actualSeconds) : QStringLiteral("–"), colour,
                         diffText);
    }

    QMap<QString, int> warningsByCategory;
    for (const Tip &tip : report.tips) {
        if (tip.category == TipCategory::Timing)
            continue; // already covered by the table
        if (tip.severity == TipSeverity::Positive)
            wentWell << tip.text;
        else if (tip.severity == TipSeverity::Warning)
            ++warningsByCategory[categoryName(tip.category)];
    }
    for (auto it = warningsByCategory.cbegin(); it != warningsByCategory.cend(); ++it)
        toImprove << tr("%1: %n warning(s) during the session.", nullptr, it.value()).arg(it.key());
    wentWell.removeDuplicates();

    if (nextSteps.isEmpty())
        nextSteps << tr("Rehearse once more to make your timing consistent.");

    const QString started = QLocale().toString(report.startedAt, QLocale::ShortFormat);
    const QString total = totalTarget > 0
        ? tr("%1 (target %2)").arg(formatDuration(report.totalSeconds), formatDuration(totalTarget))
        : formatDuration(report.totalSeconds);

    QString html;
    html += QStringLiteral("<h2>%1 report</h2>").arg(esc(kind));
    html += QStringLiteral("<p>%1 · %2</p>").arg(esc(started), esc(tr("Total time %1").arg(total)));
    html += QStringLiteral("<h3>%1</h3>").arg(esc(tr("Timing per point")));
    html += QStringLiteral("<table cellspacing='0' cellpadding='4' width='100%'>"
                           "<tr><th align='left'>%1</th><th align='right'>%2</th>"
                           "<th align='right'>%3</th><th align='right'>%4</th></tr>%5</table>")
                .arg(esc(tr("Point")), esc(tr("Target")), esc(tr("Actual")), esc(tr("Difference")), rows);
    html += QStringLiteral("<h3>%1</h3>").arg(esc(tr("What went well")));
    html += bulletList(wentWell, tr("Nothing recorded yet."));
    html += QStringLiteral("<h3>%1</h3>").arg(esc(tr("To improve")));
    html += bulletList(toImprove, tr("Nothing to flag. Well done!"));
    html += QStringLiteral("<h3>%1</h3>").arg(esc(tr("Next steps")));
    html += bulletList(nextSteps, QString());
    html += QStringLiteral("<p style='color:gray'><i>%1</i></p>")
                .arg(esc(tr("Detailed feedback on posture, expression and speech will appear here once "
                            "the AI modules are connected.")));

    m_view->setHtml(html);
}
