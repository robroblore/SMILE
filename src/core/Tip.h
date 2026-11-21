#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

// What a tip is about. Each future AI module (posture, smile, speech, ...)
// raises tips in its own category so the user can switch categories on/off.
enum class TipCategory {
    Timing,
    Posture,
    Expression,
    Speech,
    Appearance,
    Content,  // key points of the agenda
    Language, // professional vocabulary
    General,
};

enum class TipSeverity {
    Positive, // something the user did well
    Info,     // a gentle suggestion
    Warning,  // something to fix now
};

struct Tip {
    TipCategory category = TipCategory::General;
    TipSeverity severity = TipSeverity::Info;
    QString text;
    QString source; // which module raised it, e.g. "timer" or "demo"
    QString reason; // why it fired, in plain words (shown in debug mode)
    QString key;    // stable id of the kind of tip ("covered", "fillers", ...)
    QDateTime raisedAt = QDateTime::currentDateTime();
    QString warningId;
};

QString categoryName(TipCategory category);

// SMILE corrects body behaviour live; everything else (timing, topic, pace,
// lighting, wording…) waits for the results. Returns how urgent a tip is when
// shown live (0 = most urgent), or -1 if it must not pop up during a session.
// Order: face covered/out of frame, posture and movement, gaze, filler words, smile.
int livePriority(const Tip &tip);

// Important tips (face covered or gone, looking away, not smiling for long, filler
// words) bypass the quiet time between pop-ups; the rest respect it.
bool isUrgentTip(const QString &key);
// How long before the same kind of tip may repeat
int repeatCooldownMs(const QString &key);
const QList<TipCategory> &allTipCategories();
