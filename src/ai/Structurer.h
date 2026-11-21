#pragma once

#include <QString>
#include <functional>

#include "core/Agenda.h"

class AiClient;

// Turns a speech, presentation or meeting notes into ordered sections with
// headings that fit the content, a purpose, key points and suggested times.
namespace Structurer {

struct Result {
    QString presentationType; // e.g. "Business proposal"
    Agenda sections;
    QString error;
};

// Ask the AI (headings adapt to the type of talk).
void withAi(AiClient *ai, const QString &text, int targetMinutes, std::function<void(const Result &)> done);

// Offline fallback: split by headings and paragraphs.
Result locally(const QString &text, int targetMinutes);

// Plain text <-> bullet points
QString toBullets(const QString &text);
QString toText(const QString &bullets);

} // namespace Structurer
