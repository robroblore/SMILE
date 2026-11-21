#pragma once

#include <QBuffer>
#include <QElapsedTimer>
#include <QTimer>
#include <QObject>
#include <QVector>

class QAudioSink;

// Plays a WAV clip (the avatar's voice) and reports how loud it is right now,
// which drives the avatar's lip sync.
class AudioPlayer : public QObject
{
    Q_OBJECT

public:
    explicit AudioPlayer(QObject *parent = nullptr);
    ~AudioPlayer() override;

    bool play(const QByteArray &wav); // false if the format can't be played
    void stop();
    bool isPlaying() const { return m_sink != nullptr || m_silent.isActive(); }
    qreal level() const; // 0..1 at the current playback position

signals:
    void finished();

private:
    QAudioSink *m_sink = nullptr;
    QBuffer m_buffer;
    QByteArray m_pcm;
    QVector<float> m_envelope; // 20 ms windows, 0..1
    // SMILE_MUTE_AUDIO: play silently with the same timing (tests, demos)
    QTimer m_silent;
    QElapsedTimer m_silentClock;
};
