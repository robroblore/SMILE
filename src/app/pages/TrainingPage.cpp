#include "app/pages/TrainingPage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "app/AgendaEditor.h"
#include "app/SessionRunWidget.h"
#include "core/SessionController.h"
#include "core/TimeFormat.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

TrainingPage::TrainingPage(SessionController *controller, QWidget *parent)
    : QWidget(parent)
    , m_controller(controller)
    , m_editor(new AgendaEditor)
    , m_summary(Ui::label(QString(), "secondary"))
    , m_add(new QPushButton(tr("Add point")))
    , m_idle(new QWidget)
    , m_run(new SessionRunWidget(controller))
{
    // Agenda card
    auto *agendaCard = new Card(tr("Your agenda"));
    QLabel *agendaHint = Ui::label(tr("The points you want to get across, and how long each should take. "
                                      "The same agenda is used in the meeting."),
                                   "secondary");
    agendaHint->setWordWrap(true);
    agendaCard->body()->addWidget(agendaHint);
    agendaCard->body()->addSpacing(4);
    agendaCard->body()->addWidget(m_editor);

    m_add->setObjectName(QStringLiteral("subtle"));
    m_add->setIcon(Icons::icon(Icons::Icon::Add, Theme::colors().accent, 14));
    m_add->setCursor(Qt::PointingHandCursor);
    auto *footer = new QHBoxLayout;
    footer->addWidget(m_add);
    footer->addStretch();
    footer->addWidget(m_summary);
    agendaCard->body()->addLayout(footer);

    // Rehearse card
    auto *rehearseCard = new Card(tr("Rehearse"));
    auto *idleLayout = new QHBoxLayout(m_idle);
    idleLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *idleText = Ui::label(tr("Present out loud as if you were in the meeting. SMILE times each point; "
                                    "the companion only pops up when you run long."),
                                 "secondary");
    idleText->setWordWrap(true);
    auto *start = new QPushButton(tr("Start rehearsal"));
    start->setObjectName(QStringLiteral("accent"));
    start->setIcon(Icons::icon(Icons::Icon::Play, Theme::colors().onAccent, 14));
    start->setCursor(Qt::PointingHandCursor);
    idleLayout->addWidget(idleText, 1);
    idleLayout->addSpacing(16);
    idleLayout->addWidget(start);
    rehearseCard->body()->addWidget(m_idle);
    rehearseCard->body()->addWidget(m_run);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(36, 28, 36, 28);
    layout->setSpacing(12);
    layout->addWidget(Ui::pageHeader(tr("Training"), tr("Plan what you'll say, then practise it against the clock.")));
    layout->addWidget(agendaCard);
    layout->addWidget(rehearseCard);
    layout->addStretch();

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(Ui::scrollPage(content));

    connect(m_add, &QPushButton::clicked, this, [this] { m_editor->addPoint(QString(), 2.0, true); });
    connect(m_editor, &AgendaEditor::changed, this, &TrainingPage::commitAgenda);
    connect(start, &QPushButton::clicked, this, [this] {
        commitAgenda();
        m_controller->start(SessionMode::Training);
    });
    connect(m_controller, &SessionController::runningChanged, this, &TrainingPage::onRunningChanged);

    // A sample agenda so the app is usable straight away.
    m_editor->addPoint(tr("Introduce yourself and your role"), 1.0);
    m_editor->addPoint(tr("The problem we are solving"), 2.0);
    m_editor->addPoint(tr("Our proposal and its benefits"), 3.0);
    m_editor->addPoint(tr("Questions and next steps"), 2.0);

    onRunningChanged(false);
}

void TrainingPage::commitAgenda()
{
    m_controller->setAgenda(m_editor->agenda());
    int total = 0;
    for (const AgendaPoint &point : m_controller->agenda())
        total += point.targetSeconds;
    const int count = int(m_controller->agenda().size());
    m_summary->setText(tr("%n point(s)", nullptr, count) + QStringLiteral("  ·  ")
                       + tr("%1 total").arg(formatDuration(total)));
}

void TrainingPage::onRunningChanged(bool running)
{
    const bool mine = running && m_controller->mode() == SessionMode::Training;
    m_run->setVisible(mine);
    m_idle->setVisible(!mine);
    m_idle->setEnabled(!running);
    m_editor->setEnabled(!running);
    m_add->setEnabled(!running);
}
