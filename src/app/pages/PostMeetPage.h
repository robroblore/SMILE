#pragma once

#include <QWidget>

#include "core/SessionReport.h"

class QVBoxLayout;

// Phase 4: what went well, what to improve, and next steps. Today the report is
// built from timing and the tips logged; the LLM feedback engine will add to it.
class PostMeetPage : public QWidget
{
    Q_OBJECT

public:
    explicit PostMeetPage(QWidget *parent = nullptr);

public slots:
    void showReport(const SessionReport &report);

private:
    QWidget *m_empty;
    QWidget *m_report;
    QVBoxLayout *m_reportLayout;
};
