#pragma once

#include <QWidget>

class QListWidget;
class QPushButton;
class SessionController;
class SessionRunWidget;
class TipSource;

// Phase 3: the live meeting. Tracks time per agenda point and lets the user
// choose which kinds of tips the companion may show.
class MeetPage : public QWidget
{
    Q_OBJECT

public:
    MeetPage(SessionController *controller, TipSource *demoSource, QWidget *parent = nullptr);

private:
    void refreshAgenda();
    void onRunningChanged(bool running);

    SessionController *m_controller;
    QListWidget *m_agenda;
    QPushButton *m_start;
    SessionRunWidget *m_run;
};
