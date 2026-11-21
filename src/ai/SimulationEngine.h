#pragma once

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QTimer>

#include "companion/AvatarWidget.h"
#include "core/Agenda.h"
#include "core/SessionReport.h"
#include "core/Tip.h"

class AiClient;
class AudioPlayer;
class MicrophoneMonitor;
class SessionController;

struct SimulationConfig {
    QString meetingType;  // "Business proposal", "Job interview"…
    QString role;         // who the avatar plays: "a critical investor"
    QString audience;     // context about the audience
    QString objectives;   // what the user wants to achieve
    QString difficulty;   // beginner, intermediate, advanced
    QString voice;        // Kokoro voice id
    int seconds = 300;
    QString speech;       // the user's speech or notes
    Agenda sections;
};

// Runs a realistic meeting with the SMILE avatar as another participant. The
// AI plays a role, reacts to what you actually say, asks unexpected questions,
// challenges, interrupts (harder levels) and finally evaluates how you did.
// Your voice is transcribed by Whisper; the avatar speaks with a synthetic voice.
class SimulationEngine : public QObject
{
    Q_OBJECT

public:
    SimulationEngine(AiClient *ai, MicrophoneMonitor *mic, SessionController *controller, QObject *parent = nullptr);

    void start(const SimulationConfig &config);
    void end();       // wrap up now
    void abort();     // stop without evaluating
    bool isRunning() const { return m_running; }
    void sendTyped(const QString &text);
    AudioPlayer *player() const { return m_player; }
    int elapsedSeconds() const { return m_clock.isValid() ? int(m_clock.elapsed() / 1000) : 0; }
    int totalSeconds() const { return m_config.seconds; }

    static QString systemPrompt(const SimulationConfig &config);

signals:
    void stateChanged(AvatarWidget::State state);
    void moodChanged(const QString &mood);
    void message(bool fromAvatar, const QString &text, const QString &intent);
    void statusChanged(const QString &status);
    void finished(const QJsonObject &evaluation, const SessionReport &report);
    void liveTip(const Tip &tip); // behaviour feedback shown during the simulation
    void failed(const QString &error);

private:
    void onUtterance(const QByteArray &wav, int speechMs);
    void userSaid(const QString &text, bool interrupted);
    void respond(const QString &instruction);
    void say(const QString &text, const QString &mood, const QString &intent, bool last);
    void checkInterrupt();
    void onBehaviourTip(const Tip &tip);
    void intervene(const QString &line);
    void listen();
    void finish();
    void evaluate(const SessionReport &report);

    AiClient *m_ai;
    MicrophoneMonitor *m_mic;
    SessionController *m_controller;
    AudioPlayer *m_player;
    SimulationConfig m_config;
    QJsonArray m_history;
    QStringList m_transcript; // "Avatar: …" / "You: …"
    QTimer m_watch;
    QElapsedTimer m_clock;
    bool m_running = false;
    bool m_busy = false;      // waiting for the AI
    bool m_ending = false;
    bool m_micAcquired = false;
    int m_interruptions = 0;
    int m_turns = 0;
    int m_interventions = 0;
    QElapsedTimer m_lastIntervention;
    QString m_pendingIntervention;
};
