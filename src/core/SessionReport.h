#pragma once

#include <QDateTime>
#include <QList>

#include "core/Agenda.h"
#include "core/Tip.h"

enum class SessionMode {
    Training, // a rehearsal
    Meeting,  // a real meeting
};

// Everything that happened in one session. The Post-Meet page turns this into
// feedback; later the LLM feedback engine will receive it too.
struct SessionReport {
    SessionMode mode = SessionMode::Training;
    QDateTime startedAt;
    int totalSeconds = 0;
    Agenda points;
    QList<Tip> tips;
};
