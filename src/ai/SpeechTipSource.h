#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QMap>
#include <QTimer>

#include "core/TipSource.h"

class AiClient;
class MicrophoneMonitor;

// Listens during a session with two systems in tandem:
//  * locally (twice a second): speaking/pausing, long monologues and an
//    estimated speaking rate from syllable peaks;
//  * Whisper (every ~9-15 s of speech): the exact words, filler words and the
//    true words-per-minute, which also calibrates the local rate estimate.
class SpeechTipSource : public TipSource
{
    Q_OBJECT

public:
    SpeechTipSource(MicrophoneMonitor *mic, AiClient *ai, QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("speech"); }
    void start() override;
    void stop() override;
    QVariantMap reportData() const override;

    static QMap<QString, int> countFillers(const QString &text);
    static int countWords(const QString &text);

signals:
    void transcribed(const QString &text); // for key-point tracking

private:
    void poll();
    void onChunk(const QByteArray &wav, int durationMs, int speechMs);
    void tip(const QString &key, TipCategory category, TipSeverity severity, const QString &text,
             const QString &reason);
    qint64 now() const { return m_session.elapsed(); }

    MicrophoneMonitor *m_mic;
    AiClient *m_ai;
    QTimer m_poll;
    QElapsedTimer m_session;
    bool m_running = false;
    bool m_acquired = false;
    bool m_monologueTipped = false;
    bool m_wasSpeaking = false;
    qint64 m_fastSince = -1;
    QHash<QString, QElapsedTimer> m_lastTip;

    // Local pace calibration: Whisper wpm / local estimate
    double m_paceFactor = 1.0;
    int m_paceSamples = 0;
    double m_localWpmSum = 0;
    int m_localWpmCount = 0;

    struct Chunk {
        qint64 atMs;
        QMap<QString, int> fillers;
        QString text;
    };
    QList<Chunk> m_recent; // last ~minute of transcripts

    QString m_transcript;
    QMap<QString, int> m_fillers;
    int m_words = 0;
    qint64 m_transcribedSpeechMs = 0;
    QString m_lastError;
    qint64 m_speechMs = 0;
    qint64 m_longestRunMs = 0;
};
