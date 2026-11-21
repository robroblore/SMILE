#include "app/DebugWindow.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <cmath>

#include "core/AppSettings.h"
#include "core/SessionController.h"
#include "sensors/CameraMonitor.h"
#include "sensors/MicrophoneMonitor.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

namespace {
QString yesNo(int v)
{
    return v < 0 ? DebugWindow::tr("unknown") : (v ? DebugWindow::tr("yes") : DebugWindow::tr("no"));
}
} // namespace

DebugWindow::DebugWindow(CameraMonitor *camera, MicrophoneMonitor *mic, SessionController *controller, QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_camera(camera)
    , m_mic(mic)
    , m_controller(controller)
    , m_preview(new CameraPreview)
    , m_cameraStats(Ui::label(QString(), "secondary"))
    , m_banner(Ui::label(QString()))
    , m_chips(new QGridLayout)
    , m_meter(new LevelMeter)
    , m_voice(Ui::label(QString()))
    , m_cloud(Ui::label(QString()))
    , m_cloudJson(new QPlainTextEdit)
    , m_log(new QListWidget)
{
    setObjectName(QStringLiteral("root"));
    setAttribute(Qt::WA_StyledBackground);
    setWindowTitle(tr("SMILE · Debug view"));
    resize(1180, 780);

    m_preview->setOverlay(true);
    m_preview->setMinimumSize(480, 300);

    // ---- Left: camera + voice
    auto *cameraCard = new Card(tr("Face tracking (on this computer)"));
    cameraCard->body()->addWidget(m_preview, 1);
    m_cameraStats->setWordWrap(true);
    cameraCard->body()->addWidget(m_cameraStats);

    auto *voiceCard = new Card(tr("Voice"));
    voiceCard->body()->addWidget(m_meter);
    m_voice->setWordWrap(true);
    m_voice->setTextFormat(Qt::RichText);
    voiceCard->body()->addWidget(m_voice);

    auto *left = new QVBoxLayout;
    left->setSpacing(12);
    left->addWidget(cameraCard, 1);
    left->addWidget(voiceCard, 0);

    // ---- Right: banner, detections, cloud, log
    m_banner->setWordWrap(true);
    m_banner->setMinimumHeight(54);
    m_banner->setTextFormat(Qt::RichText);
    m_banner->setContentsMargins(14, 8, 14, 8);
    m_banner->hide();

    auto *detections = new Card(tr("What SMILE detects right now"));
    m_chips->setHorizontalSpacing(8);
    m_chips->setVerticalSpacing(8);
    for (int i = 0; i < 8; ++i) {
        QLabel *chip = Ui::label(QString());
        chip->setTextFormat(Qt::RichText);
        chip->setWordWrap(true);
        chip->setMinimumHeight(58);
        m_chipLabels << chip;
        m_chips->addWidget(chip, i / 4, i % 4);
    }
    detections->body()->addLayout(m_chips);

    auto *cloudCard = new Card(tr("Cloud AI (every few seconds, or when the face tracker sees a change)"));
    m_cloud->setWordWrap(true);
    m_cloud->setTextFormat(Qt::RichText);
    cloudCard->body()->addWidget(m_cloud);
    m_cloudJson->setReadOnly(true);
    m_cloudJson->setFixedHeight(120);
    QFont mono(QStringLiteral("monospace"));
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSizeF(font().pointSizeF() * 0.85);
    m_cloudJson->setFont(mono);
    cloudCard->body()->addWidget(m_cloudJson);

    auto *logCard = new Card(tr("Alerts and tips, with why they fired"));
    m_log->setWordWrap(true);
    m_log->setFrameShape(QFrame::NoFrame);
    m_log->setStyleSheet(QStringLiteral("QListWidget { background: transparent; } QListWidget::item { padding: 6px 2px; }"));
    logCard->body()->addWidget(m_log, 1);
    auto *clear = new QPushButton(tr("Clear"));
    clear->setObjectName(QStringLiteral("subtle"));
    connect(clear, &QPushButton::clicked, this, [this] {
        LiveSignals::instance().clearEvents();
        m_log->clear();
    });
    logCard->body()->addWidget(clear, 0, Qt::AlignRight);

    auto *right = new QVBoxLayout;
    right->setSpacing(12);
    right->addWidget(m_banner);
    right->addWidget(detections);
    right->addWidget(cloudCard);
    right->addWidget(logCard, 1);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    layout->addLayout(left, 5);
    layout->addLayout(right, 6);

    m_refresh.setInterval(100);
    connect(&m_refresh, &QTimer::timeout, this, &DebugWindow::refresh);
    m_bannerTimer.setSingleShot(true);
    connect(&m_bannerTimer, &QTimer::timeout, m_banner, &QWidget::hide);
    connect(m_camera, &CameraMonitor::frameReady, this, [this](const QImage &frame) {
        if (isVisible())
            m_preview->setFrame(frame);
    });
    connect(&LiveSignals::instance(), &LiveSignals::eventAdded, this, &DebugWindow::onEvent);
    connect(m_mic, &MicrophoneMonitor::levelChanged, m_meter, &LevelMeter::setLevel);
    connect(m_mic, &MicrophoneMonitor::speakingChanged, m_meter, &LevelMeter::setSpeaking);

    for (const LiveSignals::Event &e : LiveSignals::instance().events())
        onEvent(e);
}

void DebugWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_acquired) {
        m_acquired = true;
        m_camera->acquire();
        m_mic->acquire();
    }
    m_refresh.start();
}

void DebugWindow::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_refresh.stop();
    if (m_acquired) {
        m_acquired = false;
        m_camera->release();
        m_mic->release();
    }
}

void DebugWindow::setChip(int index, const QString &name, const QString &value, int rating, const QString &detail)
{
    const Theme::Colors &c = Theme::colors();
    const QColor colour = Theme::ratingColor(rating);
    QColor bg = colour;
    bg.setAlpha(rating ? (c.dark ? 40 : 28) : 12);
    QLabel *chip = m_chipLabels.value(index);
    if (!chip)
        return;
    chip->setStyleSheet(QStringLiteral("background: %1; border-radius: 8px; padding: 6px 10px;").arg(Theme::css(bg)));
    chip->setText(QStringLiteral("<span style='color:%1; font-size:8.5pt; font-weight:600'>%2</span><br>"
                                 "<span style='color:%3; font-size:13pt; font-weight:700'>%4</span><br>"
                                 "<span style='color:%1; font-size:8pt'>%5</span>")
                      .arg(Theme::css(c.textSecondary), name.toUpper().toHtmlEscaped(),
                           Theme::css(rating ? colour : c.text), value.toHtmlEscaped(), detail.toHtmlEscaped()));
}

void DebugWindow::refresh()
{
    const LiveSignals &s = LiveSignals::instance();
    const LiveSignals::Vision &v = s.vision;
    const LiveSignals::Voice &a = s.voice;
    const bool session = m_controller->isRunning();
    const AppSettings::Thresholds t = AppSettings::instance().thresholds();
    const Theme::Colors &c = Theme::colors();

    // Camera + face (straight from the tracker, so it works outside sessions too)
    const FaceFrame face = m_camera->latestFace();
    const bool looking = face.found && std::abs(face.yaw) < 0.22f
                         && (!session || v.lookingLocal || std::abs(face.pitch - v.pitchBaseline) < 0.16f);
    m_preview->setFace(face, looking);
    if (!m_camera->faceTrackingAvailable())
        m_cameraStats->setText(tr("This build has no local face tracker; the cloud AI does all face analysis."));
    else
        m_cameraStats->setText(tr("Camera %1 · %2 fps shown · conversion %3 ms · face detection %4 ms (YuNet, ~15×/s) · %5")
                                   .arg(m_camera->formatDescription().isEmpty() ? tr("starting…") : m_camera->formatDescription())
                                   .arg(m_camera->framesPerSecond(), 0, 'f', 0)
                                   .arg(m_camera->convertMs(), 0, 'f', 1)
                                   .arg(face.detectMs, 0, 'f', 1)
                                   .arg(m_camera->errorString().isEmpty() ? tr("OK") : m_camera->errorString()));

    // Detections
    setChip(0, tr("Face"), face.found ? tr("in frame") : tr("not found"), face.found ? 1 : 3,
            face.found ? tr("score %1, size %2%").arg(face.score, 0, 'f', 2).arg(int(face.faceHeight * 100))
                       : (session ? tr("missing %1 s (limit %2)").arg(v.faceMissingSeconds).arg(t.faceMissingSeconds) : QString()));
    setChip(1, tr("Eye contact"), face.found ? (looking ? tr("yes") : tr("looking away")) : tr("–"),
            face.found ? (looking ? 1 : 2) : 0,
            session ? tr("away %1 s (limit %2)").arg(v.lookAwaySeconds).arg(t.lookAwaySeconds)
                    : tr("yaw %1 (±0.22)").arg(face.yaw, 0, 'f', 2));
    setChip(2, tr("Smiling"), session ? yesNo(v.smilingFused) : tr("–"),
            v.smilingFused < 0 || !session ? 0 : (v.smilingFused ? 1 : 2),
            session ? tr("via %1 · no smile %2 s (limit %3)").arg(v.smilingSource).arg(v.noSmileSeconds).arg(t.noSmileSeconds)
                    : tr("starts with a session"));
    setChip(3, tr("Engagement"), session ? QStringLiteral("%1%").arg(int(v.engagement * 100)) : tr("–"),
            !session ? 0 : (v.engagement >= 0.7f ? 1 : (v.engagement >= 0.5f ? 2 : 3)),
            tr("eye contact + smile + in frame"));
    const QString posture = v.cloud.value(QLatin1String("posture")).toString();
    setChip(4, tr("Posture"), posture.isEmpty() ? tr("–") : QString(posture).replace(QLatin1Char('_'), QLatin1Char(' ')),
            posture.isEmpty() ? 0 : (posture == QLatin1String("upright") ? 1 : 2), tr("cloud AI"));
    const QString expression = v.cloud.value(QLatin1String("expression")).toString();
    setChip(5, tr("Expression"), expression.isEmpty() ? tr("–") : expression,
            expression.isEmpty() ? 0 : (expression == QLatin1String("warm") ? 1 : (expression == QLatin1String("tense") ? 3 : 2)),
            tr("cloud AI"));
    const int wpm = a.wpmWhisper > 0 && a.wpmLocal == 0 ? a.wpmWhisper : a.wpmLocal;
    setChip(6, tr("Pace"), wpm > 0 ? tr("%1 wpm").arg(wpm) : tr("–"),
            wpm == 0 ? 0 : (wpm > t.fastWpm ? 3 : (wpm < 110 ? 2 : 1)),
            tr("local %1 · Whisper %2 · limit %3").arg(a.wpmLocal).arg(a.wpmWhisper).arg(t.fastWpm));
    setChip(7, tr("Filler words"), session ? tr("%1 / min").arg(a.fillersLastMinute) : tr("–"),
            !session ? 0 : (a.fillersLastMinute >= t.fillersPerMinute ? 3 : (a.fillersLastMinute >= 2 ? 2 : 1)),
            a.lastFillers.isEmpty() ? tr("limit %1 per minute").arg(t.fillersPerMinute) : a.lastFillers);

    // Voice
    m_voice->setText(tr("Level <b>%1 dB</b> · noise floor %2 dB · speech threshold %3 dB · "
                        "<b style='color:%4'>%5</b><br>"
                        "Talking without a pause: %6 s (limit %7 s) · %8 syllables/s<br>"
                        "Last transcript (%9 ms): <i>%10</i>%11")
                         .arg(m_mic->levelDb(), 0, 'f', 0)
                         .arg(m_mic->noiseFloorDb(), 0, 'f', 0)
                         .arg(m_mic->thresholdDb(), 0, 'f', 0)
                         .arg(Theme::css(m_mic->isSpeaking() ? c.success : c.textSecondary))
                         .arg(m_mic->isSpeaking() ? tr("speaking") : tr("quiet"))
                         .arg(m_mic->currentRunMs() / 1000)
                         .arg(t.monologueSeconds)
                         .arg(m_mic->syllablesPerSecond(), 0, 'f', 1)
                         .arg(a.transcribeLatencyMs)
                         .arg(a.lastTranscript.isEmpty() ? tr("none yet") : a.lastTranscript.left(200).toHtmlEscaped())
                         .arg(a.error.isEmpty() ? QString()
                                                : QStringLiteral("<br><span style='color:%1'>%2</span>")
                                                      .arg(Theme::css(c.critical), a.error.toHtmlEscaped())));

    // Cloud
    if (!session) {
        m_cloud->setText(tr("The cloud AI looks while a rehearsal or meeting is running."));
    } else {
        m_cloud->setText(tr("Checks: <b>%1</b> · last answer %2 s ago in %3 ms · last request because: <b>%4</b>%5"
                            "<br><i>%6</i>")
                             .arg(v.cloudChecks)
                             .arg(v.cloudAgeMs < 0 ? 0 : v.cloudAgeMs / 1000)
                             .arg(v.cloudLatencyMs)
                             .arg(v.cloudTrigger.toHtmlEscaped())
                             .arg(v.cloudError.isEmpty() ? QString()
                                                         : QStringLiteral("<br><span style='color:%1'>%2</span>")
                                                               .arg(Theme::css(c.critical), v.cloudError.toHtmlEscaped()))
                             .arg(v.cloud.value(QLatin1String("observation")).toString().toHtmlEscaped()));
        const QString json = QString::fromUtf8(QJsonDocument(v.cloud).toJson(QJsonDocument::Indented));
        if (m_cloudJson->toPlainText() != json)
            m_cloudJson->setPlainText(json);
    }
}

void DebugWindow::onEvent(const LiveSignals::Event &event)
{
    const Theme::Colors &c = Theme::colors();
    const QString time = event.time.toString(QStringLiteral("HH:mm:ss"));
    QString kind;
    QColor colour;
    switch (event.kind) {
    case LiveSignals::Event::Alert:
        kind = tr("DETECTED");
        colour = c.orange;
        break;
    case LiveSignals::Event::Tip:
        kind = tr("TIP");
        colour = c.accent;
        break;
    case LiveSignals::Event::Info:
        kind = tr("INFO");
        colour = c.success;
        break;
    }
    auto *item = new QListWidgetItem(QStringLiteral("%1  %2  %3%4")
                                         .arg(time, kind, event.title,
                                              event.detail.isEmpty() ? QString() : QStringLiteral("\n    ") + event.detail));
    item->setForeground(event.kind == LiveSignals::Event::Info ? c.textSecondary : c.text);
    m_log->insertItem(0, item);
    while (m_log->count() > 200)
        delete m_log->takeItem(m_log->count() - 1);

    // Prominent banner for live detections and tips
    if (event.kind != LiveSignals::Event::Info) {
        QColor bg = colour;
        bg.setAlpha(c.dark ? 60 : 40);
        m_banner->setStyleSheet(QStringLiteral("background: %1; border: 1px solid %2; border-radius: 8px;")
                                    .arg(Theme::css(bg), Theme::css(colour)));
        m_banner->setText(QStringLiteral("<span style='font-size:13pt; font-weight:700'>%1</span><br>%2")
                              .arg(event.title.toHtmlEscaped(), event.detail.toHtmlEscaped()));
        m_banner->show();
        m_bannerTimer.start(5000);
    }
}
