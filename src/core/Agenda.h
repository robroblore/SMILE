#pragma once

#include <QList>
#include <QString>

// One point the user wants to get across, with the time they want it to take.
struct AgendaPoint {
    QString title;
    int targetSeconds = 120; // 0 means "no time limit"
    int actualSeconds = 0;   // filled in when a session runs
    bool reached = false;    // false if the session ended before this point
};

using Agenda = QList<AgendaPoint>;
