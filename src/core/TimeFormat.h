#pragma once

#include <QString>
#include <cstdlib>

// 75 -> "1:15", -30 -> "-0:30"
inline QString formatDuration(int seconds)
{
    const QString sign = seconds < 0 ? QStringLiteral("-") : QString();
    const int s = std::abs(seconds);
    return QStringLiteral("%1%2:%3").arg(sign).arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
}
