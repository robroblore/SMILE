#pragma once

#include <QWidget>

class AiClient;
class CameraMonitor;
class LevelMeter;
class MicrophoneMonitor;
class QComboBox;
class QLabel;
class QLineEdit;

// AI key, camera and microphone choice, privacy switch.
class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    SettingsPage(CameraMonitor *camera, MicrophoneMonitor *mic, AiClient *ai, QWidget *parent = nullptr);

signals:
    void openDebugRequested();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void refreshKeyStatus();
    void fillDevices();
    void describeSensitivity();

    CameraMonitor *m_camera;
    MicrophoneMonitor *m_mic;
    AiClient *m_ai;
    bool m_micAcquired = false;

    QLabel *m_keyStatus;
    QLineEdit *m_keyEdit;
    QLabel *m_testResult;
    QComboBox *m_cameraCombo;
    QComboBox *m_micCombo;
    LevelMeter *m_meter;
    QLabel *m_sensitivityText = nullptr;
};
