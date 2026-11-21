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
    QDateTime raisedAt = QDateTime::currentDateTime();
};

QString categoryName(TipCategory category);
const QList<TipCategory> &allTipCategories();
