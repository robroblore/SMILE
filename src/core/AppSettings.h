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

    // How quickly SMILE speaks up. Each preset maps to the thresholds below.
    enum class Sensitivity { Relaxed, Balanced, Responsive };
    Sensitivity sensitivity() const;
    void setSensitivity(Sensitivity s);

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
    };
    Thresholds thresholds() const;

    bool debugMode() const;
    void setDebugMode(bool on);
    bool hapticFeedbackEnabled() const;
    void setHapticFeedbackEnabled(bool on);

    // The Training page's speech text, its mode and the structured sections
    QString speechText() const;
    void setSpeechText(const QString &text);
    bool speechAsBullets() const;
    void setSpeechAsBullets(bool bullets);
    int targetMinutes() const;
    void setTargetMinutes(int minutes);
    QString presentationType() const;
    void setPresentationType(const QString &type);
    QByteArray agendaJson() const;
    void setAgendaJson(const QByteArray &json);

signals:
    void changed();

private:
    AppSettings() = default;
    QString keyFromDotEnv() const;
};
