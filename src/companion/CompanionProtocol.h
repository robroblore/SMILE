#pragma once

#include <QJsonObject>

#include "companion/CompanionWindow.h"
#include "core/Tip.h"

// Messages between the main app and the companion process: one JSON object
// per line over a local socket.
namespace CompanionProtocol {

QJsonObject tipToJson(const Tip &tip);
Tip tipFromJson(const QJsonObject &o);

QJsonObject timerToJson(const SectionTimerInfo &t);
SectionTimerInfo timerFromJson(const QJsonObject &o);

} // namespace CompanionProtocol
