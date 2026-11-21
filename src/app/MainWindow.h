#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QTimer>

#include "core/Tip.h"

class AiClient;
class CameraMonitor;
class CompanionClient;
class DebugWindow;
class MicrophoneMonitor;
class NavButton;
class QStackedWidget;
class QSystemTrayIcon;
class SessionController;

// The main SMILE window: a navigation pane with the four phases and a page for
// each. During a meeting it minimizes; the companion and the tray icon remain.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    enum Page { HomePageIndex, TrainingPageIndex, PreMeetPageIndex, MeetPageIndex, PostMeetPageIndex, SettingsPageIndex };

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QWidget *buildNavigation();
    void buildTray();
    void showPage(Page page);
    void bringToFront();
    void showAbout();
    void previewTip();
    void showDebug();
    void updateCompanionTimer();
    void notifyHaptics(const Tip &tip);
    void repeatHaptics();
    void stopHaptics();

    struct ActiveHapticWarning {
        Tip tip;
        QElapsedTimer sinceFirstPulse;
    };

    SessionController *m_controller;
    AiClient *m_ai;
    CameraMonitor *m_camera;
    MicrophoneMonitor *m_mic;
    CompanionClient *m_companion; // the floating avatar (separate process)
    QStackedWidget *m_pages;
    QList<NavButton *> m_nav;
    QSystemTrayIcon *m_tray = nullptr;
    DebugWindow *m_debug = nullptr;
    QTimer *m_timerTick = nullptr;
    QTimer m_hapticsTimer;
    QHash<QString, ActiveHapticWarning> m_hapticWarnings;
};
