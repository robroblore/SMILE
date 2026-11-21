#pragma once

#include <QList>
#include <QMainWindow>

class CompanionWindow;
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

    enum Page { TrainingPageIndex, PreMeetPageIndex, MeetPageIndex, PostMeetPageIndex };

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QWidget *buildNavigation();
    void buildTray();
    void showPage(Page page);
    void bringToFront();
    void showAbout();
    void previewTip();

    SessionController *m_controller;
    CompanionWindow *m_companion; // top-level window, owned here
    QStackedWidget *m_pages;
    QList<NavButton *> m_nav;
    QSystemTrayIcon *m_tray = nullptr;
};
