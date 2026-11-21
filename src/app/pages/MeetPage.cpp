#include "app/pages/MeetPage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include "app/SessionRunWidget.h"
#include "core/AppSettings.h"
#include "core/SessionController.h"
#include "core/TimeFormat.h"
#include "core/TipSource.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

namespace {
QString describe(TipCategory category)
{
    switch (category) {
    case TipCategory::Timing:
        return MeetPage::tr("A section runs over.");
    case TipCategory::Posture:
        return MeetPage::tr("Posture, fidgeting, hands.");
    case TipCategory::Expression:
        return MeetPage::tr("Smile and eye contact.");
    case TipCategory::Speech:
        return MeetPage::tr("Filler words (um, uh).");
    case TipCategory::Appearance:
        return MeetPage::tr("Face covered or out of frame.");
    case TipCategory::Content:
        return MeetPage::tr("A key point was skipped.");
    case TipCategory::Language:
        return MeetPage::tr("Unprofessional words.");
    case TipCategory::General:
        break;
    }
    return QString();
}
QString liveName(TipCategory category)
{
    switch (category) {
    case TipCategory::Appearance:
        return MeetPage::tr("Face & framing");
    case TipCategory::Posture:
        return MeetPage::tr("Body language");
    case TipCategory::Expression:
        return MeetPage::tr("Smile & eye contact");
    case TipCategory::Speech:
        return MeetPage::tr("Filler words");
    default:
        break;
    }
    return categoryName(category);
}
} // namespace

MeetPage::MeetPage(SessionController *controller, TipSource *demoSource, QWidget *parent)
    : QWidget(parent)
    , m_controller(controller)
    , m_agendaRows(new QVBoxLayout)
    , m_idle(new QWidget)
    , m_sensors(Ui::label(QString(), "secondary"))
    , m_run(new SessionRunWidget(controller))
{
    // Start / live card
    auto *sessionCard = new Card(tr("Meeting"));
    auto *idleLayout = new QHBoxLayout(m_idle);
    idleLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *idleText = Ui::label(tr("SMILE hides and only pops up when something needs fixing."), "secondary");
    idleText->setWordWrap(true);
    auto *preview = new QPushButton(tr("Preview a tip"));
    preview->setIcon(Icons::icon(Icons::Icon::Eye, Theme::colors().text, 14));
    preview->setCursor(Qt::PointingHandCursor);
    auto *start = new QPushButton(tr("Start meeting"));
    start->setObjectName(QStringLiteral("accent"));
    start->setIcon(Icons::icon(Icons::Icon::Play, Theme::colors().onAccent, 14));
    start->setCursor(Qt::PointingHandCursor);
    idleLayout->addWidget(idleText, 1);
    idleLayout->addSpacing(16);
    idleLayout->addWidget(preview);
    idleLayout->addWidget(start);
    sessionCard->body()->addWidget(m_idle);
    sessionCard->body()->addWidget(m_run);
    m_sensors->setWordWrap(true);
    m_sensors->setTextFormat(Qt::RichText);
    sessionCard->body()->addWidget(m_sensors);

    // Agenda card
    auto *agendaCard = new Card(tr("Agenda"));
    m_agendaRows->setSpacing(0);
    agendaCard->body()->addLayout(m_agendaRows);

    // Options: the things you may want to change right before a meeting
    AppSettings &settings = AppSettings::instance();
    auto *optionsCard = new Card(tr("Options"));
    optionsCard->body()->setSpacing(0);
    auto *timer = new ToggleSwitch;
    timer->setChecked(settings.sectionTimer());
    connect(timer, &QAbstractButton::toggled, &settings, &AppSettings::setSectionTimer);
    connect(&settings, &AppSettings::changed, timer, [timer] {
        const QSignalBlocker block(timer);
        timer->setChecked(AppSettings::instance().sectionTimer());
    });
    auto *mute = new ToggleSwitch;
    mute->setChecked(settings.tipsMuted());
    connect(mute, &QAbstractButton::toggled, &settings, &AppSettings::setTipsMuted);
    connect(&settings, &AppSettings::changed, mute, [mute] {
        const QSignalBlocker block(mute);
        mute->setChecked(AppSettings::instance().tipsMuted());
    });
    optionsCard->body()->addWidget(new OptionRow(tr("Mute all tips"), tr("Nothing pops up; everything still goes into your results."), mute));
    optionsCard->body()->addWidget(new OptionRow(tr("Section timer next to SMILE"), tr("Current section and time left."), timer));
    auto *level = new QSlider(Qt::Horizontal);
    level->setRange(1, 5);
    level->setFixedWidth(160);
    level->setValue(settings.liveLevel());
    level->setToolTip(tr("1 = only big problems, 5 = picky"));
    connect(level, &QSlider::valueChanged, &settings, &AppSettings::setLiveLevel);
    connect(&settings, &AppSettings::changed, level, [level] {
        const QSignalBlocker block(level);
        level->setValue(AppSettings::instance().liveLevel());
    });
    optionsCard->body()->addWidget(new OptionRow(tr("How picky SMILE is"), tr("Left: only big problems. Right: everything."), level));

    // Tips: which kinds the companion may show (collapsed, most people never change it)
    auto *tipsCard = new Expander(tr("Tip types"), tr("Choose what SMILE may tell you."), -1, Theme::colors().accent, QString());
    tipsCard->body()->setSpacing(0);
    // Only body behaviour pops up live; timing, topic and wording are in the results
    for (TipCategory category : {TipCategory::Appearance, TipCategory::Posture, TipCategory::Expression, TipCategory::Speech}) {
        auto *toggle = new ToggleSwitch;
        toggle->setChecked(m_controller->isCategoryShownLive(category));
        connect(toggle, &QAbstractButton::toggled, this,
                [this, category](bool on) { m_controller->setCategoryShownLive(category, on); });
        tipsCard->body()->addWidget(new OptionRow(liveName(category), describe(category), toggle));
    }
    auto *demo = new ToggleSwitch;
    demo->setChecked(demoSource->isEnabled());
    connect(demo, &QAbstractButton::toggled, this, [demoSource](bool on) { demoSource->setEnabled(on); });
    auto *demoRow = new OptionRow(tr("Demo tips"), tr("Random sample tips (debug)."), demo);
    demoRow->setVisible(settings.debugMode());
    connect(&settings, &AppSettings::changed, demoRow, [demoRow] { demoRow->setVisible(AppSettings::instance().debugMode()); });
    tipsCard->body()->addWidget(demoRow);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(36, 28, 36, 28);
    layout->setSpacing(12);
    layout->addWidget(Ui::pageHeader(tr("Meet"), tr("Coaching during the real meeting.")));
    layout->addWidget(sessionCard);
    layout->addWidget(optionsCard);
    layout->addWidget(agendaCard);
    layout->addWidget(tipsCard);
    layout->addStretch();

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(Ui::scrollPage(content));

    connect(start, &QPushButton::clicked, this, [this] { m_controller->start(SessionMode::Meeting); });
    connect(preview, &QPushButton::clicked, this, &MeetPage::previewTipRequested);
    connect(m_controller, &SessionController::agendaChanged, this, &MeetPage::refreshAgenda);
    connect(m_controller, &SessionController::currentPointChanged, this, &MeetPage::refreshAgenda);
    connect(m_controller, &SessionController::runningChanged, this, &MeetPage::onRunningChanged);

    connect(&AppSettings::instance(), &AppSettings::changed, this, &MeetPage::refreshSensors);

    refreshAgenda();
    refreshSensors();
    onRunningChanged(false);
}

void MeetPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refreshSensors();
}

void MeetPage::refreshSensors()
{
    const AppSettings &s = AppSettings::instance();
    const Theme::Colors &c = Theme::colors();
    auto chip = [&](const QString &name, bool on) {
        return QStringLiteral("<span style='color:%1'>%2 %3</span>")
            .arg(Theme::css(on ? c.success : c.textSecondary), on ? QStringLiteral("●") : QStringLiteral("○"), name);
    };
    const bool ai = !s.apiKey().isEmpty() && s.cloudAnalysis();
    QString text = chip(tr("Camera"), s.useCamera()) + QStringLiteral("&nbsp;&nbsp;&nbsp;")
                   + chip(tr("Microphone"), s.useMicrophone()) + QStringLiteral("&nbsp;&nbsp;&nbsp;")
                   + chip(tr("AI coach"), ai);
    if (!ai)
        text += QStringLiteral("<br>") + tr("Add an AI key in Settings for smile, posture and filler-word tips.").toHtmlEscaped();
    m_sensors->setText(text);
}

void MeetPage::refreshAgenda()
{
    while (QLayoutItem *item = m_agendaRows->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    const bool live = m_controller->isRunning() && m_controller->mode() == SessionMode::Meeting;
    const Agenda &agenda = live ? m_controller->activeAgenda() : m_controller->agenda();
    const Theme::Colors &c = Theme::colors();

    if (agenda.isEmpty()) {
        m_agendaRows->addWidget(Ui::label(tr("No agenda yet. Add your points on the Training page."), "secondary"));
        return;
    }

    for (int i = 0; i < agenda.size(); ++i) {
        const AgendaPoint &point = agenda.at(i);
        const bool current = live && i == m_controller->currentIndex();
        const bool done = live && i < m_controller->currentIndex();

        auto *row = new QWidget;
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(10, 8, 10, 8);
        h->setSpacing(12);
        QLabel *num = Ui::label(QString::number(i + 1), "secondary", row);
        num->setFixedWidth(18);
        QLabel *title = Ui::label(point.title, nullptr, row);
        QLabel *time = Ui::label(point.targetSeconds > 0 ? formatDuration(point.targetSeconds) : tr("no limit"),
                                 "secondary", row);
        if (current) {
            QFont f = title->font();
            f.setWeight(QFont::DemiBold);
            title->setFont(f);
            row->setStyleSheet(QStringLiteral("background: %1; border-radius: 6px;").arg(Theme::css(c.subtleHover)));
        } else if (done) {
            title->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::css(c.textDisabled)));
        }
        h->addWidget(num);
        h->addWidget(title, 1);
        h->addWidget(time);
        m_agendaRows->addWidget(row);
    }
}

void MeetPage::onRunningChanged(bool running)
{
    const bool mine = running && m_controller->mode() == SessionMode::Meeting;
    m_run->setVisible(mine);
    m_idle->setVisible(!mine);
    m_idle->setEnabled(!running);
    refreshAgenda();
}
