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
#include "sensors/CameraMonitor.h"
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
        {SimulationView::tr("Presentation"), SimulationView::tr("a senior stakeholder"),
         SimulationView::tr("Leaders hearing your presentation; they ask questions and want clear answers.")},
        {SimulationView::tr("Interview"), SimulationView::tr("the hiring manager"),
         SimulationView::tr("A hiring manager assessing your experience and fit for the role.")},
        {SimulationView::tr("Negotiation"), SimulationView::tr("a tough counterpart"),
         SimulationView::tr("The other side of a deal: they push back on price, terms and timing.")},
        {SimulationView::tr("Business proposal"), SimulationView::tr("a critical investor"),
         SimulationView::tr("Investors deciding whether to fund the idea; they care about cost, risk and return.")},
        {SimulationView::tr("Sales pitch"), SimulationView::tr("a busy prospective customer"),
         SimulationView::tr("A buyer comparing options with a tight budget.")},
    };
}

QLabel *bubble(const QString &text, bool fromAvatar)
{
    const Theme::Colors &c = Theme::colors();
    QLabel *l = Ui::label(text);
    l->setWordWrap(true);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    l->setStyleSheet(QStringLiteral("background: %1; color: %2; border-radius: 12px; padding: 8px 12px;")
                         .arg(Theme::css(fromAvatar ? c.button : c.accent), Theme::css(fromAvatar ? c.text : c.onAccent)));
    l->setMaximumWidth(300);
    return l;
}

// A video tile: rounded box with a name label underneath
QFrame *tile(QWidget *content, QLabel *name)
{
    auto *frame = new QFrame;
    frame->setObjectName(QStringLiteral("card"));
    auto *v = new QVBoxLayout(frame);
    v->setContentsMargins(10, 10, 10, 10);
    v->setSpacing(6);
    v->addWidget(content, 1);
    v->addWidget(name, 0, Qt::AlignLeft);
    return frame;
}
} // namespace

QWidget *SimulationView::segmented(const QStringList &labels, int checked, std::function<void(int)> onPick,
                                   QList<QPushButton *> *out)
{
    auto *host = new QWidget;
    auto *row = new QHBoxLayout(host);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(4);
    auto *group = new QButtonGroup(host);
    for (int i = 0; i < labels.size(); ++i) {
        auto *b = new QPushButton(labels[i]);
        b->setObjectName(QStringLiteral("segment"));
        b->setCheckable(true);
        b->setChecked(i == checked);
        b->setCursor(Qt::PointingHandCursor);
        group->addButton(b, i);
        row->addWidget(b);
        if (out)
            out->append(b);
    }
    row->addStretch();
    connect(group, &QButtonGroup::idClicked, this, onPick);
    return host;
}

SimulationView::SimulationView(SessionController *controller, AiClient *ai, MicrophoneMonitor *mic, CameraMonitor *camera,
                               QWidget *parent)
    : QWidget(parent)
    , m_controller(controller)
    , m_ai(ai)
    , m_mic(mic)
    , m_camera(camera)
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
    , m_self(new CameraPreview)
    , m_roleLabel(Ui::label(QString()))
    , m_status(Ui::label(QString(), "secondary"))
    , m_clock(Ui::label(QString(), "secondary"))
    , m_feedback(Ui::label(QString()))
    , m_noise(Ui::label(QString()))
    , m_meter(new LevelMeter)
    , m_chatScroll(new QScrollArea)
    , m_chat(new QVBoxLayout)
    , m_typed(new QLineEdit)
    , m_results(new QVBoxLayout)
{
    const Theme::Colors &c = Theme::colors();
    AppSettings &settings = AppSettings::instance();

    // ================= Setup =================
    auto *setupPage = new QWidget;
    auto *setupLayout = new QVBoxLayout(setupPage);
    setupLayout->setContentsMargins(0, 0, 0, 0);
    auto *setup = new Card;
    QLabel *intro = Ui::label(tr("Practise with SMILE as the other person. It reacts to what you say and to how you "
                                 "come across."),
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
    m_objectives->setPlaceholderText(tr("Optional: what you want to achieve"));
    m_objectives->setFixedHeight(56);
    m_length->setSeconds(5 * 60);
    m_length->setFixedWidth(m_length->sizeHint().width());
    const QList<QPair<QString, QString>> voices = {
        {tr("Michael"), QStringLiteral("am_michael")}, {tr("Heart"), QStringLiteral("af_heart")},
        {tr("George (UK)"), QStringLiteral("bm_george")}, {tr("Emma (UK)"), QStringLiteral("bf_emma")},
    };
    for (const auto &[label, id] : voices) {
        m_voice->addItem(label, id);
        if (id == settings.avatarVoice())
            m_voice->setCurrentIndex(m_voice->count() - 1);
    }

    int row = 0;
    grid->addWidget(Ui::label(tr("Type")), row, 0);
    grid->addWidget(m_type, row++, 1);
    grid->addWidget(Ui::label(tr("SMILE plays")), row, 0);
    grid->addWidget(m_role, row++, 1);
    grid->addWidget(Ui::label(tr("Context")), row, 0);
    grid->addWidget(m_audience, row++, 1);
    grid->addWidget(Ui::label(tr("Goal")), row, 0, Qt::AlignTop);
    grid->addWidget(m_objectives, row++, 1);
    grid->addWidget(Ui::label(tr("Difficulty")), row, 0);
    grid->addWidget(segmented({tr("Beginner"), tr("Intermediate"), tr("Advanced")}, 1, [](int) {}, &m_levels), row++, 1);
    grid->addWidget(Ui::label(tr("SMILE steps in")), row, 0);
    auto *stepIn = segmented({tr("Never"), tr("Light"), tr("Balanced"), tr("Strict")}, settings.avatarIntervention(),
                             [](int id) { AppSettings::instance().setAvatarIntervention(id); }, nullptr);
    stepIn->setToolTip(tr("How often the avatar comments on your behaviour: a covered face, lots of movement, "
                          "slouching… Strict also covers expression, eye contact and filler words."));
    grid->addWidget(stepIn, row++, 1);
    auto *lengthRow = new QHBoxLayout;
    lengthRow->addWidget(m_length);
    lengthRow->addSpacing(16);
    lengthRow->addWidget(Ui::label(tr("Voice")));
    lengthRow->addWidget(m_voice);
    lengthRow->addStretch();
    grid->addWidget(Ui::label(tr("Length")), row, 0);
    grid->addLayout(lengthRow, row++, 1);
    setup->body()->addLayout(grid);

    auto *startRow = new QHBoxLayout;
    auto *startButton = new QPushButton(tr("Start"));
    startButton->setObjectName(QStringLiteral("accent"));
    startButton->setIcon(Icons::icon(Icons::Icon::Play, c.onAccent, 14));
    startButton->setCursor(Qt::PointingHandCursor);
    startButton->setMinimumSize(140, 40);
    m_setupNote->setWordWrap(true);
    startRow->addWidget(startButton);
    startRow->addSpacing(12);
    startRow->addWidget(Ui::label(tr("Headphones work best."), "secondary"));
    startRow->addWidget(m_setupNote, 1);
    setup->body()->addLayout(startRow);
    setupLayout->addWidget(setup);
    setupLayout->addStretch();

    // ================= Live meeting (video-call layout) =================
    auto *livePage = new QWidget;
    auto *live = new QHBoxLayout(livePage);
    live->setContentsMargins(0, 0, 0, 0);
    live->setSpacing(12);

    auto *left = new QVBoxLayout;
    left->setSpacing(10);
    auto *tiles = new QHBoxLayout;
    tiles->setSpacing(10);
    m_avatar->setMinimumSize(280, 240);
    QFrame *avatarTile = tile(m_avatar, m_roleLabel);
    m_self->setOverlay(false);
    m_self->setMinimumSize(280, 240);
    QFrame *selfTile = tile(m_self, Ui::label(tr("You")));
    tiles->addWidget(avatarTile, 1);
    tiles->addWidget(selfTile, 1);
    left->addLayout(tiles, 1);

    m_feedback->setWordWrap(true);
    m_feedback->setMinimumHeight(42);
    m_feedback->hide();
    m_noise->setWordWrap(true);
    m_noise->setText(tr("It's noisy around you. Put on headphones or move somewhere quieter."));
    QColor noiseBg = c.orange;
    noiseBg.setAlpha(40);
    m_noise->setStyleSheet(QStringLiteral("background: %1; border: 1px solid %2; border-radius: 8px; padding: 8px 12px; font-weight: 600;")
                               .arg(Theme::css(noiseBg), Theme::css(c.orange)));
    m_noise->hide();
    left->addWidget(m_feedback);
    left->addWidget(m_noise);

    auto *controls = new QHBoxLayout;
    QLabel *micIcon = Ui::label(QString());
    micIcon->setPixmap(Icons::icon(Icons::Icon::Mic, c.textSecondary, 16).pixmap(16, 16));
    controls->addWidget(micIcon);
    m_meter->setMaximumWidth(160);
    controls->addWidget(m_meter);
    controls->addSpacing(10);
    m_status->setWordWrap(true);
    controls->addWidget(m_status, 1);
    controls->addWidget(m_clock);
    auto *endButton = new QPushButton(tr("End"));
    endButton->setCursor(Qt::PointingHandCursor);
    endButton->setStyleSheet(QStringLiteral("QPushButton { background: %1; color: white; border: none; } QPushButton:hover { background: %2; }")
                                 .arg(Theme::css(c.critical), Theme::css(c.critical.darker(110))));
    controls->addWidget(endButton);
    left->addLayout(controls);
    live->addLayout(left, 3);

    auto *conversation = new Card(tr("Chat"));
    conversation->setMaximumWidth(380);
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
    m_chatScroll->setMinimumHeight(300);
    conversation->body()->addWidget(m_chatScroll, 1);
    m_typed->setPlaceholderText(tr("Or type, then Enter"));
    conversation->body()->addWidget(m_typed);
    live->addWidget(conversation, 2);

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
    connect(m_typed, &QLineEdit::returnPressed, this, [this] {
        m_engine->sendTyped(m_typed->text());
        m_typed->clear();
    });

    m_avatar->setLevelSource([this] { return m_engine->player()->level(); });
    connect(m_engine, &SimulationEngine::stateChanged, m_avatar, &AvatarWidget::setState);
    connect(m_engine, &SimulationEngine::moodChanged, m_avatar, &AvatarWidget::setMood);
    connect(m_engine, &SimulationEngine::message, this, &SimulationView::addBubble);
    connect(m_engine, &SimulationEngine::statusChanged, m_status, &QLabel::setText);
    connect(m_engine, &SimulationEngine::liveTip, this, &SimulationView::showFeedback);
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
    connect(m_mic, &MicrophoneMonitor::noisyChanged, this, [this](bool noisy) {
        m_noise->setVisible(noisy && m_engine->isRunning());
    });
    connect(m_camera, &CameraMonitor::frameReady, this, [this](const QImage &frame) {
        if (m_stack->currentIndex() == 1 && isVisible())
            m_self->setFrame(frame);
    });
    m_clockTimer.setInterval(500);
    connect(&m_clockTimer, &QTimer::timeout, this, [this] {
        m_clock->setText(QStringLiteral("%1 / %2").arg(formatDuration(m_engine->elapsedSeconds()), formatDuration(m_engine->totalSeconds())));
    });
    m_feedbackTimer.setSingleShot(true);
    m_feedbackTimer.setInterval(6000);
    connect(&m_feedbackTimer, &QTimer::timeout, m_feedback, &QWidget::hide);
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
    if (m_objectives->toPlainText().trimmed().isEmpty()) {
        const Agenda agenda = m_controller->agenda();
        if (!agenda.isEmpty() && !agenda.last().summary.isEmpty())
            m_objectives->setPlainText(agenda.last().summary);
    }
}

void SimulationView::start()
{
    if (m_controller->isRunning()) {
        m_setupNote->setText(tr("Finish the session that's running first."));
        return;
    }
    if (!m_ai->isConfigured()) {
        m_setupNote->setText(tr("Add your AI key in Settings first."));
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
        if (item->layout()) {
            while (QLayoutItem *inner = item->layout()->takeAt(0)) {
                delete inner->widget();
                delete inner;
            }
        }
        delete item;
    }
    QString role = config.role;
    if (!role.isEmpty())
        role[0] = role[0].toUpper();
    m_roleLabel->setText(role.isEmpty() ? tr("SMILE") : role);
    m_setupNote->clear();
    m_feedback->hide();
    m_noise->hide();
    m_self->setMessage(AppSettings::instance().useCamera() ? tr("Starting camera…") : tr("Camera off"));
    m_stack->setCurrentIndex(1);
    m_clockTimer.start();
    m_engine->start(config);
}

void SimulationView::addBubble(bool fromAvatar, const QString &text, const QString &)
{
    auto *row = new QHBoxLayout;
    QLabel *b = bubble(text, fromAvatar);
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

void SimulationView::showFeedback(const Tip &tip)
{
    // Live behaviour feedback, right under your video, like the real companion
    const Theme::Colors &c = Theme::colors();
    const QColor tone = tip.severity == TipSeverity::Warning ? c.orange : (tip.severity == TipSeverity::Positive ? c.success : c.accent);
    QColor bg = tone;
    bg.setAlpha(c.dark ? 50 : 30);
    m_feedback->setStyleSheet(QStringLiteral("background: %1; border-left: 4px solid %2; border-radius: 6px; padding: 8px 12px; "
                                             "font-weight: 600;")
                                  .arg(Theme::css(bg), Theme::css(tone)));
    m_feedback->setText(tip.text);
    m_feedback->show();
    m_feedbackTimer.start();
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
    // The score comes from what was measured (behaviour), so it always exists
    const Evaluation ev = PostMeetPage::evaluate(report, QJsonObject{{QStringLiteral("summary"), e.value(QLatin1String("summary"))},
                                                                    {QStringLiteral("improvements"), e.value(QLatin1String("improvements"))}});

    auto *hero = new Card;
    auto *heroRow = new QHBoxLayout;
    heroRow->setSpacing(24);
    heroRow->addWidget(new ScoreRing(ev.overall, Theme::ratingColor(int(ev.rating)), 140, tr("/ 100")));
    auto *text = new QVBoxLayout;
    text->addStretch();
    text->addWidget(Ui::label(tr("SIMULATION · %1 · %2").arg(m_type->currentText().toUpper(), formatDuration(report.totalSeconds)), "caption"));
    QLabel *headline = Ui::label(ev.headline, "bigValue");
    headline->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::css(Theme::ratingColor(int(ev.rating)))));
    text->addWidget(headline);
    QLabel *summary = Ui::label(ev.assessment);
    summary->setWordWrap(true);
    text->addWidget(summary);
    text->addWidget(Ui::label(tr("%1 exchanges · SMILE stepped in %2×")
                                  .arg(e.value(QLatin1String("turns")).toInt())
                                  .arg(e.value(QLatin1String("interventions")).toInt()),
                              "secondary"));
    text->addStretch();
    heroRow->addLayout(text, 1);
    hero->body()->addLayout(heroRow);
    m_results->addWidget(hero);

    if (!ev.improvements.isEmpty()) {
        auto *imp = new Card(tr("Work on this next"));
        int n = 1;
        for (const QString &t : ev.improvements) {
            QLabel *l = Ui::label(QStringLiteral("<span style='color:%1; font-weight:700'>%2</span>&nbsp;&nbsp;%3")
                                      .arg(Theme::css(c.accent))
                                      .arg(n++)
                                      .arg(t.toHtmlEscaped()));
            l->setTextFormat(Qt::RichText);
            l->setWordWrap(true);
            imp->body()->addWidget(l);
        }
        m_results->addWidget(imp);
    }
    const QJsonArray highlights = e.value(QLatin1String("highlights")).toArray();
    if (!highlights.isEmpty()) {
        auto *card = new Card(tr("Well done"));
        for (const QJsonValue &v : highlights) {
            QLabel *l = Ui::label(QStringLiteral("<span style='color:%1'>●</span>&nbsp; %2").arg(Theme::css(c.success), v.toString().toHtmlEscaped()));
            l->setTextFormat(Qt::RichText);
            l->setWordWrap(true);
            card->body()->addWidget(l);
        }
        m_results->addWidget(card);
    }

    for (const ReportCategory &cat : ev.categories) {
        if (cat.score < 0 || cat.name == PostMeetPage::tr("Timing") || cat.name == PostMeetPage::tr("Key points"))
            continue;
        auto *x = new Expander(cat.name, cat.summary, cat.score, Theme::ratingColor(int(cat.rating)), QStringLiteral("%1%").arg(cat.score));
        for (const QString &d : cat.details)
            x->body()->addWidget(Ui::label(QStringLiteral("• ") + d, "secondary"));
        for (const QString &moment : cat.events)
            x->body()->addWidget(Ui::label(QStringLiteral("⚑ ") + moment));
        for (const QString &r : cat.recommendations)
            x->body()->addWidget(Ui::label(QStringLiteral("→ ") + r));
        m_results->addWidget(x);
    }

    auto *again = new QPushButton(tr("Try again"));
    again->setObjectName(QStringLiteral("accent"));
    again->setCursor(Qt::PointingHandCursor);
    again->setMinimumHeight(38);
    connect(again, &QPushButton::clicked, this, [this] { m_stack->setCurrentIndex(0); });
    m_results->addWidget(again, 0, Qt::AlignLeft);
    m_stack->setCurrentIndex(2);
}
