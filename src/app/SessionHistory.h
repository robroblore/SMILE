#pragma once

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QObject>

#include "app/pages/PostMeetPage.h"
#include "core/SessionReport.h"

// A short record of every finished session, kept on this computer only
// (history.json in the app data folder). It feeds the Home page: scores over
// time, streaks, levels and badges.
struct HistoryEntry {
    QString id;                    // when the session started; the same session is updated, not duplicated
    QDateTime at;
    SessionMode mode = SessionMode::Training;
    int seconds = 0;
    int score = -1;
    QString headline;
    QMap<QString, int> categories; // category name -> score
    // The full report, to reopen and compare later
    QString assessment;
    QStringList improvements;
    QList<ReportCategory> breakdown;
};

class SessionHistory : public QObject
{
    Q_OBJECT

public:
    static SessionHistory &instance();

    const QList<HistoryEntry> &entries() const { return m_entries; }
    // Adds the session, or updates it if it's already there (e.g. once the AI review arrives)
    void add(const SessionReport &report, const Evaluation &evaluation);
    void clear();
    // The session before `id` (or the latest one if id is empty); nullptr if none
    const HistoryEntry *previous(const QString &id) const;
    static QString idFor(const SessionReport &report);

    // Gamification
    int streakDays() const;      // consecutive days with a session, ending today or yesterday
    int bestScore() const;
    int xp() const;
    int level() const;           // 1, 2, 3…
    int xpForLevel(int level) const;
    static QString modeName(SessionMode mode);

    struct Badge {
        QString title;
        QString hint;
        bool earned = false;
    };
    QList<Badge> badges() const;

signals:
    void changed();

private:
    SessionHistory();
    void save() const;
    QString path() const;

    QList<HistoryEntry> m_entries;
};
