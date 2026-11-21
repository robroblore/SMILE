#include "companion/CompanionProtocol.h"

namespace CompanionProtocol {

QJsonObject tipToJson(const Tip &tip)
{
    return {{QStringLiteral("category"), int(tip.category)},
            {QStringLiteral("severity"), int(tip.severity)},
            {QStringLiteral("text"), tip.text},
            {QStringLiteral("source"), tip.source},
            {QStringLiteral("reason"), tip.reason},
            {QStringLiteral("warningId"), tip.warningId},
            {QStringLiteral("key"), tip.key},
            {QStringLiteral("raisedAt"), tip.raisedAt.toString(Qt::ISODateWithMs)}};
}

Tip tipFromJson(const QJsonObject &o)
{
    Tip t;
    t.category = TipCategory(o.value(QLatin1String("category")).toInt());
    t.severity = TipSeverity(o.value(QLatin1String("severity")).toInt());
    t.text = o.value(QLatin1String("text")).toString();
    t.source = o.value(QLatin1String("source")).toString();
    t.reason = o.value(QLatin1String("reason")).toString();
    t.warningId = o.value(QLatin1String("warningId")).toString();
    t.key = o.value(QLatin1String("key")).toString();
    const auto raisedAt = QDateTime::fromString(o.value(QLatin1String("raisedAt")).toString(), Qt::ISODateWithMs);
    if (raisedAt.isValid())
        t.raisedAt = raisedAt;
    return t;
}

QJsonObject timerToJson(const SectionTimerInfo &t)
{
    return {{QStringLiteral("running"), t.running},
            {QStringLiteral("section"), t.section},
            {QStringLiteral("index"), t.index},
            {QStringLiteral("count"), t.count},
            {QStringLiteral("elapsed"), t.elapsedSeconds},
            {QStringLiteral("target"), t.targetSeconds},
            {QStringLiteral("delta"), t.scheduleDeltaSeconds},
            {QStringLiteral("status"), t.status}};
}

SectionTimerInfo timerFromJson(const QJsonObject &o)
{
    SectionTimerInfo t;
    t.running = o.value(QLatin1String("running")).toBool();
    t.section = o.value(QLatin1String("section")).toString();
    t.index = o.value(QLatin1String("index")).toInt();
    t.count = o.value(QLatin1String("count")).toInt();
    t.elapsedSeconds = o.value(QLatin1String("elapsed")).toInt();
    t.targetSeconds = o.value(QLatin1String("target")).toInt();
    t.scheduleDeltaSeconds = o.value(QLatin1String("delta")).toInt();
    t.status = o.value(QLatin1String("status")).toString();
    return t;
}

} // namespace CompanionProtocol
