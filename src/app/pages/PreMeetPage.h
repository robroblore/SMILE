#pragma once

#include <QList>
#include <QWidget>

class AiClient;
class CameraMonitor;
class CameraPreview;
class LevelMeter;
class MicrophoneMonitor;
class QAbstractButton;
class QLabel;
class QPushButton;
class QVBoxLayout;

// Phase 2: a readiness check before joining. The camera preview is the focus:
// face tracking points are drawn live, and after "Check my setup" each issue is
// marked in red on the area it concerns, with what to change.
class PreMeetPage : public QWidget
{
    Q_OBJECT

public:
    PreMeetPage(CameraMonitor *camera, AiClient *ai, MicrophoneMonitor *mic, QWidget *parent = nullptr);

    static QString checkPrompt();

signals:
    void goToMeetRequested();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void runCheck();
    void updateProgress();
    void updateCameraState();
    void updateInfraredPreview();
    void updateSound();
    void showIssues(const QStringList &issues, const QStringList &good, const QString &advice);

    CameraMonitor *m_camera;
    AiClient *m_ai;
    bool m_acquired = false;
    MicrophoneMonitor *m_mic;
    bool m_micAcquired = false;
    qint64 m_listeningSince = 0;
    LevelMeter *m_meter;
    QLabel *m_sound;

    CameraPreview *m_preview;
    QWidget *m_infraredCard = nullptr;
    QPushButton *m_infraredButton = nullptr;
    CameraPreview *m_infraredPreview = nullptr;
    CameraMonitor *m_infraredCamera = nullptr;
    bool m_infraredAcquired = false;
    QPushButton *m_checkButton;
    QLabel *m_status;
    QVBoxLayout *m_issues;
    QLabel *m_tech;
    QList<QAbstractButton *> m_items;
    QLabel *m_progressText;
};
