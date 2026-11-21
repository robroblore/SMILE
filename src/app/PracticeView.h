#pragma once

#include <QJsonObject>
#include <QWidget>

#include "core/Agenda.h"
#include "core/SessionReport.h"

class AiClient;
class QLabel;
class QListWidget;
class QPushButton;
class QVBoxLayout;
class SessionController;
class SessionRunWidget;

// Training → Practise a section: pick one section, read your notes, practise
// it, get specific feedback, repeat, and compare with earlier attempts.
class PracticeView : public QWidget
{
    Q_OBJECT

public:
    PracticeView(SessionController *controller, AiClient *ai, QWidget *parent = nullptr);

    void refreshSections();

protected:
    void showEvent(QShowEvent *event) override;

private:
    struct Attempt {
        QString when;
        int seconds = 0;
        int overall = -1;
        QMap<QString, int> categories; // category name -> score
    };

    void select(int index);
    void startAttempt();
    void onFinished(const SessionReport &report);
    void showAttemptResult(const SessionReport &report);
    void requestFeedback(const SessionReport &report, int attemptNumber);
    void renderHistory();
    QList<Attempt> attemptsFor(const QString &section) const;
    void saveAttempt(const QString &section, const Attempt &attempt);

    SessionController *m_controller;
    AiClient *m_ai;
    Agenda m_sections;
    int m_index = -1;
    bool m_practising = false;

    QListWidget *m_list;
    QLabel *m_title;
    QLabel *m_meta;
    QLabel *m_purpose;
    QLabel *m_notes;
    QLabel *m_keyPoints;
    QPushButton *m_start;
    SessionRunWidget *m_run;
    QVBoxLayout *m_result;
    QVBoxLayout *m_history;
    int m_feedbackId = 0;
};
