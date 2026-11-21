#include "core/SessionController.h"

#include "core/Coverage.h"
#include "core/LiveSignals.h"
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
    if (m_active.isEmpty()) {
        AgendaPoint open;
        open.title = tr("Open discussion");
        open.targetSeconds = 0;
        m_active.append(open);
    }
    for (AgendaPoint &point : m_active) {
        point.actualSeconds = 0;
        point.reached = false;
        point.spoken.clear();
        point.keyPointCovered = QList<bool>(point.keyPoints.size(), false);
    }
    m_previousIndex = -1;
    m_heardSpeech = false;

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
    for (int i = 0; i < m_active.size(); ++i)
        updateCoverage(i, false);

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
    m_previousIndex = m_index;
    m_switchedAtMs = totalElapsed() * 1000LL;
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

    // Speech from the last few seconds is still being transcribed: check the
    // key points once it has arrived.
    if (!point.keyPoints.isEmpty()) {
        const int index = m_index;
        QTimer::singleShot(13000, this, [this, index] { checkSkippedKeyPoints(index); });
    }

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
        tip.reason = tr("%1 of the %2 planned for this section have passed.")
                         .arg(formatDuration(elapsed), formatDuration(point.targetSeconds));
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
        tip.reason = tr("Section target %1; you've spent %2 on it.")
                         .arg(formatDuration(point.targetSeconds), formatDuration(elapsed));
        tip.source = QStringLiteral("timer");
        raiseTip(tip);
    }
}

void SessionController::addTranscript(const QString &text)
{
    if (!m_running || m_index < 0 || text.trimmed().isEmpty())
        return;
    m_heardSpeech = true;
    m_active[m_index].spoken += QLatin1Char(' ') + text;
    updateCoverage(m_index, true);
    // Words spoken just before switching sections arrive a little late
    if (m_previousIndex >= 0 && totalElapsed() * 1000LL - m_switchedAtMs < 15000) {
        m_active[m_previousIndex].spoken += QLatin1Char(' ') + text;
        updateCoverage(m_previousIndex, true);
    }
}

void SessionController::updateCoverage(int index, bool announce)
{
    if (index < 0 || index >= m_active.size())
        return;
    AgendaPoint &point = m_active[index];
    if (point.keyPointCovered.size() != point.keyPoints.size())
        point.keyPointCovered = QList<bool>(point.keyPoints.size(), false);
    const QSet<QString> heard = Coverage::keywords(point.spoken);
    for (int k = 0; k < point.keyPoints.size(); ++k) {
        if (point.keyPointCovered[k] || !Coverage::covered(point.keyPoints[k], heard))
            continue;
        point.keyPointCovered[k] = true;
        if (announce)
            LiveSignals::instance().addEvent(LiveSignals::Event::Info, tr("Key point covered"),
                                             tr("\"%1\" in section \"%2\"").arg(point.keyPoints[k], point.title));
    }
}

void SessionController::checkSkippedKeyPoints(int index)
{
    if (!m_running || !m_heardSpeech || index < 0 || index >= m_active.size())
        return; // without transcripts we can't know what was said
    updateCoverage(index, true);
    const AgendaPoint &point = m_active[index];
    QStringList missed;
    for (int k = 0; k < point.keyPoints.size(); ++k)
        if (!point.keyPointCovered.value(k))
            missed << point.keyPoints[k];
    if (missed.isEmpty())
        return;
    Tip tip;
    tip.category = TipCategory::Content;
    tip.severity = TipSeverity::Info;
    tip.text = missed.size() == 1 ? tr("You didn't mention: \"%1\".").arg(missed.first())
                                  : tr("You skipped %1 key points in \"%2\".").arg(missed.size()).arg(point.title);
    tip.reason = tr("In \"%1\" SMILE heard %2 words, but none matched the main words of: %3.")
                     .arg(point.title)
                     .arg(point.spoken.split(QLatin1Char(' '), Qt::SkipEmptyParts).size())
                     .arg(missed.join(QStringLiteral("; ")));
    tip.source = QStringLiteral("coverage");
    raiseTip(tip);
}

void SessionController::raiseTip(const Tip &tip)
{
    if (!m_running)
        return;

    m_log.append(tip);
    emit tipLogged(tip);
    if (isCategoryShownLive(tip.category))
        emit tipRaised(tip);
}
