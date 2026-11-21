#include "sensors/MicrophoneMonitor.h"

#include <QAudioDevice>
#include <QAudioSource>
#include <QCoreApplication>
#include <QMediaDevices>
#include <QtEndian>
#include <cmath>
#include <cstring>

#if QT_CONFIG(permissions)
#include <QPermissions>
#endif

#include "core/AppSettings.h"

namespace {
constexpr int kRate = 16000;
constexpr int kFrameSamples = 480; // 30 ms
constexpr int kFrameMs = 30;
constexpr int kHangoverMs = 300;
constexpr int kRunBreakMs = 1500;    // a pause this long ends a monologue
constexpr int kChunkMinMs = 9000;    // send audio for transcription every ~9-15 s
constexpr int kChunkMaxMs = 15000;
constexpr int kWindowFrames = 334;   // ~10 s of 30 ms frames
constexpr int kEnvelope = 11;        // ~330 ms around a candidate syllable

QByteArray wavFromPcm16(const QVector<qint16> &samples, int rate)
{
    const quint32 dataBytes = quint32(samples.size() * 2);
    QByteArray wav;
    wav.reserve(int(44 + dataBytes));
    auto u32 = [&](quint32 v) {
        char b[4];
        qToLittleEndian(v, b);
        wav.append(b, 4);
    };
    auto u16 = [&](quint16 v) {
        char b[2];
        qToLittleEndian(v, b);
        wav.append(b, 2);
    };
    wav.append("RIFF");
    u32(36 + dataBytes);
    wav.append("WAVEfmt ");
    u32(16);
    u16(1); // PCM
    u16(1); // mono
    u32(quint32(rate));
    u32(quint32(rate * 2));
    u16(2);
    u16(16);
    wav.append("data");
    u32(dataBytes);
    wav.append(reinterpret_cast<const char *>(samples.constData()), int(dataBytes));
    return wav;
}
} // namespace

MicrophoneMonitor::MicrophoneMonitor(QObject *parent)
    : QObject(parent)
{
    m_frame.reserve(kFrameSamples);
    m_window.fill(0, kWindowFrames);
}

double MicrophoneMonitor::syllablesPerSecond() const
{
    int speech = 0, syllables = 0;
    for (quint8 f : m_window) {
        speech += (f & 1) ? 1 : 0;
        syllables += (f & 2) ? 1 : 0;
    }
    return speech >= 30 ? syllables / (speech * kFrameMs / 1000.0) : 0.0; // need ~1 s of speech
}

int MicrophoneMonitor::recentSpeechMs() const
{
    int speech = 0;
    for (quint8 f : m_window)
        speech += (f & 1) ? 1 : 0;
    return speech * kFrameMs;
}

MicrophoneMonitor::~MicrophoneMonitor()
{
    stopMic();
}

void MicrophoneMonitor::acquire()
{
    if (m_users++ == 0)
        startMic();
}

void MicrophoneMonitor::release()
{
    if (m_users == 0)
        return;
    if (--m_users == 0)
        stopMic();
}

void MicrophoneMonitor::resetStats()
{
    m_speechMs = 0;
    m_inRun = false;
    m_runMs = 0;
    m_pauseMs = 0;
    m_longestRunMs = 0;
    m_pauses = 0;
    m_chunk.clear();
    m_chunkSpeechMs = 0;
    m_window.fill(0, kWindowFrames);
}

void MicrophoneMonitor::setUtterances(bool on)
{
    m_utterances = on;
    discardUtterance();
}

void MicrophoneMonitor::setIgnoreInput(bool ignore)
{
    m_ignore = ignore;
    if (ignore)
        discardUtterance();
}

void MicrophoneMonitor::discardUtterance()
{
    m_inUtt = false;
    m_utt.clear();
    m_uttSpeechMs = 0;
    m_uttSilenceMs = 0;
}

QByteArray MicrophoneMonitor::currentUtteranceWav() const
{
    return m_utt.isEmpty() ? QByteArray() : wavFromPcm16(m_utt, kRate);
}

void MicrophoneMonitor::setChunking(bool on)
{
    if (!on)
        flushChunk(true);
    m_chunking = on;
    m_chunk.clear();
    m_chunkSpeechMs = 0;
}

void MicrophoneMonitor::fail(const QString &message)
{
    m_error = message;
    emit errorOccurred(message);
}

void MicrophoneMonitor::startMic()
{
    if (m_source || !AppSettings::instance().useMicrophone())
        return;

#if QT_CONFIG(permissions)
    QMicrophonePermission permission;
    switch (qApp->checkPermission(permission)) {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(permission, this, [this](const QPermission &p) {
            if (p.status() == Qt::PermissionStatus::Granted && m_users > 0)
                startMic();
            else if (p.status() != Qt::PermissionStatus::Granted)
                fail(tr("Microphone access was denied. Allow it in your system privacy settings."));
        });
        return;
    case Qt::PermissionStatus::Denied:
        fail(tr("Microphone access is blocked. Allow it in your system privacy settings."));
        return;
    case Qt::PermissionStatus::Granted:
        break;
    }
#endif

    QAudioDevice device = QMediaDevices::defaultAudioInput();
    const QByteArray wanted = AppSettings::instance().microphoneId();
    for (const QAudioDevice &d : QMediaDevices::audioInputs()) {
        if (!wanted.isEmpty() && d.id() == wanted)
            device = d;
    }
    if (device.isNull()) {
        fail(tr("No microphone found."));
        return;
    }

    QAudioFormat format;
    format.setSampleRate(kRate);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    if (!device.isFormatSupported(format))
        format = device.preferredFormat(); // converted to 16 kHz mono below
    m_format = format;

    m_source = new QAudioSource(device, format, this);
    m_source->setBufferSize(format.bytesForDuration(200000));
    m_io = m_source->start();
    if (!m_io) {
        fail(tr("Could not open the microphone."));
        m_source->deleteLater();
        m_source = nullptr;
        return;
    }
    m_error.clear();
    m_input.clear();
    m_resamplePos = 0.0;
    connect(m_io, &QIODevice::readyRead, this, &MicrophoneMonitor::onReadyRead);
    emit activeChanged(true);
}

void MicrophoneMonitor::stopMic()
{
    if (!m_source)
        return;
    m_source->stop();
    m_source->deleteLater();
    m_source = nullptr;
    m_io = nullptr;
    if (m_speaking) {
        m_speaking = false;
        emit speakingChanged(false);
    }
    m_level = 0;
    emit levelChanged(0);
    emit activeChanged(false);
}

void MicrophoneMonitor::onReadyRead()
{
    if (m_io)
        processInput(m_io->readAll());
}

void MicrophoneMonitor::processInput(const QByteArray &data)
{
    const int channels = qMax(1, m_format.channelCount());
    const int bytesPerSample = m_format.bytesPerSample();
    if (bytesPerSample <= 0)
        return;
    const int frames = int(data.size() / (bytesPerSample * channels));
    const char *p = data.constData();

    // 1) Down-mix to mono float
    for (int i = 0; i < frames; ++i) {
        float sum = 0;
        for (int c = 0; c < channels; ++c) {
            const char *s = p + (i * channels + c) * bytesPerSample;
            switch (m_format.sampleFormat()) {
            case QAudioFormat::Int16:
                sum += qFromLittleEndian<qint16>(s) / 32768.0f;
                break;
            case QAudioFormat::Int32:
                sum += float(qFromLittleEndian<qint32>(s) / 2147483648.0);
                break;
            case QAudioFormat::Float: {
                float f;
                memcpy(&f, s, 4);
                sum += f;
                break;
            }
            case QAudioFormat::UInt8:
                sum += (quint8(*s) - 128) / 128.0f;
                break;
            default:
                break;
            }
        }
        m_input.append(sum / channels);
    }

    // 2) Resample to 16 kHz (linear interpolation is fine for speech)
    const double step = double(m_format.sampleRate()) / kRate;
    int consumed = 0;
    while (m_resamplePos + 1 < m_input.size()) {
        const int i = int(m_resamplePos);
        const double frac = m_resamplePos - i;
        const float sample = float(m_input[i] * (1.0 - frac) + m_input[i + 1] * frac);
        m_frame.append(sample);
        if (m_frame.size() == kFrameSamples) {
            processFrame(m_frame.constData(), kFrameSamples);
            m_frame.clear();
        }
        m_resamplePos += step;
        consumed = int(m_resamplePos);
    }
    if (consumed > 0) {
        m_input.remove(0, qMin(consumed, int(m_input.size())));
        m_resamplePos -= consumed;
    }
}

void MicrophoneMonitor::processFrame(const float *samples, int count)
{
    double energy = 0;
    for (int i = 0; i < count; ++i)
        energy += double(samples[i]) * samples[i];
    const double rms = std::sqrt(energy / count);
    const double db = 20.0 * std::log10(rms + 1e-9);

    // Adaptive noise floor: drops quickly, rises slowly (only while quiet)
    if (db < m_noiseFloorDb)
        m_noiseFloorDb = 0.8 * m_noiseFloorDb + 0.2 * db;
    else if (!m_speaking)
        m_noiseFloorDb += 0.003 * (db - m_noiseFloorDb);
    const double threshold = qMax(m_noiseFloorDb + 10.0, -50.0);
    const bool voiced = db > threshold;
    m_levelDb = db;
    m_thresholdDb = threshold;

    const bool wasSpeaking = m_speaking;
    if (voiced) {
        m_speaking = true;
        m_hangMs = kHangoverMs;
    } else if (m_hangMs > 0) {
        m_hangMs -= kFrameMs;
    } else {
        m_speaking = false;
    }
    if (m_speaking != wasSpeaking)
        emit speakingChanged(m_speaking);

    // Talk-time statistics
    if (m_speaking) {
        if (m_pauseMs >= 400 && m_pauseMs <= 2500)
            ++m_pauses;
        m_speechMs += kFrameMs;
        if (!m_inRun) {
            m_inRun = true;
            m_runMs = 0;
        }
        m_pauseMs = 0;
    } else {
        m_pauseMs += kFrameMs;
        if (m_inRun && m_pauseMs > kRunBreakMs) {
            m_longestRunMs = qMax(m_longestRunMs, m_runMs);
            m_inRun = false;
            m_runMs = 0;
        }
    }
    if (m_inRun)
        m_runMs += kFrameMs;

    // Syllable nuclei: a local peak in loudness, clearly above its surroundings,
    // at least 120 ms after the previous one. Counting them gives a speaking
    // rate long before a transcript is available.
    m_envelope.append(db);
    if (m_envelope.size() > kEnvelope)
        m_envelope.removeFirst();
    bool syllable = false;
    if (m_envelope.size() == kEnvelope) {
        const int mid = kEnvelope / 2;
        const double centre = m_envelope[mid];
        double lowest = centre;
        bool isPeak = true;
        for (int i = 0; i < kEnvelope; ++i) {
            if (i != mid && m_envelope[i] > centre)
                isPeak = false;
            lowest = qMin(lowest, m_envelope[i]);
        }
        if (isPeak && m_speaking && centre > threshold + 3.0 && centre - lowest >= 5.0
            && m_frameIndex - m_lastSyllable >= 4) {
            syllable = true;
            m_lastSyllable = m_frameIndex;
        }
    }
    m_window[m_windowPos] = quint8((m_speaking ? 1 : 0) | (syllable ? 2 : 0));
    m_windowPos = (m_windowPos + 1) % kWindowFrames;
    ++m_frameIndex;

    // Meter
    const qreal level = qBound<qreal>(0.0, (db + 60.0) / 55.0, 1.0);
    m_level = 0.6 * m_level + 0.4 * level;
    if (!m_levelTimer.isValid() || m_levelTimer.elapsed() > 50) {
        m_levelTimer.restart();
        emit levelChanged(m_level);
    }

    // Utterances: one sentence at a time, sent once you pause
    if (m_utterances && !m_ignore) {
        QVector<qint16> frame(count);
        for (int i = 0; i < count; ++i)
            frame[i] = qint16(qBound(-1.0f, samples[i], 1.0f) * 32767);
        if (m_speaking) {
            if (!m_inUtt) {
                m_inUtt = true;
                m_utt = m_preroll; // keep the start of the first word
                m_uttSpeechMs = 0;
            }
            m_utt += frame;
            m_uttSpeechMs += kFrameMs;
            m_uttSilenceMs = 0;
        } else if (m_inUtt) {
            m_utt += frame;
            m_uttSilenceMs += kFrameMs;
            if (m_uttSilenceMs >= 1100) {
                if (m_uttSpeechMs >= 600)
                    emit utteranceReady(wavFromPcm16(m_utt, kRate), m_uttSpeechMs);
                discardUtterance();
            }
        }
        m_preroll += frame;
        if (m_preroll.size() > kFrameSamples * 10)
            m_preroll.remove(0, m_preroll.size() - kFrameSamples * 10);
    }

    // Chunking
    if (m_chunking) {
        for (int i = 0; i < count; ++i)
            m_chunk.append(qint16(qBound(-1.0f, samples[i], 1.0f) * 32767));
        if (m_speaking)
            m_chunkSpeechMs += kFrameMs;
        flushChunk(false);
    }
}

void MicrophoneMonitor::flushChunk(bool force)
{
    const int durationMs = int(m_chunk.size() * 1000LL / kRate);
    const bool atPause = !m_speaking && m_pauseMs >= 400;
    if (!force && !(durationMs >= kChunkMinMs && atPause) && durationMs < kChunkMaxMs)
        return;
    if (m_chunkSpeechMs >= 1500) // don't send silence
        emit chunkReady(wavFromPcm16(m_chunk, kRate), durationMs, m_chunkSpeechMs);
    m_chunk.clear();
    m_chunkSpeechMs = 0;
}
