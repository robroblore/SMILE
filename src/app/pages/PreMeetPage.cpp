#include "app/pages/PreMeetPage.h"

#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "ai/AiClient.h"
#include "core/AppSettings.h"
#include "sensors/CameraMonitor.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

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
    , m_result(Ui::label(QString(), "secondary"))
    , m_progressText(Ui::label(QString(), "secondary"))
    , m_progress(new QProgressBar)
{
    // ---- Camera card
    auto *cameraCard = new Card(tr("How you look on camera"));
    auto *cameraRow = new QHBoxLayout;
    cameraRow->setSpacing(20);
    m_preview->setFixedSize(360, 240);
    cameraRow->addWidget(m_preview, 0, Qt::AlignTop);

    auto *side = new QVBoxLayout;
    side->setSpacing(10);
    QLabel *intro = Ui::label(tr("This is a private preview. SMILE only looks; it never changes the video "
                                 "the others see."),
                              "secondary");
    intro->setWordWrap(true);
    side->addWidget(intro);
    m_checkButton->setObjectName(QStringLiteral("accent"));
    m_checkButton->setIcon(Icons::icon(Icons::Icon::Sparkle, Theme::colors().onAccent, 14));
    m_checkButton->setCursor(Qt::PointingHandCursor);
    side->addWidget(m_checkButton, 0, Qt::AlignLeft);
    m_result->setWordWrap(true);
    m_result->setTextFormat(Qt::RichText);
    side->addWidget(m_result);
    side->addStretch();
    cameraRow->addLayout(side, 1);
    cameraCard->body()->addLayout(cameraRow);

    // ---- Checklist
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
        {tr("Agenda ready"), tr("Your points are listed on the Training page.")},
    };
    auto *checklist = new Card(tr("Readiness checklist"));
    checklist->body()->setSpacing(2);
    for (const Entry &entry : entries) {
        auto *check = new CheckCircle;
        checklist->body()->addWidget(new OptionRow(entry.title, entry.hint, check, true));
        connect(check, &QAbstractButton::toggled, this, &PreMeetPage::updateProgress);
        m_items.append(check);
    }

    // ---- Ready footer
    auto *ready = new Card;
    auto *readyRow = new QHBoxLayout;
    auto *progressCol = new QVBoxLayout;
    progressCol->setSpacing(8);
    progressCol->addWidget(m_progressText);
    m_progress->setTextVisible(false);
    progressCol->addWidget(m_progress);
    auto *go = new QPushButton(tr("Go to Meet"));
    go->setObjectName(QStringLiteral("accent"));
    go->setIcon(Icons::icon(Icons::Icon::Next, Theme::colors().onAccent, 14));
    go->setCursor(Qt::PointingHandCursor);
    readyRow->addLayout(progressCol, 1);
    readyRow->addSpacing(24);
    readyRow->addWidget(go);
    ready->body()->addLayout(readyRow);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(36, 28, 36, 28);
    layout->setSpacing(12);
    layout->addWidget(Ui::pageHeader(tr("Pre-Meet"), tr("A one-minute check so you look your best before you join.")));
    layout->addWidget(cameraCard);
    layout->addWidget(checklist);
    layout->addWidget(ready);
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
    connect(m_camera, &CameraMonitor::errorOccurred, this, [this](const QString &message) {
        m_preview->setMessage(message);
    });
    connect(&AppSettings::instance(), &AppSettings::changed, this, &PreMeetPage::updateCameraState);

    updateProgress();
    updateCameraState();
}

QString PreMeetPage::checkPrompt()
{
    return QStringLiteral(
        "You are helping someone get ready for an important video meeting (for example a job interview). "
        "Look at this webcam frame and reply with ONLY a JSON object:\n"
        "{\n"
        "  \"face_visible\": true|false,\n"
        "  \"eye_level\": true|false,          // camera roughly level with the eyes, face reasonably centred\n"
        "  \"lighting_good\": true|false,      // face clearly lit, not backlit or too dark\n"
        "  \"collar_straight\": true|false,    // collar and clothing look tidy (true if not visible)\n"
        "  \"nothing_distracting\": true|false,// no notes, papers, clutter or distracting objects in view\n"
        "  \"upright\": true|false,            // sitting upright, not slouching\n"
        "  \"advice\": \"one or two short, friendly sentences: the most important fix, or a compliment\"\n"
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
    if (!cloud)
        m_result->setText(tr("Add an AI key in Settings and SMILE will check your framing, light, collar and "
                             "background for you."));
}

void PreMeetPage::runCheck()
{
    const QImage frame = m_camera->latestFrame();
    if (frame.isNull()) {
        m_result->setText(tr("Waiting for the camera…"));
        return;
    }
    m_checkButton->setEnabled(false);
    m_checkButton->setText(tr("Looking…"));
    m_result->setText(tr("Analysing a snapshot. This takes a second or two."));

    m_ai->analyzeImage(frame, checkPrompt(), [this](const QJsonObject &r, const QString &error) {
        m_checkButton->setEnabled(true);
        m_checkButton->setText(tr("Check again"));
        if (!error.isEmpty()) {
            m_result->setText(QStringLiteral("<span style='color:%1'>%2</span>")
                                  .arg(Theme::css(Theme::colors().critical), error.toHtmlEscaped()));
            return;
        }
        if (!r.value(QLatin1String("face_visible")).toBool(true)) {
            m_result->setText(tr("I can't see your face. Move into the frame and try again."));
            return;
        }

        struct Check {
            const char *key;
            int item;
            QString issue;
        };
        const QList<Check> checks = {
            {"eye_level", EyeLevel, tr("Raise or lower the camera to eye level and centre your face.")},
            {"lighting_good", Lighting, tr("Improve your lighting: face a window or lamp.")},
            {"collar_straight", Collar, tr("Straighten your collar or clothing.")},
            {"nothing_distracting", Distractions, tr("Move notes or clutter out of view.")},
            {"upright", Upright, tr("Sit up straight.")},
        };
        QStringList issues;
        for (const Check &check : checks) {
            const bool ok = r.value(QLatin1String(check.key)).toBool();
            m_items[check.item]->setChecked(ok);
            if (!ok)
                issues << check.issue;
        }

        const Theme::Colors &c = Theme::colors();
        QString html = QStringLiteral("<p>%1</p>").arg(r.value(QLatin1String("advice")).toString().toHtmlEscaped());
        if (issues.isEmpty()) {
            html += QStringLiteral("<p style='color:%1'><b>%2</b></p>")
                        .arg(Theme::css(c.success), tr("Everything the camera can see looks good."));
        } else {
            html += QStringLiteral("<ul style='margin-left:-20px'>");
            for (const QString &issue : std::as_const(issues))
                html += QStringLiteral("<li>%1</li>").arg(issue.toHtmlEscaped());
            html += QStringLiteral("</ul>");
        }
        m_result->setText(html);
    });
}

void PreMeetPage::updateProgress()
{
    int done = 0;
    for (const QAbstractButton *item : std::as_const(m_items))
        done += item->isChecked() ? 1 : 0;
    const int total = int(m_items.size());
    m_progressText->setText(done == total ? tr("All set. You're ready to join.")
                                          : tr("%1 of %2 ready").arg(done).arg(total));
    m_progress->setRange(0, total);
    m_progress->setValue(done);
    const QString state = done == total ? QStringLiteral("done") : QStringLiteral("normal");
    if (m_progress->property("state").toString() != state) {
        m_progress->setProperty("state", state);
        Theme::repolish(m_progress);
    }
}
