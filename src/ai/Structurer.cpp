#include "ai/Structurer.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <algorithm>

#include "ai/AiClient.h"

namespace Structurer {

namespace {

QStringList sentences(const QString &text)
{
    static const QRegularExpression split(QStringLiteral("(?<=[.!?])\\s+"));
    QStringList result;
    for (const QString &s : text.split(split, Qt::SkipEmptyParts)) {
        const QString t = s.trimmed();
        if (!t.isEmpty())
            result << t;
    }
    return result;
}

QString shorten(const QString &text, int words)
{
    QStringList w = text.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    const bool cut = w.size() > words;
    w = w.mid(0, words);
    QString s = w.join(QLatin1Char(' '));
    s.remove(QRegularExpression(QStringLiteral("[.,;:!?]+$")));
    return cut ? s + QStringLiteral("…") : s;
}

bool isHeading(const QString &line)
{
    const QString t = line.trimmed();
    return t.startsWith(QLatin1Char('#')) || (t.endsWith(QLatin1Char(':')) && t.size() < 70);
}

QString stripBullet(const QString &line)
{
    static const QRegularExpression bullet(QStringLiteral("^\\s*(?:[•\\-*–·]|\\d+[.)])\\s*"));
    QString t = line;
    t.remove(bullet);
    return t.trimmed();
}

int wordCount(const QString &text)
{
    return int(text.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).size());
}

void distributeMinutes(Agenda &sections, const QList<int> &weights, int targetMinutes)
{
    int total = 0;
    for (int w : weights)
        total += qMax(1, w);
    for (int i = 0; i < sections.size(); ++i) {
        const double share = double(qMax(1, weights.value(i))) / qMax(1, total);
        // round to half minutes, at least 30 s
        const int seconds = qMax(30, int(qRound(share * targetMinutes * 2.0)) * 30);
        sections[i].targetSeconds = seconds;
    }
}

} // namespace

QString toBullets(const QString &text)
{
    QStringList out;
    for (const QString &para : text.split(QRegularExpression(QStringLiteral("\\n\\s*\\n")), Qt::SkipEmptyParts)) {
        for (const QString &line : para.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            if (isHeading(line)) {
                out << line.trimmed();
                continue;
            }
            for (const QString &s : sentences(stripBullet(line)))
                out << QStringLiteral("• ") + s;
        }
        out << QString();
    }
    while (!out.isEmpty() && out.last().isEmpty())
        out.removeLast();
    return out.join(QLatin1Char('\n'));
}

QString toText(const QString &bullets)
{
    QStringList paragraphs;
    QStringList current;
    for (const QString &line : bullets.split(QLatin1Char('\n'))) {
        const QString t = line.trimmed();
        if (t.isEmpty()) {
            if (!current.isEmpty())
                paragraphs << current.join(QLatin1Char(' '));
            current.clear();
        } else if (isHeading(t)) {
            if (!current.isEmpty())
                paragraphs << current.join(QLatin1Char(' '));
            current.clear();
            paragraphs << t;
        } else {
            QString s = stripBullet(t);
            if (!s.isEmpty() && !QStringLiteral(".!?").contains(s.back()))
                s += QLatin1Char('.');
            current << s;
        }
    }
    if (!current.isEmpty())
        paragraphs << current.join(QLatin1Char(' '));
    return paragraphs.join(QStringLiteral("\n\n"));
}

Result locally(const QString &text, int targetMinutes)
{
    Result result;
    result.presentationType = QCoreApplication::translate("Structurer", "Presentation");

    // Group lines under headings, or treat each paragraph as a section.
    struct Block {
        QString heading;
        QString body;
    };
    // A heading starts a section that runs until the next heading; without
    // headings, each paragraph is a section.
    QList<Block> blocks;
    bool underHeading = false;
    for (const QString &para : text.split(QRegularExpression(QStringLiteral("\\n\\s*\\n")), Qt::SkipEmptyParts)) {
        bool paragraphStarted = false;
        for (const QString &line : para.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            if (isHeading(line)) {
                QString h = line.trimmed();
                h.remove(QRegularExpression(QStringLiteral("^#+\\s*|:$")));
                blocks.append({h, QString()});
                underHeading = true;
                continue;
            }
            if (blocks.isEmpty() || (!underHeading && !paragraphStarted))
                blocks.append({QString(), QString()});
            paragraphStarted = true;
            blocks.last().body += QLatin1Char(' ') + stripBullet(line);
        }
    }
    blocks.erase(std::remove_if(blocks.begin(), blocks.end(),
                                [](const Block &b) { return b.heading.isEmpty() && b.body.trimmed().isEmpty(); }),
                 blocks.end());

    // One long paragraph: split it into chunks of ~4 sentences
    if (blocks.size() == 1 && blocks.first().heading.isEmpty()) {
        const QStringList all = sentences(blocks.first().body);
        blocks.clear();
        for (int i = 0; i < all.size(); i += 4)
            blocks.append({QString(), all.mid(i, 4).join(QLatin1Char(' '))});
    }

    QList<int> weights;
    for (const Block &b : std::as_const(blocks)) {
        const QStringList s = sentences(b.body);
        AgendaPoint p;
        p.title = b.heading.isEmpty() ? shorten(s.value(0), 6) : b.heading;
        p.summary = s.value(0);
        p.sourceText = b.body.trimmed();
        for (int i = 0; i < qMin(3, int(s.size())); ++i)
            p.keyPoints << shorten(s[i], 12);
        result.sections << p;
        weights << wordCount(b.body);
    }
    distributeMinutes(result.sections, weights, targetMinutes);
    return result;
}

void withAi(AiClient *ai, const QString &text, int targetMinutes, std::function<void(const Result &)> done)
{
    const QString system = QStringLiteral(
        "You help professionals prepare high-stakes talks: presentations, job interviews, pitches and meetings. "
        "Turn the user's draft (a speech, slides text or rough notes) into a clear structure for delivering it.\n"
        "Reply with ONLY a JSON object:\n"
        "{\"presentation_type\": \"short label, e.g. Business proposal, Job interview answer, Sales pitch, Project update, Lecture\",\n"
        " \"sections\": [{\"title\": \"heading that fits this content and type of talk, max 6 words\",\n"
        "                \"summary\": \"one sentence: what this section must achieve\",\n"
        "                \"text\": \"the speaker's own words for this section, copied from the draft (verbatim)\",\n"
        "                \"key_points\": [\"2-4 concrete points the speaker must get across, short, in their words\"],\n"
        "                \"minutes\": number}]}\n"
        "Rules: 3-7 sections in delivery order; headings adapt to the type of talk instead of generic labels; "
        "minutes add up to about %1; keep the speaker's own content and facts, never invent new claims.")
                               .arg(targetMinutes);
    ai->chat(system, text.left(20000), 6000, [done, text, targetMinutes](const QString &reply, const QString &error) {
        if (!error.isEmpty()) {
            Result r = locally(text, targetMinutes);
            r.error = error;
            done(r);
            return;
        }
        const QJsonObject json = AiClient::extractJson(reply);
        Result r;
        r.presentationType = json.value(QLatin1String("presentation_type")).toString();
        for (const QJsonValue &v : json.value(QLatin1String("sections")).toArray()) {
            const QJsonObject o = v.toObject();
            AgendaPoint p;
            p.title = o.value(QLatin1String("title")).toString().trimmed();
            p.summary = o.value(QLatin1String("summary")).toString().trimmed();
            p.sourceText = o.value(QLatin1String("text")).toString().trimmed();
            for (const QJsonValue &k : o.value(QLatin1String("key_points")).toArray())
                if (!k.toString().trimmed().isEmpty())
                    p.keyPoints << k.toString().trimmed();
            const double minutes = o.value(QLatin1String("minutes")).toDouble(1.0);
            p.targetSeconds = qMax(30, int(qRound(minutes * 2.0)) * 30);
            if (!p.title.isEmpty())
                r.sections << p;
        }
        if (r.sections.isEmpty()) {
            r = locally(text, targetMinutes);
            r.error = QCoreApplication::translate("Structurer", "The AI's answer couldn't be read, so SMILE split it locally.");
        }
        done(r);
    });
}

} // namespace Structurer
