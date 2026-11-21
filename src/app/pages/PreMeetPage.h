#pragma once

#include <QList>
#include <QWidget>

class AiClient;
class CameraMonitor;
class CameraPreview;
class QAbstractButton;
class QLabel;
class QProgressBar;
class QPushButton;

// Phase 2: a readiness check before joining. A live camera preview, an AI
// look at your setup (framing, light, collar, clutter, posture), and a
// checklist for the things a camera can't see.
class PreMeetPage : public QWidget
{
    Q_OBJECT

public:
    PreMeetPage(CameraMonitor *camera, AiClient *ai, QWidget *parent = nullptr);

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

    CameraMonitor *m_camera;
    AiClient *m_ai;
    bool m_acquired = false;

    CameraPreview *m_preview;
    QPushButton *m_checkButton;
    QLabel *m_result;
    QList<QAbstractButton *> m_items;
    QLabel *m_progressText;
    QProgressBar *m_progress;
};
