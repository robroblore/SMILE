#pragma once

#include <QAudioFormat>
#include <QElapsedTimer>
#include <QObject>
#include <QVector>

class QAudioSource;
class QIODevice;

// Listens to the microphone (never records to disk). It works out locally
// when you are speaking, how long you've been talking without a break, and
// collects short audio chunks that can be transcribed for filler words.
class MicrophoneMonitor : public QObject
{
    Q_OBJECT

public:
    explicit MicrophoneMonitor(QObject *parent = nullptr);
    ~MicrophoneMonitor() override;

    void acquire();
    void release();
    bool isActive() const { return m_source != nullptr; }
    QString errorString() const { return m_error; }

    qreal level() const { return m_level; } // 0..1, for meters
    bool isSpeaking() const { return m_speaking; }

    // Statistics since resetStats()
    void resetStats();
    qint64 speechMs() const { return m_speechMs; }
    qint64 currentRunMs() const { return m_inRun ? m_runMs : 0; } // talking without a real pause
    qint64 longestRunMs() const { return qMax(m_longestRunMs, currentRunMs()); }

    // When on, audio is gathered into ~25 s chunks and chunkReady is emitted.
    void setChunking(bool on);

signals:
    void levelChanged(qreal level);
    void speakingChanged(bool speaking);
    void chunkReady(const QByteArray &wav, int durationMs, int speechMs);
    void activeChanged(bool active);
    void errorOccurred(const QString &message);

private:
    friend class MicrophoneMonitorTest;
    void processInput(const QByteArray &data); // raw bytes in m_format
    void startMic();
    void stopMic();
    void onReadyRead();
    void processFrame(const float *samples, int count);
    void flushChunk(bool force);
    void fail(const QString &message);

    int m_users = 0;
    QAudioSource *m_source = nullptr;
    QIODevice *m_io = nullptr;
    QAudioFormat m_format;
    QString m_error;

    // Resampling to 16 kHz mono
    QVector<float> m_input;
    double m_resamplePos = 0.0;
    QVector<float> m_frame; // 30 ms at 16 kHz

    // Voice activity
    double m_noiseFloorDb = -55.0;
    bool m_speaking = false;
    int m_hangMs = 0;
    qreal m_level = 0;
    QElapsedTimer m_levelTimer;

    // Stats
    qint64 m_speechMs = 0;
    bool m_inRun = false;
    qint64 m_runMs = 0;
    qint64 m_pauseMs = 0;
    qint64 m_longestRunMs = 0;

    // Chunking for transcription
    bool m_chunking = false;
    QVector<qint16> m_chunk;
    int m_chunkSpeechMs = 0;
};
