#include "app/pages/TrainingPage.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

#include "ai/AiClient.h"
#include "ai/Structurer.h"
#include "app/AgendaEditor.h"
#include "app/SessionRunWidget.h"
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

TrainingPage::TrainingPage(SessionController *controller, AiClient *ai, QWidget *parent)
    : QWidget(parent)
    , m_controller(controller)
    , m_ai(ai)
    , m_speech(new QPlainTextEdit)
    , m_textMode(new QPushButton(tr("Text")))
    , m_bulletMode(new QPushButton(tr("Bullet points")))
    , m_target(new QSpinBox)
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
    m_target->setRange(1, 180);
    m_target->setSuffix(tr(" min"));
    m_target->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_target->setAlignment(Qt::AlignRight);
    m_target->setFixedWidth(84);
    m_target->setValue(settings.targetMinutes());
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

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(36, 28, 36, 28);
    layout->setSpacing(12);
    layout->addWidget(Ui::pageHeader(tr("Training"), tr("Plan what you'll say, then practise it against the clock.")));
    layout->addWidget(rehearseCard);
    layout->addWidget(speechCard);
    layout->addWidget(sectionsCard);
    layout->addStretch();

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(Ui::scrollPage(content));

    // ---- Behaviour
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(600);
    connect(&m_saveTimer, &QTimer::timeout, this, &TrainingPage::save);
    connect(m_speech, &QPlainTextEdit::textChanged, this, [this] {
        updateWordCount();
        m_saveTimer.start();
    });
    connect(m_target, &QSpinBox::valueChanged, this, [this] { m_saveTimer.start(); });
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
    if (!m_editor->agenda().isEmpty() && m_editor->agenda().size() > 0 && !AppSettings::instance().agendaJson().isEmpty()) {
        const auto answer = QMessageBox::question(this, tr("Replace sections?"),
                                                  tr("This replaces your current sections with new ones built from "
                                                     "your text. Continue?"));
        if (answer != QMessageBox::Yes)
            return;
    }

    auto apply = [this](const Structurer::Result &r) {
        m_structure->setEnabled(true);
        m_structure->setText(tr("Structure again"));
        if (r.sections.isEmpty()) {
            m_structureStatus->setText(tr("Couldn't find any sections in that text."));
            return;
        }
        m_editor->setAgenda(r.sections);
        AppSettings::instance().setPresentationType(r.presentationType);
        m_type->setText(r.presentationType.isEmpty() ? QString() : tr("Detected: %1").arg(r.presentationType));
        int points = 0;
        for (const AgendaPoint &p : r.sections)
            points += int(p.keyPoints.size());
        QString status = tr("%1 sections and %2 key points.").arg(r.sections.size()).arg(points);
        if (!r.error.isEmpty())
            status += QLatin1Char(' ') + tr("(AI unavailable: split locally. %1)").arg(r.error);
        m_structureStatus->setText(status);
        save();
    };

    if (m_ai->isConfigured()) {
        m_structure->setEnabled(false);
        m_structure->setText(tr("Structuring…"));
        m_structureStatus->setText(tr("The AI is reading your text."));
        Structurer::withAi(m_ai, text, m_target->value(), apply);
    } else {
        apply(Structurer::locally(text, m_target->value()));
        m_structureStatus->setText(m_structureStatus->text() + QLatin1Char(' ')
                                   + tr("Add an AI key in Settings for smarter headings."));
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
    s.setTargetMinutes(m_target->value());
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
