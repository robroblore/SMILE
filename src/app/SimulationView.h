#pragma once

#include <QTimer>
#include <QWidget>

#include "ai/SimulationEngine.h"

class AiClient;
class AvatarWidget;
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

// Training → Simulate a meeting: set up the meeting, talk with the SMILE avatar
// playing another participant, then see how you handled it.
class SimulationView : public QWidget
{
    Q_OBJECT

public:
    SimulationView(SessionController *controller, AiClient *ai, MicrophoneMonitor *mic, QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void start();
    void addBubble(bool fromAvatar, const QString &text, const QString &intent);
    void showResults(const QJsonObject &evaluation, const SessionReport &report);
    void applyTypeDefaults(const QString &type);

    SessionController *m_controller;
    AiClient *m_ai;
    MicrophoneMonitor *m_mic;
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
    QLabel *m_roleLabel;
    QLabel *m_status;
    QLabel *m_clock;
    LevelMeter *m_meter;
    QScrollArea *m_chatScroll;
    QVBoxLayout *m_chat;
    QLineEdit *m_typed;
    QTimer m_clockTimer;
    // results
    QVBoxLayout *m_results;
};
