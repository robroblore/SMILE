#pragma once

#include <QTimer>
#include <QWidget>

#include "ai/SimulationEngine.h"

class AiClient;
class AvatarWidget;
class CameraMonitor;
class CameraPreview;
class LevelMeter;
class MicrophoneMonitor;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QVBoxLayout;
class SessionController;

// Training → Simulate a meeting: a video-call-like practice meeting with the
// SMILE avatar as the other participant, your camera beside it, live behaviour
// feedback, and a behaviour-focused result.
class SimulationView : public QWidget
{
    Q_OBJECT

public:
    SimulationView(SessionController *controller, AiClient *ai, MicrophoneMonitor *mic, CameraMonitor *camera,
                   QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void start();
    void addBubble(bool fromAvatar, const QString &text, const QString &intent);
    void showFeedback(const Tip &tip);
    void showResults(const QJsonObject &evaluation, const SessionReport &report);
    void applyTypeDefaults(const QString &type);
    void fitTiles();
    QWidget *segmented(const QStringList &labels, int checked, std::function<void(int)> onPick, QList<class QPushButton *> *out);

    SessionController *m_controller;
    AiClient *m_ai;
    MicrophoneMonitor *m_mic;
    CameraMonitor *m_camera;
    SimulationEngine *m_engine;

    QStackedWidget *m_stack;
    // setup
    QComboBox *m_type;
    QLineEdit *m_role;
    QLineEdit *m_audience;
    QPlainTextEdit *m_objectives;
    QList<QPushButton *> m_levels;
    class DurationEdit *m_length;
    QComboBox *m_voice;
    QLabel *m_setupNote;
    // live
    AvatarWidget *m_avatar;
    CameraPreview *m_self;
    QLabel *m_roleLabel;
    QLabel *m_status;
    QLabel *m_clock;
    QLabel *m_feedback;
    QLabel *m_noise;
    LevelMeter *m_meter;
    QScrollArea *m_chatScroll;
    QVBoxLayout *m_chat;
    QLineEdit *m_typed;
    QTimer m_clockTimer;
    QTimer m_feedbackTimer;
    // results
    QVBoxLayout *m_results;
};
