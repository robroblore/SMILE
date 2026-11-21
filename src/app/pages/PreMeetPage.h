#pragma once

#include <QList>
#include <QWidget>

class QCheckBox;
class QLabel;
class QPushButton;

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

    QList<QCheckBox *> m_items;
    QLabel *m_progress;
    QPushButton *m_autoCheck;
    QPushButton *m_goToMeet;
};
