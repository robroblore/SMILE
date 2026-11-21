#pragma once

#include <QObject>

class AiClient;
class SessionController;

// Understands what you are talking about, by meaning rather than exact words.
// For each new piece of transcript it asks the AI which key points you got
// across and which section you're on, then updates the session (marks key
// points, moves to the next section, nudges you back if you drift off-agenda).
class CoverageJudge : public QObject
{
    Q_OBJECT

public:
    CoverageJudge(AiClient *ai, SessionController *controller, QObject *parent = nullptr);

public slots:
    void judge(const QString &text);

private:
    AiClient *m_ai;
    SessionController *m_controller;
    QString m_recent;     // the last few chunks, for context
    int m_offTopic = 0;   // consecutive chunks that matched no section
};
