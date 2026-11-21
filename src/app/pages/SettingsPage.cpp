#include "app/pages/SettingsPage.h"

#include <QAudioDevice>
#include <QButtonGroup>
#include <QCameraDevice>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMediaDevices>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

#include "ai/AiClient.h"
#include "core/AppSettings.h"
#include "sensors/CameraMonitor.h"
#include "sensors/MicrophoneMonitor.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

SettingsPage::SettingsPage(CameraMonitor *camera, MicrophoneMonitor *mic, AiClient *ai, QWidget *parent)
    : QWidget(parent)
    , m_camera(camera)
    , m_mic(mic)
    , m_ai(ai)
    , m_keyStatus(Ui::label(QString(), "secondary"))
    , m_keyEdit(new QLineEdit)
    , m_testResult(Ui::label(QString(), "secondary"))
    , m_cameraCombo(new QComboBox)
    , m_micCombo(new QComboBox)
    , m_meter(new LevelMeter)
{
    AppSettings &settings = AppSettings::instance();

    // ---- AI coach
    auto *aiCard = new Card(tr("AI coach"));
    QLabel *aiIntro = Ui::label(tr("SMILE uses Together AI to read your expression and posture from camera "
                                   "snapshots, transcribe your speech for filler words, and write your feedback."),
                                "secondary");
    aiIntro->setWordWrap(true);
    aiCard->body()->addWidget(aiIntro);
    aiCard->body()->addWidget(m_keyStatus);

    m_keyEdit->setEchoMode(QLineEdit::Password);
    m_keyEdit->setPlaceholderText(tr("Paste your Together AI API key"));
    auto *save = new QPushButton(tr("Save"));
    save->setObjectName(QStringLiteral("accent"));
    auto *test = new QPushButton(tr("Test connection"));
    auto *remove = new QPushButton(tr("Remove"));
    remove->setObjectName(QStringLiteral("subtle"));
    for (QPushButton *b : {save, test, remove})
        b->setCursor(Qt::PointingHandCursor);
    auto *keyRow = new QHBoxLayout;
    keyRow->setSpacing(8);
    keyRow->addWidget(m_keyEdit, 1);
    keyRow->addWidget(save);
    keyRow->addWidget(test);
    keyRow->addWidget(remove);
    aiCard->body()->addLayout(keyRow);
    m_testResult->setWordWrap(true);
    aiCard->body()->addWidget(m_testResult);
    aiCard->body()->addWidget(Ui::divider());

    auto *cloud = new ToggleSwitch;
    cloud->setChecked(settings.cloudAnalysis());
    aiCard->body()->addWidget(new OptionRow(
        tr("Analyse with AI"),
        tr("Send a small camera snapshot every few seconds and short speech clips to Together AI over HTTPS "
           "while a session runs. SMILE stores nothing in the cloud."),
        cloud));
    connect(cloud, &QAbstractButton::toggled, &settings, &AppSettings::setCloudAnalysis);

    // ---- Coaching: how quickly SMILE speaks up
    auto *coachCard = new Card(tr("Coaching"));
    QLabel *coachIntro = Ui::label(tr("How quickly SMILE reacts. The face tracker on your computer watches ~15 times a "
                                      "second; the cloud AI looks every few seconds and straight away when the tracker "
                                      "notices a change."),
                                   "secondary");
    coachIntro->setWordWrap(true);
    coachCard->body()->addWidget(coachIntro);
    auto *sensRow = new QHBoxLayout;
    sensRow->setSpacing(4);
    auto *sensGroup = new QButtonGroup(this);
    const QStringList names = {tr("Relaxed"), tr("Balanced"), tr("Responsive")};
    for (int i = 0; i < names.size(); ++i) {
        auto *b = new QPushButton(names[i]);
        b->setObjectName(QStringLiteral("segment"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setChecked(int(settings.sensitivity()) == i);
        sensGroup->addButton(b, i);
        sensRow->addWidget(b);
    }
    sensRow->addStretch();
    coachCard->body()->addLayout(sensRow);
    m_sensitivityText = Ui::label(QString(), "secondary");
    m_sensitivityText->setWordWrap(true);
    coachCard->body()->addWidget(m_sensitivityText);
    connect(sensGroup, &QButtonGroup::idClicked, this, [this](int id) {
        AppSettings::instance().setSensitivity(AppSettings::Sensitivity(id));
        describeSensitivity();
    });
    describeSensitivity();

    auto *interval = new QSpinBox;
    interval->setRange(2, 30);
    interval->setSuffix(tr(" s"));
    interval->setValue(settings.snapshotIntervalSeconds());
    interval->setButtonSymbols(QAbstractSpinBox::NoButtons);
    interval->setFixedWidth(80);
    interval->setAlignment(Qt::AlignRight);
    auto *intervalRow = new QHBoxLayout;
    intervalRow->setContentsMargins(10, 4, 10, 4);
    auto *intervalText = new QVBoxLayout;
    intervalText->setSpacing(1);
    intervalText->addWidget(Ui::label(tr("Cloud AI looks at least every")));
    intervalText->addWidget(Ui::label(tr("Shorter is more responsive; each look costs about $0.0001."), "secondary"));
    intervalRow->addLayout(intervalText, 1);
    intervalRow->addWidget(interval);
    coachCard->body()->addLayout(intervalRow);
    connect(interval, &QSpinBox::valueChanged, &settings, &AppSettings::setSnapshotIntervalSeconds);

    auto *advance = new ToggleSwitch;
    advance->setChecked(settings.autoAdvance());
    connect(advance, &QAbstractButton::toggled, &settings, &AppSettings::setAutoAdvance);
    coachCard->body()->addWidget(new OptionRow(tr("Move to the next section automatically"),
                                               tr("When you start talking about the next section's topic, SMILE moves on "
                                                  "(and resets the section timer). You can always move on by hand."),
                                               advance));

    // ---- Appearance
    auto *lookCard = new Card(tr("Appearance"));
    auto *lookRow = new QHBoxLayout;
    lookRow->setSpacing(4);
    auto *lookGroup = new QButtonGroup(this);
    const QStringList looks = {tr("System"), tr("Light (white & blue)"), tr("Dark")};
    for (int i = 0; i < looks.size(); ++i) {
        auto *b = new QPushButton(looks[i]);
        b->setObjectName(QStringLiteral("segment"));
        b->setCheckable(true);
        b->setChecked(settings.themeMode() == i);
        b->setCursor(Qt::PointingHandCursor);
        lookGroup->addButton(b, i);
        lookRow->addWidget(b);
    }
    lookRow->addStretch();
    lookCard->body()->addLayout(lookRow);
    connect(lookGroup, &QButtonGroup::idClicked, this, [](int id) {
        AppSettings::instance().setThemeMode(id);
        Theme::reapply();
    });

    // ---- Debugging
    auto *debugCard = new Card(tr("Debugging"));
    auto *debugToggle = new ToggleSwitch;
    debugToggle->setChecked(settings.debugMode());
    debugCard->body()->addWidget(new OptionRow(
        tr("Debug mode"),
        tr("Show face-tracking points on the camera preview, explain why every tip fired, and open a live view of "
           "everything SMILE detects when a session starts."),
        debugToggle));
    connect(debugToggle, &QAbstractButton::toggled, &settings, &AppSettings::setDebugMode);
    auto *hapticToggle = new ToggleSwitch;
    hapticToggle->setChecked(settings.hapticFeedbackEnabled());
    debugCard->body()->addWidget(new OptionRow(
        tr("Simulate mouse haptics"),
        tr("Log a simulated mouse pulse when a warning appears during a meeting."),
        hapticToggle));
    connect(hapticToggle, &QAbstractButton::toggled, &settings, &AppSettings::setHapticFeedbackEnabled);
    auto *physicalHaptics = new ToggleSwitch;
    physicalHaptics->setChecked(settings.physicalHapticsEnabled());
    debugCard->body()->addWidget(new OptionRow(
        tr("Play mouse haptics"),
        tr("Pulse an MX Master 4 for visible meeting warnings. Requires the SMILE plugin in Logi Options+. "
           "Overrides simulation while on."),
        physicalHaptics));
    connect(physicalHaptics, &QAbstractButton::toggled, &settings, &AppSettings::setPhysicalHapticsEnabled);
    auto addHapticSetting = [debugCard](const QString &title, const QString &description, QWidget *control) {
        auto *row = new QHBoxLayout;
        row->setContentsMargins(10, 4, 10, 4);
        auto *labels = new QVBoxLayout;
        labels->setSpacing(1);
        labels->addWidget(Ui::label(title));
        labels->addWidget(Ui::label(description, "secondary"));
        row->addLayout(labels, 1);
        control->setAccessibleName(title);
        row->addWidget(control);
        debugCard->body()->addLayout(row);
    };
    auto *hapticDuration = new QSpinBox;
    hapticDuration->setRange(0, 120);
    hapticDuration->setSpecialValueText(tr("Until resolved"));
    hapticDuration->setSuffix(tr(" s"));
    hapticDuration->setValue(settings.hapticDurationSeconds());
    addHapticSetting(tr("Vibration duration"),
                     tr("Repeat while the warning is visible, until the issue clears or this time limit is reached."),
                     hapticDuration);
    connect(hapticDuration, &QSpinBox::valueChanged, &settings, &AppSettings::setHapticDurationSeconds);
    auto *hapticIntensity = new QComboBox;
    hapticIntensity->addItems({tr("Low · gentle tap"), tr("Medium · sharp pulse"), tr("High · multi-tap")});
    hapticIntensity->setCurrentIndex(settings.hapticIntensity());
    addHapticSetting(tr("Vibration intensity"),
                     tr("Choose how noticeable each pulse feels."), hapticIntensity);
    connect(hapticIntensity, &QComboBox::currentIndexChanged, &settings, &AppSettings::setHapticIntensity);
    auto *hapticSpeed = new QComboBox;
    hapticSpeed->addItem(tr("Slow · every 3 seconds"), 3000);
    hapticSpeed->addItem(tr("Normal · every second"), 1000);
    hapticSpeed->addItem(tr("Fast · twice a second"), 500);
    hapticSpeed->setCurrentIndex(hapticSpeed->findData(settings.hapticPulseIntervalMs()));
    addHapticSetting(tr("Vibration speed"),
                     tr("Choose how often pulses repeat while a warning stays visible."), hapticSpeed);
    connect(hapticSpeed, &QComboBox::currentIndexChanged, &settings, [hapticSpeed](int index) {
        AppSettings::instance().setHapticPulseIntervalMs(hapticSpeed->itemData(index).toInt());
    });
    auto *openDebug = new QPushButton(tr("Open debug view"));
    openDebug->setIcon(Icons::icon(Icons::Icon::Bug, Theme::colors().text, 14));
    openDebug->setCursor(Qt::PointingHandCursor);
    connect(openDebug, &QPushButton::clicked, this, &SettingsPage::openDebugRequested);
    auto *debugRow = new QHBoxLayout;
    debugRow->setContentsMargins(10, 0, 10, 0);
    debugRow->addWidget(openDebug);
    debugRow->addStretch();
    debugCard->body()->addLayout(debugRow);

    // ---- Camera
    auto *cameraCard = new Card(tr("Camera"));
    auto *useCamera = new ToggleSwitch;
    useCamera->setChecked(settings.useCamera());
    cameraCard->body()->addWidget(new OptionRow(tr("Use the camera"),
                                                tr("For the Pre-Meet check and posture and expression tips."), useCamera));
    connect(useCamera, &QAbstractButton::toggled, &settings, &AppSettings::setUseCamera);
    auto *cameraRow = new QHBoxLayout;
    cameraRow->setContentsMargins(10, 0, 10, 0);
    cameraRow->addWidget(Ui::label(tr("Device")));
    cameraRow->addStretch();
    m_cameraCombo->setMinimumWidth(280);
    cameraRow->addWidget(m_cameraCombo);
    cameraCard->body()->addLayout(cameraRow);

    // ---- Microphone
    auto *micCard = new Card(tr("Microphone"));
    auto *useMic = new ToggleSwitch;
    useMic->setChecked(settings.useMicrophone());
    micCard->body()->addWidget(new OptionRow(tr("Use the microphone"),
                                             tr("For talk-time, long monologues, filler words and pace."), useMic));
    connect(useMic, &QAbstractButton::toggled, &settings, &AppSettings::setUseMicrophone);
    auto *micRow = new QHBoxLayout;
    micRow->setContentsMargins(10, 0, 10, 0);
    micRow->addWidget(Ui::label(tr("Device")));
    micRow->addStretch();
    m_micCombo->setMinimumWidth(280);
    micRow->addWidget(m_micCombo);
    micCard->body()->addLayout(micRow);
    auto *meterRow = new QHBoxLayout;
    meterRow->setContentsMargins(10, 4, 10, 4);
    meterRow->addWidget(Ui::label(tr("Input level")));
    meterRow->addSpacing(16);
    meterRow->addWidget(m_meter, 1);
    micCard->body()->addLayout(meterRow);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(36, 28, 36, 28);
    layout->setSpacing(12);
    layout->addWidget(Ui::pageHeader(tr("Settings"), tr("AI, camera, microphone and privacy.")));
    layout->addWidget(aiCard);
    layout->addWidget(coachCard);
    layout->addWidget(lookCard);
    layout->addWidget(cameraCard);
    layout->addWidget(micCard);
    layout->addWidget(debugCard);
    layout->addStretch();

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(Ui::scrollPage(content));

    connect(save, &QPushButton::clicked, this, [this] {
        if (m_keyEdit->text().trimmed().isEmpty())
            return;
        AppSettings::instance().setApiKey(m_keyEdit->text());
        m_keyEdit->clear();
        refreshKeyStatus();
        m_testResult->setText(tr("Saved."));
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        AppSettings::instance().setApiKey(QString());
        refreshKeyStatus();
        m_testResult->clear();
    });
    connect(test, &QPushButton::clicked, this, [this, test] {
        test->setEnabled(false);
        m_testResult->setText(tr("Testing…"));
        m_ai->testConnection([this, test](const QString &text, const QString &error) {
            test->setEnabled(true);
            const Theme::Colors &c = Theme::colors();
            m_testResult->setText(QStringLiteral("<span style='color:%1'>%2</span>")
                                      .arg(Theme::css(error.isEmpty() ? c.success : c.critical),
                                           (error.isEmpty() ? text : error).toHtmlEscaped()));
        });
    });
    connect(m_cameraCombo, &QComboBox::activated, this, [this] {
        AppSettings::instance().setCameraId(m_cameraCombo->currentData().toByteArray());
    });
    connect(m_micCombo, &QComboBox::activated, this, [this] {
        AppSettings::instance().setMicrophoneId(m_micCombo->currentData().toByteArray());
        if (m_micAcquired) { // restart on the new device
            m_mic->release();
            m_mic->acquire();
        }
    });
    connect(m_mic, &MicrophoneMonitor::levelChanged, m_meter, &LevelMeter::setLevel);
    connect(m_mic, &MicrophoneMonitor::speakingChanged, m_meter, &LevelMeter::setSpeaking);

    refreshKeyStatus();
    fillDevices();
}

void SettingsPage::describeSensitivity()
{
    const AppSettings::Thresholds t = AppSettings::instance().thresholds();
    m_sensitivityText->setText(tr("Tips after: tense or cold expression %1 s · looking to the side %2 s · face out of frame %3 s · bad posture %4 s · "
                                  "speaking over %5 wpm · %6 filler words a minute · talking %7 s without a pause. "
                                  "The same tip waits %8 s before repeating; at least %9 s between tips in a meeting.")
                                   .arg(t.noSmileSeconds)
                                   .arg(t.lookAwaySeconds)
                                   .arg(t.faceMissingSeconds)
                                   .arg(t.postureSeconds)
                                   .arg(t.fastWpm)
                                   .arg(t.fillersPerMinute)
                                   .arg(t.monologueSeconds)
                                   .arg(t.repeatCooldownSeconds)
                                   .arg(t.meetingGapSeconds));
}

void SettingsPage::refreshKeyStatus()
{
    const Theme::Colors &c = Theme::colors();
    QString text;
    QColor colour = c.success;
    switch (AppSettings::instance().apiKeySource()) {
    case AppSettings::KeySource::Settings:
        text = tr("Key saved on this computer.");
        break;
    case AppSettings::KeySource::Environment:
        text = tr("Using the key from the TOGETHER_API_KEY environment variable.");
        break;
    case AppSettings::KeySource::DotEnv:
        text = tr("Using the key from the .env file.");
        break;
    case AppSettings::KeySource::None:
        text = tr("No key yet. SMILE still times your agenda, checks lighting and spots long monologues; "
                  "add a key for smile, posture and speech coaching.");
        colour = c.textSecondary;
        break;
    }
    m_keyStatus->setText(QStringLiteral("<span style='color:%1'>%2</span>").arg(Theme::css(colour), text.toHtmlEscaped()));
}

void SettingsPage::fillDevices()
{
    const AppSettings &settings = AppSettings::instance();
    m_cameraCombo->clear();
    m_cameraCombo->addItem(tr("System default"), QByteArray());
    for (const QCameraDevice &d : QMediaDevices::videoInputs()) {
        m_cameraCombo->addItem(d.description(), d.id());
        if (d.id() == settings.cameraId())
            m_cameraCombo->setCurrentIndex(m_cameraCombo->count() - 1);
    }
    m_micCombo->clear();
    m_micCombo->addItem(tr("System default"), QByteArray());
    for (const QAudioDevice &d : QMediaDevices::audioInputs()) {
        m_micCombo->addItem(d.description(), d.id());
        if (d.id() == settings.microphoneId())
            m_micCombo->setCurrentIndex(m_micCombo->count() - 1);
    }
}

void SettingsPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    fillDevices();
    refreshKeyStatus();
    if (!m_micAcquired && AppSettings::instance().useMicrophone()) {
        m_micAcquired = true; // live level meter while this page is open
        m_mic->acquire();
    }
}

void SettingsPage::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    if (m_micAcquired) {
        m_micAcquired = false;
        m_mic->release();
    }
}
