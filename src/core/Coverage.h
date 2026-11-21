#pragma once

#include <QSet>
#include <QString>
#include <QStringList>

// Quick, local check of whether a key point was said, by comparing its
// meaningful words with what was transcribed. The Post-Meet report asks the
// cloud AI for a more careful judgement.
namespace Coverage {

QSet<QString> keywords(const QString &text);

// Fraction (0..1) of the key point's keywords found in the spoken text.
double score(const QString &keyPoint, const QSet<QString> &spokenKeywords);

inline bool covered(const QString &keyPoint, const QSet<QString> &spokenKeywords)
{
    return score(keyPoint, spokenKeywords) >= 0.4; // lenient: the AI judges meaning on top
}

} // namespace Coverage
