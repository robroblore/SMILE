#include "ai/SpeechTipSource.h"

#include <QElapsedTimer>
#include <QRegularExpression>
#include <algorithm>

#include "ai/AiClient.h"
#include "core/AppSettings.h"
#include "core/LiveSignals.h"
#include "sensors/MicrophoneMonitor.h"

namespace {
constexpr double kSyllablesPerWord = 1.45; // English average; refined by Whisper
constexpr int kFastSustainMs = 6000;

QString describeFillers(const QMap<QString, int> &fillers)
{
    QStringList parts;
    for (auto it = fillers.cbegin(); it != fillers.cend(); ++it)
        parts << QStringLiteral("\"%1\" ×%2").arg(it.key()).arg(it.value());
    return parts.join(QStringLiteral(", "));
}
} // namespace

SpeechTipSource::SpeechTipSource(MicrophoneMonitor *mic, AiClient *ai, QObject *parent)
    : TipSource(parent)
    , m_mic(mic)
    , m_ai(ai)
{
    m_poll.setInterval(500);
    connect(&m_poll, &QTimer::timeout, this, &SpeechTipSource::poll);
    connect(m_mic, &MicrophoneMonitor::chunkReady, this, &SpeechTipSource::onChunk);
}

void SpeechTipSource::start()
{
    m_running = true;
    m_session.start();
    m_monologueTipped = false;
    m_wasSpeaking = false;
    m_fastSince = -1;
    m_lastTip.clear();
    m_paceFactor = 1.0;
    m_paceSamples = 0;
    m_localWpmSum = 0;
    m_localWpmCount = 0;
    m_recent.clear();
    m_transcript.clear();
    m_fillers.clear();
    m_words = 0;
    m_transcribedSpeechMs = 0;
    m_lastError.clear();
    m_speechMs = m_longestRunMs = 0;
    m_flagged.clear();
    m_slang.clear();
    m_hedges = 0;
    m_recentDb.clear();
    m_baselineDb.clear();
    m_quietPolls = m_speakingPolls = m_pauses = 0;
    LiveSignals::instance().voice = LiveSignals::Voice();

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
        m_pauses = m_mic->pauseCount();
        m_mic->setChunking(false);
        m_mic->release();
        m_acquired = false;
    }
    m_running = false;
}

void SpeechTipSource::poll()
{
    const AppSettings::Thresholds t = AppSettings::instance().thresholds();
    LiveSignals::Voice &v = LiveSignals::instance().voice;
    v.active = m_mic->isActive();
    v.level = float(m_mic->level());
    v.levelDb = float(m_mic->levelDb());
    v.noiseFloorDb = float(m_mic->noiseFloorDb());
    v.thresholdDb = float(m_mic->thresholdDb());
    v.speaking = m_mic->isSpeaking();
    v.runSeconds = int(m_mic->currentRunMs() / 1000);
    v.error = m_mic->errorString().isEmpty() ? m_lastError : m_mic->errorString();

    m_wasSpeaking = v.speaking;

    // --- Volume, relative to how you normally speak (mic gain differs a lot
    // between computers, so absolute levels alone would be unfair)
    if (v.speaking && m_mic->levelDb() > -80) {
        ++m_speakingPolls;
        m_recentDb.append(float(m_mic->levelDb()));
        while (m_recentDb.size() > 40) // 20 s at 2 polls/s
            m_recentDb.removeFirst();
        if (m_baselineDb.size() < 120)
            m_baselineDb.append(float(m_mic->levelDb()));
        auto median = [](QList<float> l) {
            std::sort(l.begin(), l.end());
            return l.isEmpty() ? -90.f : l[l.size() / 2];
        };
        v.speechDb = median(m_recentDb);
        v.baselineDb = median(m_baselineDb);
        const bool quiet = m_baselineDb.size() >= 60 && m_recentDb.size() >= 30
                           && (v.speechDb < v.baselineDb - 8.f || v.speechDb < -48.f);
        if (quiet)
            ++m_quietPolls;
        if (quiet)
            tip(QStringLiteral("quiet"), TipCategory::Speech, TipSeverity::Info, tr("Speak up a little."),
                tr("Your voice has been around %1 dB for the last 20 s, %2 dB quieter than your usual %3 dB.")
                    .arg(v.speechDb, 0, 'f', 0)
                    .arg(v.baselineDb - v.speechDb, 0, 'f', 0)
                    .arg(v.baselineDb, 0, 'f', 0));
    }
    const double speechMinutes = m_mic->speechMs() / 60000.0;
    v.pausesPerMinute = speechMinutes > 0.25 ? int(m_mic->pauseCount() / speechMinutes) : 0;

    // --- Monologue
    const qint64 run = m_mic->currentRunMs();
    if (run == 0) {
        m_monologueTipped = false;
    } else if (run >= t.monologueSeconds * 1000LL && !m_monologueTipped) {
        m_monologueTipped = true;
        tip(QStringLiteral("monologue"), TipCategory::Speech, TipSeverity::Info,
            tr("You've been talking for %1 minutes. Pause and invite questions.").arg(qMax<qint64>(1, run / 60000)),
            tr("Speech detected for %1 s with no pause longer than 1.5 s (limit %2 s).")
                .arg(run / 1000)
                .arg(t.monologueSeconds));
    }

    // --- Local pace (syllables), calibrated by Whisper
    const double syl = m_mic->syllablesPerSecond();
    const int recentSpeech = m_mic->recentSpeechMs();
    v.syllablesPerSecond = float(syl);
    const int localWpm = syl > 0 ? int(syl * 60.0 / kSyllablesPerWord * m_paceFactor) : 0;
    v.wpmLocal = localWpm;
    if (localWpm > 0 && v.speaking) {
        m_localWpmSum += localWpm;
        ++m_localWpmCount;
    }
    if (localWpm > t.fastWpm && recentSpeech >= kFastSustainMs) {
        if (m_fastSince < 0)
            m_fastSince = now();
        if (now() - m_fastSince >= 2000) {
            tip(QStringLiteral("pace"), TipCategory::Speech, TipSeverity::Warning,
                tr("You're speaking quickly. Slow down a little."),
                tr("Local estimate: %1 words/min (%2 syllables/s over the last %3 s of speech%4); limit %5.")
                    .arg(localWpm)
                    .arg(syl, 0, 'f', 1)
                    .arg(recentSpeech / 1000)
                    .arg(m_paceSamples ? tr(", calibrated against Whisper ×%1").arg(m_paceFactor, 0, 'f', 2)
                                       : QString())
                    .arg(t.fastWpm));
            m_fastSince = -1;
        }
    } else {
        m_fastSince = -1;
    }

    // --- Fillers in the last minute
    while (!m_recent.isEmpty() && now() - m_recent.first().atMs > 60000)
        m_recent.removeFirst();
    QMap<QString, int> lastMinute;
    for (const Chunk &c : std::as_const(m_recent))
        for (auto it = c.fillers.cbegin(); it != c.fillers.cend(); ++it)
            lastMinute[it.key()] += it.value();
    int total = 0;
    for (int n : lastMinute)
        total += n;
    v.fillersLastMinute = total;
    v.lastFillers = describeFillers(lastMinute);
    LiveSignals::instance().notifyChanged();
}

void SpeechTipSource::onChunk(const QByteArray &wav, int durationMs, int speechMs)
{
    Q_UNUSED(durationMs)
    if (!m_ai->isConfigured() || !AppSettings::instance().cloudAnalysis())
        return;
    const int localWpmAtSend = LiveSignals::instance().voice.wpmLocal;
    auto *timer = new QElapsedTimer;
    timer->start();
    m_ai->transcribe(wav, [this, speechMs, localWpmAtSend, timer](const QString &text, const QString &error) {
        LiveSignals::Voice &v = LiveSignals::instance().voice;
        v.transcribeLatencyMs = int(timer->elapsed());
        delete timer;
        if (!error.isEmpty()) {
            m_lastError = error;
            return;
        }
        if (text.isEmpty())
            return;
        m_transcript += (m_transcript.isEmpty() ? QString() : QStringLiteral(" ")) + text;
        const QMap<QString, int> fillers = countFillers(text);
        const int words = countWords(text);
        for (auto it = fillers.cbegin(); it != fillers.cend(); ++it)
            m_fillers[it.key()] += it.value();
        m_words += words;
        m_transcribedSpeechMs += speechMs;
        v.lastTranscript = text;

        const LanguageHits lang = checkLanguage(text);
        m_hedges += lang.hedges;
        for (auto it = lang.slang.cbegin(); it != lang.slang.cend(); ++it)
            m_slang[it.key()] += it.value();
        for (const QString &word : lang.profanity) {
            const int at = int(text.indexOf(word, 0, Qt::CaseInsensitive));
            const QString context = text.mid(qMax(0, at - 40), word.size() + 80).simplified();
            m_flagged.append({{QStringLiteral("word"), word}, {QStringLiteral("context"), context}});
            v.lastFlaggedWord = word;
        }

        const double minutes = qMax(speechMs, 1) / 60000.0;
        const int whisperWpm = speechMs >= 4000 ? int(words / minutes) : 0;
        if (whisperWpm > 0) {
            v.wpmWhisper = whisperWpm;
            // Calibrate the local estimate towards Whisper's ground truth
            if (localWpmAtSend > 30) {
                const double ratio = qBound(0.5, double(whisperWpm) / (localWpmAtSend / m_paceFactor), 2.0);
                m_paceFactor = m_paceSamples == 0 ? ratio : 0.7 * m_paceFactor + 0.3 * ratio;
                ++m_paceSamples;
            }
        }
        emit transcribed(text);

        if (!m_running)
            return; // a late result after the session ended: keep for the report only

        m_recent.append({now(), fillers, text});
        const AppSettings::Thresholds t = AppSettings::instance().thresholds();

        if (!lang.profanity.isEmpty())
            tip(QStringLiteral("language"), TipCategory::Language, TipSeverity::Warning,
                tr("Watch your language. Keep it professional."),
                tr("Whisper heard \"%1\". Alternatives are suggested in your review.").arg(lang.profanity.join(QStringLiteral("\", \""))));

        QMap<QString, int> lastMinute;
        for (const Chunk &c : std::as_const(m_recent))
            for (auto it = c.fillers.cbegin(); it != c.fillers.cend(); ++it)
                lastMinute[it.key()] += it.value();
        int total = 0;
        QString top;
        int best = 0;
        for (auto it = lastMinute.cbegin(); it != lastMinute.cend(); ++it) {
            total += it.value();
            if (it.value() > best) {
                best = it.value();
                top = it.key();
            }
        }
        if (total >= t.fillersPerMinute && !fillers.isEmpty())
            tip(QStringLiteral("fillers"), TipCategory::Speech, TipSeverity::Warning,
                tr("A few \"%1\"s just now. Try a short pause instead.").arg(top),
                tr("Whisper heard %1 filler words in the last minute (limit %2): %3. Latest: \"%4\"")
                    .arg(total)
                    .arg(t.fillersPerMinute)
                    .arg(describeFillers(lastMinute))
                    .arg(text.left(120)));
        if (whisperWpm > t.fastWpm && speechMs >= 6000)
            tip(QStringLiteral("pace"), TipCategory::Speech, TipSeverity::Warning,
                tr("You're speaking quickly. Slow down a little."),
                tr("Whisper counted %1 words in %2 s of speech = %3 words/min (limit %4).")
                    .arg(words)
                    .arg(speechMs / 1000)
                    .arg(whisperWpm)
                    .arg(t.fastWpm));
    });
}

SpeechTipSource::LanguageHits SpeechTipSource::checkLanguage(const QString &text)
{
    static const QRegularExpression profanity(
        QStringLiteral("\\b(f+u+c+k\\w*|sh[i1]t\\w*|bullshit|bitch\\w*|bastard\\w*|asshole\\w*|dick(head)?|cunt\\w*|"
                       "motherf\\w*|wtf|piss(ed|ing)?|damn(ed|it)?|crap\\w*|hell)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression slang(
        QStringLiteral("\\b(gonna|wanna|gotta|kinda|sorta|ain't|y'all|dude|nope|yeah yeah|whatever|stuff like that|"
                       "and stuff|super (?:cool|awesome)|awesome|cool)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression hedges(
        QStringLiteral("\\b(i think|i guess|maybe|sort of|kind of|probably|hopefully|i'm not sure|i feel like)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    LanguageHits hits;
    auto p = profanity.globalMatch(text);
    while (p.hasNext()) {
        const QString w = p.next().captured(1);
        // "hell" only counts as an exclamation, not inside "hello"/"shell" (word boundaries handle that)
        hits.profanity << w;
    }
    auto s = slang.globalMatch(text);
    while (s.hasNext())
        ++hits.slang[s.next().captured(1).toLower()];
    auto h = hedges.globalMatch(text);
    while (h.hasNext()) {
        h.next();
        ++hits.hedges;
    }
    return hits;
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

bool SpeechTipSource::isWarningActive(const QString &warningId) const
{
    if (!m_running || !m_mic->isActive() || !AppSettings::instance().useMicrophone())
        return false;
    const auto thresholds = AppSettings::instance().thresholds();
    if (warningId == QLatin1String("pace")) {
        const double wpm = m_mic->syllablesPerSecond() * 60.0 / kSyllablesPerWord * m_paceFactor;
        return m_mic->isSpeaking() && int(wpm) > thresholds.fastWpm
            && m_mic->recentSpeechMs() >= kFastSustainMs;
    }
    if (warningId != QLatin1String("fillers") || !AppSettings::instance().cloudAnalysis())
        return false;
    const auto cutoff = now() - 60000;
    int total = 0;
    for (const Chunk &chunk : m_recent) {
        if (chunk.atMs >= cutoff) {
            for (int count : chunk.fillers)
                total += count;
        }
    }
    return total >= thresholds.fillersPerMinute;
}

void SpeechTipSource::tip(const QString &key, TipCategory category, TipSeverity severity, const QString &text,
                          const QString &reason)
{
    const int cooldown = AppSettings::instance().thresholds().repeatCooldownSeconds;
    QElapsedTimer &last = m_lastTip[key];
    if (last.isValid() && last.elapsed() < cooldown * 1000LL)
        return;
    last.restart();
    Tip t;
    t.category = category;
    t.severity = severity;
    t.text = text;
    t.reason = reason;
    t.key = key;
    t.source = name();
    if (severity == TipSeverity::Warning && isWarningActive(key))
        t.warningId = key;
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
    QVariantList flagged;
    for (const QVariantMap &f : m_flagged)
        flagged << f;
    QVariantMap slang;
    for (auto it = m_slang.cbegin(); it != m_slang.cend(); ++it)
        slang.insert(it.key(), it.value());
    const double minutes = m_transcribedSpeechMs / 60000.0;
    const int whisperWpm = minutes > 0.1 ? int(qRound(m_words / minutes)) : 0;
    const int localWpm = m_localWpmCount ? int(m_localWpmSum / m_localWpmCount) : 0;
    return {
        {QStringLiteral("speechSeconds"), int(m_speechMs / 1000)},
        {QStringLiteral("longestMonologueSeconds"), int(m_longestRunMs / 1000)},
        {QStringLiteral("fillers"), fillers},
        {QStringLiteral("fillerTotal"), totalFillers},
        {QStringLiteral("fillersPerMinute"), minutes > 0.1 ? qRound(totalFillers / minutes * 10) / 10.0 : -1.0},
        {QStringLiteral("wordsPerMinute"), whisperWpm > 0 ? whisperWpm : localWpm},
        {QStringLiteral("paceSource"), whisperWpm > 0 ? QStringLiteral("Whisper") : QStringLiteral("local estimate")},
        {QStringLiteral("transcript"), m_transcript},
        {QStringLiteral("flagged"), flagged},
        {QStringLiteral("slang"), slang},
        {QStringLiteral("hedges"), m_hedges},
        {QStringLiteral("pausesPerMinute"), m_speechMs > 15000 ? qRound(m_pauses / (m_speechMs / 60000.0)) : -1},
        {QStringLiteral("quietPct"), m_speakingPolls > 0 ? qRound(100.0 * m_quietPolls / m_speakingPolls) : -1},
        {QStringLiteral("lastError"), m_lastError},
    };
}
