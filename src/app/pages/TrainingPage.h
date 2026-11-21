#pragma once

#include <QWidget>

class AgendaEditor;
class QLabel;
class QPushButton;
class SessionController;
class SessionRunWidget;

// Phase 1: the user lists the points to get across and how long each should
// take, then rehearses. Feedback lands on the Post-Meet page.
class TrainingPage : public QWidget
{
    Q_OBJECT

public:
    explicit TrainingPage(SessionController *controller, QWidget *parent = nullptr);

private:
    void commitAgenda();
    void onRunningChanged(bool running);

    SessionController *m_controller;
    AgendaEditor *m_editor;
    QLabel *m_summary;
    QPushButton *m_add;
    QWidget *m_idle;
    SessionRunWidget *m_run;
};
