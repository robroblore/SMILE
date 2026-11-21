#include "core/Tip.h"

#include <QHash>
#include <QSet>

#include "core/AppSettings.h"

#include <QCoreApplication>

QString categoryName(TipCategory category)
{
    switch (category) {
    case TipCategory::Timing:
        return QCoreApplication::translate("Tip", "Timing");
    case TipCategory::Posture:
        return QCoreApplication::translate("Tip", "Body language");
    case TipCategory::Expression:
        return QCoreApplication::translate("Tip", "Expression");
    case TipCategory::Speech:
        return QCoreApplication::translate("Tip", "Speech");
    case TipCategory::Appearance:
        return QCoreApplication::translate("Tip", "Appearance");
    case TipCategory::Content:
        return QCoreApplication::translate("Tip", "Key points");
    case TipCategory::Language:
        return QCoreApplication::translate("Tip", "Language");
    case TipCategory::General:
        break;
    }
    return QCoreApplication::translate("Tip", "General");
}

const QList<TipCategory> &allTipCategories()
{
    static const QList<TipCategory> categories = {
        TipCategory::Timing,     TipCategory::Posture,    TipCategory::Expression,
        TipCategory::Speech,     TipCategory::Language,   TipCategory::Appearance, TipCategory::Content,
        TipCategory::General,
    };
    return categories;
}

int livePriority(const Tip &tip)
{
    static const QHash<QString, int> order = {
        {QStringLiteral("covered"), 0},       {QStringLiteral("noface"), 1},
        {QStringLiteral("posture"), 2},       {QStringLiteral("restless"), 3},
        {QStringLiteral("hands"), 3},         {QStringLiteral("hands-inappropriate"), 3},
        {QStringLiteral("touching"), 3},      {QStringLiteral("uncomfortable"), 3},
        {QStringLiteral("eyes"), 4},          {QStringLiteral("notes"), 4},
        {QStringLiteral("fillers"), 5},       {QStringLiteral("cold"), 6},
        {QStringLiteral("flat"), 6},          {QStringLiteral("nosmile"), 6},
    };
    if (tip.source == QLatin1String("demo"))
        return 9;
    return order.value(tip.key, -1);
}

bool isUrgentTip(const QString &key)
{
    static const QSet<QString> urgent = {
        QStringLiteral("covered"), QStringLiteral("noface"), QStringLiteral("eyes"), QStringLiteral("notes"),
        QStringLiteral("nosmile"), QStringLiteral("cold"), QStringLiteral("fillers"),
    };
    return urgent.contains(key);
}

int repeatCooldownMs(const QString &key)
{
    // Urgent ones may come back soon if the problem comes back; others wait the
    // strictness setting's cooldown
    if (isUrgentTip(key))
        return 15000;
    return AppSettings::instance().thresholds().repeatCooldownSeconds * 1000;
}
