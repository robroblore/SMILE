#pragma once

#include <QList>
#include <QWidget>

class QAbstractButton;
class QLabel;
class QProgressBar;

// Phase 2: a readiness checklist before joining. Today the user ticks items by
// hand; later the vision module will check framing, collar, visible notes etc.
class PreMeetPage : public QWidget
{
    Q_OBJECT

public:
    explicit PreMeetPage(QWidget *parent = nullptr);

signals:
    void goToMeetRequested();

private:
    void updateProgress();

    QList<QAbstractButton *> m_items;
    QLabel *m_progressText;
    QProgressBar *m_progress;
};
