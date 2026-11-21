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

    int snapshotIntervalSeconds() const;
    void setSnapshotIntervalSeconds(int seconds);

signals:
    void changed();

private:
    AppSettings() = default;
    QString keyFromDotEnv() const;
};
