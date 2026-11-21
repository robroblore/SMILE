#include "app/pages/PostMeetPage.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLocale>
#include <QScrollArea>
#include <QVBoxLayout>
#include <algorithm>

#include "ai/AiClient.h"
#include "core/Plural.h"
#include "core/TimeFormat.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

namespace {

int clampScore(double v)
{
    return qBound(0, int(qRound(v)), 100);
}

// Average of the parts that were measured (score >= 0), with weights
int blend(const QList<QPair<double, double>> &parts)
{
    double sum = 0, weight = 0;
    for (const auto &[score, w] : parts) {
        if (score < 0)
            continue;
        sum += score * w;
        weight += w;
    }
    return weight > 0 ? clampScore(sum / weight) : -1;
}

QLabel *bulletLine(const QString &text, const QColor &dot)
{
    QLabel *l = Ui::label(QStringLiteral("<span style='color:%1'>●</span>&nbsp; %2").arg(Theme::css(dot), text.toHtmlEscaped()));
    l->setTextFormat(Qt::RichText);
    l->setWordWrap(true);
    return l;
}

QLabel *sectionLabel(const QString &text)
{
    QLabel *l = Ui::label(text.toUpper(), "caption");
    l->setContentsMargins(0, 6, 0, 0);
    return l;
}

} // namespace

Rating PostMeetPage::ratingFor(int score)
{
    if (score < 0)
        return Rating::None;
    return score >= 75 ? Rating::Good : (score >= 50 ? Rating::NeedsWork : Rating::Poor);
}

// ---------------------------------------------------------------- scoring

Evaluation PostMeetPage::evaluate(const SessionReport &report, const QJsonObject &review)
{
    Evaluation ev;
    const QVariantMap vision = report.sources.value(QStringLiteral("vision")).toMap();
    const QVariantMap speech = report.sources.value(QStringLiteral("speech")).toMap();
    const bool heard = !speech.value(QStringLiteral("transcript")).toString().trimmed().isEmpty();
    const QJsonObject aiCategories = review.value(QLatin1String("categories")).toObject();
    const double speechMinutes = speech.value(QStringLiteral("speechSeconds")).toInt() / 60.0;
    auto aiNote = [&aiCategories](const QString &name) { return aiCategories.value(name).toString(); };

    // --- Speech delivery
    {
        ReportCategory c;
        c.name = tr("Speech delivery");
        c.measured = tr("Pace, filler words, pauses, volume and long stretches without a break.");
        const int wpm = speech.value(QStringLiteral("wordsPerMinute")).toInt();
        const double fillers = speech.value(QStringLiteral("fillersPerMinute"), -1.0).toDouble();
        const int pauses = speech.value(QStringLiteral("pausesPerMinute"), -1).toInt();
        const int quiet = speech.value(QStringLiteral("quietPct"), -1).toInt();
        const int longest = speech.value(QStringLiteral("longestMonologueSeconds")).toInt();
        double paceScore = -1, fillerScore = -1, pauseScore = -1, volumeScore = -1;
        if (wpm > 0) {
            const int off = wpm < 130 ? 130 - wpm : (wpm > 165 ? wpm - 165 : 0);
            paceScore = clampScore(100 - off * 1.4);
            c.details << tr("Pace: %1 words per minute (comfortable is 130–165).").arg(wpm);
            if (wpm > 165)
                c.recommendations << tr("Slow down: pause briefly after each key sentence.");
            else if (wpm < 120)
                c.recommendations << tr("Pick up the pace a little to sound more energetic.");
        }
        if (fillers >= 0) {
            fillerScore = clampScore(fillers <= 2 ? 100 : 100 - (fillers - 2) * 16);
            c.details << tr("Filler words: %1 per minute (%2 in total).")
                             .arg(fillers, 0, 'f', 1)
                             .arg(speech.value(QStringLiteral("fillerTotal")).toInt());
            if (fillers > 2)
                c.recommendations << tr("Replace \"um\" and \"uh\" with a silent pause; it sounds more confident.");
        }
        if (pauses >= 0) {
            pauseScore = pauses >= 5 ? 100 : 60 + pauses * 8;
            c.details << tr("Natural pauses: %1 per minute.").arg(pauses);
            if (pauses < 5)
                c.recommendations << tr("Use more short pauses to let important points land.");
        }
        if (quiet >= 0) {
            volumeScore = clampScore(100 - quiet * 0.8);
            if (quiet > 20) {
                c.details << tr("Quieter than your usual voice %1% of the time.").arg(quiet);
                c.recommendations << tr("Keep your volume up, especially at the end of sentences.");
            }
        }
        if (longest > 0)
            c.details << tr("Longest stretch without a pause: %1.").arg(formatDuration(longest));
        int score = blend({{paceScore, 1.2}, {fillerScore, 1.0}, {pauseScore, 0.6}, {volumeScore, 0.6}});
        if (score >= 0 && longest > 180)
            score = clampScore(score - 10);
        const QJsonObject delivery = review.value(QLatin1String("delivery")).toObject();
        if (score >= 0 && !delivery.isEmpty()) {
            const int ai = blend({{delivery.value(QLatin1String("clarity")).toDouble(-1), 1},
                                  {delivery.value(QLatin1String("confidence")).toDouble(-1), 1},
                                  {delivery.value(QLatin1String("articulation")).toDouble(-1), 1}});
            if (ai >= 0) {
                c.details << tr("AI rating: clarity %1, confidence %2, articulation %3.")
                                 .arg(delivery.value(QLatin1String("clarity")).toInt())
                                 .arg(delivery.value(QLatin1String("confidence")).toInt())
                                 .arg(delivery.value(QLatin1String("articulation")).toInt());
                score = clampScore(0.6 * score + 0.4 * ai);
            }
        }
        c.score = score;
        c.summary = !aiNote(c.name).isEmpty() ? aiNote(c.name)
                    : score < 0               ? tr("Needs the microphone (and an AI key for words per minute and fillers).")
                    : score >= 75             ? tr("Clear, well-paced delivery.")
                                              : tr("Your delivery has room to become calmer and clearer.");
        ev.categories << c;
    }

    // --- Facial engagement (no smile required: a relaxed, attentive face scores well)
    {
        ReportCategory c;
        c.name = tr("Facial engagement");
        c.measured = tr("How warm, relaxed and attentive your face looked. A calm neutral face counts as good.");
        const int expression = vision.value(QStringLiteral("expressionScore"), -1).toInt();
        const int engagement = vision.value(QStringLiteral("engagementPct"), -1).toInt();
        const int relaxed = vision.value(QStringLiteral("relaxedPct"), -1).toInt();
        const int smiling = vision.value(QStringLiteral("smilingPct"), -1).toInt();
        // Smile score from the on-device expression model: a real smile ~100, a calm
        // attentive face ~45, so neutral-but-engaged is never punished to zero
        const int smileScore = vision.value(QStringLiteral("smileScore"), -1).toInt();
        if (smileScore >= 0) {
            c.score = blend({{double(qBound(0, smileScore + 20, 100)), 0.65}, {double(engagement), 0.35}});
            c.details << tr("Warmth: %1 / 100 (a smile ≈ 100, a calm, attentive face ≈ 45).").arg(smileScore);
        } else {
            c.score = blend({{double(expression), 0.6}, {double(engagement), 0.4}});
        }
        const QVariantMap exprs = vision.value(QStringLiteral("expressions")).toMap();
        if (!exprs.isEmpty()) {
            QStringList parts;
            for (auto it = exprs.cbegin(); it != exprs.cend(); ++it)
                parts << QStringLiteral("%1 ×%2").arg(QString(it.key()).replace(QLatin1Char('_'), QLatin1Char(' '))).arg(it.value().toInt());
            c.details << tr("Expressions seen: %1.").arg(parts.join(QStringLiteral(", ")));
        }
        if (relaxed >= 0)
            c.details << tr("Relaxed and natural: %1% of the time.").arg(relaxed);
        if (smiling >= 0)
            c.details << tr("Smiling: %1% of the time.").arg(smiling);
        const int tense = exprs.value(QStringLiteral("tense")).toInt() + exprs.value(QStringLiteral("unapproachable")).toInt();
        if (tense > 0 && tense * 4 >= exprs.value(QStringLiteral("neutral_relaxed")).toInt() + 1)
            c.recommendations << tr("Before you start, drop your shoulders and relax your jaw: it softens your expression.");
        if (c.score >= 0 && c.score < 75)
            c.recommendations << tr("Smile when you greet people and when you agree; a slight smile is plenty.");
        c.summary = !aiNote(c.name).isEmpty() ? aiNote(c.name)
                    : c.score < 0            ? tr("Needs the camera and an AI key.")
                    : c.score >= 75          ? tr("You looked calm, attentive and approachable.")
                                             : tr("You sometimes looked tense or distant.");
        ev.categories << c;
    }

    // --- Eye contact (in context: reading notes briefly is fine)
    {
        ReportCategory c;
        c.name = tr("Eye contact");
        c.measured = tr("Time spent facing the camera. Brief glances and reading your notes don't count against you.");
        c.score = vision.value(QStringLiteral("eyeContactPct"), -1).toInt();
        const int glances = vision.value(QStringLiteral("notesGlances")).toInt();
        if (glances > 0)
            c.details << tr("Glanced at your notes %1 times (fine).").arg(glances);
        if (c.score >= 0 && c.score < 75)
            c.recommendations << tr("When you finish a sentence, look straight into the camera lens.");
        c.summary = !aiNote(c.name).isEmpty() ? aiNote(c.name)
                    : c.score < 0            ? tr("Needs the camera.")
                    : c.score >= 75          ? tr("Good, steady eye contact.")
                                             : tr("You often looked away for longer stretches.");
        ev.categories << c;
    }

    // --- Professional language
    {
        ReportCategory c;
        c.name = tr("Professional language");
        c.measured = tr("Swearing, slang and hedging words such as \"I think\" or \"maybe\".");
        if (heard) {
            const QVariantList flagged = speech.value(QStringLiteral("flagged")).toList();
            const QVariantMap slang = speech.value(QStringLiteral("slang")).toMap();
            const int hedges = speech.value(QStringLiteral("hedges")).toInt();
            int slangTotal = 0;
            for (const QVariant &n : slang)
                slangTotal += n.toInt();
            double score = 100 - flagged.size() * 18.0;
            if (speechMinutes > 0.3) {
                score -= qMax(0.0, slangTotal / speechMinutes - 0.5) * 12;
                score -= qMax(0.0, hedges / speechMinutes - 1.5) * 8;
            }
            c.score = clampScore(score);
            for (const QVariant &f : flagged) {
                const QVariantMap m = f.toMap();
                c.events << tr("\"%1\": …%2…").arg(m.value(QStringLiteral("word")).toString(), m.value(QStringLiteral("context")).toString());
            }
            if (slangTotal > 0)
                c.details << tr("Informal words: %1.").arg(QStringList(slang.keys()).join(QStringLiteral(", ")));
            c.details << plural(hedges, tr("%1 hedging phrase (\"I think\", \"maybe\")."), tr("%1 hedging phrases (\"I think\", \"maybe\")."));
            for (const QJsonValue &v : review.value(QLatin1String("language")).toArray()) {
                const QJsonObject o = v.toObject();
                c.recommendations << tr("Instead of \"%1\", say \"%2\". %3")
                                         .arg(o.value(QLatin1String("phrase")).toString(), o.value(QLatin1String("suggestion")).toString(),
                                              o.value(QLatin1String("why")).toString());
            }
            if (hedges / qMax(0.3, speechMinutes) > 1.5)
                c.recommendations << tr("Replace \"I think\" with direct statements when you're sure.");
        }
        c.summary = !aiNote(c.name).isEmpty() ? aiNote(c.name)
                    : c.score < 0            ? tr("Needs the microphone and an AI key.")
                    : c.score >= 75          ? tr("Professional, confident wording.")
                                             : tr("Some words could sound more professional.");
        ev.categories << c;
    }

    // --- Face & framing: visible, uncovered, steady in the frame. The most important
    // thing on a video call, so it weighs the most
    {
        ReportCategory c;
        c.name = tr("Face & framing");
        c.measured = tr("Your face visible, not covered by a hand or object, and steady in the frame.");
        const int visible = vision.value(QStringLiteral("faceVisiblePct"), -1).toInt();
        const int coveredSeconds = vision.value(QStringLiteral("coveredSeconds")).toInt();
        const int coveredPct = vision.value(QStringLiteral("coveredPct"), 0).toInt();
        const int framing = vision.value(QStringLiteral("framingScore"), -1).toInt();
        int coveredMoments = 0;
        for (const QVariant &e : vision.value(QStringLiteral("events")).toList()) {
            const QVariantMap m = e.toMap();
            if (m.value(QStringLiteral("kind")).toString() == QLatin1String("covered")) {
                ++coveredMoments;
                c.events << QStringLiteral("%1 · %2").arg(m.value(QStringLiteral("at")).toString(), m.value(QStringLiteral("detail")).toString());
            }
        }
        int score = blend({{double(visible), 0.6}, {double(framing), 0.4}});
        if (score >= 0) {
            score = clampScore(score - coveredPct * 3.0 - coveredMoments * 6.0);
            if (visible >= 0)
                c.details << tr("Face visible: %1% of the time.").arg(visible);
            if (coveredSeconds > 0)
                c.details << tr("Face covered for %1 (%2 times).").arg(formatDuration(coveredSeconds)).arg(qMax(1, coveredMoments));
            if (framing >= 0)
                c.details << tr("Steady framing: %1 / 100.").arg(framing);
            if (coveredSeconds > 0)
                c.recommendations << tr("Keep your hands away from your face; rest them on the desk.");
            if (visible >= 0 && visible < 85)
                c.recommendations << tr("Stay centred in the camera; don't lean out of the frame.");
            if (framing >= 0 && framing < 70)
                c.recommendations << tr("Sit still at one distance from the camera.");
        }
        c.score = score;
        c.summary = score < 0        ? tr("Needs the camera.")
                    : coveredSeconds ? tr("Your face was covered at times: the audience lost you.")
                    : score >= 75    ? tr("Always visible and well framed.")
                                     : tr("You were hard to see at times.");
        ev.categories << c;
    }

    // --- Body language
    {
        ReportCategory c;
        c.name = tr("Body language");
        c.measured = tr("Posture, restless movement, hand gestures and visible discomfort.");
        const int upright = vision.value(QStringLiteral("uprightPct"), -1).toInt();
        const int restless = vision.value(QStringLiteral("restlessPct"), -1).toInt();
        const QVariantMap hands = vision.value(QStringLiteral("hands")).toMap();
        int handChecks = 0, badHands = 0;
        for (auto it = hands.cbegin(); it != hands.cend(); ++it) {
            if (it.key() == QLatin1String("not_visible"))
                continue;
            handChecks += it.value().toInt();
            if (it.key() == QLatin1String("distracting") || it.key() == QLatin1String("inappropriate"))
                badHands += it.value().toInt();
        }
        const double handScore = handChecks ? 100.0 - 100.0 * badHands / handChecks : -1;
        const int discomfort = vision.value(QStringLiteral("discomfortChecks")).toInt();
        const int checks = vision.value(QStringLiteral("snapshots")).toInt();
        const double comfortScore = checks ? 100.0 - 100.0 * discomfort / checks : -1;
        c.score = blend({{double(upright), 0.5},
                         {restless >= 0 ? 100.0 - restless * 2.0 : -1.0, 0.25},
                         {handScore, 0.15},
                         {comfortScore, 0.1}});
        if (upright >= 0)
            c.details << tr("Sat upright in %1% of checks.").arg(upright);
        if (restless > 0)
            c.details << tr("Restless movement %1% of the time.").arg(restless);
        if (badHands > 0)
            c.details << tr("Distracting hand gestures in %1 of %2 checks where your hands were visible.").arg(badHands).arg(handChecks);
        int movementMoments = 0;
        for (const QVariant &e : vision.value(QStringLiteral("events")).toList()) {
            const QVariantMap m = e.toMap();
            const QString kind = m.value(QStringLiteral("kind")).toString();
            if (kind == QLatin1String("covered") || kind == QLatin1String("expression"))
                continue;
            if (kind == QLatin1String("movement") || kind == QLatin1String("gesture") || kind == QLatin1String("hands"))
                ++movementMoments;
            c.events << QStringLiteral("%1 · %2").arg(m.value(QStringLiteral("at")).toString(), m.value(QStringLiteral("detail")).toString());
        }
        // Disturbing movement is what an audience notices most: each moment costs
        if (c.score >= 0)
            c.score = clampScore(c.score - movementMoments * 4.0);
        if (upright >= 0 && upright < 75)
            c.recommendations << tr("Sit back in your chair with both feet on the floor; it stops you leaning in.");
        if (badHands > 0)
            c.recommendations << tr("Rest your hands on the desk between gestures.");
        c.summary = !aiNote(c.name).isEmpty() ? aiNote(c.name)
                    : c.score < 0            ? tr("Needs the camera and an AI key.")
                    : c.score >= 75          ? tr("Composed and confident.")
                                             : tr("Your posture or movement drew attention at times.");
        ev.categories << c;
    }

    // --- Key points
    {
        ReportCategory c;
        c.name = tr("Key points");
        c.measured = tr("Whether you actually said the points you planned in each section.");
        int total = 0, covered = 0;
        for (const AgendaPoint &p : report.points) {
            for (int k = 0; k < p.keyPoints.size(); ++k) {
                ++total;
                const bool ok = p.keyPointCovered.value(k);
                covered += ok ? 1 : 0;
                c.details << QStringLiteral("%1  %2 — %3").arg(ok ? QStringLiteral("✓") : QStringLiteral("✗"), p.title, p.keyPoints[k]);
                if (!ok)
                    c.recommendations << tr("Make sure to say: \"%1\".").arg(p.keyPoints[k]);
            }
        }
        if (total > 0 && heard)
            c.score = clampScore(100.0 * covered / total);
        if (report.offAgendaNudges > 0)
            c.details << tr("SMILE brought you back on topic %1×.").arg(report.offAgendaNudges);
        c.summary = total == 0 ? tr("No key points set. Add them to your sections on the Training page.")
                    : !heard   ? tr("Needs the microphone and an AI key to hear what you said.")
                               : tr("You got %1 of %2 key points across.").arg(covered).arg(total);
        while (c.recommendations.size() > 4)
            c.recommendations.removeLast();
        ev.categories << c;
    }

    // --- Timing
    {
        ReportCategory c;
        c.name = tr("Timing");
        c.measured = tr("Sections finished within their target time (10% leeway).");
        int timed = 0, onTime = 0, unreached = 0, target = 0;
        for (const AgendaPoint &p : report.points) {
            target += p.targetSeconds;
            QString line;
            if (!p.reached) {
                ++unreached;
                line = tr("%1: not reached").arg(p.title);
                c.recommendations << tr("Leave time for \"%1\" by tightening earlier sections.").arg(p.title);
            } else if (p.targetSeconds > 0) {
                ++timed;
                const bool ok = p.actualSeconds <= p.targetSeconds * 1.1;
                onTime += ok ? 1 : 0;
                line = tr("%1: %2 of %3").arg(p.title, formatDuration(p.actualSeconds), formatDuration(p.targetSeconds));
                if (!ok)
                    c.recommendations << tr("Trim \"%1\" by about %2.").arg(p.title, formatDuration(p.actualSeconds - p.targetSeconds));
            } else {
                line = tr("%1: %2").arg(p.title, formatDuration(p.actualSeconds));
            }
            c.details << line;
        }
        if (timed + unreached > 0)
            c.score = clampScore(100.0 * onTime / (timed + unreached) - unreached * 10.0);
        c.summary = timed + unreached == 0
                        ? tr("No target times set.")
                        : tr("Total %1 against a plan of %2.").arg(formatDuration(report.totalSeconds), formatDuration(target));
        ev.categories << c;
    }

    for (ReportCategory &c : ev.categories)
        c.rating = ratingFor(c.score);

    // --- Overall
    // How you come across (face, body, expression, eyes) outweighs timing and key points
    const QHash<QString, double> weights = {
        {tr("Face & framing"), 2.0},    {tr("Body language"), 2.0},         {tr("Facial engagement"), 1.5},
        {tr("Eye contact"), 1.5},       {tr("Speech delivery"), 1.25},      {tr("Professional language"), 1.0},
        {tr("Key points"), 0.6},        {tr("Timing"), 0.5},
    };
    QList<QPair<double, double>> parts;
    int faceScore = -1, bodyScore = -1;
    for (const ReportCategory &c : ev.categories) {
        parts << qMakePair(double(c.score), weights.value(c.name, 1.0));
        if (c.name == tr("Face & framing"))
            faceScore = c.score;
        if (c.name == tr("Body language"))
            bodyScore = c.score;
    }
    ev.overall = blend(parts);
    // Show what matters most first
    std::stable_sort(ev.categories.begin(), ev.categories.end(), [&weights](const ReportCategory &a, const ReportCategory &b) {
        return weights.value(a.name, 1.0) > weights.value(b.name, 1.0);
    });
    // A hidden face or a restless body can't be made up for by good timing
    if (ev.overall >= 0) {
        if (faceScore >= 0 && faceScore < 60)
            ev.overall = qMin(ev.overall, faceScore + 15);
        if (bodyScore >= 0 && bodyScore < 50)
            ev.overall = qMin(ev.overall, bodyScore + 20);
    }
    // Training is graded harder than a real meeting
    const bool strict = report.mode != SessionMode::Meeting;
    const int good = strict ? 82 : 75, ok = strict ? 60 : 50, great = strict ? 90 : 85;
    ev.rating = ev.overall < 0 ? Rating::None : ev.overall >= good ? Rating::Good : ev.overall >= ok ? Rating::NeedsWork : Rating::Poor;
    ev.headline = ev.overall < 0        ? tr("Not enough data")
                  : ev.overall >= great ? tr("Excellent!")
                  : ev.overall >= good  ? tr("Good job")
                  : ev.overall >= ok    ? tr("Getting there")
                                        : tr("Keep practising");
    ev.assessment = review.value(QLatin1String("summary")).toString();
    if (ev.assessment.isEmpty())
        ev.assessment = ev.overall >= great  ? tr("You communicated clearly and confidently.")
                        : ev.overall >= good ? tr("You communicated effectively, with room to improve.")
                        : ev.overall >= ok   ? tr("A solid base; a few habits are holding you back.")
                                           : tr("Focus on the improvements below and rehearse again.");

    // Most important improvements: the AI's list, else the weakest categories' advice
    for (const QJsonValue &v : review.value(QLatin1String("improvements")).toArray())
        ev.improvements << v.toString();
    if (ev.improvements.isEmpty()) {
        QList<ReportCategory> sorted = ev.categories;
        std::sort(sorted.begin(), sorted.end(), [](const ReportCategory &a, const ReportCategory &b) {
            return (a.score < 0 ? 999 : a.score) < (b.score < 0 ? 999 : b.score);
        });
        for (const ReportCategory &c : sorted)
            if (c.score >= 0 && c.score < good && !c.recommendations.isEmpty())
                ev.improvements << c.recommendations.first();
    }
    while (ev.improvements.size() > 3)
        ev.improvements.removeLast();
    return ev;
}

// ------------------------------------------------------------------- page

PostMeetPage::PostMeetPage(AiClient *ai, QWidget *parent)
    : QWidget(parent)
    , m_ai(ai)
    , m_empty(new Card)
    , m_reportWidget(new QWidget)
    , m_reportLayout(new QVBoxLayout(m_reportWidget))
{
    auto *emptyCard = static_cast<Card *>(m_empty);
    emptyCard->body()->setContentsMargins(24, 40, 24, 40);
    emptyCard->body()->addWidget(new SmileyBadge(64, 0.5), 0, Qt::AlignHCenter);
    emptyCard->body()->addSpacing(8);
    QLabel *emptyTitle = Ui::label(tr("No results yet"), "cardTitle");
    emptyTitle->setAlignment(Qt::AlignHCenter);
    emptyCard->body()->addWidget(emptyTitle);
    QLabel *emptyText = Ui::label(tr("Finish a rehearsal or a meeting and your results will appear here."), "secondary");
    emptyText->setAlignment(Qt::AlignHCenter);
    emptyText->setWordWrap(true);
    emptyCard->body()->addWidget(emptyText);

    m_reportLayout->setContentsMargins(0, 0, 0, 0);
    m_reportLayout->setSpacing(12);
    m_reportWidget->hide();

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(36, 28, 36, 28);
    layout->setSpacing(12);
    layout->addWidget(Ui::pageHeader(tr("Results"), QString()));
    layout->addWidget(m_empty);
    layout->addWidget(m_reportWidget);
    layout->addStretch();

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(Ui::scrollPage(content));
}

void PostMeetPage::showReport(const SessionReport &report)
{
    m_report = report;
    m_review = QJsonObject();
    m_reviewError.clear();
    m_coverageFromAi = false;
    m_reviewPending = m_ai->isConfigured();
    ++m_reportId;
    rebuild();
    if (m_ai->isConfigured()) {
        requestCoverageJudgement();
        requestReview();
    }
}

void PostMeetPage::rebuild()
{
    while (QLayoutItem *item = m_reportLayout->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    const Theme::Colors &c = Theme::colors();
    const Evaluation ev = evaluate(m_report, m_review);
    const QColor overallColour = Theme::ratingColor(int(ev.rating));

    // ---- Hero: the overall score dominates
    auto *hero = new Card;
    hero->body()->setContentsMargins(28, 26, 28, 26);
    auto *heroRow = new QHBoxLayout;
    heroRow->setSpacing(28);
    heroRow->addWidget(new ScoreRing(ev.overall, overallColour, 168, tr("/ 100")));
    auto *heroText = new QVBoxLayout;
    heroText->setSpacing(6);
    const QString kind = m_report.mode == SessionMode::Training ? tr("REHEARSAL SCORE") : tr("OVERALL MEETING SCORE");
    heroText->addStretch();
    heroText->addWidget(Ui::label(QStringLiteral("%1 · %2 · %3")
                                      .arg(kind, QLocale().toString(m_report.startedAt, QLocale::ShortFormat),
                                           formatDuration(m_report.totalSeconds)),
                                  "caption"));
    QLabel *headline = Ui::label(ev.headline);
    QFont hf = headline->font();
    hf.setPointSizeF(hf.pointSizeF() * 2.0);
    hf.setWeight(QFont::Bold);
    headline->setFont(hf);
    headline->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::css(ev.rating == Rating::None ? c.text : overallColour)));
    heroText->addWidget(headline);
    QLabel *assessment = Ui::label(ev.assessment);
    QFont af = assessment->font();
    af.setPointSizeF(af.pointSizeF() * 1.12);
    assessment->setFont(af);
    assessment->setWordWrap(true);
    heroText->addWidget(assessment);
    if (m_reviewPending)
        heroText->addWidget(Ui::label(tr("The AI coach is reviewing your session…"), "secondary"));
    else if (!m_reviewError.isEmpty())
        heroText->addWidget(Ui::label(tr("AI review unavailable: %1").arg(m_reviewError), "secondary"));
    heroText->addStretch();
    heroRow->addLayout(heroText, 1);
    hero->body()->addLayout(heroRow);
    m_reportLayout->addWidget(hero);

    // ---- Most important improvements
    if (!ev.improvements.isEmpty()) {
        auto *imp = new Card(tr("Most important improvements"));
        int n = 1;
        for (const QString &text : ev.improvements) {
            QLabel *l = Ui::label(QStringLiteral("<span style='color:%1; font-weight:700'>%2</span>&nbsp;&nbsp;%3")
                                      .arg(Theme::css(c.accent))
                                      .arg(n++)
                                      .arg(text.toHtmlEscaped()));
            l->setTextFormat(Qt::RichText);
            l->setWordWrap(true);
            imp->body()->addWidget(l);
        }
        m_reportLayout->addWidget(imp);
    }

    // ---- Performance breakdown
    m_reportLayout->addWidget(sectionLabel(tr("Performance breakdown")));
    for (const ReportCategory &cat : ev.categories) {
        const QColor colour = cat.rating == Rating::None ? c.textSecondary : Theme::ratingColor(int(cat.rating));
        auto *row = new Expander(cat.name, cat.summary, cat.score, colour,
                                 cat.score >= 0 ? QStringLiteral("%1%").arg(cat.score) : tr("–"));
        QVBoxLayout *b = row->body();
        b->addWidget(Ui::divider());
        QLabel *measured = Ui::label(cat.measured, "secondary");
        measured->setWordWrap(true);
        b->addWidget(measured);
        if (!cat.details.isEmpty()) {
            b->addWidget(sectionLabel(tr("What we measured")));
            for (const QString &d : cat.details)
                b->addWidget(bulletLine(d, c.textSecondary));
        }
        if (!cat.events.isEmpty()) {
            b->addWidget(sectionLabel(tr("Moments detected")));
            for (const QString &e : cat.events.mid(0, 8))
                b->addWidget(bulletLine(e, c.orange));
        }
        if (!cat.recommendations.isEmpty()) {
            b->addWidget(sectionLabel(tr("Recommendations")));
            for (const QString &r : cat.recommendations)
                b->addWidget(bulletLine(r, c.accent));
        }
        m_reportLayout->addWidget(row);
    }

    if (!m_ai->isConfigured()) {
        QLabel *note = Ui::label(tr("Add an AI key in Settings for expression, language and key-point analysis and a written review."),
                                 "secondary");
        note->setWordWrap(true);
        m_reportLayout->addWidget(note);
    }

    m_empty->hide();
    m_reportWidget->show();
}

QString PostMeetPage::summarizeForAi(const SessionReport &report)
{
    QString s;
    s += report.mode == SessionMode::Meeting ? QStringLiteral("Session: real meeting\n") : QStringLiteral("Session: training (grade strictly)\n");
    s += QStringLiteral("Total time: %1\n\nSections (target / actual):\n").arg(formatDuration(report.totalSeconds));
    for (const AgendaPoint &p : report.points) {
        s += QStringLiteral("- %1: %2 / %3\n")
                 .arg(p.title, p.targetSeconds > 0 ? formatDuration(p.targetSeconds) : QStringLiteral("no limit"),
                      p.reached ? formatDuration(p.actualSeconds) : QStringLiteral("not reached"));
        for (int k = 0; k < p.keyPoints.size(); ++k)
            s += QStringLiteral("    key point: %1 [%2]\n")
                     .arg(p.keyPoints[k], p.keyPointCovered.value(k) ? QStringLiteral("said") : QStringLiteral("not heard"));
    }
    const QVariantMap vision = report.sources.value(QStringLiteral("vision")).toMap();
    if (!vision.isEmpty()) {
        s += QStringLiteral("\nCamera: expression score %1/100, relaxed %2%, eye contact %3% (notes glances excluded), upright %4%, "
                            "restless %5%. Expressions: %6. Hands: %7.\n")
                 .arg(vision.value(QStringLiteral("expressionScore")).toInt())
                 .arg(vision.value(QStringLiteral("relaxedPct")).toInt())
                 .arg(vision.value(QStringLiteral("eyeContactPct")).toInt())
                 .arg(vision.value(QStringLiteral("uprightPct")).toInt())
                 .arg(vision.value(QStringLiteral("restlessPct")).toInt())
                 .arg(QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(vision.value(QStringLiteral("expressions")).toMap()))
                                            .toJson(QJsonDocument::Compact)),
                      QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(vision.value(QStringLiteral("hands")).toMap()))
                                            .toJson(QJsonDocument::Compact)));
        s += QStringLiteral("Face visible %1%, face covered for %2 s, framing steadiness %3/100, warmth (smile) %4/100.\n")
                 .arg(vision.value(QStringLiteral("faceVisiblePct")).toInt())
                 .arg(vision.value(QStringLiteral("coveredSeconds")).toInt())
                 .arg(vision.value(QStringLiteral("framingScore")).toInt())
                 .arg(vision.value(QStringLiteral("smileScore")).toInt());
    }
    const QVariantMap speech = report.sources.value(QStringLiteral("speech")).toMap();
    if (!speech.isEmpty()) {
        s += QStringLiteral("Voice: %1 words/min, %2 fillers/min, %3 pauses/min, longest stretch without pausing %4, hedges %5.\n")
                 .arg(speech.value(QStringLiteral("wordsPerMinute")).toInt())
                 .arg(speech.value(QStringLiteral("fillersPerMinute")).toDouble())
                 .arg(speech.value(QStringLiteral("pausesPerMinute")).toInt())
                 .arg(formatDuration(speech.value(QStringLiteral("longestMonologueSeconds")).toInt()))
                 .arg(speech.value(QStringLiteral("hedges")).toInt());
        const QString transcript = speech.value(QStringLiteral("transcript")).toString();
        if (!transcript.isEmpty())
            s += QStringLiteral("\nTranscript (may be partial):\n\"%1\"\n").arg(transcript.left(12000));
    }
    QStringList tips;
    for (const Tip &t : report.tips)
        tips << QStringLiteral("[%1] %2").arg(categoryName(t.category), t.text);
    if (!tips.isEmpty())
        s += QStringLiteral("\nLive tips shown:\n- ") + tips.join(QStringLiteral("\n- ")) + QLatin1Char('\n');
    return s;
}

void PostMeetPage::requestReview()
{
    const int id = m_reportId;
    const QString system = QStringLiteral(
        "You are SMILE, a warm, direct communication coach for professionals in high-stakes online meetings and job "
        "interviews. Review the session using only the data given. Be realistic: a calm neutral face is fine; brief "
        "glances at notes are fine. What matters most is how they come across: a covered or hidden face and distracting "
        "movement are the biggest problems, more than timing or exact key points. Reply with ONLY JSON:\n"
        "{\"summary\": \"one encouraging, specific sentence assessing the whole performance\",\n"
        " \"improvements\": [\"the 3 most important, concrete improvements, most important first\"],\n"
        " \"delivery\": {\"clarity\": 0-100, \"confidence\": 0-100, \"articulation\": 0-100},\n"
        " \"language\": [{\"phrase\": \"exact unprofessional or weak phrase from the transcript\", \"suggestion\": \"a more "
        "professional alternative\", \"why\": \"a few words\"}],\n"
        " \"categories\": {\"Face & framing\": \"one sentence\", \"Speech delivery\": \"one sentence\", \"Facial engagement\": \"one sentence\", \"Eye contact\": "
        "\"one sentence\", \"Professional language\": \"one sentence\", \"Body language\": \"one sentence\"}}\n"
        "Only include language items that really appear in the transcript; leave the list empty if there are none. "
        "If there is no transcript, omit delivery.");
    m_ai->chat(system, summarizeForAi(m_report), 2500, [this, id](const QString &text, const QString &error) {
        if (id != m_reportId)
            return;
        m_reviewPending = false;
        if (!error.isEmpty())
            m_reviewError = error;
        else
            m_review = AiClient::extractJson(text);
        rebuild();
    });
}

void PostMeetPage::requestCoverageJudgement()
{
    const QString transcript =
        m_report.sources.value(QStringLiteral("speech")).toMap().value(QStringLiteral("transcript")).toString();
    QJsonArray sections;
    for (const AgendaPoint &p : m_report.points) {
        if (p.keyPoints.isEmpty())
            continue;
        sections.append(QJsonObject{{QStringLiteral("title"), p.title},
                                    {QStringLiteral("key_points"), QJsonArray::fromStringList(p.keyPoints)},
                                    {QStringLiteral("heard_during_section"), p.spoken.trimmed()}});
    }
    if (transcript.trimmed().isEmpty() || sections.isEmpty())
        return;

    const int id = m_reportId;
    const QString system = QStringLiteral(
        "You check whether a speaker got their key points across. For each key point decide if the speaker "
        "clearly made that point (same meaning, any wording). Use the text heard during that section first, then "
        "the full transcript. Reply with ONLY JSON: {\"sections\": [{\"title\": \"...\", \"covered\": [true|false, ...]}]} "
        "with one boolean per key point, in order.");
    const QString user = QStringLiteral("Sections:\n%1\n\nFull transcript:\n\"%2\"")
                             .arg(QString::fromUtf8(QJsonDocument(sections).toJson(QJsonDocument::Indented)), transcript.left(15000));
    m_ai->chat(system, user, 2000, [this, id](const QString &text, const QString &error) {
        if (id != m_reportId || !error.isEmpty())
            return;
        const QJsonArray judged = AiClient::extractJson(text).value(QLatin1String("sections")).toArray();
        if (judged.isEmpty())
            return;
        for (AgendaPoint &p : m_report.points) {
            for (const QJsonValue &v : judged) {
                const QJsonObject o = v.toObject();
                if (o.value(QLatin1String("title")).toString() != p.title)
                    continue;
                const QJsonArray covered = o.value(QLatin1String("covered")).toArray();
                p.keyPointCovered = QList<bool>(p.keyPoints.size(), false);
                for (int k = 0; k < p.keyPoints.size() && k < covered.size(); ++k)
                    p.keyPointCovered[k] = covered[k].toBool();
            }
        }
        m_coverageFromAi = true;
        rebuild();
    });
}
