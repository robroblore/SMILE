#include "app/pages/SettingsPage.h"

#include <QAudioDevice>
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

    auto *interval = new QSpinBox;
    interval->setRange(5, 120);
    interval->setSuffix(tr(" s"));
    interval->setValue(settings.snapshotIntervalSeconds());
    interval->setButtonSymbols(QAbstractSpinBox::NoButtons);
    interval->setFixedWidth(80);
    interval->setAlignment(Qt::AlignRight);
    auto *intervalRow = new QHBoxLayout;
    intervalRow->setContentsMargins(10, 4, 10, 4);
    auto *intervalText = new QVBoxLayout;
    intervalText->setSpacing(1);
    intervalText->addWidget(Ui::label(tr("Look every")));
    intervalText->addWidget(Ui::label(tr("How often a snapshot is analysed. Shorter is more responsive."), "secondary"));
    intervalRow->addLayout(intervalText, 1);
    intervalRow->addWidget(interval);
    aiCard->body()->addLayout(intervalRow);
    connect(interval, &QSpinBox::valueChanged, &settings, &AppSettings::setSnapshotIntervalSeconds);

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
    layout->addWidget(cameraCard);
    layout->addWidget(micCard);
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
