#pragma once

#include <QWidget>

#include "core/Agenda.h"

class QLabel;
class QPushButton;
class QTableWidget;
class SessionController;
class SessionRunWidget;

// Phase 1: the user lists the points to get across and how long each should
// take, then rehearses. Timing tips run live; feedback lands on Post-Meet.
class TrainingPage : public QWidget
{
    Q_OBJECT

public:
    explicit TrainingPage(SessionController *controller, QWidget *parent = nullptr);

private:
    void addRow(const QString &title, double minutes);
    void removeSelectedRows();
    void commitAgenda();
    Agenda readAgenda() const;
    void updateTotal();
    void onRunningChanged(bool running);

    SessionController *m_controller;
    QTableWidget *m_table;
    QLabel *m_total;
    QPushButton *m_add;
    QPushButton *m_remove;
    QPushButton *m_start;
    SessionRunWidget *m_run;
};
