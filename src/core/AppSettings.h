#pragma once

#include <QObject>
#include <QString>

// User preferences and the AI key. The key is never compiled into SMILE: it
// comes from (in order) the Settings page, the TOGETHER_API_KEY environment
// variable, or a .env file in the working directory or next to the app.
class AppSettings : public QObject
{
    Q_OBJECT

public:
    static AppSettings &instance();

    enum class KeySource { None, Settings, Environment, DotEnv };

    QString apiKey() const;
    KeySource apiKeySource() const;
    void setApiKey(const QString &key); // empty clears the stored key

    QString apiBaseUrl() const;
    QString visionModel() const;
    QString chatModel() const;
    static QString fastVisionModel() { return QStringLiteral("nvidia/NVIDIA-Nemotron-3.5-Super-VL"); }
    QString transcribeModel() const;

    bool useCamera() const;
    void setUseCamera(bool on);
    bool useMicrophone() const;
    void setUseMicrophone(bool on);
    bool cloudAnalysis() const; // allowed to send snapshots/audio to the AI
    void setCloudAnalysis(bool on);

    QByteArray cameraId() const; // empty = system default
    void setCameraId(const QByteArray &id);
    QByteArray microphoneId() const;
    void setMicrophoneId(const QByteArray &id);

    // How often the cloud vision model looks (it is also asked immediately
    // when the local face tracker notices a change).
    int snapshotIntervalSeconds() const;
    void setSnapshotIntervalSeconds(int seconds);

    // How strict / talkative SMILE is, 1 (relaxed) .. 5 (strict). Live meetings
    // and training have separate levels; training is stricter by default.
    int liveLevel() const;
    void setLiveLevel(int level);
    int trainingLevel() const;
    void setTrainingLevel(int level);
    // Set while a training-type session (rehearsal, practice, simulation) runs
    void setTrainingActive(bool training) { m_trainingActive = training; }
    bool trainingActive() const { return m_trainingActive; }

    // AI intelligence: 0 standard (fast, cheapest), 1 enhanced, 2 maximum
    int intelligence() const;
    void setIntelligence(int level);
    // How much the avatar steps in about your behaviour in simulations: 0 off .. 3 strict
    int avatarIntervention() const;
    void setAvatarIntervention(int level);

    struct Thresholds {
        int noSmileSeconds;      // no smile for this long -> "Try smiling more"
        int lookAwaySeconds;     // looking away from the camera
        int faceMissingSeconds;  // face out of frame
        int postureSeconds;      // same bad posture for this long
        int fastWpm;             // speaking faster than this
        int fillersPerMinute;    // filler words in the last minute
        int monologueSeconds;    // talking without a pause
        int repeatCooldownSeconds; // before the same tip can repeat
        int meetingGapSeconds;   // minimum quiet time between pop-ups in a meeting
        int trainingGapSeconds;  // ... and in training
        int faceCoveredSeconds;  // hand or object over the face
    };
    // For the session type that's running (training or live)
    Thresholds thresholds() const;
    static Thresholds thresholdsFor(int level);

    bool debugMode() const;
    // Live tips muted until the user unmutes them (companion menu, tray, Meet page)
    bool tipsMuted() const;
    void setTipsMuted(bool muted);
    void setDebugMode(bool on);

    // Small timer next to the floating avatar (off by default)
    bool sectionTimer() const;
    void setSectionTimer(bool on);
    // Move to the next section automatically when you start talking about it
    bool autoAdvance() const;
    void setAutoAdvance(bool on);

    // Appearance: 0 follow system, 1 light (white & blue), 2 dark
    int themeMode() const;
    void setThemeMode(int mode);

    // Voice of the avatar in meeting simulations (Together "Kokoro" voices)
    QString avatarVoice() const;
    void setAvatarVoice(const QString &voice);
    QString speechModel() const;
    QString dialogueModel() const; // fast model for the live simulation conversation
    bool physicalHapticsEnabled() const;
    void setPhysicalHapticsEnabled(bool on);
    int hapticDurationSeconds() const;
    void setHapticDurationSeconds(int seconds);
    int hapticIntensity() const;
    void setHapticIntensity(int intensity);
    int hapticPulseIntervalMs() const;
    void setHapticPulseIntervalMs(int milliseconds);

    // The Training page's speech text, its mode and the structured sections
    QString speechText() const;
    void setSpeechText(const QString &text);
    bool speechAsBullets() const;
    void setSpeechAsBullets(bool bullets);
    int targetMinutes() const;
    void setTargetMinutes(int minutes);
    int targetSeconds() const;
    void setTargetSeconds(int seconds);
    QString presentationType() const;
    void setPresentationType(const QString &type);
    QByteArray agendaJson() const;
    void setAgendaJson(const QByteArray &json);

signals:
    void changed();
    void cameraSettingsChanged();

private:
    AppSettings() = default;
    bool m_trainingActive = false;
    QString keyFromDotEnv() const;
};
