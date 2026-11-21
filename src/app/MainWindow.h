#pragma once

#include <QMainWindow>

class CompanionWindow;
class QAction;
class QListWidget;
class QStackedWidget;
class SessionController;

// The main SMILE window: a sidebar with the four phases and a page for each.
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
    void buildMenus();
    void showPage(Page page);
    void bringToFront();
    void showAbout();

    SessionController *m_controller;
    CompanionWindow *m_companion; // top-level window, owned here
    QListWidget *m_nav;
    QStackedWidget *m_pages;
    QAction *m_showCompanion = nullptr;
};
