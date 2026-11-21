#include "core/Tip.h"

#include <QCoreApplication>

QString categoryName(TipCategory category)
{
    switch (category) {
    case TipCategory::Timing:
        return QCoreApplication::translate("Tip", "Timing");
    case TipCategory::Posture:
        return QCoreApplication::translate("Tip", "Posture");
    case TipCategory::Expression:
        return QCoreApplication::translate("Tip", "Expression");
    case TipCategory::Speech:
        return QCoreApplication::translate("Tip", "Speech");
    case TipCategory::Appearance:
        return QCoreApplication::translate("Tip", "Appearance");
    case TipCategory::Content:
        return QCoreApplication::translate("Tip", "Key points");
    case TipCategory::General:
        break;
    }
    return QCoreApplication::translate("Tip", "General");
}

const QList<TipCategory> &allTipCategories()
{
    static const QList<TipCategory> categories = {
        TipCategory::Timing,     TipCategory::Posture,    TipCategory::Expression,
        TipCategory::Speech,     TipCategory::Appearance, TipCategory::Content,
        TipCategory::General,
    };
    return categories;
}
