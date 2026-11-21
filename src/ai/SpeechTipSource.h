#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QMap>
#include <QTimer>

#include "core/TipSource.h"

class AiClient;
class MicrophoneMonitor;

// Listens during a session. Locally it notices long monologues; if AI
// analysis is allowed, ~25 s chunks of speech are transcribed to count filler
// words ("uhm", "uh", "you know") and measure speaking pace.
class SpeechTipSource : public TipSource
{
    Q_OBJECT

public:
    SpeechTipSource(MicrophoneMonitor *mic, AiClient *ai, QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("speech"); }
    void start() override;
    void stop() override;
    QVariantMap reportData() const override;

    // Exposed for testing
    static QMap<QString, int> countFillers(const QString &text);
    static int countWords(const QString &text);

private:
    void checkMonologue();
    void onChunk(const QByteArray &wav, int durationMs, int speechMs);
    void tip(const QString &key, TipCategory category, TipSeverity severity, const QString &text, int cooldownS);

    MicrophoneMonitor *m_mic;
    AiClient *m_ai;
    QTimer m_poll;
    bool m_running = false;
    bool m_acquired = false;
    bool m_monologueTipped = false;
    QHash<QString, QElapsedTimer> m_lastTip;

    QString m_transcript;
    QMap<QString, int> m_fillers;
    int m_words = 0;
    qint64 m_transcribedSpeechMs = 0;
    int m_pendingChunks = 0;
    QString m_lastError;
    // kept after stop() for the report
    qint64 m_speechMs = 0;
    qint64 m_longestRunMs = 0;
};
