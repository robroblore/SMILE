#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

// One section of what the user wants to communicate: a heading, the points it
// must get across, and how long it should take.
struct AgendaPoint {
    QString title;
    int targetSeconds = 120; // 0 means "no time limit"
    int actualSeconds = 0;   // filled in when a session runs
    bool reached = false;    // false if the session ended before this point
    QString summary;         // one line: what this section is for
    QString sourceText;      // the user's own notes/speech for this section
    QStringList keyPoints;   // objectives SMILE checks you actually said
    QString spoken;          // transcript heard while this section was active
    QList<bool> keyPointCovered; // local estimate, same size as keyPoints

    QJsonObject toJson() const
    {
        return {{QStringLiteral("title"), title},
                {QStringLiteral("seconds"), targetSeconds},
                {QStringLiteral("summary"), summary},
                {QStringLiteral("text"), sourceText},
                {QStringLiteral("keyPoints"), QJsonArray::fromStringList(keyPoints)}};
    }

    static AgendaPoint fromJson(const QJsonObject &o)
    {
        AgendaPoint p;
        p.title = o.value(QLatin1String("title")).toString();
        p.targetSeconds = o.value(QLatin1String("seconds")).toInt(120);
        p.summary = o.value(QLatin1String("summary")).toString();
        p.sourceText = o.value(QLatin1String("text")).toString();
        for (const QJsonValue &v : o.value(QLatin1String("keyPoints")).toArray())
            p.keyPoints << v.toString();
        return p;
    }
};

using Agenda = QList<AgendaPoint>;
