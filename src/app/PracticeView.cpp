#include "app/PracticeView.h"

#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

#include "ai/AiClient.h"
#include "app/SessionRunWidget.h"
#include "app/pages/PostMeetPage.h"
#include "core/SessionController.h"
#include "core/TimeFormat.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

namespace {
const char *kAttemptsKey = "practice/attempts";

void clear(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        else if (item->layout()) {
            clear(item->layout());
            delete item->layout();
        }
        delete item;
    }
}
} // namespace

PracticeView::PracticeView(SessionController *controller, AiClient *ai, QWidget *parent)
    : QWidget(parent)
    , m_controller(controller)
    , m_ai(ai)
    , m_list(new QListWidget)
    , m_title(Ui::label(QString(), "bigValue"))
    , m_meta(Ui::label(QString(), "secondary"))
    , m_purpose(Ui::label(QString()))
    , m_notes(Ui::label(QString()))
    , m_keyPoints(Ui::label(QString()))
    , m_start(new QPushButton(tr("Start practising")))
    , m_run(new SessionRunWidget(controller))
    , m_result(new QVBoxLayout)
    , m_history(new QVBoxLayout)
{
    const Theme::Colors &c = Theme::colors();

    // Left: the sections
    auto *listCard = new Card(tr("Sections"));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setStyleSheet(QStringLiteral("QListWidget { background: transparent; } QListWidget::item { padding: 10px 8px; "
                                         "border-radius: 6px; } QListWidget::item:selected { background: %1; color: %2; }")
                              .arg(Theme::css(c.button), Theme::css(c.text)));
    m_list->setMinimumWidth(220);
    listCard->body()->addWidget(m_list, 1);
    listCard->setMaximumWidth(300);

    // Right: the selected section
    auto *section = new Card;
    m_title->setWordWrap(true);
    m_purpose->setWordWrap(true);
    m_notes->setWordWrap(true);
    m_notes->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont nf = m_notes->font();
    nf.setPointSizeF(nf.pointSizeF() * 1.12);
    m_notes->setFont(nf);
    m_notes->setStyleSheet(QStringLiteral("background: %1; border-radius: 8px; padding: 12px 14px;").arg(Theme::css(c.background)));
    m_keyPoints->setTextFormat(Qt::RichText);
    m_keyPoints->setWordWrap(true);
    m_start->setObjectName(QStringLiteral("accent"));
    m_start->setIcon(Icons::icon(Icons::Icon::Play, c.onAccent, 14));
    m_start->setCursor(Qt::PointingHandCursor);
    m_start->setMinimumHeight(38);

    section->body()->addWidget(m_title);
    section->body()->addWidget(m_meta);
    section->body()->addWidget(m_purpose);
    section->body()->addWidget(Ui::label(tr("YOUR NOTES"), "caption"));
    section->body()->addWidget(m_notes);
    section->body()->addWidget(Ui::label(tr("KEY POINTS TO GET ACROSS"), "caption"));
    section->body()->addWidget(m_keyPoints);
    section->body()->addSpacing(4);
    section->body()->addWidget(m_start, 0, Qt::AlignLeft);
    section->body()->addWidget(m_run);

    auto *right = new QVBoxLayout;
    right->setSpacing(12);
    right->addWidget(section);
    m_result->setSpacing(12);
    right->addLayout(m_result);
    m_history->setSpacing(6);
    right->addLayout(m_history);
    right->addStretch();

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->addWidget(listCard, 0, Qt::AlignTop);
    layout->addLayout(right, 1);

    connect(m_list, &QListWidget::currentRowChanged, this, &PracticeView::select);
    connect(m_start, &QPushButton::clicked, this, &PracticeView::startAttempt);
    connect(m_controller, &SessionController::runningChanged, this, [this](bool running) {
        const bool mine = running && m_controller->mode() == SessionMode::Practice;
        m_run->setVisible(mine);
        m_start->setVisible(!mine);
        m_start->setEnabled(!running);
        m_list->setEnabled(!running);
    });
    connect(m_controller, &SessionController::finished, this, &PracticeView::onFinished);
    m_run->hide();
    refreshSections();
}

void PracticeView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refreshSections();
}

void PracticeView::refreshSections()
{
    if (m_controller->isRunning())
        return;
    m_sections = m_controller->agenda();
    const int keep = m_index;
    const QSignalBlocker block(m_list);
    m_list->clear();
    for (int i = 0; i < m_sections.size(); ++i) {
        const QList<Attempt> attempts = attemptsFor(m_sections[i].title);
        QString label = QStringLiteral("%1.  %2").arg(i + 1).arg(m_sections[i].title);
        if (!attempts.isEmpty() && attempts.last().overall >= 0)
            label += QStringLiteral("   ·  %1").arg(attempts.last().overall);
        m_list->addItem(label);
    }
    const int index = qBound(0, keep < 0 ? 0 : keep, int(m_sections.size()) - 1);
    if (!m_sections.isEmpty()) {
        m_list->setCurrentRow(index);
        select(index);
    } else {
        m_title->setText(tr("No sections yet"));
        m_meta->setText(tr("Add sections on the Plan tab first."));
        m_start->setEnabled(false);
    }
}

void PracticeView::select(int index)
{
    if (index < 0 || index >= m_sections.size())
        return;
    m_index = index;
    ++m_feedbackId; // any feedback still on its way belongs to something else now
    const AgendaPoint &p = m_sections[index];
    const Theme::Colors &c = Theme::colors();
    m_title->setText(p.title);
    m_meta->setText(p.targetSeconds > 0 ? tr("Target %1").arg(formatDuration(p.targetSeconds)) : tr("No time limit"));
    m_purpose->setText(p.summary);
    m_purpose->setVisible(!p.summary.isEmpty());
    m_notes->setText(p.sourceText.isEmpty() ? tr("No notes for this section. Add them on the Plan tab (open the section) "
                                                 "or structure your speech.")
                                            : p.sourceText);
    QString points;
    for (const QString &k : p.keyPoints)
        points += QStringLiteral("<span style='color:%1'>●</span>&nbsp; %2<br>").arg(Theme::css(c.accent), k.toHtmlEscaped());
    m_keyPoints->setText(points.isEmpty() ? tr("No key points set.") : points);
    m_start->setEnabled(!m_controller->isRunning());
    m_start->setText(attemptsFor(p.title).isEmpty() ? tr("Start practising") : tr("Practise again"));
    clear(m_result);
    renderHistory();
}

void PracticeView::startAttempt()
{
    if (m_index < 0 || m_index >= m_sections.size() || m_controller->isRunning())
        return;
    ++m_feedbackId;
    clear(m_result);
    m_practising = true;
    m_controller->startWith({m_sections[m_index]}, SessionMode::Practice);
}

void PracticeView::onFinished(const SessionReport &report)
{
    if (!m_practising || report.mode != SessionMode::Practice)
        return;
    m_practising = false;
    showAttemptResult(report);
}

void PracticeView::showAttemptResult(const SessionReport &report)
{
    const QString section = m_sections.value(m_index).title;
    const Evaluation ev = PostMeetPage::evaluate(report);

    Attempt attempt;
    attempt.when = QDateTime::currentDateTime().toString(QStringLiteral("dd MMM HH:mm"));
    attempt.seconds = report.totalSeconds;
    attempt.overall = ev.overall;
    for (const ReportCategory &c : ev.categories)
        if (c.score >= 0)
            attempt.categories.insert(c.name, c.score);
    const QList<Attempt> before = attemptsFor(section);
    saveAttempt(section, attempt);

    // Result card
    auto *card = new Card;
    auto *row = new QHBoxLayout;
    row->setSpacing(20);
    row->addWidget(new ScoreRing(ev.overall, Theme::ratingColor(int(ev.rating)), 120, tr("/ 100")));
    auto *text = new QVBoxLayout;
    text->addStretch();
    text->addWidget(Ui::label(tr("ATTEMPT %1 · %2").arg(before.size() + 1).arg(formatDuration(report.totalSeconds)), "caption"));
    QLabel *headline = Ui::label(ev.headline, "cardTitle");
    text->addWidget(headline);
    if (!before.isEmpty() && before.last().overall >= 0 && ev.overall >= 0) {
        const int delta = ev.overall - before.last().overall;
        const Theme::Colors &c = Theme::colors();
        QLabel *d = Ui::label(delta >= 0 ? tr("▲ %1 points better than last time").arg(delta)
                                         : tr("▼ %1 points lower than last time").arg(-delta));
        d->setStyleSheet(QStringLiteral("color: %1; font-weight: 600;").arg(Theme::css(delta >= 0 ? c.success : c.orange)));
        text->addWidget(d);
    }
    text->addStretch();
    row->addLayout(text, 1);
    auto *again = new QPushButton(tr("Try again"));
    again->setObjectName(QStringLiteral("accent"));
    again->setCursor(Qt::PointingHandCursor);
    connect(again, &QPushButton::clicked, this, &PracticeView::startAttempt);
    row->addWidget(again, 0, Qt::AlignVCenter);
    card->body()->addLayout(row);
    m_result->addWidget(card);

    for (const ReportCategory &cat : ev.categories) {
        if (cat.score < 0)
            continue;
        auto *x = new Expander(cat.name, cat.summary, cat.score, Theme::ratingColor(int(cat.rating)), QStringLiteral("%1%").arg(cat.score));
        for (const QString &d : cat.details)
            x->body()->addWidget(Ui::label(QStringLiteral("• ") + d, "secondary"));
        for (const QString &r : cat.recommendations)
            x->body()->addWidget(Ui::label(QStringLiteral("→ ") + r));
        m_result->addWidget(x);
    }

    // Specific feedback for this section (AI)
    if (m_ai->isConfigured())
        requestFeedback(report, int(before.size()) + 1);
    renderHistory();
    refreshSections();
}

void PracticeView::requestFeedback(const SessionReport &report, int attemptNumber)
{
    const AgendaPoint &p = m_sections.value(m_index);
    auto *card = new Card(tr("Feedback on this attempt"));
    QLabel *body = Ui::label(tr("The AI coach is looking at your attempt…"), "secondary");
    body->setWordWrap(true);
    body->setTextFormat(Qt::RichText);
    card->body()->addWidget(body);
    m_result->insertWidget(1, card);

    const QList<Attempt> attempts = attemptsFor(p.title);
    QString history;
    for (const Attempt &a : attempts)
        history += QStringLiteral("- %1: overall %2\n").arg(a.when).arg(a.overall);
    const QString system = QStringLiteral(
        "You coach someone practising ONE section of a talk. Using the section's plan and the measurements of this "
        "attempt, give specific, actionable feedback. Reply with ONLY JSON: {\"strengths\": [\"...\"], \"weaknesses\": "
        "[\"specific weaknesses, quoting their words when possible\"], \"suggestions\": [\"concrete things to try on the "
        "next attempt\"], \"compared\": \"one sentence comparing with earlier attempts, or empty\"}. 2-3 items each.");
    const QString user = QStringLiteral("Section: %1\nPurpose: %2\nTarget: %3\nKey points: %4\nNotes: %5\n\nAttempt %6 data:\n%7\n"
                                        "Earlier attempts:\n%8")
                             .arg(p.title, p.summary, formatDuration(p.targetSeconds), p.keyPoints.join(QStringLiteral("; ")),
                                  p.sourceText.left(3000))
                             .arg(attemptNumber)
                             .arg(PostMeetPage::summarizeForAi(report), history);
    const int id = ++m_feedbackId;
    m_ai->chat(system, user, 1800, [this, body, id](const QString &text, const QString &error) {
        if (id != m_feedbackId)
            return;
        if (!error.isEmpty()) {
            body->setText(error.toHtmlEscaped());
            return;
        }
        const QJsonObject j = AiClient::extractJson(text);
        const Theme::Colors &c = Theme::colors();
        QString html;
        auto list = [&](const QString &title, const char *key, const QColor &dot) {
            const QJsonArray items = j.value(QLatin1String(key)).toArray();
            if (items.isEmpty())
                return;
            html += QStringLiteral("<p style='margin-bottom:2px'><b>%1</b></p>").arg(title.toHtmlEscaped());
            for (const QJsonValue &v : items)
                html += QStringLiteral("<span style='color:%1'>●</span>&nbsp; %2<br>").arg(Theme::css(dot), v.toString().toHtmlEscaped());
        };
        list(tr("What worked"), "strengths", c.success);
        list(tr("What to fix"), "weaknesses", c.orange);
        list(tr("Try this next time"), "suggestions", c.accent);
        const QString compared = j.value(QLatin1String("compared")).toString();
        if (!compared.isEmpty())
            html += QStringLiteral("<p><i>%1</i></p>").arg(compared.toHtmlEscaped());
        body->setObjectName(QString());
        Theme::repolish(body);
        body->setText(html.isEmpty() ? text.toHtmlEscaped() : html);
    });
}

void PracticeView::renderHistory()
{
    clear(m_history);
    const QString section = m_sections.value(m_index).title;
    const QList<Attempt> attempts = attemptsFor(section);
    if (attempts.isEmpty())
        return;
    const Theme::Colors &c = Theme::colors();
    auto *card = new Card(tr("Your attempts"));
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(8);
    const QStringList columns = {tr("ATTEMPT"), tr("WHEN"), tr("TIME"), tr("OVERALL"), tr("DELIVERY"), tr("KEY POINTS"), tr("FACE")};
    for (int col = 0; col < columns.size(); ++col)
        grid->addWidget(Ui::label(columns[col], "caption"), 0, col);
    auto cell = [&](int value, int previous) {
        QString t = value >= 0 ? QString::number(value) : tr("–");
        QColor colour = Theme::ratingColor(int(PostMeetPage::ratingFor(value)));
        if (value >= 0 && previous >= 0 && value != previous)
            t += value > previous ? QStringLiteral("  ▲") : QStringLiteral("  ▼");
        QLabel *l = Ui::label(t);
        l->setStyleSheet(QStringLiteral("color: %1; font-weight: 600;").arg(Theme::css(value >= 0 ? colour : c.textSecondary)));
        return l;
    };
    for (int i = 0; i < attempts.size(); ++i) {
        const Attempt &a = attempts[i];
        const Attempt prev = i > 0 ? attempts[i - 1] : Attempt();
        const int r = i + 1;
        grid->addWidget(Ui::label(QStringLiteral("#%1").arg(r)), r, 0);
        grid->addWidget(Ui::label(a.when, "secondary"), r, 1);
        grid->addWidget(Ui::label(formatDuration(a.seconds)), r, 2);
        grid->addWidget(cell(a.overall, prev.overall), r, 3);
        const QString delivery = PostMeetPage::tr("Speech delivery");
        const QString points = PostMeetPage::tr("Key points");
        const QString face = PostMeetPage::tr("Facial engagement");
        grid->addWidget(cell(a.categories.value(delivery, -1), prev.categories.value(delivery, -1)), r, 4);
        grid->addWidget(cell(a.categories.value(points, -1), prev.categories.value(points, -1)), r, 5);
        grid->addWidget(cell(a.categories.value(face, -1), prev.categories.value(face, -1)), r, 6);
    }
    card->body()->addLayout(grid);
    m_history->addWidget(card);
}

QList<PracticeView::Attempt> PracticeView::attemptsFor(const QString &section) const
{
    QList<Attempt> list;
    const QJsonObject all = QJsonDocument::fromJson(QSettings().value(QLatin1String(kAttemptsKey)).toByteArray()).object();
    for (const QJsonValue &v : all.value(section).toArray()) {
        const QJsonObject o = v.toObject();
        Attempt a;
        a.when = o.value(QLatin1String("when")).toString();
        a.seconds = o.value(QLatin1String("seconds")).toInt();
        a.overall = o.value(QLatin1String("overall")).toInt(-1);
        const QJsonObject cats = o.value(QLatin1String("categories")).toObject();
        for (auto it = cats.begin(); it != cats.end(); ++it)
            a.categories.insert(it.key(), it.value().toInt());
        list << a;
    }
    return list;
}

void PracticeView::saveAttempt(const QString &section, const Attempt &a)
{
    QSettings settings;
    QJsonObject all = QJsonDocument::fromJson(settings.value(QLatin1String(kAttemptsKey)).toByteArray()).object();
    QJsonArray list = all.value(section).toArray();
    QJsonObject cats;
    for (auto it = a.categories.cbegin(); it != a.categories.cend(); ++it)
        cats.insert(it.key(), it.value());
    list.append(QJsonObject{{QStringLiteral("when"), a.when},
                            {QStringLiteral("seconds"), a.seconds},
                            {QStringLiteral("overall"), a.overall},
                            {QStringLiteral("categories"), cats}});
    while (list.size() > 12)
        list.removeFirst();
    all.insert(section, list);
    settings.setValue(QLatin1String(kAttemptsKey), QJsonDocument(all).toJson(QJsonDocument::Compact));
}
