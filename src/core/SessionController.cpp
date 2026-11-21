#include "core/SessionController.h"

#include "core/TimeFormat.h"
#include "core/TipSource.h"

namespace {
constexpr int kNearEndSeconds = 30;
constexpr int kRepeatOvertimeEverySeconds = 60;
} // namespace

SessionController::SessionController(QObject *parent)
    : QObject(parent)
{
    m_ticker.setInterval(250);
    connect(&m_ticker, &QTimer::timeout, this, &SessionController::onTick);
}

void SessionController::setAgenda(const Agenda &agenda)
{
    m_agenda = agenda;
    emit agendaChanged();
}

void SessionController::addTipSource(TipSource *source)
{
    source->setParent(this);
    m_sources.append(source);
    connect(source, &TipSource::tipRaised, this, &SessionController::raiseTip);
}

bool SessionController::isCategoryShownLive(TipCategory category) const
{
    return !m_hiddenCategories.contains(static_cast<int>(category));
}

void SessionController::setCategoryShownLive(TipCategory category, bool shown)
{
    if (shown)
        m_hiddenCategories.remove(static_cast<int>(category));
    else
        m_hiddenCategories.insert(static_cast<int>(category));
}

int SessionController::totalElapsed() const
{
    return m_running ? static_cast<int>(m_clock.elapsed() / 1000) : 0;
}

int SessionController::elapsedOnCurrent() const
{
    return m_running ? totalElapsed() - m_pointStartedAt : 0;
}

void SessionController::start(SessionMode mode)
{
    if (m_running)
        return;

    m_mode = mode;
    m_active = m_agenda;
    if (m_active.isEmpty())
        m_active.append({tr("Open discussion"), 0, 0, false});
    for (AgendaPoint &point : m_active) {
        point.actualSeconds = 0;
        point.reached = false;
    }

    m_log.clear();
    m_startedAt = QDateTime::currentDateTime();
    m_clock.start();
    m_running = true;
    m_ticker.start();

    for (TipSource *source : std::as_const(m_sources)) {
        if (source->isEnabled())
            source->start();
    }

    emit runningChanged(true);
    beginPoint(0);
}

void SessionController::nextPoint()
{
    if (!m_running)
        return;

    closeCurrentPoint();
    if (m_index + 1 >= m_active.size()) {
        stop();
        return;
    }
    beginPoint(m_index + 1);
}

void SessionController::stop()
{
    if (!m_running)
        return;

    closeCurrentPoint();
    const int total = totalElapsed();

    m_ticker.stop();
    for (TipSource *source : std::as_const(m_sources))
        source->stop();
    m_running = false;
    m_index = -1;

    SessionReport report;
    report.mode = m_mode;
    report.startedAt = m_startedAt;
    report.totalSeconds = total;
    report.points = m_active;
    report.tips = m_log;
    for (TipSource *source : std::as_const(m_sources)) {
        const QVariantMap data = source->reportData();
        if (!data.isEmpty())
            report.sources.insert(source->name(), data);
    }

    emit runningChanged(false);
    emit finished(report);
}

void SessionController::beginPoint(int index)
{
    m_index = index;
    m_active[index].reached = true;
    m_pointStartedAt = totalElapsed();
    m_overtimeWarnings = 0;
    m_nearEndWarned = false;
    emit currentPointChanged(index);
    emit ticked();
}

void SessionController::closeCurrentPoint()
{
    if (m_index < 0 || m_index >= m_active.size())
        return;

    AgendaPoint &point = m_active[m_index];
    point.actualSeconds = elapsedOnCurrent();

    if (point.targetSeconds > 0 && point.actualSeconds <= point.targetSeconds) {
        Tip tip;
        tip.category = TipCategory::Timing;
        tip.severity = TipSeverity::Positive;
        tip.text = tr("Nice, \"%1\" finished on time.").arg(point.title);
        tip.source = QStringLiteral("timer");
        raiseTip(tip);
    }
}

void SessionController::onTick()
{
    if (!m_running || m_index < 0)
        return;

    emit ticked();

    const AgendaPoint &point = m_active.at(m_index);
    if (point.targetSeconds <= 0)
        return;

    const int elapsed = elapsedOnCurrent();
    const int remaining = point.targetSeconds - elapsed;

    if (!m_nearEndWarned && point.targetSeconds > 2 * kNearEndSeconds
        && remaining <= kNearEndSeconds && remaining > 0) {
        m_nearEndWarned = true;
        Tip tip;
        tip.category = TipCategory::Timing;
        tip.severity = TipSeverity::Info;
        tip.text = tr("%1 s left on \"%2\".").arg(kNearEndSeconds).arg(point.title);
        tip.source = QStringLiteral("timer");
        raiseTip(tip);
    }

    const int over = -remaining;
    const int threshold = m_overtimeWarnings * kRepeatOvertimeEverySeconds;
    if (over >= threshold && over >= 0) {
        ++m_overtimeWarnings;
        Tip tip;
        tip.category = TipCategory::Timing;
        tip.severity = TipSeverity::Warning;
        tip.text = over < kRepeatOvertimeEverySeconds
            ? tr("Time's up for \"%1\". Wrap up and move on.").arg(point.title)
            : tr("\"%1\" is %2 over its target.").arg(point.title, formatDuration(over));
        tip.source = QStringLiteral("timer");
        raiseTip(tip);
    }
}

void SessionController::raiseTip(const Tip &tip)
{
    if (!m_running)
        return;

    m_log.append(tip);
    if (isCategoryShownLive(tip.category))
        emit tipRaised(tip);
}
