#include "app/SimulationView.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "ai/AiClient.h"
#include "app/pages/PostMeetPage.h"
#include "companion/AvatarWidget.h"
#include "core/AppSettings.h"
#include "core/SessionController.h"
#include "core/TimeFormat.h"
#include "sensors/AudioPlayer.h"
#include "sensors/MicrophoneMonitor.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

namespace {
struct MeetingType {
    QString name;
    QString role;
    QString audience;
};

QList<MeetingType> meetingTypes()
{
    return {
        {SimulationView::tr("Business proposal"), SimulationView::tr("a critical investor"),
         SimulationView::tr("Investors deciding whether to fund the idea; they care about cost, risk and return.")},
        {SimulationView::tr("Job interview"), SimulationView::tr("the hiring manager"),
         SimulationView::tr("A hiring manager assessing skills, experience and fit for the role.")},
        {SimulationView::tr("Sales pitch"), SimulationView::tr("a busy prospective customer"),
         SimulationView::tr("A buyer comparing options with a tight budget.")},
        {SimulationView::tr("Project update"), SimulationView::tr("a senior stakeholder"),
         SimulationView::tr("Leadership checking progress, risks and next steps.")},
        {SimulationView::tr("Performance review"), SimulationView::tr("your manager"),
         SimulationView::tr("A manager discussing achievements and goals.")},
        {SimulationView::tr("Board presentation"), SimulationView::tr("a demanding board member"),
         SimulationView::tr("Board members focused on strategy, numbers and accountability.")},
    };
}

QLabel *bubble(const QString &text, bool fromAvatar, const QString &tag)
{
    const Theme::Colors &c = Theme::colors();
    QLabel *l = Ui::label(QString());
    l->setTextFormat(Qt::RichText);
    l->setWordWrap(true);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    const QString head = tag.isEmpty() ? QString()
                                       : QStringLiteral("<span style='font-size:8pt; color:%1; font-weight:600'>%2</span><br>")
                                             .arg(Theme::css(fromAvatar ? c.accent : c.textSecondary), tag.toUpper().toHtmlEscaped());
    l->setText(head + text.toHtmlEscaped());
    l->setStyleSheet(QStringLiteral("background: %1; color: %2; border-radius: 12px; padding: 8px 12px;")
                         .arg(Theme::css(fromAvatar ? c.button : c.accent), Theme::css(fromAvatar ? c.text : c.onAccent)));
    l->setMaximumWidth(460);
    return l;
}
} // namespace

SimulationView::SimulationView(SessionController *controller, AiClient *ai, MicrophoneMonitor *mic, QWidget *parent)
    : QWidget(parent)
    , m_controller(controller)
    , m_ai(ai)
    , m_mic(mic)
    , m_engine(new SimulationEngine(ai, mic, controller, this))
    , m_stack(new QStackedWidget)
    , m_type(new QComboBox)
    , m_role(new QLineEdit)
    , m_audience(new QLineEdit)
    , m_objectives(new QPlainTextEdit)
    , m_length(new DurationEdit)
    , m_voice(new QComboBox)
    , m_setupNote(Ui::label(QString(), "secondary"))
    , m_avatar(new AvatarWidget)
    , m_roleLabel(Ui::label(QString(), "cardTitle"))
    , m_status(Ui::label(QString(), "secondary"))
    , m_clock(Ui::label(QString(), "secondary"))
    , m_meter(new LevelMeter)
    , m_chatScroll(new QScrollArea)
    , m_chat(new QVBoxLayout)
    , m_typed(new QLineEdit)
    , m_results(new QVBoxLayout)
{
    const Theme::Colors &c = Theme::colors();

    // ================= Setup =================
    auto *setupPage = new QWidget;
    auto *setupLayout = new QVBoxLayout(setupPage);
    setupLayout->setContentsMargins(0, 0, 0, 0);
    setupLayout->setSpacing(12);

    auto *setup = new Card(tr("Simulate a meeting"));
    QLabel *intro = Ui::label(tr("Practise the real thing. SMILE joins as another participant: it asks questions, "
                                 "challenges your arguments, asks for evidence and, on harder levels, interrupts. "
                                 "Speak normally and pause when you've finished; it answers out loud."),
                              "secondary");
    intro->setWordWrap(true);
    setup->body()->addWidget(intro);

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(10);
    grid->setColumnStretch(1, 1);
    m_type->setEditable(true);
    for (const MeetingType &t : meetingTypes())
        m_type->addItem(t.name);
    m_role->setPlaceholderText(tr("Who SMILE plays, e.g. a critical investor"));
    m_audience->setPlaceholderText(tr("Who they are and what they care about"));
    m_objectives->setPlaceholderText(tr("What you want to achieve, e.g. approval for a one-month pilot"));
    m_objectives->setFixedHeight(64);
    m_length->setSeconds(5 * 60);
    m_length->setFixedWidth(m_length->sizeHint().width());
    const QList<QPair<QString, QString>> voices = {
        {tr("Michael (US, male)"), QStringLiteral("am_michael")}, {tr("Heart (US, female)"), QStringLiteral("af_heart")},
        {tr("George (UK, male)"), QStringLiteral("bm_george")},   {tr("Emma (UK, female)"), QStringLiteral("bf_emma")},
        {tr("Adam (US, male)"), QStringLiteral("am_adam")},       {tr("Bella (US, female)"), QStringLiteral("af_bella")},
    };
    for (const auto &[label, id] : voices) {
        m_voice->addItem(label, id);
        if (id == AppSettings::instance().avatarVoice())
            m_voice->setCurrentIndex(m_voice->count() - 1);
    }

    auto *levels = new QHBoxLayout;
    levels->setSpacing(4);
    auto *levelGroup = new QButtonGroup(this);
    const QStringList levelNames = {tr("Beginner"), tr("Intermediate"), tr("Advanced")};
    for (int i = 0; i < 3; ++i) {
        auto *b = new QPushButton(levelNames[i]);
        b->setObjectName(QStringLiteral("segment"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        levelGroup->addButton(b, i);
        levels->addWidget(b);
        m_levels << b;
    }
    m_levels[1]->setChecked(true);
    levels->addStretch();
    QLabel *levelHint = Ui::label(QString(), "secondary");
    levelHint->setWordWrap(true);
    auto describeLevel = [levelHint](int id) {
        const QStringList text = {
            tr("Friendly participant, predictable questions, supportive."),
            tr("More challenging questions, requests for clarification, occasional interruptions."),
            tr("High pressure: tough objections, unexpected questions, demands for evidence, interruptions."),
        };
        levelHint->setText(text.value(id));
    };
    describeLevel(1);
    connect(levelGroup, &QButtonGroup::idClicked, this, describeLevel);

    int row = 0;
    grid->addWidget(Ui::label(tr("Meeting")), row, 0);
    grid->addWidget(m_type, row++, 1);
    grid->addWidget(Ui::label(tr("SMILE plays")), row, 0);
    grid->addWidget(m_role, row++, 1);
    grid->addWidget(Ui::label(tr("Audience")), row, 0);
    grid->addWidget(m_audience, row++, 1);
    grid->addWidget(Ui::label(tr("Your objectives")), row, 0, Qt::AlignTop);
    grid->addWidget(m_objectives, row++, 1);
    grid->addWidget(Ui::label(tr("Difficulty")), row, 0);
    grid->addLayout(levels, row++, 1);
    grid->addWidget(levelHint, row++, 1);
    auto *lengthRow = new QHBoxLayout;
    lengthRow->addWidget(m_length);
    lengthRow->addSpacing(20);
    lengthRow->addWidget(Ui::label(tr("Voice")));
    lengthRow->addWidget(m_voice);
    lengthRow->addStretch();
    grid->addWidget(Ui::label(tr("Length")), row, 0);
    grid->addLayout(lengthRow, row++, 1);
    setup->body()->addLayout(grid);

    auto *startRow = new QHBoxLayout;
    auto *startButton = new QPushButton(tr("Start the meeting"));
    startButton->setObjectName(QStringLiteral("accent"));
    startButton->setIcon(Icons::icon(Icons::Icon::Play, c.onAccent, 14));
    startButton->setCursor(Qt::PointingHandCursor);
    startButton->setMinimumHeight(38);
    m_setupNote->setWordWrap(true);
    startRow->addWidget(startButton);
    startRow->addSpacing(12);
    startRow->addWidget(m_setupNote, 1);
    setup->body()->addLayout(startRow);
    setupLayout->addWidget(setup);
    setupLayout->addStretch();

    // ================= Live meeting =================
    auto *livePage = new QWidget;
    auto *live = new QHBoxLayout(livePage);
    live->setContentsMargins(0, 0, 0, 0);
    live->setSpacing(12);

    auto *stage = new Card;
    stage->setGraphicsEffect(nullptr); // smooth 60 fps animation
    m_avatar->setMinimumSize(300, 300);
    stage->body()->addWidget(m_roleLabel, 0, Qt::AlignHCenter);
    stage->body()->addWidget(m_avatar, 1);
    m_status->setAlignment(Qt::AlignHCenter);
    m_status->setWordWrap(true);
    stage->body()->addWidget(m_status);
    auto *meterRow = new QHBoxLayout;
    QLabel *micIcon = Ui::label(QString());
    micIcon->setPixmap(Icons::icon(Icons::Icon::Mic, c.textSecondary, 16).pixmap(16, 16));
    meterRow->addWidget(micIcon);
    meterRow->addWidget(m_meter, 1);
    meterRow->addWidget(m_clock);
    stage->body()->addLayout(meterRow);
    auto *endButton = new QPushButton(tr("End meeting"));
    endButton->setCursor(Qt::PointingHandCursor);
    stage->body()->addWidget(endButton, 0, Qt::AlignHCenter);

    auto *conversation = new Card(tr("Conversation"));
    auto *chatHost = new QWidget;
    chatHost->setObjectName(QStringLiteral("page"));
    auto *chatOuter = new QVBoxLayout(chatHost);
    chatOuter->setContentsMargins(0, 0, 6, 0);
    m_chat->setSpacing(8);
    chatOuter->addLayout(m_chat);
    chatOuter->addStretch();
    m_chatScroll->setWidget(chatHost);
    m_chatScroll->setWidgetResizable(true);
    m_chatScroll->setFrameShape(QFrame::NoFrame);
    m_chatScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_chatScroll->setMinimumHeight(380);
    conversation->body()->addWidget(m_chatScroll, 1);
    auto *typeRow = new QHBoxLayout;
    m_typed->setPlaceholderText(tr("Prefer typing? Write your answer and press Enter"));
    auto *send = new QPushButton(tr("Send"));
    typeRow->addWidget(m_typed, 1);
    typeRow->addWidget(send);
    conversation->body()->addLayout(typeRow);

    live->addWidget(stage, 2);
    live->addWidget(conversation, 3);

    // ================= Results =================
    auto *resultsPage = new QWidget;
    m_results->setContentsMargins(0, 0, 0, 0);
    m_results->setSpacing(12);
    auto *resultsOuter = new QVBoxLayout(resultsPage);
    resultsOuter->setContentsMargins(0, 0, 0, 0);
    resultsOuter->addLayout(m_results);
    resultsOuter->addStretch();

    m_stack->addWidget(setupPage);
    m_stack->addWidget(livePage);
    m_stack->addWidget(resultsPage);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_stack);

    // ================= Behaviour =================
    connect(m_type, &QComboBox::currentTextChanged, this, &SimulationView::applyTypeDefaults);
    applyTypeDefaults(m_type->currentText());
    connect(startButton, &QPushButton::clicked, this, &SimulationView::start);
    connect(endButton, &QPushButton::clicked, m_engine, &SimulationEngine::end);
    auto sendTyped = [this] {
        m_engine->sendTyped(m_typed->text());
        m_typed->clear();
    };
    connect(send, &QPushButton::clicked, this, sendTyped);
    connect(m_typed, &QLineEdit::returnPressed, this, sendTyped);

    m_avatar->setLevelSource([this] { return m_engine->player()->level(); });
    connect(m_engine, &SimulationEngine::stateChanged, m_avatar, &AvatarWidget::setState);
    connect(m_engine, &SimulationEngine::moodChanged, m_avatar, &AvatarWidget::setMood);
    connect(m_engine, &SimulationEngine::message, this, &SimulationView::addBubble);
    connect(m_engine, &SimulationEngine::statusChanged, m_status, &QLabel::setText);
    connect(m_engine, &SimulationEngine::failed, this, [this](const QString &error) {
        m_setupNote->setText(error);
        m_stack->setCurrentIndex(0);
    });
    connect(m_engine, &SimulationEngine::finished, this, &SimulationView::showResults);
    connect(m_mic, &MicrophoneMonitor::levelChanged, m_meter, &LevelMeter::setLevel);
    connect(m_mic, &MicrophoneMonitor::speakingChanged, this, [this](bool speaking) {
        m_meter->setSpeaking(speaking);
        m_avatar->setUserSpeaking(speaking);
    });
    m_clockTimer.setInterval(500);
    connect(&m_clockTimer, &QTimer::timeout, this, [this] {
        m_clock->setText(tr("%1 / %2").arg(formatDuration(m_engine->elapsedSeconds()), formatDuration(m_engine->totalSeconds())));
    });
}

void SimulationView::applyTypeDefaults(const QString &type)
{
    for (const auto &t : meetingTypes()) {
        if (t.name == type) {
            m_role->setText(t.role);
            m_audience->setText(t.audience);
        }
    }
}

void SimulationView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // Suggest objectives from the plan: what the last section asks for, plus
    // the key points
    if (m_objectives->toPlainText().trimmed().isEmpty()) {
        const Agenda agenda = m_controller->agenda();
        QStringList objectives;
        if (!agenda.isEmpty() && !agenda.last().summary.isEmpty())
            objectives << agenda.last().summary;
        if (!agenda.isEmpty() && !agenda.last().keyPoints.isEmpty())
            objectives << agenda.last().keyPoints;
        m_objectives->setPlainText(objectives.join(QLatin1Char('\n')));
    }
}

void SimulationView::start()
{
    if (m_controller->isRunning()) {
        m_setupNote->setText(tr("Finish the session that's running first."));
        return;
    }
    if (!m_ai->isConfigured()) {
        m_setupNote->setText(tr("The simulation needs an AI key (Settings → AI coach)."));
        return;
    }
    SimulationConfig config;
    config.meetingType = m_type->currentText();
    config.role = m_role->text().trimmed();
    config.audience = m_audience->text().trimmed();
    config.objectives = m_objectives->toPlainText().trimmed();
    const QStringList levels = {QStringLiteral("beginner"), QStringLiteral("intermediate"), QStringLiteral("advanced")};
    for (int i = 0; i < m_levels.size(); ++i)
        if (m_levels[i]->isChecked())
            config.difficulty = levels[i];
    config.voice = m_voice->currentData().toString();
    AppSettings::instance().setAvatarVoice(config.voice);
    config.seconds = qMax(60, m_length->seconds());
    config.speech = AppSettings::instance().speechText();
    config.sections = m_controller->agenda();

    while (QLayoutItem *item = m_chat->takeAt(0)) {
        delete item->widget();
        delete item->layout();
        delete item;
    }
    QString role = config.role;
    if (!role.isEmpty())
        role[0] = role[0].toUpper();
    m_roleLabel->setText(role.isEmpty() ? tr("SMILE") : role);
    m_setupNote->clear();
    m_stack->setCurrentIndex(1);
    m_clockTimer.start();
    m_engine->start(config);
}

void SimulationView::addBubble(bool fromAvatar, const QString &text, const QString &intent)
{
    auto *row = new QHBoxLayout;
    QLabel *b = bubble(text, fromAvatar, QString(intent).replace(QLatin1Char('_'), QLatin1Char(' ')));
    if (fromAvatar) {
        row->addWidget(b);
        row->addStretch();
    } else {
        row->addStretch();
        row->addWidget(b);
    }
    m_chat->addLayout(row);
    QTimer::singleShot(30, this, [this] { m_chatScroll->verticalScrollBar()->setValue(m_chatScroll->verticalScrollBar()->maximum()); });
}

void SimulationView::showResults(const QJsonObject &e, const SessionReport &report)
{
    m_clockTimer.stop();
    while (QLayoutItem *item = m_results->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    const Theme::Colors &c = Theme::colors();
    const int score = e.value(QLatin1String("score")).toInt(-1);
    const Rating rating = PostMeetPage::ratingFor(score);

    auto *hero = new Card;
    auto *heroRow = new QHBoxLayout;
    heroRow->setSpacing(24);
    heroRow->addWidget(new ScoreRing(score, Theme::ratingColor(int(rating)), 140, tr("/ 100")));
    auto *text = new QVBoxLayout;
    text->addStretch();
    text->addWidget(Ui::label(tr("MEETING SIMULATION · %1 · %2").arg(m_type->currentText().toUpper(), formatDuration(report.totalSeconds)), "caption"));
    QLabel *summary = Ui::label(e.value(QLatin1String("summary")).toString(e.value(QLatin1String("error")).toString()));
    QFont f = summary->font();
    f.setPointSizeF(f.pointSizeF() * 1.15);
    summary->setFont(f);
    summary->setWordWrap(true);
    text->addWidget(summary);
    text->addWidget(Ui::label(tr("%1 exchanges · %2 interruptions").arg(e.value(QLatin1String("turns")).toInt()).arg(e.value(QLatin1String("interruptions")).toInt()), "secondary"));
    text->addStretch();
    heroRow->addLayout(text, 1);
    hero->body()->addLayout(heroRow);
    m_results->addWidget(hero);

    auto addScore = [&](const QString &name, const char *key) {
        const int v = e.value(QLatin1String(key)).toInt(-1);
        auto *x = new Expander(name, QString(), v, Theme::ratingColor(int(PostMeetPage::ratingFor(v))), v >= 0 ? QStringLiteral("%1%").arg(v) : tr("–"));
        x->body()->addWidget(Ui::label(QString()));
        m_results->addWidget(x);
    };
    addScore(tr("Answering questions"), "answers");
    addScore(tr("Evidence and arguments"), "evidence");
    addScore(tr("Composure under pressure"), "composure");
    addScore(tr("Clarity"), "clarity");

    auto listCard = [&](const QString &title, const char *key, const QColor &dot) {
        const QJsonArray items = e.value(QLatin1String(key)).toArray();
        if (items.isEmpty())
            return;
        auto *card = new Card(title);
        for (const QJsonValue &v : items) {
            QLabel *l = Ui::label(QStringLiteral("<span style='color:%1'>●</span>&nbsp; %2").arg(Theme::css(dot), v.toString().toHtmlEscaped()));
            l->setTextFormat(Qt::RichText);
            l->setWordWrap(true);
            card->body()->addWidget(l);
        }
        m_results->addWidget(card);
    };
    listCard(tr("Most important improvements"), "improvements", c.accent);
    listCard(tr("Handled well"), "handled_well", c.success);
    listCard(tr("Where you struggled"), "struggled", c.orange);

    const QJsonArray objectives = e.value(QLatin1String("objectives")).toArray();
    if (!objectives.isEmpty()) {
        auto *card = new Card(tr("Objectives"));
        for (const QJsonValue &v : objectives) {
            const QJsonObject o = v.toObject();
            const bool met = o.value(QLatin1String("met")).toBool();
            QLabel *l = Ui::label(QStringLiteral("<span style='color:%1; font-weight:700'>%2</span>&nbsp; %3")
                                      .arg(Theme::css(met ? c.success : c.critical), met ? QStringLiteral("✓") : QStringLiteral("✗"),
                                           o.value(QLatin1String("objective")).toString().toHtmlEscaped()));
            l->setTextFormat(Qt::RichText);
            l->setWordWrap(true);
            card->body()->addWidget(l);
        }
        m_results->addWidget(card);
    }

    // Delivery: the same breakdown as the Results page, compact
    const Evaluation ev = PostMeetPage::evaluate(report);
    QLabel *caption = Ui::label(tr("YOUR DELIVERY"), "caption");
    m_results->addWidget(caption);
    for (const ReportCategory &cat : ev.categories) {
        if (cat.score < 0 || cat.name == PostMeetPage::tr("Timing"))
            continue;
        auto *x = new Expander(cat.name, cat.summary, cat.score, Theme::ratingColor(int(cat.rating)), QStringLiteral("%1%").arg(cat.score));
        for (const QString &r : cat.recommendations)
            x->body()->addWidget(Ui::label(QStringLiteral("• ") + r));
        m_results->addWidget(x);
    }

    auto *again = new QPushButton(tr("Run another simulation"));
    again->setObjectName(QStringLiteral("accent"));
    again->setCursor(Qt::PointingHandCursor);
    connect(again, &QPushButton::clicked, this, [this] { m_stack->setCurrentIndex(0); });
    m_results->addWidget(again, 0, Qt::AlignLeft);
    m_stack->setCurrentIndex(2);
}
