#pragma once

#include <QString>

// Traffic-light rating used consistently across the report and debug view.
enum class Rating {
    None,      // not measured
    Good,      // green: objective achieved
    NeedsWork, // orange: mediocre or inconsistent
    Poor,      // red: poor, or an important objective was missed
};

struct RatedMetric {
    QString label;   // "Smiling"
    QString value;   // "34%"
    Rating rating = Rating::None;
    QString measured; // what was measured
    QString why;      // why it got this rating
};

namespace Ratings {

QString name(Rating r);
int points(Rating r); // Good 100, NeedsWork 60, Poor 20

// Higher is better: good when >= good, poor when < poor.
Rating higherIsBetter(double value, double good, double poor);
// Lower is better: good when <= good, poor when > poor.
Rating lowerIsBetter(double value, double good, double poor);
// Inside [goodLow, goodHigh] is good; inside [okLow, okHigh] needs work.
Rating within(double value, double goodLow, double goodHigh, double okLow, double okHigh);

} // namespace Ratings
