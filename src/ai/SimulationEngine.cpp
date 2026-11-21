#include "ai/SimulationEngine.h"

#include <QJsonDocument>
#include <QRandomGenerator>

#include "ai/AiClient.h"
#include "core/AppSettings.h"
#include "core/LiveSignals.h"
#include "core/SessionController.h"
#include "core/TimeFormat.h"
#include "sensors/AudioPlayer.h"
#include "sensors/MicrophoneMonitor.h"

namespace {
int interruptAfterMs(const QString &difficulty)
{
    if (difficulty == QLatin1String("advanced"))
        return 28000;
    if (difficulty == QLatin1String("intermediate"))
        return 50000;
    return -1; // beginners are never interrupted
}

QJsonObject msg(const QString &role, const QString &content)
{
    return {{QStringLiteral("role"), role}, {QStringLiteral("content"), content}};
}
} // namespace

SimulationEngine::SimulationEngine(AiClient *ai, MicrophoneMonitor *mic, SessionController *controller, QObject *parent)
    : QObject(parent)
    , m_ai(ai)
    , m_mic(mic)
    , m_controller(controller)
    , m_player(new AudioPlayer(this))
{
    m_watch.setInterval(1000);
    connect(&m_watch, &QTimer::timeout, this, &SimulationEngine::checkInterrupt);
    connect(m_mic, &MicrophoneMonitor::utteranceReady, this, &SimulationEngine::onUtterance);
    connect(m_player, &AudioPlayer::finished, this, [this] {
        if (!m_running)
            return;
        if (m_ending) {
            finish();
            return;
        }
        // Small gap so the end of the avatar's voice isn't picked up as yours
        QTimer::singleShot(350, this, &SimulationEngine::listen);
    });
}

QString SimulationEngine::systemPrompt(const SimulationConfig &c)
{
    QString level;
    if (c.difficulty == QLatin1String("advanced"))
        level = QStringLiteral(
            "ADVANCED: a high-pressure meeting. You are sceptical and hard to convince. Raise difficult objections, "
            "ask for evidence and numbers, challenge assumptions, ask unexpected questions, express disagreement and "
            "push back on vague answers. Stay professional, never rude.");
    else if (c.difficulty == QLatin1String("intermediate"))
        level = QStringLiteral(
            "INTERMEDIATE: a realistic meeting. Ask more challenging questions, request clarification when something "
            "is vague, ask for supporting evidence sometimes and occasionally raise an objection.");
    else
        level = QStringLiteral(
            "BEGINNER: a friendly, supportive participant. Ask predictable, open questions and react encouragingly. "
            "Never interrupt.");

    QString material = c.speech.left(6000);
    if (!c.sections.isEmpty()) {
        material += QStringLiteral("\n\nTheir planned sections:\n");
        for (const AgendaPoint &p : c.sections)
            material += QStringLiteral("- %1: %2\n").arg(p.title, p.keyPoints.join(QStringLiteral("; ")));
    }
    return QStringLiteral(
               "You are role-playing %1 in a %2 held over video. The other participant is the user, who is "
               "practising for the real thing.\nAudience and context: %3\nThe user's objectives: %4\n"
               "The user's prepared material (you have NOT seen it in the meeting; use it only to ask relevant, "
               "probing questions):\n%5\n\n%6\n\n"
               "Behave like a real person: keep each turn short (1-3 sentences, at most 45 words), ask one question "
               "at a time, react to what the user actually said, refer to their words, ask follow-ups when an answer "
               "is weak, and don't repeat questions. Never coach or give feedback during the meeting; stay in "
               "character. Spoken style, no lists or markdown.\n"
               "Reply with ONLY JSON: {\"say\": \"what you say\", \"intent\": \"greeting|question|challenge|clarify|"
               "interrupt|objection|evidence|elaborate|agree|wrap_up\", \"mood\": \"friendly|neutral|impressed|"
               "skeptical|confused|concerned\", \"end\": false}. Set \"end\" to true only on your final closing line.")
        .arg(c.role.isEmpty() ? QStringLiteral("a meeting participant") : c.role,
             c.meetingType.isEmpty() ? QStringLiteral("professional meeting") : c.meetingType,
             c.audience.isEmpty() ? QStringLiteral("a typical professional audience") : c.audience,
             c.objectives.isEmpty() ? QStringLiteral("to communicate their message clearly") : c.objectives,
             material.isEmpty() ? QStringLiteral("(none given)") : material, level);
}

void SimulationEngine::start(const SimulationConfig &config)
{
    if (m_running)
        return;
    if (!m_ai->isConfigured()) {
        emit failed(tr("The meeting simulation needs an AI key (Settings → AI coach)."));
        return;
    }
    m_config = config;
    m_history = QJsonArray{msg(QStringLiteral("system"), systemPrompt(config))};
    m_transcript.clear();
    m_running = true;
    m_ending = false;
    m_busy = false;
    m_interruptions = 0;
    m_turns = 0;
    m_clock.start();

    // Run a normal session alongside, so camera and voice are analysed too
    AgendaPoint simulation;
    simulation.title = config.meetingType.isEmpty() ? tr("Meeting simulation") : config.meetingType;
    simulation.targetSeconds = config.seconds;
    for (const AgendaPoint &p : config.sections)
        simulation.keyPoints += p.keyPoints;
    m_controller->startWith({simulation}, SessionMode::Simulation);

    if (AppSettings::instance().useMicrophone()) {
        m_mic->acquire();
        m_micAcquired = true;
        m_mic->setUtterances(true);
    }
    m_watch.start();
    emit moodChanged(QStringLiteral("friendly"));
    respond(QStringLiteral("Open the meeting now: greet the user briefly in character and invite them to begin."));
}

void SimulationEngine::listen()
{
    if (!m_running)
        return;
    m_mic->setIgnoreInput(false);
    emit stateChanged(AvatarWidget::State::Listening);
    emit statusChanged(m_micAcquired ? tr("Your turn. Speak, then pause when you're done.") : tr("Your turn. Type your answer below."));
}

void SimulationEngine::onUtterance(const QByteArray &wav, int speechMs)
{
    Q_UNUSED(speechMs)
    if (!m_running || m_busy || m_player->isPlaying())
        return;
    m_busy = true;
    emit stateChanged(AvatarWidget::State::Thinking);
    emit statusChanged(tr("Listening to what you said…"));
    m_ai->transcribe(wav, [this](const QString &text, const QString &error) {
        if (!m_running)
            return;
        m_busy = false;
        if (!error.isEmpty() || text.trimmed().isEmpty()) {
            listen();
            return;
        }
        userSaid(text.trimmed(), false);
    });
}

void SimulationEngine::sendTyped(const QString &text)
{
    if (!m_running || m_busy || text.trimmed().isEmpty())
        return;
    m_player->stop();
    userSaid(text.trimmed(), false);
}

void SimulationEngine::userSaid(const QString &text, bool interrupted)
{
    emit message(false, text, interrupted ? QStringLiteral("interrupted") : QString());
    m_transcript << tr("You: %1").arg(text);
    m_controller->addTranscript(text);
    m_history.append(msg(QStringLiteral("user"), text));
    ++m_turns;

    const int elapsed = elapsedSeconds();
    QString instruction = QStringLiteral("(Meeting time: %1 of %2.)").arg(formatDuration(elapsed), formatDuration(m_config.seconds));
    if (interrupted)
        instruction += QStringLiteral(" The user has been talking for a long time without getting to the point. Interrupt "
                                      "them now, politely but firmly, with a pointed question about what they just said.");
    else if (elapsed >= m_config.seconds) {
        instruction += QStringLiteral(" Time is up: close the meeting naturally in one or two sentences and set end to true.");
        m_ending = true;
    } else if (elapsed >= m_config.seconds - 45)
        instruction += QStringLiteral(" The meeting is almost over: ask a final question or start wrapping up.");
    respond(instruction);
}

void SimulationEngine::respond(const QString &instruction)
{
    if (!m_running)
        return;
    m_busy = true;
    m_mic->setIgnoreInput(true);
    emit stateChanged(AvatarWidget::State::Thinking);
    emit statusChanged(tr("%1 is thinking…").arg(m_config.role.isEmpty() ? tr("SMILE") : m_config.role));
    QJsonArray messages = m_history;
    messages.append(msg(QStringLiteral("system"), instruction));
    m_ai->chatMessages(messages, 1200, [this](const QString &reply, const QString &error) {
        if (!m_running)
            return;
        m_busy = false;
        if (!error.isEmpty()) {
            emit statusChanged(tr("The AI didn't answer (%1). Keep going or try again.").arg(error));
            listen();
            return;
        }
        QJsonObject j = AiClient::extractJson(reply);
        QString text = j.value(QLatin1String("say")).toString().trimmed();
        if (text.isEmpty())
            text = reply.trimmed(); // not JSON: use the plain text
        m_history.append(msg(QStringLiteral("assistant"), QString::fromUtf8(QJsonDocument(j).toJson(QJsonDocument::Compact))));
        const bool last = j.value(QLatin1String("end")).toBool() || m_ending;
        say(text, j.value(QLatin1String("mood")).toString(QStringLiteral("neutral")),
            j.value(QLatin1String("intent")).toString(), last);
    });
}

void SimulationEngine::say(const QString &text, const QString &mood, const QString &intent, bool last)
{
    m_ending = last;
    emit moodChanged(mood);
    emit message(true, text, intent);
    m_transcript << tr("Avatar: %1").arg(text);
    LiveSignals::instance().addEvent(LiveSignals::Event::Info, tr("Simulation: %1").arg(intent), text);
    m_mic->setIgnoreInput(true);
    m_ai->speak(text, m_config.voice.isEmpty() ? AppSettings::instance().avatarVoice() : m_config.voice,
                [this](const QByteArray &wav, const QString &error) {
                    if (!m_running)
                        return;
                    if (error.isEmpty() && m_player->play(wav)) {
                        emit stateChanged(AvatarWidget::State::Speaking);
                        emit statusChanged(QString());
                        return;
                    }
                    // No voice: show the text and carry on
                    emit statusChanged(tr("(Voice unavailable: %1)").arg(error.isEmpty() ? tr("audio output") : error));
                    if (m_ending)
                        QTimer::singleShot(2500, this, &SimulationEngine::finish);
                    else
                        QTimer::singleShot(600, this, &SimulationEngine::listen);
                });
}

void SimulationEngine::checkInterrupt()
{
    if (!m_running || m_busy || m_player->isPlaying())
        return;
    // Out of time while the user is still talking: let them finish this sentence
    if (elapsedSeconds() > m_config.seconds + 60 && !m_ending) {
        m_ending = true;
        respond(QStringLiteral("(The meeting has run over time.) Close the meeting now in one sentence and set end to true."));
        return;
    }
    const int limit = interruptAfterMs(m_config.difficulty);
    if (limit < 0 || m_mic->currentUtteranceMs() < limit)
        return;
    // Interrupt occasionally, not every time
    if (QRandomGenerator::global()->bounded(100) > (m_config.difficulty == QLatin1String("advanced") ? 60 : 35))
        return;
    const QByteArray wav = m_mic->currentUtteranceWav();
    m_mic->discardUtterance();
    m_busy = true;
    ++m_interruptions;
    emit stateChanged(AvatarWidget::State::Thinking);
    m_ai->transcribe(wav, [this](const QString &text, const QString &) {
        if (!m_running)
            return;
        m_busy = false;
        userSaid(text.trimmed().isEmpty() ? tr("(long answer)") : text.trimmed(), true);
    });
}

void SimulationEngine::end()
{
    if (!m_running)
        return;
    if (m_busy || m_player->isPlaying()) {
        m_player->stop();
    }
    m_ending = true;
    respond(QStringLiteral("The user has to leave now. Close the meeting politely in one sentence and set end to true."));
}

void SimulationEngine::abort()
{
    if (!m_running)
        return;
    m_running = false;
    m_watch.stop();
    m_player->stop();
    m_mic->setUtterances(false);
    m_mic->setIgnoreInput(false);
    if (m_micAcquired) {
        m_mic->release();
        m_micAcquired = false;
    }
    if (m_controller->isRunning())
        m_controller->stop();
}

void SimulationEngine::finish()
{
    if (!m_running)
        return;
    m_running = false;
    m_watch.stop();
    m_player->stop();
    m_mic->setUtterances(false);
    m_mic->setIgnoreInput(false);
    if (m_micAcquired) {
        m_mic->release();
        m_micAcquired = false;
    }
    emit stateChanged(AvatarWidget::State::Thinking);
    emit statusChanged(tr("Evaluating how the meeting went…"));

    // Collect the delivery report from the session that ran alongside
    SessionReport report;
    QMetaObject::Connection c = connect(m_controller, &SessionController::finished, this,
                                        [&report](const SessionReport &r) { report = r; });
    if (m_controller->isRunning())
        m_controller->stop();
    disconnect(c);
    evaluate(report);
}

void SimulationEngine::evaluate(const SessionReport &report)
{
    const QString system = QStringLiteral(
        "You evaluate a practice meeting between a user and a simulated participant. Judge only the user. Be fair, "
        "specific and constructive. Reply with ONLY JSON:\n"
        "{\"score\": 0-100, \"summary\": \"one or two sentences\",\n"
        " \"answers\": 0-100, \"evidence\": 0-100, \"composure\": 0-100, \"clarity\": 0-100,\n"
        " \"handled_well\": [\"moments the user handled well, quoting them briefly\"],\n"
        " \"struggled\": [\"questions or objections the user struggled with, and why\"],\n"
        " \"objectives\": [{\"objective\": \"...\", \"met\": true|false}],\n"
        " \"improvements\": [\"3 concrete things to do better next time\"]}");
    const QString user = QStringLiteral("Meeting: %1, the participant played %2, difficulty %3.\nUser's objectives: %4\n"
                                        "Interruptions: %5. Duration: %6.\n\nConversation:\n%7")
                             .arg(m_config.meetingType, m_config.role, m_config.difficulty, m_config.objectives)
                             .arg(m_interruptions)
                             .arg(formatDuration(elapsedSeconds()))
                             .arg(m_transcript.join(QLatin1Char('\n')).left(15000));
    m_ai->chat(system, user, 3000, [this, report](const QString &text, const QString &error) {
        QJsonObject evaluation = AiClient::extractJson(text);
        if (!error.isEmpty())
            evaluation.insert(QStringLiteral("error"), error);
        evaluation.insert(QStringLiteral("turns"), m_turns);
        evaluation.insert(QStringLiteral("interruptions"), m_interruptions);
        emit stateChanged(AvatarWidget::State::Idle);
        emit statusChanged(QString());
        emit finished(evaluation, report);
    });
}
