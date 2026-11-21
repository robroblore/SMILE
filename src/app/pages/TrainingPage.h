#pragma once

#include <QTimer>
#include <QWidget>

class AgendaEditor;
class AiClient;
class MicrophoneMonitor;
class QStackedWidget;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class DurationEdit;
class SessionController;
class SessionRunWidget;

// Phase 1: write or paste what you want to say, let SMILE structure it into
// sections with key points, adjust them, then rehearse against the clock.
class TrainingPage : public QWidget
{
    Q_OBJECT

public:
    TrainingPage(SessionController *controller, AiClient *ai, MicrophoneMonitor *mic, QWidget *parent = nullptr);

private:
    void setBulletMode(bool bullets);
    void structure();
    void commitAgenda();
    void save();
    void updateWordCount();
    void onRunningChanged(bool running);

    SessionController *m_controller;
    AiClient *m_ai;
    QPlainTextEdit *m_speech;
    QPushButton *m_textMode;
    QPushButton *m_bulletMode;
    DurationEdit *m_target;
    QPushButton *m_structure;
    QLabel *m_structureStatus;
    QLabel *m_wordCount;
    QLabel *m_type;
    AgendaEditor *m_editor;
    QLabel *m_summary;
    QPushButton *m_add;
    QWidget *m_idle;
    SessionRunWidget *m_run;
    QTimer m_saveTimer;
    bool m_bullets = false;
    QStackedWidget *m_tabs = nullptr;
};
