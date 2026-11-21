#pragma once

#include <QWidget>

class QGridLayout;
class QLabel;
class QProgressBar;
class QVBoxLayout;

// The start screen: four big buttons for the four things you do with SMILE,
// plus your progress (level, streak, scores over time and badges).
class HomePage : public QWidget
{
    Q_OBJECT

public:
    explicit HomePage(QWidget *parent = nullptr);

signals:
    void openTraining();
    void openSetupCheck();
    void openMeet();
    void openResults();

private:
    void refresh();

    QLabel *m_greeting;
    QLabel *m_level;
    QProgressBar *m_xp;
    QLabel *m_xpText;
    QLabel *m_streak;
    QLabel *m_best;
    QLabel *m_last;
    class Sparkline *m_spark;
    QGridLayout *m_badges;
    QVBoxLayout *m_recent;
};
