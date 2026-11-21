#include "app/MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QStackedWidget>
#include <QSystemTrayIcon>
#include <QVBoxLayout>

#include "app/pages/MeetPage.h"
#include "app/pages/PostMeetPage.h"
#include "app/pages/PreMeetPage.h"
#include "app/pages/SettingsPage.h"
#include "app/pages/TrainingPage.h"
#include "ai/AiClient.h"
#include "ai/SpeechTipSource.h"
#include "ai/VisionTipSource.h"
#include "companion/CompanionWindow.h"
#include "companion/SmileyPainter.h"
#include "core/DemoTipSource.h"
#include "core/SessionController.h"
#include "sensors/CameraMonitor.h"
#include "sensors/MicrophoneMonitor.h"
#include "ui/Widgets.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_controller(new SessionController(this))
    , m_ai(new AiClient(this))
    , m_camera(new CameraMonitor(this))
    , m_mic(new MicrophoneMonitor(this))
    , m_companion(new CompanionWindow)
    , m_pages(new QStackedWidget(this))
{
    setWindowTitle(tr("SMILE"));
    resize(1080, 720);
    setMinimumSize(820, 560);

    // Tip sources: camera (expression, posture, appearance), microphone
    // (monologues, filler words, pace) and the optional demo source.
    m_controller->addTipSource(new VisionTipSource(m_camera, m_ai));
    m_controller->addTipSource(new SpeechTipSource(m_mic, m_ai));
    auto *demo = new DemoTipSource;
    m_controller->addTipSource(demo);

    auto *training = new TrainingPage(m_controller);
    auto *preMeet = new PreMeetPage(m_camera, m_ai);
    auto *meet = new MeetPage(m_controller, demo);
    auto *postMeet = new PostMeetPage(m_ai);
    m_pages->addWidget(training);
    m_pages->addWidget(preMeet);
    m_pages->addWidget(meet);
    m_pages->addWidget(postMeet);
    m_pages->addWidget(new SettingsPage(m_camera, m_mic, m_ai));

    auto *root = new QWidget(this);
    root->setObjectName(QStringLiteral("root"));
    auto *layout = new QHBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(buildNavigation());
    layout->addWidget(m_pages, 1);
    setCentralWidget(root);

    buildTray();

    auto *quit = new QAction(tr("Quit"), this);
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, this, &QWidget::close);
    addAction(quit);

    connect(preMeet, &PreMeetPage::goToMeetRequested, this, [this] { showPage(MeetPageIndex); });
    connect(meet, &MeetPage::previewTipRequested, this, &MainWindow::previewTip);

    // Session -> companion. Praise is kept for the report during a meeting so
    // the companion only interrupts when something needs fixing.
    connect(m_controller, &SessionController::tipRaised, this, [this](const Tip &tip) {
        if (m_controller->mode() == SessionMode::Meeting && tip.severity == TipSeverity::Positive)
            return;
        m_companion->showTip(tip);
    });
    connect(m_controller, &SessionController::runningChanged, this, [this](bool running) {
        m_companion->setSessionActive(running);
        const bool meeting = running && m_controller->mode() == SessionMode::Meeting;
        m_companion->setMinimumGap(meeting ? 45000 : 20000);
        if (meeting) {
            showMinimized(); // get out of the way during the call
            if (m_tray)
                m_tray->showMessage(tr("SMILE is running"),
                                    tr("Tips will pop up only when needed. Right-click the tray icon to end."),
                                    SmileyPainter::appIcon(), 4000);
        }
    });
    connect(m_controller, &SessionController::finished, this, [this, postMeet](const SessionReport &report) {
        postMeet->showReport(report);
        showPage(PostMeetPageIndex);
        bringToFront();
    });

    connect(m_companion, &CompanionWindow::openMainWindowRequested, this, &MainWindow::bringToFront);
    connect(m_companion, &CompanionWindow::nextPointRequested, m_controller, &SessionController::nextPoint);
    connect(m_companion, &CompanionWindow::endSessionRequested, m_controller, &SessionController::stop);

    showPage(TrainingPageIndex);
}

MainWindow::~MainWindow()
{
    delete m_companion;
}

QWidget *MainWindow::buildNavigation()
{
    auto *pane = new QWidget;
    pane->setObjectName(QStringLiteral("navPane"));
    pane->setFixedWidth(240);
    auto *v = new QVBoxLayout(pane);
    v->setContentsMargins(8, 16, 8, 12);
    v->setSpacing(2);

    auto *brand = new QHBoxLayout;
    brand->setContentsMargins(14, 0, 0, 0);
    brand->setSpacing(10);
    brand->addWidget(new SmileyBadge(24, 0.8));
    brand->addWidget(Ui::label(tr("SMILE"), "appTitle"));
    brand->addStretch();
    v->addLayout(brand);
    v->addSpacing(18);

    auto *group = new QButtonGroup(this);
    group->setExclusive(true);
    const QList<std::pair<Icons::Icon, QString>> items = {
        {Icons::Icon::Training, tr("Training")},
        {Icons::Icon::PreMeet, tr("Pre-Meet")},
        {Icons::Icon::Meet, tr("Meet")},
        {Icons::Icon::PostMeet, tr("Post-Meet")},
    };
    for (int i = 0; i < items.size(); ++i) {
        auto *button = new NavButton(items[i].first, items[i].second);
        group->addButton(button, i);
        m_nav.append(button);
        v->addWidget(button);
    }
    connect(group, &QButtonGroup::idClicked, this, [this](int id) { showPage(Page(id)); });

    v->addStretch();
    auto *settings = new NavButton(Icons::Icon::Settings, tr("Settings"));
    group->addButton(settings, SettingsPageIndex);
    m_nav.append(settings);
    v->addWidget(settings);
    auto *about = new NavButton(Icons::Icon::Info, tr("About"));
    about->setCheckable(false);
    connect(about, &QAbstractButton::clicked, this, &MainWindow::showAbout);
    v->addWidget(about);
    return pane;
}

void MainWindow::buildTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_tray = new QSystemTrayIcon(SmileyPainter::appIcon(), this);
    m_tray->setToolTip(tr("SMILE"));
    auto *menu = new QMenu(this);
    menu->addAction(tr("Open SMILE"), this, &MainWindow::bringToFront);
    menu->addSeparator();
    QAction *next = menu->addAction(tr("Next point"), m_controller, &SessionController::nextPoint);
    QAction *end = menu->addAction(tr("End session"), m_controller, &SessionController::stop);
    menu->addSeparator();
    menu->addAction(tr("Quit SMILE"), this, &QWidget::close);
    next->setEnabled(false);
    end->setEnabled(false);
    connect(m_controller, &SessionController::runningChanged, this, [next, end](bool running) {
        next->setEnabled(running);
        end->setEnabled(running);
    });
    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            bringToFront();
    });
    m_tray->show();
}

void MainWindow::showPage(Page page)
{
    m_pages->setCurrentIndex(page);
    if (page >= 0 && page < m_nav.size())
        m_nav[page]->setChecked(true);
}

void MainWindow::bringToFront()
{
    if (isMinimized())
        showNormal();
    show();
    raise();
    activateWindow();
}

void MainWindow::previewTip()
{
    static const char *const samples[] = {
        QT_TR_NOOP("Try smiling more."),
        QT_TR_NOOP("Sit up straight. You're slouching a little."),
        QT_TR_NOOP("You've been talking for 3 minutes. Pause for questions."),
    };
    static const TipCategory categories[] = {TipCategory::Expression, TipCategory::Posture, TipCategory::Speech};
    static const TipSeverity severities[] = {TipSeverity::Info, TipSeverity::Warning, TipSeverity::Info};
    static int next = 0;
    const int i = next++ % 3;

    Tip tip;
    tip.category = categories[i];
    tip.severity = severities[i];
    tip.text = tr(samples[i]);
    tip.source = QStringLiteral("preview");
    m_companion->previewTip(tip);
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
    if (m_tray)
        m_tray->hide();
    event->accept();
    QApplication::quit();
}
