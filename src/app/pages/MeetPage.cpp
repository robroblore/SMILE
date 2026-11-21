#include "app/pages/MeetPage.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "app/SessionRunWidget.h"
#include "core/SessionController.h"
#include "core/TimeFormat.h"
#include "core/TipSource.h"

MeetPage::MeetPage(SessionController *controller, TipSource *demoSource, QWidget *parent)
    : QWidget(parent)
    , m_controller(controller)
    , m_agenda(new QListWidget(this))
    , m_start(new QPushButton(tr("Start meeting"), this))
    , m_run(new SessionRunWidget(controller, this))
{
    auto *title = new QLabel(tr("Meet"), this);
    QFont f = title->font();
    f.setPointSizeF(f.pointSizeF() * 1.8);
    f.setBold(true);
    title->setFont(f);

    auto *intro = new QLabel(tr("Start when your meeting starts. The companion shows tips only on your "
                                "screen; SMILE never changes your camera or video feed."),
                             this);
    intro->setWordWrap(true);

    auto *agendaGroup = new QGroupBox(tr("Agenda"), this);
    auto *agendaLayout = new QVBoxLayout(agendaGroup);
    m_agenda->setSelectionMode(QAbstractItemView::NoSelection);
    agendaLayout->addWidget(m_agenda);

    auto *tipsGroup = new QGroupBox(tr("Live tips"), this);
    auto *tipsLayout = new QGridLayout(tipsGroup);
    int i = 0;
    for (TipCategory category : allTipCategories()) {
        auto *box = new QCheckBox(categoryName(category), tipsGroup);
        box->setChecked(m_controller->isCategoryShownLive(category));
        connect(box, &QCheckBox::toggled, this,
                [this, category](bool on) { m_controller->setCategoryShownLive(category, on); });
        tipsLayout->addWidget(box, i / 3, i % 3);
        ++i;
    }
    auto *demo = new QCheckBox(tr("Demo tips (sample posture, smile and speech tips until the AI modules exist)"),
                               tipsGroup);
    demo->setChecked(demoSource->isEnabled());
    connect(demo, &QCheckBox::toggled, this, [demoSource](bool on) { demoSource->setEnabled(on); });
    tipsLayout->addWidget(demo, (i + 2) / 3, 0, 1, 3);

    m_start->setMinimumHeight(36);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->addWidget(title);
    layout->addWidget(intro);
    layout->addSpacing(8);
    layout->addWidget(agendaGroup, 1);
    layout->addWidget(tipsGroup);
    layout->addSpacing(8);
    layout->addWidget(m_start, 0, Qt::AlignLeft);
    layout->addWidget(m_run);

    connect(m_start, &QPushButton::clicked, this, [this] { m_controller->start(SessionMode::Meeting); });
    connect(m_controller, &SessionController::agendaChanged, this, &MeetPage::refreshAgenda);
    connect(m_controller, &SessionController::currentPointChanged, this, &MeetPage::refreshAgenda);
    connect(m_controller, &SessionController::runningChanged, this, &MeetPage::onRunningChanged);

    refreshAgenda();
    onRunningChanged(false);
}

void MeetPage::refreshAgenda()
{
    const bool live = m_controller->isRunning() && m_controller->mode() == SessionMode::Meeting;
    const Agenda &agenda = live ? m_controller->activeAgenda() : m_controller->agenda();

    m_agenda->clear();
    if (agenda.isEmpty()) {
        m_agenda->addItem(tr("No agenda yet. Add points on the Training page."));
        return;
    }
    for (int i = 0; i < agenda.size(); ++i) {
        const AgendaPoint &point = agenda.at(i);
        const QString target = point.targetSeconds > 0 ? formatDuration(point.targetSeconds) : tr("no limit");
        auto *item = new QListWidgetItem(tr("%1. %2  (%3)").arg(i + 1).arg(point.title, target), m_agenda);
        if (live && i == m_controller->currentIndex()) {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
        } else if (live && i < m_controller->currentIndex()) {
            item->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
        }
    }
}

void MeetPage::onRunningChanged(bool running)
{
    const bool mine = running && m_controller->mode() == SessionMode::Meeting;
    m_run->setVisible(mine);
    m_start->setVisible(!mine);
    m_start->setEnabled(!running);
    refreshAgenda();
}
