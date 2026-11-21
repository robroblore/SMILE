#pragma once

#include <QWidget>

#include "core/SessionReport.h"

class AiClient;
class QLabel;
class QVBoxLayout;

// Phase 4: what went well, what to improve, and next steps. Built from the
// agenda timing, the camera and voice statistics and, if an AI key is set,
// personal written feedback from the AI coach.
class PostMeetPage : public QWidget
{
    Q_OBJECT

public:
    explicit PostMeetPage(AiClient *ai, QWidget *parent = nullptr);

    // The text sent to the AI to write feedback (exposed for testing).
    static QString summarizeForAi(const SessionReport &report);

public slots:
    void showReport(const SessionReport &report);

private:
    void requestCoachFeedback(const SessionReport &report);

    AiClient *m_ai;
    QWidget *m_empty;
    QWidget *m_report;
    QVBoxLayout *m_reportLayout;
    QLabel *m_coach = nullptr;
    int m_reportId = 0;
};
