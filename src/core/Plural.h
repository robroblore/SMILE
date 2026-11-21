#pragma once

#include <QString>

// "1 section" / "3 sections" without needing translation files.
// Both strings use %1 for the number.
inline QString plural(int n, const QString &one, const QString &many)
{
    return (n == 1 ? one : many).arg(n);
}
