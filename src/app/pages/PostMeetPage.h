#pragma once

#include <QJsonObject>
#include <QList>
#include <QStringList>
#include <QWidget>

#include "core/Rating.h"
#include "core/SessionReport.h"

class AiClient;
class QVBoxLayout;

// One part of the performance breakdown (Speech delivery, Eye contact…)
struct ReportCategory {
    QString name;
    QString measured;            // what was measured, in one line
    int score = -1;              // 0..100, -1 = not measured
    Rating rating = Rating::None;
    QString summary;             // why it got this score
    QStringList details;         // the numbers behind it
    QStringList events;          // notable moments
    QStringList recommendations;
};

struct Evaluation {
    int overall = -1;
    Rating rating = Rating::None;
    QString headline;            // "Good performance"
    QString assessment;          // one sentence
    QList<ReportCategory> categories;
    QStringList improvements;    // the most important ones first
};

// Phase 4: the results. A large overall score, the most important
// improvements, and a clean breakdown whose categories expand into details.
class PostMeetPage : public QWidget
{
    Q_OBJECT

public:
    explicit PostMeetPage(AiClient *ai, QWidget *parent = nullptr);

    // Scores a session. `review` is the optional AI review (may be empty).
    static Evaluation evaluate(const SessionReport &report, const QJsonObject &review = {});
    static QString summarizeForAi(const SessionReport &report);
    static Rating ratingFor(int score);

public slots:
    void showReport(const SessionReport &report);

private:
    void rebuild();
    void rebuildHistory();
    void requestReview();
    void requestCoverageJudgement();

    AiClient *m_ai;
    SessionReport m_report;
    QJsonObject m_review;
    bool m_reviewPending = false;
    QString m_reviewError;
    bool m_coverageFromAi = false;
    QWidget *m_empty;
    QWidget *m_reportWidget;
    QVBoxLayout *m_reportLayout;
    QVBoxLayout *m_historyLayout;
    int m_reportId = 0;
};
