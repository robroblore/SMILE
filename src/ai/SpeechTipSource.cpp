#include "ai/SpeechTipSource.h"

#include <QRegularExpression>

#include "ai/AiClient.h"
#include "core/AppSettings.h"
#include "sensors/MicrophoneMonitor.h"

namespace {
constexpr qint64 kMonologueMs = 120000;  // 2 minutes without a real pause
constexpr double kFillersPerMinute = 6.0;
constexpr double kFastWpm = 180.0;
} // namespace

SpeechTipSource::SpeechTipSource(MicrophoneMonitor *mic, AiClient *ai, QObject *parent)
    : TipSource(parent)
    , m_mic(mic)
    , m_ai(ai)
{
    m_poll.setInterval(1000);
    connect(&m_poll, &QTimer::timeout, this, &SpeechTipSource::checkMonologue);
    connect(m_mic, &MicrophoneMonitor::chunkReady, this, &SpeechTipSource::onChunk);
}

void SpeechTipSource::start()
{
    m_running = true;
    m_monologueTipped = false;
    m_lastTip.clear();
    m_transcript.clear();
    m_fillers.clear();
    m_words = 0;
    m_transcribedSpeechMs = 0;
    m_pendingChunks = 0;
    m_lastError.clear();
    m_speechMs = m_longestRunMs = 0;

    if (!AppSettings::instance().useMicrophone())
        return;
    m_mic->acquire();
    m_acquired = true;
    m_mic->resetStats();
    m_mic->setChunking(m_ai->isConfigured() && AppSettings::instance().cloudAnalysis());
    m_poll.start();
}

void SpeechTipSource::stop()
{
    m_poll.stop();
    if (m_acquired) {
        m_speechMs = m_mic->speechMs();
        m_longestRunMs = m_mic->longestRunMs();
        m_mic->setChunking(false);
        m_mic->release();
        m_acquired = false;
    }
    m_running = false;
}

void SpeechTipSource::checkMonologue()
{
    const qint64 run = m_mic->currentRunMs();
    if (run == 0) {
        m_monologueTipped = false;
        return;
    }
    if (run >= kMonologueMs && !m_monologueTipped) {
        m_monologueTipped = true;
        tip(QStringLiteral("monologue"), TipCategory::Speech, TipSeverity::Info,
            tr("You've been talking for %1 minutes. Pause and invite questions.").arg(run / 60000), 60);
    }
}

void SpeechTipSource::onChunk(const QByteArray &wav, int durationMs, int speechMs)
{
    Q_UNUSED(durationMs)
    if (!m_ai->isConfigured() || !AppSettings::instance().cloudAnalysis())
        return;
    ++m_pendingChunks;
    m_ai->transcribe(wav, [this, speechMs](const QString &text, const QString &error) {
        --m_pendingChunks;
        if (!error.isEmpty()) {
            m_lastError = error;
            return;
        }
        if (text.isEmpty())
            return;
        m_transcript += (m_transcript.isEmpty() ? QString() : QStringLiteral(" ")) + text;
        const QMap<QString, int> fillers = countFillers(text);
        const int words = countWords(text);
        int fillerCount = 0;
        for (auto it = fillers.cbegin(); it != fillers.cend(); ++it) {
            m_fillers[it.key()] += it.value();
            fillerCount += it.value();
        }
        m_words += words;
        m_transcribedSpeechMs += speechMs;

        if (!m_running)
            return; // late result after the session ended: keep for the report only

        const double minutes = qMax(speechMs, 1) / 60000.0;
        if (fillerCount >= 3 && fillerCount / minutes >= kFillersPerMinute) {
            QString top;
            int best = 0;
            for (auto it = fillers.cbegin(); it != fillers.cend(); ++it) {
                if (it.value() > best) {
                    best = it.value();
                    top = it.key();
                }
            }
            tip(QStringLiteral("fillers"), TipCategory::Speech, TipSeverity::Warning,
                tr("A few \"%1\"s just now. Try a short pause instead.").arg(top), 120);
        }
        if (speechMs >= 10000 && words / minutes > kFastWpm)
            tip(QStringLiteral("pace"), TipCategory::Speech, TipSeverity::Info,
                tr("You're speaking quite fast. Slow down a little."), 180);
    });
}

QMap<QString, int> SpeechTipSource::countFillers(const QString &text)
{
    static const QRegularExpression single(
        QStringLiteral("\\b(u+m+|u+h+m*|e+r+m*|a+h+|h+m+|m+h*m+)\\b"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression phrase(QStringLiteral("\\byou know\\b"), QRegularExpression::CaseInsensitiveOption);

    QMap<QString, int> counts;
    auto it = single.globalMatch(text);
    while (it.hasNext()) {
        QString word = it.next().captured(1).toLower();
        // Normalise "ummm" -> "um", "uhh" -> "uh", "erm" -> "erm"
        if (word.startsWith(QLatin1Char('u')))
            word = word.contains(QLatin1Char('m')) ? (word.contains(QLatin1Char('h')) ? QStringLiteral("uhm") : QStringLiteral("um"))
                                                   : QStringLiteral("uh");
        else if (word.startsWith(QLatin1Char('e')))
            word = QStringLiteral("erm");
        else if (word.startsWith(QLatin1Char('a')))
            word = QStringLiteral("ah");
        else
            word = QStringLiteral("hmm");
        ++counts[word];
    }
    auto p = phrase.globalMatch(text);
    while (p.hasNext()) {
        p.next();
        ++counts[QStringLiteral("you know")];
    }
    return counts;
}

int SpeechTipSource::countWords(const QString &text)
{
    static const QRegularExpression word(QStringLiteral("\\b[\\w']+\\b"));
    int n = 0;
    auto it = word.globalMatch(text);
    while (it.hasNext()) {
        it.next();
        ++n;
    }
    return n;
}

void SpeechTipSource::tip(const QString &key, TipCategory category, TipSeverity severity, const QString &text,
                          int cooldownS)
{
    QElapsedTimer &last = m_lastTip[key];
    if (last.isValid() && last.elapsed() < cooldownS * 1000LL)
        return;
    last.restart();
    Tip t;
    t.category = category;
    t.severity = severity;
    t.text = text;
    t.source = name();
    emit tipRaised(t);
}

QVariantMap SpeechTipSource::reportData() const
{
    if (m_speechMs == 0 && m_transcript.isEmpty())
        return {};
    QVariantMap fillers;
    int totalFillers = 0;
    for (auto it = m_fillers.cbegin(); it != m_fillers.cend(); ++it) {
        fillers.insert(it.key(), it.value());
        totalFillers += it.value();
    }
    const double minutes = m_transcribedSpeechMs / 60000.0;
    return {
        {QStringLiteral("speechSeconds"), int(m_speechMs / 1000)},
        {QStringLiteral("longestMonologueSeconds"), int(m_longestRunMs / 1000)},
        {QStringLiteral("fillers"), fillers},
        {QStringLiteral("fillerTotal"), totalFillers},
        {QStringLiteral("fillersPerMinute"), minutes > 0.1 ? qRound(totalFillers / minutes * 10) / 10.0 : 0.0},
        {QStringLiteral("wordsPerMinute"), minutes > 0.1 ? qRound(m_words / minutes) : 0},
        {QStringLiteral("transcript"), m_transcript},
        {QStringLiteral("lastError"), m_lastError},
    };
}
