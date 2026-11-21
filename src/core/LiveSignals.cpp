#include "core/LiveSignals.h"

LiveSignals &LiveSignals::instance()
{
    static LiveSignals signals_;
    return signals_;
}

void LiveSignals::addEvent(Event::Kind kind, const QString &title, const QString &detail)
{
    Event e;
    e.time = QDateTime::currentDateTime();
    e.kind = kind;
    e.title = title;
    e.detail = detail;
    m_events.prepend(e);
    while (m_events.size() > 200)
        m_events.removeLast();
    emit eventAdded(e);
}

void LiveSignals::clearEvents()
{
    m_events.clear();
    emit changed();
}
