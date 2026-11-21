#include "app/SessionHistory.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QtMath>

#include "app/pages/PostMeetPage.h"

namespace {
int modeToInt(SessionMode m) { return int(m); }
SessionMode intToMode(int v)
{
    return v >= 0 && v <= int(SessionMode::Simulation) ? SessionMode(v) : SessionMode::Training;
}
} // namespace

SessionHistory &SessionHistory::instance()
{
    static SessionHistory history;
    return history;
}

SessionHistory::SessionHistory()
{
    QFile f(path());
    if (!f.open(QIODevice::ReadOnly))
        return;
    for (const QJsonValue &v : QJsonDocument::fromJson(f.readAll()).array()) {
        const QJsonObject o = v.toObject();
        HistoryEntry e;
        e.at = QDateTime::fromString(o.value(QLatin1String("at")).toString(), Qt::ISODate);
        e.mode = intToMode(o.value(QLatin1String("mode")).toInt());
        e.seconds = o.value(QLatin1String("seconds")).toInt();
        e.score = o.value(QLatin1String("score")).toInt(-1);
        e.headline = o.value(QLatin1String("headline")).toString();
        const QJsonObject cats = o.value(QLatin1String("categories")).toObject();
        for (auto it = cats.begin(); it != cats.end(); ++it)
            e.categories.insert(it.key(), it.value().toInt());
        if (e.at.isValid())
            m_entries << e;
    }
}

QString SessionHistory::path() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/history.json");
}

void SessionHistory::save() const
{
    QJsonArray list;
    for (const HistoryEntry &e : m_entries) {
        QJsonObject cats;
        for (auto it = e.categories.cbegin(); it != e.categories.cend(); ++it)
            cats.insert(it.key(), it.value());
        list.append(QJsonObject{{QStringLiteral("at"), e.at.toString(Qt::ISODate)},
                                {QStringLiteral("mode"), modeToInt(e.mode)},
                                {QStringLiteral("seconds"), e.seconds},
                                {QStringLiteral("score"), e.score},
                                {QStringLiteral("headline"), e.headline},
                                {QStringLiteral("categories"), cats}});
    }
    QSaveFile f(path());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(list).toJson(QJsonDocument::Indented));
        f.commit();
    }
}

void SessionHistory::add(const SessionReport &report, const Evaluation &evaluation)
{
    if (report.totalSeconds < 20)
        return; // a quick test, not a session
    HistoryEntry e;
    e.at = QDateTime::currentDateTime();
    e.mode = report.mode;
    e.seconds = report.totalSeconds;
    e.score = evaluation.overall;
    e.headline = evaluation.headline;
    for (const ReportCategory &c : evaluation.categories)
        if (c.score >= 0)
            e.categories.insert(c.name, c.score);
    m_entries << e;
    while (m_entries.size() > 500)
        m_entries.removeFirst();
    save();
    emit changed();
}

int SessionHistory::streakDays() const
{
    QSet<QDate> days;
    for (const HistoryEntry &e : m_entries)
        days.insert(e.at.date());
    QDate d = QDate::currentDate();
    if (!days.contains(d))
        d = d.addDays(-1);
    int streak = 0;
    while (days.contains(d)) {
        ++streak;
        d = d.addDays(-1);
    }
    return streak;
}

int SessionHistory::bestScore() const
{
    int best = -1;
    for (const HistoryEntry &e : m_entries)
        best = qMax(best, e.score);
    return best;
}

int SessionHistory::xp() const
{
    // Every session counts; good ones count more
    int xp = 0;
    for (const HistoryEntry &e : m_entries)
        xp += 20 + qMax(0, e.score) / 2 + qMin(e.seconds / 60, 20) * 2;
    return xp;
}

int SessionHistory::xpForLevel(int level) const
{
    // 0, 100, 300, 600, 1000…
    return 50 * (level - 1) * level;
}

int SessionHistory::level() const
{
    int level = 1;
    while (xp() >= xpForLevel(level + 1))
        ++level;
    return level;
}

QString SessionHistory::modeName(SessionMode mode)
{
    switch (mode) {
    case SessionMode::Meeting:
        return tr("Meeting");
    case SessionMode::Practice:
        return tr("Practice");
    case SessionMode::Simulation:
        return tr("Simulation");
    case SessionMode::Training:
        break;
    }
    return tr("Rehearsal");
}

QList<SessionHistory::Badge> SessionHistory::badges() const
{
    int simBest = -1, faceBest = -1, meetings = 0;
    for (const HistoryEntry &e : m_entries) {
        if (e.mode == SessionMode::Simulation)
            simBest = qMax(simBest, e.score);
        if (e.mode == SessionMode::Meeting)
            ++meetings;
        faceBest = qMax(faceBest, e.categories.value(PostMeetPage::tr("Face & framing"), -1));
    }
    return {
        {tr("First steps"), tr("Finish your first session"), !m_entries.isEmpty()},
        {tr("On a roll"), tr("Practise 3 days in a row"), streakDays() >= 3},
        {tr("Sharp"), tr("Score 80 or more"), bestScore() >= 80},
        {tr("Camera ready"), tr("Face & framing 90+"), faceBest >= 90},
        {tr("Simulation star"), tr("Score 75+ in a simulation"), simBest >= 75},
        {tr("The real deal"), tr("Use SMILE in a real meeting"), meetings > 0},
        {tr("Dedicated"), tr("Finish 10 sessions"), m_entries.size() >= 10},
    };
}
