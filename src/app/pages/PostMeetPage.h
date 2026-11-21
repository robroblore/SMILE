#pragma once

#include <QList>
#include <QWidget>

#include "core/Rating.h"
#include "core/SessionReport.h"

class AiClient;
class QGridLayout;
class QLabel;
class QVBoxLayout;

// Phase 4: a colour-coded dashboard (green = achieved, orange = needs work,
// red = poor or objective missed), key points covered, timing, and written
// feedback from the AI coach.
class PostMeetPage : public QWidget
{
    Q_OBJECT

public:
    explicit PostMeetPage(AiClient *ai, QWidget *parent = nullptr);

    // keyPointsCovered: per section, per key point (from local or AI judging)
    static QList<RatedMetric> metrics(const SessionReport &report, RatedMetric *overall);
    static QString summarizeForAi(const SessionReport &report);

public slots:
    void showReport(const SessionReport &report);

private:
    void rebuild();
    void requestCoachFeedback();
    void requestCoverageJudgement();

    AiClient *m_ai;
    SessionReport m_report;
    bool m_coverageFromAi = false;
    QString m_coachText;
    QString m_coachError;
    bool m_coachPending = false;
    QWidget *m_empty;
    QWidget *m_reportWidget;
    QVBoxLayout *m_reportLayout;
    int m_reportId = 0;
};
