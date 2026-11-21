#include "core/HapticMouse.h"

#include <QCoreApplication>
#include <QHostAddress>
#include <QUdpSocket>

#include "core/LiveSignals.h"

void HapticMouse::requestPulse(TipCategory category)
{
    LiveSignals::instance().addEvent(
        LiveSignals::Event::Info,
        QCoreApplication::translate("HapticMouse", "Simulated mouse pulse requested"),
        categoryName(category));
}

void HapticMouse::playPulse(TipCategory category)
{
    const QByteArray payload = QByteArrayLiteral("SMILE:pulse:v1");
    QUdpSocket socket;
    const bool sent = socket.writeDatagram(payload, QHostAddress::LocalHost, 48643) == payload.size();
    const QString title = sent
        ? QCoreApplication::translate("HapticMouse", "Mouse pulse request sent")
        : QCoreApplication::translate("HapticMouse", "Could not send mouse pulse request");
    LiveSignals::instance().addEvent(LiveSignals::Event::Info, title, categoryName(category));
}
