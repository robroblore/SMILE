#include "app/pages/MeetPage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "app/SessionRunWidget.h"
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
        return MeetPage::tr("When a point runs over its time.");
    case TipCategory::Posture:
        return MeetPage::tr("Slouching, leaning in, tense shoulders.");
    case TipCategory::Expression:
        return MeetPage::tr("Smiling, eye contact with the camera.");
    case TipCategory::Speech:
        return MeetPage::tr("Filler words, pace, talking for too long.");
    case TipCategory::Appearance:
        return MeetPage::tr("Collar, framing and lighting.");
    case TipCategory::General:
        break;
    }
    return QString();
}
} // namespace

MeetPage::MeetPage(SessionController *controller, TipSource *demoSource, QWidget *parent)
    : QWidget(parent)
    , m_controller(controller)
    , m_agendaRows(new QVBoxLayout)
    , m_idle(new QWidget)
    , m_run(new SessionRunWidget(controller))
{
    // Start / live card
    auto *sessionCard = new Card(tr("Meeting"));
    auto *idleLayout = new QHBoxLayout(m_idle);
    idleLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *idleText = Ui::label(tr("When you start, SMILE minimizes and stays out of your way. "
                                    "The companion pops up briefly only when something needs attention."),
                                 "secondary");
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

    // Agenda card
    auto *agendaCard = new Card(tr("Agenda"));
    m_agendaRows->setSpacing(0);
    agendaCard->body()->addLayout(m_agendaRows);

    // Tips card
    auto *tipsCard = new Card(tr("What the companion may tell you"));
    tipsCard->body()->setSpacing(0);
    for (TipCategory category : allTipCategories()) {
        if (category == TipCategory::General)
            continue;
        auto *toggle = new ToggleSwitch;
        toggle->setChecked(m_controller->isCategoryShownLive(category));
        connect(toggle, &QAbstractButton::toggled, this,
                [this, category](bool on) { m_controller->setCategoryShownLive(category, on); });
        tipsCard->body()->addWidget(new OptionRow(categoryName(category), describe(category), toggle));
    }
    tipsCard->body()->addSpacing(6);
    tipsCard->body()->addWidget(Ui::divider());
    tipsCard->body()->addSpacing(6);
    auto *demo = new ToggleSwitch;
    demo->setChecked(demoSource->isEnabled());
    connect(demo, &QAbstractButton::toggled, this, [demoSource](bool on) { demoSource->setEnabled(on); });
    tipsCard->body()->addWidget(new OptionRow(
        tr("Demo tips"),
        tr("Sample posture, smile and speech tips, until the camera and microphone modules are built."), demo));

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(36, 28, 36, 28);
    layout->setSpacing(12);
    layout->addWidget(Ui::pageHeader(tr("Meet"), tr("Private, glanceable coaching during the real thing.")));
    layout->addWidget(sessionCard);
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

    refreshAgenda();
    onRunningChanged(false);
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
