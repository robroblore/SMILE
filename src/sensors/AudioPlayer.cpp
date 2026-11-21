#include "sensors/AudioPlayer.h"

#include <QAudioFormat>
#include <QAudioSink>
#include <QMediaDevices>
#include <QtEndian>
#include <cmath>

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject(parent)
{
    m_silent.setSingleShot(true);
    connect(&m_silent, &QTimer::timeout, this, &AudioPlayer::finished);
}

AudioPlayer::~AudioPlayer()
{
    stop();
}

bool AudioPlayer::play(const QByteArray &wav)
{
    stop();
    // Find the "fmt " and "data" chunks of the RIFF file
    if (!wav.startsWith("RIFF") || wav.size() < 44)
        return false;
    int rate = 24000, channels = 1, bits = 16;
    int pos = 12;
    m_pcm.clear();
    while (pos + 8 <= wav.size()) {
        const QByteArray id = wav.mid(pos, 4);
        const quint32 size = qFromLittleEndian<quint32>(wav.constData() + pos + 4);
        const int body = pos + 8;
        if (id == "fmt " && body + 16 <= wav.size()) {
            channels = qFromLittleEndian<quint16>(wav.constData() + body + 2);
            rate = int(qFromLittleEndian<quint32>(wav.constData() + body + 4));
            bits = qFromLittleEndian<quint16>(wav.constData() + body + 14);
        } else if (id == "data") {
            const quint32 available = quint32(wav.size() - body);
            m_pcm = wav.mid(body, int(size == 0xFFFFFFFF || size > available ? available : size));
            break;
        }
        pos = body + int(size) + int(size & 1);
    }
    if (m_pcm.isEmpty() || bits != 16)
        return false;

    // Loudness envelope for lip sync
    const int frame = qMax(1, rate * channels / 50); // 20 ms
    const auto *samples = reinterpret_cast<const qint16 *>(m_pcm.constData());
    const int count = int(m_pcm.size() / 2);
    m_envelope.clear();
    float peak = 0.001f;
    for (int i = 0; i < count; i += frame) {
        double e = 0;
        const int n = qMin(frame, count - i);
        for (int k = 0; k < n; ++k)
            e += double(samples[i + k]) * samples[i + k];
        const float rms = float(std::sqrt(e / qMax(1, n)) / 32768.0);
        m_envelope.append(rms);
        peak = qMax(peak, rms);
    }
    for (float &v : m_envelope)
        v = qMin(1.0f, v / (peak * 0.8f));

    if (qEnvironmentVariableIsSet("SMILE_MUTE_AUDIO")) {
        m_silentClock.start();
        m_silent.start(int(m_envelope.size()) * 20);
        return true;
    }

    QAudioFormat format;
    format.setSampleRate(rate);
    format.setChannelCount(channels);
    format.setSampleFormat(QAudioFormat::Int16);
    m_sink = new QAudioSink(QMediaDevices::defaultAudioOutput(), format, this);
    connect(m_sink, &QAudioSink::stateChanged, this, [this](QAudio::State state) {
        if (state == QAudio::IdleState || state == QAudio::StoppedState) {
            if (m_sink && m_sink->error() != QAudio::NoError && state == QAudio::StoppedState) {
                // fall through: treat as finished
            }
            if (m_sink && (state == QAudio::IdleState || m_sink->error() != QAudio::NoError)) {
                QAudioSink *sink = m_sink;
                m_sink = nullptr;
                sink->stop();
                sink->deleteLater();
                emit finished();
            }
        }
    });
    m_buffer.close();
    m_buffer.setData(m_pcm);
    m_buffer.open(QIODevice::ReadOnly);
    m_sink->start(&m_buffer);
    return true;
}

void AudioPlayer::stop()
{
    m_silent.stop();
    if (!m_sink)
        return;
    QAudioSink *sink = m_sink;
    m_sink = nullptr;
    sink->disconnect(this);
    sink->stop();
    sink->deleteLater();
    m_buffer.close();
}

qreal AudioPlayer::level() const
{
    if (m_envelope.isEmpty())
        return 0;
    if (m_silent.isActive()) {
        const int index = int(m_silentClock.elapsed() / 20);
        return index < m_envelope.size() ? m_envelope[index] : 0;
    }
    if (!m_sink)
        return 0;
    const qint64 us = m_sink->processedUSecs();
    const int index = int(us / 20000);
    return index < m_envelope.size() ? m_envelope[index] : 0;
}
