#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QObject>

#include "vision/FaceFrame.h"

// Everything SMILE is detecting right now, in one place. The detectors write
// here; the Debug view and tip explanations read from here. Nothing in it is
// stored or sent anywhere.
class LiveSignals : public QObject
{
    Q_OBJECT

public:
    static LiveSignals &instance();

    struct Vision {
        bool localAvailable = false;
        FaceFrame face;
        bool lookingLocal = false;
        float pitchBaseline = 0.6f;
        int smileLocal = -1;          // -1 unknown, 0 no, 1 yes (calibrated by the cloud)
        float smileThreshold = 0;     // mouth ratio that separates your smile from neutral
        float smileRef = 0;
        float neutralRef = 0;
        int smilingFused = -1;        // what SMILE believes now
        QString smilingSource;        // "cloud", "local", "unknown"
        float engagement = 0;         // 0..1
        QString expression;           // warm, positive, neutral_relaxed, neutral_flat, tense, unapproachable
        float expressionConfidence = 0;
        QString gaze;                 // camera, notes (down), away (side), screen
        float movement = 0;           // face movement, face-heights per second
        QString hands;                // cloud: resting, natural_gesture, distracting, ...
        QJsonObject cloud;            // last cloud answer
        qint64 cloudAgeMs = -1;
        int cloudLatencyMs = 0;
        QString cloudTrigger;         // why the last cloud check was made
        QString cloudError;
        int cloudChecks = 0;
        int noSmileSeconds = 0;
        int lookAwaySeconds = 0;
        int faceMissingSeconds = 0;
        float cameraFps = 0;
    };

    struct Voice {
        bool active = false;
        float level = 0;          // 0..1
        float levelDb = -90;
        float noiseFloorDb = -60;
        float thresholdDb = -50;
        bool speaking = false;
        int runSeconds = 0;       // talking without a real pause
        float syllablesPerSecond = 0;
        int wpmLocal = 0;         // estimated from syllables, calibrated by Whisper
        int wpmWhisper = 0;       // from the last transcript
        int fillersLastMinute = 0;
        float speechDb = -90;     // typical loudness while speaking (last ~20 s)
        float baselineDb = -90;   // your usual loudness this session
        int pausesPerMinute = 0;
        QString lastFlaggedWord;
        QString lastFillers;      // e.g. "um ×3, uh ×1"
        QString lastTranscript;
        int transcribeLatencyMs = 0;
        QString error;
    };

    struct Event {
        QDateTime time;
        enum Kind { Alert, Tip, Info } kind = Info;
        QString title;
        QString detail;
    };

    Vision vision;
    Voice voice;
    const QList<Event> &events() const { return m_events; }

    void notifyChanged() { emit changed(); }
    void addEvent(Event::Kind kind, const QString &title, const QString &detail);
    void clearEvents();

signals:
    void changed();
    void eventAdded(const LiveSignals::Event &event);

private:
    LiveSignals() = default;
    QList<Event> m_events;
};
