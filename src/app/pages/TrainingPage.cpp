#include "app/pages/TrainingPage.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "ai/AiClient.h"
#include "ai/Structurer.h"
#include "app/AgendaEditor.h"
#include "app/PracticeView.h"
#include "app/SimulationView.h"
#include "app/SessionRunWidget.h"
#include "app/StructureDialog.h"
#include "core/Plural.h"
#include "core/AppSettings.h"
#include "core/SessionController.h"
#include "core/TimeFormat.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

namespace {
Agenda sampleAgenda()
{
    auto make = [](const QString &title, double minutes, const QString &summary, const QStringList &points) {
        AgendaPoint p;
        p.title = title;
        p.targetSeconds = qRound(minutes * 60);
        p.summary = summary;
        p.keyPoints = points;
        return p;
    };
    return {
        make(TrainingPage::tr("Introduction"), 1, TrainingPage::tr("Set the scene and build trust."),
             {TrainingPage::tr("Who I am and my role"), TrainingPage::tr("Why this meeting matters today")}),
        make(TrainingPage::tr("The problem we are solving"), 2, TrainingPage::tr("Make the pain concrete."),
             {TrainingPage::tr("Onboarding takes three weeks"), TrainingPage::tr("It costs us new customers")}),
        make(TrainingPage::tr("Proposed solution"), 2, TrainingPage::tr("Explain the idea simply."),
             {TrainingPage::tr("A guided self-service setup"), TrainingPage::tr("Live in two days")}),
        make(TrainingPage::tr("Benefits and supporting arguments"), 2, TrainingPage::tr("Prove it is worth it."),
             {TrainingPage::tr("Saves 40 percent of support time"), TrainingPage::tr("Pilot customers are happier")}),
        make(TrainingPage::tr("Questions and next steps"), 1, TrainingPage::tr("Agree what happens next."),
             {TrainingPage::tr("Approve a one-month pilot")}),
    };
}
} // namespace

TrainingPage::TrainingPage(SessionController *controller, AiClient *ai, MicrophoneMonitor *mic, QWidget *parent)
    : QWidget(parent)
    , m_controller(controller)
    , m_ai(ai)
    , m_speech(new QPlainTextEdit)
    , m_textMode(new QPushButton(tr("Text")))
    , m_bulletMode(new QPushButton(tr("Bullet points")))
    , m_target(new DurationEdit)
    , m_structure(new QPushButton(tr("Structure into sections")))
    , m_structureStatus(Ui::label(QString(), "secondary"))
    , m_wordCount(Ui::label(QString(), "secondary"))
    , m_type(Ui::label(QString(), "secondary"))
    , m_editor(new AgendaEditor)
    , m_summary(Ui::label(QString(), "secondary"))
    , m_add(new QPushButton(tr("Add section")))
    , m_idle(new QWidget)
    , m_run(new SessionRunWidget(controller))
{
    AppSettings &settings = AppSettings::instance();
    const Theme::Colors &c = Theme::colors();

    // ---- Speech / notes
    auto *speechCard = new Card(tr("What do you want to say?"));
    QLabel *speechHint = Ui::label(tr("Paste or write your speech, presentation or meeting notes. SMILE splits it "
                                      "into sections and key points, then checks you get them across."),
                                   "secondary");
    speechHint->setWordWrap(true);
    speechCard->body()->addWidget(speechHint);

    auto *modeRow = new QHBoxLayout;
    modeRow->setSpacing(4);
    auto *modes = new QButtonGroup(this);
    for (QPushButton *b : {m_textMode, m_bulletMode}) {
        b->setObjectName(QStringLiteral("segment"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        modes->addButton(b);
        modeRow->addWidget(b);
    }
    m_textMode->setIcon(Icons::icon(Icons::Icon::Text, c.text, 14));
    m_bulletMode->setIcon(Icons::icon(Icons::Icon::Bullets, c.text, 14));
    modeRow->addStretch();
    modeRow->addWidget(Ui::label(tr("Target length")));
    m_target->setSeconds(settings.targetSeconds());
    m_target->setMinimumWidth(m_target->sizeHint().width());
    modeRow->addWidget(m_target);
    speechCard->body()->addLayout(modeRow);

    m_speech->setPlaceholderText(tr("For example:\n\n"
                                    "Good morning everyone. I'm Alex, product lead for onboarding. Today I want to "
                                    "show you why new customers wait three weeks to get started, and how a guided "
                                    "self-service setup can cut that to two days…"));
    m_speech->setMinimumHeight(200);
    m_speech->setPlainText(settings.speechText());
    speechCard->body()->addWidget(m_speech);

    auto *structureRow = new QHBoxLayout;
    m_structure->setObjectName(QStringLiteral("accent"));
    m_structure->setIcon(Icons::icon(Icons::Icon::Sparkle, c.onAccent, 14));
    m_structure->setCursor(Qt::PointingHandCursor);
    m_structureStatus->setWordWrap(true);
    structureRow->addWidget(m_structure);
    structureRow->addSpacing(8);
    structureRow->addWidget(m_structureStatus, 1);
    structureRow->addWidget(m_wordCount);
    speechCard->body()->addLayout(structureRow);

    // ---- Sections
    auto *sectionsCard = new Card;
    auto *sectionsHead = new QHBoxLayout;
    sectionsHead->addWidget(Ui::label(tr("Your sections"), "cardTitle"));
    sectionsHead->addStretch();
    sectionsHead->addWidget(m_type);
    sectionsCard->body()->addLayout(sectionsHead);
    QLabel *sectionsHint = Ui::label(tr("Rename, reorder and time each section. Open one to edit its purpose and the "
                                        "key points SMILE listens for."),
                                     "secondary");
    sectionsHint->setWordWrap(true);
    sectionsCard->body()->addWidget(sectionsHint);
    sectionsCard->body()->addWidget(m_editor);
    m_add->setObjectName(QStringLiteral("subtle"));
    m_add->setIcon(Icons::icon(Icons::Icon::Add, c.accent, 14));
    m_add->setCursor(Qt::PointingHandCursor);
    auto *footer = new QHBoxLayout;
    footer->addWidget(m_add);
    footer->addStretch();
    footer->addWidget(m_summary);
    sectionsCard->body()->addLayout(footer);

    // ---- Rehearse
    auto *rehearseCard = new Card(tr("Rehearse"));
    auto *idleLayout = new QHBoxLayout(m_idle);
    idleLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *idleText = Ui::label(tr("Present out loud as if you were in the meeting. SMILE times each section, "
                                    "listens for your key points and coaches you as you go."),
                                 "secondary");
    idleText->setWordWrap(true);
    auto *start = new QPushButton(tr("Start rehearsal"));
    start->setObjectName(QStringLiteral("accent"));
    start->setIcon(Icons::icon(Icons::Icon::Play, c.onAccent, 14));
    start->setCursor(Qt::PointingHandCursor);
    idleLayout->addWidget(idleText, 1);
    idleLayout->addSpacing(16);
    idleLayout->addWidget(start);
    rehearseCard->body()->addWidget(m_idle);
    rehearseCard->body()->addWidget(m_run);
    auto *timer = new ToggleSwitch;
    timer->setChecked(settings.sectionTimer());
    connect(timer, &QAbstractButton::toggled, &settings, &AppSettings::setSectionTimer);
    connect(&settings, &AppSettings::changed, timer, [timer] {
        const QSignalBlocker block(timer);
        timer->setChecked(AppSettings::instance().sectionTimer());
    });
    rehearseCard->body()->addWidget(new OptionRow(tr("Section timer next to SMILE"),
                                                  tr("A small timer by the avatar shows the current section, its time and "
                                                     "whether you're on track. It moves on when you start the next section."),
                                                  timer));

    // ---- Tab 1: plan & rehearse
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(36, 8, 36, 28);
    layout->setSpacing(12);
    layout->addWidget(rehearseCard);
    layout->addWidget(speechCard);
    layout->addWidget(sectionsCard);
    layout->addStretch();

    // ---- Tab 2: practise one section; Tab 3: simulate a meeting
    auto wrap = [](QWidget *inner) {
        auto *host = new QWidget;
        auto *l = new QVBoxLayout(host);
        l->setContentsMargins(36, 8, 36, 28);
        l->addWidget(inner);
        l->addStretch();
        return Ui::scrollPage(host);
    };
    m_tabs = new QStackedWidget;
    m_tabs->addWidget(Ui::scrollPage(content));
    m_tabs->addWidget(wrap(new PracticeView(controller, ai)));
    m_tabs->addWidget(wrap(new SimulationView(controller, ai, mic)));

    // ---- Header with tabs
    auto *top = new QWidget;
    top->setObjectName(QStringLiteral("page"));
    auto *topLayout = new QVBoxLayout(top);
    topLayout->setContentsMargins(36, 28, 36, 8);
    topLayout->setSpacing(10);
    topLayout->addWidget(Ui::pageHeader(tr("Training"), tr("Plan what you'll say, practise it section by section, and "
                                                           "rehearse a realistic meeting.")));
    auto *tabRow = new QHBoxLayout;
    tabRow->setSpacing(4);
    auto *tabGroup = new QButtonGroup(this);
    const QStringList tabNames = {tr("Plan && rehearse"), tr("Practise a section"), tr("Simulate a meeting")};
    for (int i = 0; i < tabNames.size(); ++i) {
        auto *b = new QPushButton(tabNames[i]);
        b->setObjectName(QStringLiteral("segment"));
        b->setCheckable(true);
        b->setChecked(i == 0);
        b->setCursor(Qt::PointingHandCursor);
        tabGroup->addButton(b, i);
        tabRow->addWidget(b);
    }
    tabRow->addStretch();
    topLayout->addLayout(tabRow);
    connect(tabGroup, &QButtonGroup::idClicked, m_tabs, &QStackedWidget::setCurrentIndex);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(top);
    outer->addWidget(m_tabs, 1);

    // ---- Behaviour
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(600);
    connect(&m_saveTimer, &QTimer::timeout, this, &TrainingPage::save);
    connect(m_speech, &QPlainTextEdit::textChanged, this, [this] {
        updateWordCount();
        m_saveTimer.start();
    });
    connect(m_target, &DurationEdit::secondsChanged, this, [this] { m_saveTimer.start(); });
    connect(m_textMode, &QPushButton::clicked, this, [this] { setBulletMode(false); });
    connect(m_bulletMode, &QPushButton::clicked, this, [this] { setBulletMode(true); });
    connect(m_structure, &QPushButton::clicked, this, &TrainingPage::structure);
    connect(m_add, &QPushButton::clicked, this, [this] { m_editor->addPoint(QString(), 2.0, true); });
    connect(m_editor, &AgendaEditor::changed, this, &TrainingPage::commitAgenda);
    connect(start, &QPushButton::clicked, this, [this] {
        commitAgenda();
        m_controller->start(SessionMode::Training);
    });
    connect(m_controller, &SessionController::runningChanged, this, &TrainingPage::onRunningChanged);

    // ---- Restore
    m_bullets = settings.speechAsBullets();
    (m_bullets ? m_bulletMode : m_textMode)->setChecked(true);
    const QString type = settings.presentationType();
    if (!type.isEmpty())
        m_type->setText(tr("Detected: %1").arg(type));
    Agenda saved;
    for (const QJsonValue &v : QJsonDocument::fromJson(settings.agendaJson()).array())
        saved << AgendaPoint::fromJson(v.toObject());
    m_editor->setAgenda(saved.isEmpty() ? sampleAgenda() : saved);
    updateWordCount();
    onRunningChanged(false);
}

void TrainingPage::setBulletMode(bool bullets)
{
    if (bullets == m_bullets)
        return;
    m_bullets = bullets;
    const QString text = m_speech->toPlainText();
    if (!text.trimmed().isEmpty())
        m_speech->setPlainText(bullets ? Structurer::toBullets(text) : Structurer::toText(text));
    AppSettings::instance().setSpeechAsBullets(bullets);
}

void TrainingPage::structure()
{
    const QString text = m_speech->toPlainText().trimmed();
    if (text.isEmpty()) {
        m_structureStatus->setText(tr("Write or paste something first."));
        return;
    }
    auto apply = [this](const Structurer::Result &r) {
        m_structure->setEnabled(true);
        m_structure->setText(tr("Structure again"));
        if (r.sections.isEmpty()) {
            m_structureStatus->setText(tr("Couldn't find any sections in that text."));
            return;
        }
        m_structureStatus->setText(r.error.isEmpty() ? QString() : tr("AI unavailable, split locally: %1").arg(r.error));
        // Let the user review, edit and confirm before anything is replaced
        StructureDialog dialog(r.sections, r.presentationType, r.error.isEmpty() && m_ai->isConfigured(), this);
        if (dialog.exec() != QDialog::Accepted)
            return;
        const Agenda confirmed = dialog.sections();
        m_editor->setAgenda(confirmed);
        AppSettings::instance().setPresentationType(r.presentationType);
        m_type->setText(r.presentationType.isEmpty() ? QString() : tr("Detected: %1").arg(r.presentationType));
        int points = 0;
        for (const AgendaPoint &p : confirmed)
            points += int(p.keyPoints.size());
        m_structureStatus->setText(tr("%1 sections and %2 key points confirmed.").arg(confirmed.size()).arg(points));
        save();
    };

    if (m_ai->isConfigured()) {
        m_structure->setEnabled(false);
        m_structure->setText(tr("Structuring…"));
        m_structureStatus->setText(tr("The AI is reading your text."));
        Structurer::withAi(m_ai, text, qMax(1, (m_target->seconds() + 30) / 60), apply);
    } else {
        apply(Structurer::locally(text, qMax(1, (m_target->seconds() + 30) / 60)));
    }
}

void TrainingPage::commitAgenda()
{
    m_controller->setAgenda(m_editor->agenda());
    int total = 0;
    int points = 0;
    for (const AgendaPoint &point : m_controller->agenda()) {
        total += point.targetSeconds;
        points += int(point.keyPoints.size());
    }
    const int count = int(m_controller->agenda().size());
    m_summary->setText(plural(count, tr("%1 section"), tr("%1 sections")) + QStringLiteral("  ·  ")
                       + plural(points, tr("%1 key point"), tr("%1 key points")) + QStringLiteral("  ·  ")
                       + tr("%1 total").arg(formatDuration(total)));
    m_saveTimer.start();
}

void TrainingPage::save()
{
    AppSettings &s = AppSettings::instance();
    s.setSpeechText(m_speech->toPlainText());
    s.setTargetSeconds(m_target->seconds());
    QJsonArray array;
    for (const AgendaPoint &p : m_editor->agenda())
        array.append(p.toJson());
    s.setAgendaJson(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

void TrainingPage::updateWordCount()
{
    const int words = int(m_speech->toPlainText().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).size());
    // ~140 words per minute is a comfortable speaking pace
    m_wordCount->setText(words ? tr("%1 words · about %2 min spoken").arg(words).arg(qMax(1, qRound(words / 140.0))) : QString());
}

void TrainingPage::onRunningChanged(bool running)
{
    const bool mine = running && m_controller->mode() == SessionMode::Training;
    m_run->setVisible(mine);
    m_idle->setVisible(!mine);
    m_idle->setEnabled(!running);
    m_editor->setEnabled(!running);
    m_add->setEnabled(!running);
    m_speech->setReadOnly(running);
    m_structure->setEnabled(!running);
}
