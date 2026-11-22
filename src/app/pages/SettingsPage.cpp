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
#include <QSlider>
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
    QLabel *aiIntro = Ui::label(tr("SMILE uses Together AI to understand your face and speech."), "secondary");
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
        tr("Sends snapshots and short speech clips while a session runs. Nothing is stored."),
        cloud));
    connect(cloud, &QAbstractButton::toggled, &settings, &AppSettings::setCloudAnalysis);

    // ---- Coaching: how picky SMILE is, and how smart
    auto *coachCard = new Card(tr("Coaching"));
    coachCard->body()->setSpacing(0);
    auto addControl = [](Card *card, const QString &title, const QString &hint, QWidget *control) {
        card->body()->addWidget(new OptionRow(title, hint, control));
    };
    auto makeLevel = [this](int value, std::function<void(int)> set) {
        auto *slider = new QSlider(Qt::Horizontal);
        slider->setRange(1, 5);
        slider->setPageStep(1);
        slider->setTickPosition(QSlider::TicksBelow);
        slider->setFixedWidth(180);
        slider->setValue(value);
        connect(slider, &QSlider::valueChanged, this, [this, set](int v) {
            set(v);
            describeSensitivity();
        });
        return slider;
    };
    addControl(coachCard, tr("Real meetings: how picky"), tr("Left: only big problems. Right: everything."),
               makeLevel(settings.liveLevel(), [](int v) { AppSettings::instance().setLiveLevel(v); }));
    addControl(coachCard, tr("Training: how picky"), tr("Training is stricter so you improve faster."),
               makeLevel(settings.trainingLevel(), [](int v) { AppSettings::instance().setTrainingLevel(v); }));
    m_sensitivityText = Ui::label(QString(), "caption");
    m_sensitivityText->setWordWrap(true);
    m_sensitivityText->setContentsMargins(10, 0, 10, 6);
    coachCard->body()->addWidget(m_sensitivityText);
    describeSensitivity();

    auto *brain = new QComboBox;
    brain->addItem(tr("Standard · cheapest"));
    brain->addItem(tr("Enhanced · ~5× cost"));
    brain->addItem(tr("Maximum · ~15× cost"));
    brain->setCurrentIndex(settings.intelligence());
    brain->setToolTip(tr("Smarter models read your face and speech more carefully but cost more and answer a bit slower. "
                         "Conversations in the simulation always use the fast model."));
    connect(brain, &QComboBox::currentIndexChanged, &settings, &AppSettings::setIntelligence);
    addControl(coachCard, tr("AI intelligence"), tr("Smarter costs more."), brain);

    auto *interval = new QSpinBox;
    interval->setRange(1, 30);
    interval->setSuffix(tr(" s"));
    interval->setValue(settings.snapshotIntervalSeconds());
    interval->setFixedWidth(80);
    interval->setAlignment(Qt::AlignRight);
    connect(interval, &QSpinBox::valueChanged, &settings, &AppSettings::setSnapshotIntervalSeconds);
    addControl(coachCard, tr("AI looks at you every"), tr("Shorter reacts faster (≈ $0.0001 a look)."), interval);

    auto *advance = new ToggleSwitch;
    advance->setChecked(settings.autoAdvance());
    connect(advance, &QAbstractButton::toggled, &settings, &AppSettings::setAutoAdvance);
    addControl(coachCard, tr("Move to the next section by itself"), tr("When you start on its topic."), advance);

    // ---- Appearance
    auto *lookCard = new Card(tr("Appearance"));
    auto *lookRow = new QHBoxLayout;
    lookRow->setSpacing(4);
    auto *lookGroup = new QButtonGroup(this);
    const QStringList looks = {tr("System"), tr("Light (white && blue)"), tr("Dark")};
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
        tr("Face-tracking points, why each tip fired, live detection view."),
        debugToggle));
    connect(debugToggle, &QAbstractButton::toggled, &settings, &AppSettings::setDebugMode);
    auto *hapticCard = new Card(tr("Mouse haptics"));
    auto *physicalHaptics = new ToggleSwitch;
    physicalHaptics->setChecked(settings.physicalHapticsEnabled());
    hapticCard->body()->addWidget(new OptionRow(
        tr("Mouse vibration"),
        tr("Vibrate when a coaching tip appears in Training or Meet. Requires an MX Master 4 "
           "and the SMILE plugin in Logi Options+."),
        physicalHaptics));
    connect(physicalHaptics, &QAbstractButton::toggled, &settings, &AppSettings::setPhysicalHapticsEnabled);
    auto addHapticSetting = [hapticCard](const QString &title, const QString &description, QWidget *control) {
        auto *row = new QHBoxLayout;
        row->setContentsMargins(10, 4, 10, 4);
        auto *labels = new QVBoxLayout;
        labels->setSpacing(1);
        labels->addWidget(Ui::label(title));
        labels->addWidget(Ui::label(description, "secondary"));
        row->addLayout(labels, 1);
        control->setAccessibleName(title);
        row->addWidget(control);
        hapticCard->body()->addLayout(row);
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
    layout->addWidget(Ui::pageHeader(tr("Settings"), tr("Make SMILE yours.")));
    layout->addWidget(aiCard);
    layout->addWidget(coachCard);
    layout->addWidget(lookCard);
    layout->addWidget(cameraCard);
    layout->addWidget(micCard);
    layout->addWidget(hapticCard);
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
    const AppSettings &settings = AppSettings::instance();
    auto line = [](const AppSettings::Thresholds &t) {
        return tr("face hidden %1 s · looking away %2 s · no smile %3 s · slouching %4 s · %5 fillers/min · "
                  "talking %6 min without a break")
            .arg(t.faceCoveredSeconds)
            .arg(t.lookAwaySeconds)
            .arg(t.noSmileSeconds * 2)
            .arg(t.postureSeconds)
            .arg(t.fillersPerMinute)
            .arg(t.monologueSeconds / 60.0, 0, 'g', 2);
    };
    m_sensitivityText->setText(tr("Meetings: %1\nTraining: %2")
                                   .arg(line(AppSettings::thresholdsFor(settings.liveLevel())),
                                        line(AppSettings::thresholdsFor(settings.trainingLevel()))));
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
        text = tr("No key yet: add one for full coaching.");
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
