#pragma once

#include <QWidget>

#include "core/SessionReport.h"

class QTextBrowser;

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
    QTextBrowser *m_view;
};
