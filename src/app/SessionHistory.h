#pragma once

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QObject>

#include "core/SessionReport.h"

struct Evaluation;

// A short record of every finished session, kept on this computer only
// (history.json in the app data folder). It feeds the Home page: scores over
// time, streaks, levels and badges.
struct HistoryEntry {
    QDateTime at;
    SessionMode mode = SessionMode::Training;
    int seconds = 0;
    int score = -1;
    QString headline;
    QMap<QString, int> categories; // category name -> score
};

class SessionHistory : public QObject
{
    Q_OBJECT

public:
    static SessionHistory &instance();

    const QList<HistoryEntry> &entries() const { return m_entries; }
    void add(const SessionReport &report, const Evaluation &evaluation);

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
