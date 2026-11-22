#pragma once

#include <QTimer>
#include <QWidget>

#include "core/LiveSignals.h"

class CameraMonitor;
class CameraPreview;
class LevelMeter;
class MicrophoneMonitor;
class QGridLayout;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class SessionController;

// Debug view: shows what SMILE detects and why. Live camera with face
// landmarks, voice signals, current detections, the last cloud answer and a
// log of alerts and tips with their reasons.
class DebugWindow : public QWidget
{
    Q_OBJECT

public:
    DebugWindow(CameraMonitor *camera, MicrophoneMonitor *mic, SessionController *controller,
                QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void refresh();
    void updateInfraredPreview();
    void onEvent(const LiveSignals::Event &event);
    void setChip(int index, const QString &name, const QString &value, int rating, const QString &detail);

    CameraMonitor *m_camera;
    MicrophoneMonitor *m_mic;
    SessionController *m_controller;
    bool m_acquired = false;
    QTimer m_refresh;
    QTimer m_bannerTimer;

    CameraPreview *m_preview;
    QStackedWidget *m_cameraViews;
    CameraPreview *m_infraredPreview;
    QPushButton *m_infraredButton;
    CameraMonitor *m_infraredCamera = nullptr;
    bool m_infraredAcquired = false;
    QLabel *m_cameraStats;
    QLabel *m_banner;
    QGridLayout *m_chips;
    QList<QLabel *> m_chipLabels;
    LevelMeter *m_meter;
    QLabel *m_voice;
    QLabel *m_cloud;
    QPlainTextEdit *m_cloudJson;
    QListWidget *m_log;
};
