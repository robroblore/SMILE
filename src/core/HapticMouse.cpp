#include "core/HapticMouse.h"

#include <QCoreApplication>

#include "core/LiveSignals.h"

void HapticMouse::requestPulse(TipCategory category)
{
    LiveSignals::instance().addEvent(
        LiveSignals::Event::Info,
        QCoreApplication::translate("HapticMouse", "Simulated mouse pulse requested"),
        categoryName(category));
}
