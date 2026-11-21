#include "core/Coverage.h"

#include <QRegularExpression>

namespace Coverage {

namespace {
QString stem(QString w)
{
    for (const char *suffix : {"ing", "ed", "es", "s", "ly"}) {
        const QLatin1String s(suffix);
        if (w.size() > s.size() + 3 && w.endsWith(s))
            return w.left(w.size() - s.size());
    }
    return w;
}

const QSet<QString> &stopwords()
{
    static const QSet<QString> words = {
        QStringLiteral("about"), QStringLiteral("after"), QStringLiteral("also"), QStringLiteral("because"),
        QStringLiteral("been"), QStringLiteral("before"), QStringLiteral("being"), QStringLiteral("could"),
        QStringLiteral("does"), QStringLiteral("each"), QStringLiteral("from"), QStringLiteral("have"),
        QStringLiteral("into"), QStringLiteral("just"), QStringLiteral("like"), QStringLiteral("more"),
        QStringLiteral("most"), QStringLiteral("much"), QStringLiteral("need"), QStringLiteral("only"),
        QStringLiteral("other"), QStringLiteral("over"), QStringLiteral("should"), QStringLiteral("some"),
        QStringLiteral("such"), QStringLiteral("than"), QStringLiteral("that"), QStringLiteral("their"),
        QStringLiteral("them"), QStringLiteral("then"), QStringLiteral("there"), QStringLiteral("these"),
        QStringLiteral("they"), QStringLiteral("this"), QStringLiteral("those"), QStringLiteral("very"),
        QStringLiteral("want"), QStringLiteral("were"), QStringLiteral("what"), QStringLiteral("when"),
        QStringLiteral("where"), QStringLiteral("which"), QStringLiteral("while"), QStringLiteral("will"),
        QStringLiteral("with"), QStringLiteral("would"), QStringLiteral("your"), QStringLiteral("yours"),
        QStringLiteral("make"), QStringLiteral("made"), QStringLiteral("how"), QStringLiteral("why"),
        QStringLiteral("we're"), QStringLiteral("it's"), QStringLiteral("that's"), QStringLiteral("here"),
    };
    return words;
}
} // namespace

QSet<QString> keywords(const QString &text)
{
    static const QRegularExpression word(QStringLiteral("[\\w']+"));
    QSet<QString> result;
    auto it = word.globalMatch(text.toLower());
    while (it.hasNext()) {
        const QString w = it.next().captured(0);
        if (w.size() < 4 && !w.front().isDigit())
            continue;
        if (stopwords().contains(w))
            continue;
        result.insert(stem(w));
    }
    return result;
}

double score(const QString &keyPoint, const QSet<QString> &spokenKeywords)
{
    const QSet<QString> wanted = keywords(keyPoint);
    if (wanted.isEmpty())
        return 1.0;
    int found = 0;
    for (const QString &w : wanted)
        found += spokenKeywords.contains(w) ? 1 : 0;
    return double(found) / wanted.size();
}

} // namespace Coverage
