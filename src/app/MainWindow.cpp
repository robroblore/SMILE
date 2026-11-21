#include "app/MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QStackedWidget>
#include <QStatusBar>

#include "app/pages/MeetPage.h"
#include "app/pages/PostMeetPage.h"
#include "app/pages/PreMeetPage.h"
#include "app/pages/TrainingPage.h"
#include "companion/CompanionWindow.h"
#include "core/DemoTipSource.h"
#include "core/SessionController.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_controller(new SessionController(this))
    , m_companion(new CompanionWindow)
    , m_nav(new QListWidget(this))
    , m_pages(new QStackedWidget(this))
{
    setWindowTitle(tr("SMILE: Smart Meeting Interactive Live Education"));
    resize(960, 640);

    // Tip sources. Future AI modules (posture, smile, speech) are added here.
    auto *demo = new DemoTipSource;
    m_controller->addTipSource(demo);

    auto *training = new TrainingPage(m_controller);
    auto *preMeet = new PreMeetPage;
    auto *meet = new MeetPage(m_controller, demo);
    auto *postMeet = new PostMeetPage;
    m_pages->addWidget(training);
    m_pages->addWidget(preMeet);
    m_pages->addWidget(meet);
    m_pages->addWidget(postMeet);

    m_nav->addItems({tr("1  Training"), tr("2  Pre-Meet"), tr("3  Meet"), tr("4  Post-Meet")});
    m_nav->setFixedWidth(170);
    m_nav->setFrameShape(QFrame::NoFrame);
    m_nav->setSpacing(2);
    QFont navFont = m_nav->font();
    navFont.setPointSizeF(navFont.pointSizeF() * 1.15);
    m_nav->setFont(navFont);

    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_nav);
    layout->addWidget(m_pages, 1);
    setCentralWidget(central);

    buildMenus();

    // A permanent label: plain status messages get cleared by hover status tips.
    auto *status = new QLabel(tr("Ready"), this);
    statusBar()->addWidget(status);

    connect(m_nav, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    connect(preMeet, &PreMeetPage::goToMeetRequested, this, [this] { showPage(MeetPageIndex); });

    // Session -> companion and report
    connect(m_controller, &SessionController::tipRaised, m_companion, &CompanionWindow::showTip);
    connect(m_controller, &SessionController::runningChanged, m_companion, &CompanionWindow::setSessionActive);
    connect(m_controller, &SessionController::runningChanged, this, [this, status](bool running) {
        if (running) {
            const bool training = m_controller->mode() == SessionMode::Training;
            status->setText(training ? tr("Rehearsal in progress") : tr("Meeting in progress"));
            if (!m_companion->isVisible())
                m_companion->show();
        } else {
            status->setText(tr("Ready"));
        }
    });
    connect(m_controller, &SessionController::finished, this, [this, postMeet](const SessionReport &report) {
        postMeet->showReport(report);
        showPage(PostMeetPageIndex);
        bringToFront();
    });

    connect(m_companion, &CompanionWindow::openMainWindowRequested, this, &MainWindow::bringToFront);
    connect(m_companion, &CompanionWindow::visibilityChanged, m_showCompanion, &QAction::setChecked);

    showPage(TrainingPageIndex);

    m_companion->moveToDefaultPosition();
    m_companion->show();
}

MainWindow::~MainWindow()
{
    delete m_companion;
}

void MainWindow::buildMenus()
{
    QMenu *file = menuBar()->addMenu(tr("&File"));
    file->addAction(tr("&Quit"), QKeySequence::Quit, this, &QWidget::close);

    QMenu *view = menuBar()->addMenu(tr("&View"));
    m_showCompanion = view->addAction(tr("Show &companion"));
    m_showCompanion->setCheckable(true);
    m_showCompanion->setShortcut(QKeySequence(tr("Ctrl+K")));
    connect(m_showCompanion, &QAction::toggled, m_companion, &QWidget::setVisible);
    view->addAction(tr("Reset companion position"), this, [this] { m_companion->moveToDefaultPosition(); });

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("&About SMILE"), this, &MainWindow::showAbout);
}

void MainWindow::showPage(Page page)
{
    m_nav->setCurrentRow(page);
}

void MainWindow::bringToFront()
{
    if (isMinimized())
        showNormal();
    show();
    raise();
    activateWindow();
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("About SMILE"),
                       tr("<h3>SMILE %1</h3>"
                          "<p>Smart Meeting Interactive Live Education.</p>"
                          "<p>A training companion for high-stakes online meetings. SMILE coaches you "
                          "privately and never changes your camera or video feed.</p>")
                           .arg(QApplication::applicationVersion()));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_controller->stop();
    m_companion->close();
    event->accept();
    QApplication::quit();
}
