#include "core/Rating.h"

#include <QCoreApplication>

namespace Ratings {

QString name(Rating r)
{
    switch (r) {
    case Rating::Good:
        return QCoreApplication::translate("Rating", "Good");
    case Rating::NeedsWork:
        return QCoreApplication::translate("Rating", "Needs work");
    case Rating::Poor:
        return QCoreApplication::translate("Rating", "Poor");
    case Rating::None:
        break;
    }
    return QCoreApplication::translate("Rating", "Not measured");
}

int points(Rating r)
{
    switch (r) {
    case Rating::Good:
        return 100;
    case Rating::NeedsWork:
        return 60;
    case Rating::Poor:
        return 20;
    case Rating::None:
        break;
    }
    return 0;
}

Rating higherIsBetter(double value, double good, double poor)
{
    if (value >= good)
        return Rating::Good;
    if (value < poor)
        return Rating::Poor;
    return Rating::NeedsWork;
}

Rating lowerIsBetter(double value, double good, double poor)
{
    if (value <= good)
        return Rating::Good;
    if (value > poor)
        return Rating::Poor;
    return Rating::NeedsWork;
}

Rating within(double value, double goodLow, double goodHigh, double okLow, double okHigh)
{
    if (value >= goodLow && value <= goodHigh)
        return Rating::Good;
    if (value >= okLow && value <= okHigh)
        return Rating::NeedsWork;
    return Rating::Poor;
}

} // namespace Ratings
