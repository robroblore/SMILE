#include "ai/CoverageJudge.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "ai/AiClient.h"
#include "core/AppSettings.h"
#include "core/SessionController.h"

CoverageJudge::CoverageJudge(AiClient *ai, SessionController *controller, QObject *parent)
    : QObject(parent)
    , m_ai(ai)
    , m_controller(controller)
{
    connect(m_controller, &SessionController::runningChanged, this, [this](bool running) {
        if (running) {
            m_recent.clear();
            m_offTopic = 0;
        }
    });
}

void CoverageJudge::judge(const QString &text)
{
    if (!m_controller->isRunning() || !m_ai->isConfigured() || !AppSettings::instance().cloudAnalysis())
        return;
    const Agenda &agenda = m_controller->activeAgenda();
    if (agenda.isEmpty())
        return;
    m_recent = (m_recent + QLatin1Char(' ') + text).right(1500);

    QJsonArray sections;
    for (int i = 0; i < agenda.size(); ++i) {
        const AgendaPoint &p = agenda.at(i);
        QJsonArray points;
        for (int k = 0; k < p.keyPoints.size(); ++k)
            points.append(QJsonObject{{QStringLiteral("k"), k},
                                      {QStringLiteral("point"), p.keyPoints[k]},
                                      {QStringLiteral("already_covered"), p.keyPointCovered.value(k)}});
        sections.append(QJsonObject{{QStringLiteral("s"), i},
                                    {QStringLiteral("title"), p.title},
                                    {QStringLiteral("purpose"), p.summary},
                                    {QStringLiteral("key_points"), points}});
    }
    const int current = m_controller->currentIndex();
    const QString system = QStringLiteral(
        "You follow a live talk against the speaker's plan. Judge by MEANING, not exact words: a key point counts as "
        "covered if the speaker conveyed the same idea in any wording, even briefly or partially. Be generous. "
        "Reply with ONLY JSON: {\"covered\": [[s, k], ...] (newly covered key points), \"talking_about\": s or -1 if "
        "the latest speech fits no section (small talk, digression)}.");
    const QString user = QStringLiteral("Plan:\n%1\nCurrent section: %2\nRecent speech: \"%3\"\nLatest piece: \"%4\"")
                             .arg(QString::fromUtf8(QJsonDocument(sections).toJson(QJsonDocument::Compact)))
                             .arg(current)
                             .arg(m_recent, text);
    m_ai->chat(system, user, 1200, [this, current](const QString &reply, const QString &error) {
        if (!error.isEmpty() || !m_controller->isRunning())
            return;
        const QJsonObject j = AiClient::extractJson(reply);
        for (const QJsonValue &v : j.value(QLatin1String("covered")).toArray()) {
            const QJsonArray pair = v.toArray();
            if (pair.size() == 2)
                m_controller->markKeyPointCovered(pair[0].toInt(), pair[1].toInt());
        }
        if (!j.contains(QLatin1String("talking_about")))
            return;
        const int about = j.value(QLatin1String("talking_about")).toInt(-1);
        if (about < 0) {
            if (++m_offTopic >= 2) {
                m_controller->nudgeBackOnTrack();
                m_offTopic = 0;
            }
            return;
        }
        m_offTopic = 0;
        if (about > current && about == m_controller->currentIndex() + 1 && AppSettings::instance().autoAdvance())
            m_controller->advanceTo(about, tr("The AI heard you start on this section's topic."));
    });
}
