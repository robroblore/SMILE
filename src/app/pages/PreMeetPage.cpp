#include "app/pages/PreMeetPage.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>

#include "ai/AiClient.h"
#include "core/AppSettings.h"
#include "sensors/CameraMonitor.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "vision/FaceDetector.h"

namespace {
// Checklist rows the camera check can verify, in order
enum Item { EyeLevel, Lighting, Collar, Distractions, Upright, Notifications, Agenda };
} // namespace

PreMeetPage::PreMeetPage(CameraMonitor *camera, AiClient *ai, QWidget *parent)
    : QWidget(parent)
    , m_camera(camera)
    , m_ai(ai)
    , m_preview(new CameraPreview)
    , m_checkButton(new QPushButton(tr("Check my setup")))
    , m_status(Ui::label(QString(), "secondary"))
    , m_issues(new QVBoxLayout)
    , m_tech(Ui::label(QString(), "secondary"))
    , m_progressText(Ui::label(QString(), "secondary"))
{
    const Theme::Colors &c = Theme::colors();

    // ---- The camera: the main focus of the page
    auto *cameraCard = new Card;
    cameraCard->setGraphicsEffect(nullptr); // no shadow effect: keeps the 30 fps preview smooth
    cameraCard->body()->setContentsMargins(16, 16, 16, 16);
    // Tracking points only in debug mode; the red "fix this" boxes always show
    m_preview->setOverlay(AppSettings::instance().debugMode());
    connect(&AppSettings::instance(), &AppSettings::changed, m_preview,
            [this] { m_preview->setOverlay(AppSettings::instance().debugMode()); });
    m_preview->setMinimumSize(560, 315);
    m_preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_preview->setFixedHeight(470);
    cameraCard->body()->addWidget(m_preview);

    auto *actions = new QHBoxLayout;
    m_checkButton->setObjectName(QStringLiteral("accent"));
    m_checkButton->setIcon(Icons::icon(Icons::Icon::Sparkle, c.onAccent, 14));
    m_checkButton->setCursor(Qt::PointingHandCursor);
    m_checkButton->setMinimumHeight(38);
    m_status->setWordWrap(true);
    actions->addWidget(m_checkButton);
    actions->addSpacing(12);
    actions->addWidget(m_status, 1);
    cameraCard->body()->addLayout(actions);
    m_issues->setSpacing(6);
    cameraCard->body()->addLayout(m_issues);

    QLabel *privacy = Ui::label(tr("Only you see this. Red boxes mark things to fix."), "secondary");
    privacy->setWordWrap(true);
    cameraCard->body()->addWidget(privacy);

    // ---- Collapsible: checklist
    struct Entry {
        QString title;
        QString hint;
    };
    const QList<Entry> entries = {
        {tr("Camera at eye level"), tr("Your face centred, with a little space above your head.")},
        {tr("Good lighting"), tr("Light in front of you, not behind. No harsh shadows.")},
        {tr("Collar and clothes straight"), tr("Check the preview above.")},
        {tr("Nothing distracting in view"), tr("Notes, clutter and screens out of the frame.")},
        {tr("Sitting upright"), tr("Feet flat, shoulders relaxed, back against the chair.")},
        {tr("Notifications muted"), tr("Phone silent, chat and email notifications off.")},
        {tr("Agenda ready"), tr("Your sections are set on the Training page.")},
    };
    auto *checklist = new Expander(tr("Readiness checklist"), tr("Ticked automatically by the camera check, or by you."), -1,
                                   c.accent, QString());
    checklist->body()->addWidget(m_progressText);
    for (const Entry &entry : entries) {
        auto *check = new CheckCircle;
        checklist->body()->addWidget(new OptionRow(entry.title, entry.hint, check, true));
        connect(check, &QAbstractButton::toggled, this, &PreMeetPage::updateProgress);
        m_items.append(check);
    }

    // ---- Collapsible: meeting options and technical details
    auto *options = new Expander(tr("Camera details"), QString(), -1, c.accent, QString());
    m_tech->setWordWrap(true);
    options->body()->addWidget(m_tech);

    auto *go = new QPushButton(tr("I'm ready: go to Meet"));
    go->setObjectName(QStringLiteral("accent"));
    go->setIcon(Icons::icon(Icons::Icon::Next, c.onAccent, 14));
    go->setCursor(Qt::PointingHandCursor);
    go->setMinimumHeight(38);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(36, 28, 36, 28);
    layout->setSpacing(12);
    layout->addWidget(Ui::pageHeader(tr("Setup check"), tr("Look and sound your best in one minute.")));
    layout->addWidget(cameraCard);
    layout->addWidget(checklist);
    layout->addWidget(options);
    layout->addWidget(go, 0, Qt::AlignRight);
    layout->addStretch();

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(Ui::scrollPage(content));

    connect(go, &QPushButton::clicked, this, &PreMeetPage::goToMeetRequested);
    connect(m_checkButton, &QPushButton::clicked, this, &PreMeetPage::runCheck);
    connect(m_camera, &CameraMonitor::frameReady, this, [this](const QImage &frame) {
        if (isVisible())
            m_preview->setFrame(frame);
    });
    connect(m_camera, &CameraMonitor::faceReady, this, [this](const FaceFrame &face) {
        if (isVisible())
            m_preview->setFace(face, face.found && std::abs(face.yaw) < 0.24f);
    });
    connect(m_camera, &CameraMonitor::errorOccurred, this, [this](const QString &message) { m_preview->setMessage(message); });
    connect(&AppSettings::instance(), &AppSettings::changed, this, &PreMeetPage::updateCameraState);

    auto *techTimer = new QTimer(this);
    techTimer->setInterval(1000);
    connect(techTimer, &QTimer::timeout, this, [this] {
        if (!isVisible())
            return;
        m_tech->setText(tr("Camera: %1 · %2 fps · face tracking: %3")
                            .arg(m_camera->formatDescription().isEmpty() ? tr("starting…") : m_camera->formatDescription())
                            .arg(m_camera->framesPerSecond(), 0, 'f', 0)
                            .arg(m_camera->faceTrackingAvailable() ? tr("on this computer, ~15× per second")
                                                                   : FaceDetector::libraryError()));
    });
    techTimer->start();

    updateProgress();
    updateCameraState();
}

QString PreMeetPage::checkPrompt()
{
    return QStringLiteral(
        "You are helping someone get ready for an important video meeting (for example a job interview). "
        "Look at this webcam frame. Be practical and fair. Reply with ONLY a JSON object:\n"
        "{\n"
        "  \"face_visible\": true|false,\n"
        "  \"eye_level\": true|false,          // camera roughly level with the eyes, face reasonably centred\n"
        "  \"lighting_good\": true|false,      // face clearly lit, not backlit or too dark\n"
        "  \"collar_straight\": true|false,    // collar and clothing look tidy (true if not visible)\n"
        "  \"nothing_distracting\": true|false,// no notes, papers, clutter or distracting objects in view\n"
        "  \"upright\": true|false,            // sitting upright, not slouching\n"
        "  \"issues\": [{\"kind\": \"framing|lighting|collar|clutter|posture|background\",\n"
        "              \"box\": [x1, y1, x2, y2],   // 0..1 image coordinates of the area concerned\n"
        "              \"message\": \"what to change, short and specific\"}],\n"
        "  \"good\": [\"up to 3 short things that already look good\"],\n"
        "  \"advice\": \"one friendly sentence: the most important fix, or a compliment\"\n"
        "}");
}

void PreMeetPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_acquired && AppSettings::instance().useCamera()) {
        m_acquired = true;
        m_preview->setMessage(tr("Starting camera…"));
        m_camera->acquire();
    }
}

void PreMeetPage::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    if (m_acquired) {
        m_acquired = false;
        m_camera->release();
    }
}

void PreMeetPage::updateCameraState()
{
    const bool camera = AppSettings::instance().useCamera();
    if (!camera) {
        m_preview->setMessage(tr("The camera is turned off in Settings."));
        if (m_acquired) {
            m_acquired = false;
            m_camera->release();
        }
    } else if (isVisible() && !m_acquired) {
        m_acquired = true;
        m_camera->acquire();
    }
    const bool cloud = m_ai->isConfigured() && AppSettings::instance().cloudAnalysis();
    m_checkButton->setEnabled(camera && cloud);
    m_checkButton->setToolTip(cloud ? QString() : tr("Add your AI key on the Settings page to use the automatic check."));
    if (!cloud && m_status->text().isEmpty())
        m_status->setText(tr("Add an AI key in Settings for a full check."));
}

void PreMeetPage::showIssues(const QStringList &issues, const QStringList &good, const QString &advice)
{
    while (QLayoutItem *item = m_issues->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    const Theme::Colors &c = Theme::colors();
    m_status->setText(advice);
    int n = 1;
    for (const QString &issue : issues) {
        QLabel *l = Ui::label(QStringLiteral("<span style='background:%1; color:white; border-radius:9px; padding:0 6px; "
                                             "font-weight:700'>&nbsp;%2&nbsp;</span>&nbsp; %3")
                                  .arg(Theme::css(c.critical))
                                  .arg(n++)
                                  .arg(issue.toHtmlEscaped()));
        l->setTextFormat(Qt::RichText);
        l->setWordWrap(true);
        m_issues->addWidget(l);
    }
    if (!good.isEmpty()) {
        QLabel *l = Ui::label(QStringLiteral("<span style='color:%1; font-weight:700'>✓</span>&nbsp; %2")
                                  .arg(Theme::css(c.success), good.join(QStringLiteral(" · ")).toHtmlEscaped()));
        l->setTextFormat(Qt::RichText);
        l->setWordWrap(true);
        m_issues->addWidget(l);
    }
}

void PreMeetPage::runCheck()
{
    const QImage frame = m_camera->latestFrame();
    if (frame.isNull()) {
        m_status->setText(tr("Waiting for the camera…"));
        return;
    }
    m_checkButton->setEnabled(false);
    m_checkButton->setText(tr("Looking…"));
    m_status->setText(tr("Analysing a snapshot…"));
    m_preview->setMarked({});

    m_ai->analyzeImage(frame, checkPrompt(), [this](const QJsonObject &r, const QString &error) {
        m_checkButton->setEnabled(true);
        m_checkButton->setText(tr("Check again"));
        if (!error.isEmpty()) {
            m_status->setText(error);
            return;
        }
        if (!r.value(QLatin1String("face_visible")).toBool(true)) {
            showIssues({tr("Move into the frame so your face is visible.")}, {}, tr("I can't see your face."));
            return;
        }

        // Tick what the camera could verify
        const QList<QPair<const char *, int>> checks = {{"eye_level", EyeLevel},
                                                        {"lighting_good", Lighting},
                                                        {"collar_straight", Collar},
                                                        {"nothing_distracting", Distractions},
                                                        {"upright", Upright}};
        for (const auto &[key, item] : checks)
            m_items[item]->setChecked(r.value(QLatin1String(key)).toBool());

        // Mark every issue in red on the preview, numbered like the list below
        QList<CameraPreview::Marked> marked;
        QStringList issues;
        for (const QJsonValue &v : r.value(QLatin1String("issues")).toArray()) {
            const QJsonObject o = v.toObject();
            const QString message = o.value(QLatin1String("message")).toString();
            if (message.isEmpty())
                continue;
            issues << message;
            const QJsonArray b = o.value(QLatin1String("box")).toArray();
            if (b.size() == 4) {
                QRectF box(QPointF(b[0].toDouble(), b[1].toDouble()), QPointF(b[2].toDouble(), b[3].toDouble()));
                box = box.normalized().intersected(QRectF(0, 0, 1, 1));
                if (box.width() > 0.01 && box.height() > 0.01)
                    marked.append({box, QStringLiteral("%1  %2").arg(issues.size()).arg(o.value(QLatin1String("kind")).toString())});
            }
        }
        QStringList good;
        for (const QJsonValue &v : r.value(QLatin1String("good")).toArray())
            good << v.toString();
        m_preview->setMarked(marked);
        QString advice = r.value(QLatin1String("advice")).toString();
        if (issues.isEmpty())
            advice = tr("Everything the camera can see looks good. %1").arg(advice);
        showIssues(issues, good, advice);
    });
}

void PreMeetPage::updateProgress()
{
    int done = 0;
    for (const QAbstractButton *item : std::as_const(m_items))
        done += item->isChecked() ? 1 : 0;
    const int total = int(m_items.size());
    m_progressText->setText(done == total ? tr("All set. You're ready to join.") : tr("%1 of %2 ready").arg(done).arg(total));
}
