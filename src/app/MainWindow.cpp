#include "app/MainWindow.h"

#include <optional>

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QSignalBlocker>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QStackedWidget>
#include <QSystemTrayIcon>
#include <QVBoxLayout>

#include "app/DebugWindow.h"
#include "app/pages/MeetPage.h"
#include "app/pages/PostMeetPage.h"
#include "app/pages/PreMeetPage.h"
#include "app/pages/SettingsPage.h"
#include "app/pages/TrainingPage.h"
#include "ai/AiClient.h"
#include "ai/CoverageJudge.h"
#include "app/SessionHistory.h"
#include "app/pages/HomePage.h"
#include "ai/SpeechTipSource.h"
#include "ai/VisionTipSource.h"
#include "companion/CompanionClient.h"
#include "companion/SmileyPainter.h"
#include "core/AppSettings.h"
#include "core/DemoTipSource.h"
#include "core/HapticMouse.h"
#include "core/LiveSignals.h"
#include "core/SessionController.h"
#include "sensors/CameraMonitor.h"
#include "sensors/MicrophoneMonitor.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

namespace {
constexpr int kBubbleSpacingMs = 3000;
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_controller(new SessionController(this))
    , m_ai(new AiClient(this))
    , m_camera(new CameraMonitor(this))
    , m_mic(new MicrophoneMonitor(this))
    , m_companion(new CompanionClient(this))
    , m_pages(new QStackedWidget(this))
{
    setWindowTitle(tr("SMILE"));
    resize(1080, 720);
    setMinimumSize(820, 560);

    // Tip sources: camera (expression, posture, appearance), microphone
    // (monologues, filler words, pace) and the optional demo source.
    m_controller->addTipSource(new VisionTipSource(m_camera, m_ai));
    auto *speech = new SpeechTipSource(m_mic, m_ai);
    m_controller->addTipSource(speech);
    // What you say is checked against the key points of each section
    connect(speech, &SpeechTipSource::transcribed, m_controller, &SessionController::addTranscript);
    // ...and judged by meaning (which key points you got across, which section you're on)
    auto *judge = new CoverageJudge(m_ai, m_controller, this);
    connect(speech, &SpeechTipSource::transcribed, judge, &CoverageJudge::judge);
    auto *demo = new DemoTipSource;
    m_controller->addTipSource(demo);

    auto *home = new HomePage;
    auto *training = new TrainingPage(m_controller, m_ai, m_mic, m_camera);
    auto *preMeet = new PreMeetPage(m_camera, m_ai, m_mic);
    auto *meet = new MeetPage(m_controller, demo);
    auto *postMeet = new PostMeetPage(m_ai);
    m_pages->addWidget(home);
    m_pages->addWidget(training);
    m_pages->addWidget(preMeet);
    m_pages->addWidget(meet);
    m_pages->addWidget(postMeet);
    auto *settings = new SettingsPage(m_camera, m_mic, m_ai);
    m_pages->addWidget(settings);
    connect(settings, &SettingsPage::openDebugRequested, this, &MainWindow::showDebug);

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

    connect(home, &HomePage::openTraining, this, [this] { showPage(TrainingPageIndex); });
    connect(home, &HomePage::openSetupCheck, this, [this] { showPage(PreMeetPageIndex); });
    connect(home, &HomePage::openMeet, this, [this] { showPage(MeetPageIndex); });
    connect(home, &HomePage::openResults, this, [this] { showPage(PostMeetPageIndex); });
    connect(preMeet, &PreMeetPage::goToMeetRequested, this, [this] { showPage(MeetPageIndex); });
    connect(meet, &MeetPage::previewTipRequested, this, &MainWindow::previewTip);

    // Session -> companion. Praise is kept for the report during a meeting so
    // the companion only interrupts when something needs fixing.
    connect(m_controller, &SessionController::tipRaised, this, [this](const Tip &tip) {
        if (AppSettings::instance().tipsMuted())
            return; // muted: everything still goes into the results
        if (m_controller->mode() == SessionMode::Meeting && tip.severity == TipSeverity::Positive)
            return;
        if (m_controller->mode() == SessionMode::Simulation)
            return; // the avatar is the other participant; tips go to the report
        m_companion->showTip(tip);
    });
    m_hapticsTimer.setInterval(AppSettings::instance().hapticPulseIntervalMs());
    m_hapticsTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_hapticsTimer, &QTimer::timeout, this, &MainWindow::repeatHaptics);
    connect(m_companion, &CompanionClient::liveTipPresented, this, &MainWindow::notifyHaptics);
    connect(m_companion, &CompanionClient::liveTipClosed, this, &MainWindow::stopHaptics);
    // Every tip, with its reason, goes to the debug log
    connect(m_controller, &SessionController::tipLogged, this, [](const Tip &tip) {
        LiveSignals::instance().addEvent(LiveSignals::Event::Tip, tip.text, tip.reason);
    });
    // The optional section timer next to the avatar
    m_timerTick = new QTimer(this);
    m_timerTick->setInterval(500);
    connect(m_timerTick, &QTimer::timeout, this, &MainWindow::updateCompanionTimer);
    connect(&AppSettings::instance(), &AppSettings::changed, this, [this] {
        const auto &settings = AppSettings::instance();
        if (!settings.physicalHapticsEnabled())
            stopHaptics();
        if (m_hapticsTimer.interval() != settings.hapticPulseIntervalMs())
            m_hapticsTimer.setInterval(settings.hapticPulseIntervalMs());
        m_companion->reloadSettings();
        updateCompanionTimer();
    });

    connect(m_controller, &SessionController::runningChanged, this, [this](bool running) {
        if (!running)
            stopHaptics();
        m_companion->setSessionActive(running);
        if (running)
            m_timerTick->start();
        else
            m_timerTick->stop();
        updateCompanionTimer();
        const bool meeting = running && m_controller->mode() == SessionMode::Meeting;
        // No long quiet time between pop-ups: each kind of tip has its own repeat
        // cooldown instead. A few seconds just keep two bubbles from overlapping.
        m_companion->setMinimumGap(kBubbleSpacingMs);
        if (running && AppSettings::instance().debugMode())
            showDebug();
        if (meeting) {
            showMinimized(); // get out of the way during the call
            if (m_tray)
                m_tray->showMessage(tr("SMILE is running"),
                                    tr("Tips will pop up only when needed. Right-click the tray icon to end."),
                                    SmileyPainter::appIcon(), 4000);
        }
    });
    connect(m_controller, &SessionController::finished, this, [this, postMeet](const SessionReport &report) {
        // Every session goes into the local history (Home page: progress, streaks, badges)
        SessionHistory::instance().add(report, PostMeetPage::evaluate(report, QJsonObject()));
        if (report.mode == SessionMode::Practice || report.mode == SessionMode::Simulation)
            return; // shown in the Training tab that ran them
        postMeet->showReport(report);
        showPage(PostMeetPageIndex);
        bringToFront();
    });

    connect(m_companion, &CompanionClient::openMainWindowRequested, this, &MainWindow::bringToFront);
    connect(m_companion, &CompanionClient::nextPointRequested, m_controller, &SessionController::nextPoint);
    connect(m_companion, &CompanionClient::endSessionRequested, m_controller, &SessionController::stop);
    connect(m_companion, &CompanionClient::debugViewRequested, this, &MainWindow::showDebug);
    connect(m_companion, &CompanionClient::muteRequested, &AppSettings::instance(), &AppSettings::setTipsMuted);

    showPage(HomePageIndex);
}

MainWindow::~MainWindow()
{
    delete m_debug;
}

void MainWindow::notifyHaptics(const Tip &tip)
{
    const auto &settings = AppSettings::instance();
    if (!settings.physicalHapticsEnabled() || !m_controller->isRunning()
        || tip.source == QLatin1String("demo"))
        return;

    if (m_controller->isWarningActive(tip)) {
        const QString key = tip.source + QLatin1Char(':') + tip.warningId;
        ActiveHapticWarning warning{tip, {}};
        warning.sinceFirstPulse.start();
        m_hapticWarnings.insert(key, warning);
    }
    HapticMouse::playPulse(tip.category, settings.hapticIntensity());
    if (!m_hapticWarnings.isEmpty())
        m_hapticsTimer.start();
}

void MainWindow::repeatHaptics()
{
    const auto &settings = AppSettings::instance();
    if (!settings.physicalHapticsEnabled() || !m_controller->isRunning()) {
        stopHaptics();
        return;
    }

    const int maxDuration = settings.hapticDurationSeconds();
    std::optional<TipCategory> activeCategory;
    for (auto warning = m_hapticWarnings.begin(); warning != m_hapticWarnings.end();) {
        if (!m_controller->isWarningActive(warning->tip)) {
            warning = m_hapticWarnings.erase(warning);
            continue;
        }
        if (!maxDuration || warning->sinceFirstPulse.elapsed() < maxDuration * 1000LL)
            activeCategory = warning->tip.category;
        ++warning;
    }
    if (m_hapticWarnings.isEmpty())
        m_hapticsTimer.stop();
    if (activeCategory)
        HapticMouse::playPulse(*activeCategory, settings.hapticIntensity(), false);
}

void MainWindow::stopHaptics()
{
    m_hapticsTimer.stop();
    m_hapticWarnings.clear();
}

void MainWindow::updateCompanionTimer()
{
    const bool show = m_controller->isRunning() && AppSettings::instance().sectionTimer();
    m_companion->setTimerVisible(show);
    if (!show)
        return;

    const Agenda &agenda = m_controller->activeAgenda();
    const int index = m_controller->currentIndex();
    if (index < 0 || index >= agenda.size())
        return;

    SectionTimerInfo info;
    info.running = true;
    info.section = agenda.at(index).title;
    info.index = index;
    info.count = int(agenda.size());
    info.elapsedSeconds = m_controller->elapsedOnCurrent();
    info.targetSeconds = agenda.at(index).targetSeconds;
    // Behind/ahead: how the earlier sections went, plus any overrun now
    int delta = 0;
    for (int i = 0; i < index; ++i)
        if (agenda.at(i).targetSeconds > 0)
            delta += agenda.at(i).actualSeconds - agenda.at(i).targetSeconds;
    if (info.targetSeconds > 0)
        delta += qMax(0, info.elapsedSeconds - info.targetSeconds);
    info.scheduleDeltaSeconds = delta;
    const AppSettings &s = AppSettings::instance();
    info.status = s.useMicrophone() ? tr("Listening · Live feedback active")
                  : s.useCamera()   ? tr("Watching · Live feedback active")
                                    : tr("Timing only");
    m_companion->setTimerInfo(info);
}

void MainWindow::showDebug()
{
    if (!m_debug)
        m_debug = new DebugWindow(m_camera, m_mic, m_controller);
    m_debug->show();
    m_debug->raise();
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
        {Icons::Icon::Home, tr("Home")},
        {Icons::Icon::Training, tr("Training")},
        {Icons::Icon::PreMeet, tr("Setup check")},
        {Icons::Icon::Meet, tr("Meet")},
        {Icons::Icon::PostMeet, tr("Results")},
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
    auto *themeToggle = new NavButton(Icons::Icon::Eye, Theme::colors().dark ? tr("Light mode") : tr("Dark mode"));
    themeToggle->setCheckable(false);
    connect(themeToggle, &QAbstractButton::clicked, this, [themeToggle] {
        const bool toDark = !Theme::colors().dark;
        AppSettings::instance().setThemeMode(toDark ? 2 : 1);
        Theme::reapply();
        themeToggle->setText(toDark ? tr("Light mode") : tr("Dark mode"));
    });
    v->addWidget(themeToggle);
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
    QAction *mute = menu->addAction(tr("Mute all tips"));
    mute->setCheckable(true);
    mute->setChecked(AppSettings::instance().tipsMuted());
    connect(mute, &QAction::toggled, &AppSettings::instance(), &AppSettings::setTipsMuted);
    connect(&AppSettings::instance(), &AppSettings::changed, mute, [mute] {
        const QSignalBlocker block(mute);
        mute->setChecked(AppSettings::instance().tipsMuted());
    });
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
    if (m_debug)
        m_debug->close();
    if (m_tray)
        m_tray->hide();
    event->accept();
    QApplication::quit();
}
