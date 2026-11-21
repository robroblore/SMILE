#include "core/DemoTipSource.h"

#include <QRandomGenerator>

namespace {

struct SampleTip {
    TipCategory category;
    TipSeverity severity;
    const char *text;
};

const SampleTip kSamples[] = {
    {TipCategory::Posture, TipSeverity::Warning, QT_TRANSLATE_NOOP("DemoTipSource", "You're leaning towards the screen. Sit back a little.")},
    {TipCategory::Posture, TipSeverity::Warning, QT_TRANSLATE_NOOP("DemoTipSource", "Your shoulders are creeping up. Relax them.")},
    {TipCategory::Posture, TipSeverity::Positive, QT_TRANSLATE_NOOP("DemoTipSource", "Good upright posture.")},
    {TipCategory::Expression, TipSeverity::Info, QT_TRANSLATE_NOOP("DemoTipSource", "Try a smile. You've looked tense for a while.")},
    {TipCategory::Expression, TipSeverity::Positive, QT_TRANSLATE_NOOP("DemoTipSource", "Great eye contact with the camera.")},
    {TipCategory::Expression, TipSeverity::Info, QT_TRANSLATE_NOOP("DemoTipSource", "Look at the camera, not at the screen.")},
    {TipCategory::Speech, TipSeverity::Warning, QT_TRANSLATE_NOOP("DemoTipSource", "Several \"uhm\"s just now. Pause instead.")},
    {TipCategory::Speech, TipSeverity::Info, QT_TRANSLATE_NOOP("DemoTipSource", "You've been talking for a while. Leave room for questions.")},
    {TipCategory::Speech, TipSeverity::Positive, QT_TRANSLATE_NOOP("DemoTipSource", "Nice, steady pace.")},
};

} // namespace

DemoTipSource::DemoTipSource(QObject *parent)
    : TipSource(parent)
{
    setEnabled(false); // off until the user opts in
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        raiseRandomTip();
        scheduleNext();
    });
}

QString DemoTipSource::name() const
{
    return tr("Demo tips");
}

void DemoTipSource::start()
{
    scheduleNext();
}

void DemoTipSource::stop()
{
    m_timer.stop();
}

void DemoTipSource::scheduleNext()
{
    m_timer.start(QRandomGenerator::global()->bounded(12000, 25000));
}

void DemoTipSource::raiseRandomTip()
{
    const auto count = static_cast<quint32>(std::size(kSamples));
    const SampleTip &sample = kSamples[QRandomGenerator::global()->bounded(count)];

    Tip tip;
    tip.category = sample.category;
    tip.severity = sample.severity;
    tip.text = tr(sample.text);
    tip.source = QStringLiteral("demo");
    emit tipRaised(tip);
}
