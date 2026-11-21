#pragma once

#include <QWidget>

class QLabel;
class QProgressBar;
class QPushButton;
class SessionController;

// Live view of a running session: current point, time against target,
// and Next point / Finish buttons. Shared by the Training and Meet pages.
class SessionRunWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SessionRunWidget(SessionController *controller, QWidget *parent = nullptr);

private:
    void refresh();

    SessionController *m_controller;
    QLabel *m_point;
    QLabel *m_time;
    QLabel *m_total;
    QProgressBar *m_bar;
    QPushButton *m_next;
    QPushButton *m_finish;
};
