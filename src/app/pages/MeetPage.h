#pragma once

#include <QWidget>

class QVBoxLayout;
class SessionController;
class SessionRunWidget;
class TipSource;

// Phase 3: the live meeting. Starting it minimizes SMILE; from then on only
// the companion appears, and only when something needs attention.
class MeetPage : public QWidget
{
    Q_OBJECT

public:
    MeetPage(SessionController *controller, TipSource *demoSource, QWidget *parent = nullptr);

signals:
    void previewTipRequested();

private:
    void refreshAgenda();
    void onRunningChanged(bool running);

    SessionController *m_controller;
    QVBoxLayout *m_agendaRows;
    QWidget *m_idle;
    SessionRunWidget *m_run;
};
