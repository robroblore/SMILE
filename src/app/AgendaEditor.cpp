#include "app/AgendaEditor.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTimer>
#include <QVBoxLayout>

#include "core/Plural.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

namespace {

QPushButton *iconButton(Icons::Icon icon, const QString &tip, QWidget *parent)
{
    auto *b = new QPushButton(parent);
    b->setObjectName(QStringLiteral("subtle"));
    b->setIcon(Icons::icon(icon, Theme::colors().textSecondary, 14));
    b->setToolTip(tip);
    b->setCursor(Qt::PointingHandCursor);
    b->setFixedSize(30, 30);
    return b;
}

class SectionRow : public QFrame
{
public:
    explicit SectionRow(QWidget *parent)
        : QFrame(parent)
        , number(Ui::label(QString(), "secondary", this))
        , title(new QLineEdit(this))
        , minutes(new DurationEdit(this))
        , expand(iconButton(Icons::Icon::ChevronRight, AgendaEditor::tr("Show purpose and key points"), this))
        , up(iconButton(Icons::Icon::ArrowUp, AgendaEditor::tr("Move up"), this))
        , down(iconButton(Icons::Icon::ArrowDown, AgendaEditor::tr("Move down"), this))
        , remove(iconButton(Icons::Icon::Close, AgendaEditor::tr("Remove this section"), this))
        , details(new QWidget(this))
        , summary(new QLineEdit(details))
        , notes(new QPlainTextEdit(details))
        , keyPoints(new QPlainTextEdit(details))
        , count(Ui::label(QString(), "secondary", this))
    {
        number->setFixedWidth(20);
        number->setAlignment(Qt::AlignCenter);
        title->setPlaceholderText(AgendaEditor::tr("Section heading"));
        QFont f = title->font();
        f.setWeight(QFont::DemiBold);
        title->setFont(f);

        minutes->setAllowZero(true);
        minutes->setFixedWidth(minutes->sizeHint().width());
        minutes->setToolTip(AgendaEditor::tr("Target time for this section: 2, 1:30, 90s… (0 for no limit)."));

        summary->setPlaceholderText(AgendaEditor::tr("Purpose: what this section must achieve"));
        keyPoints->setPlaceholderText(AgendaEditor::tr("Key points to get across, one per line.\n"
                                                       "SMILE checks that you actually say them."));
        keyPoints->setFixedHeight(92);
        notes->setPlaceholderText(AgendaEditor::tr("Your notes or words for this section (shown when you practise it)"));
        notes->setFixedHeight(80);
        notes->setTabChangesFocus(true);
        keyPoints->setTabChangesFocus(true);

        auto *head = new QHBoxLayout;
        head->setSpacing(6);
        head->addWidget(expand);
        head->addWidget(number);
        head->addWidget(title, 1);
        head->addWidget(count);
        head->addWidget(minutes);
        head->addWidget(up);
        head->addWidget(down);
        head->addWidget(remove);

        auto *d = new QVBoxLayout(details);
        d->setContentsMargins(64, 2, 104, 8);
        d->setSpacing(6);
        d->addWidget(summary);
        d->addWidget(notes);
        d->addWidget(keyPoints);
        details->hide();

        auto *v = new QVBoxLayout(this);
        v->setContentsMargins(0, 2, 0, 2);
        v->setSpacing(4);
        v->addLayout(head);
        v->addWidget(details);

        QObject::connect(expand, &QPushButton::clicked, this, [this] { setExpanded(details->isHidden()); });
        QObject::connect(keyPoints, &QPlainTextEdit::textChanged, this, [this] { updateCount(); });
    }

    void setExpanded(bool on)
    {
        details->setVisible(on);
        expand->setIcon(Icons::icon(on ? Icons::Icon::ChevronDown : Icons::Icon::ChevronRight,
                                    Theme::colors().textSecondary, 14));
    }

    QStringList points() const
    {
        QStringList list;
        for (const QString &line : keyPoints->toPlainText().split(QLatin1Char('\n'))) {
            QString t = line.trimmed();
            t.remove(QRegularExpression(QStringLiteral("^[•\\-*]\\s*")));
            if (!t.isEmpty())
                list << t;
        }
        return list;
    }

    void updateCount()
    {
        const int n = int(points().size());
        count->setText(n ? plural(n, AgendaEditor::tr("%1 key point"), AgendaEditor::tr("%1 key points")) : QString());
    }

    QLabel *number;
    QLineEdit *title;
    DurationEdit *minutes;
    QPushButton *expand, *up, *down, *remove;
    QWidget *details;
    QLineEdit *summary;
    QPlainTextEdit *notes;
    QPlainTextEdit *keyPoints;
    QLabel *count;
};

} // namespace

AgendaEditor::AgendaEditor(QWidget *parent)
    : QWidget(parent)
    , m_rows(new QVBoxLayout(this))
{
    m_rows->setContentsMargins(0, 0, 0, 0);
    m_rows->setSpacing(4);
}

void AgendaEditor::addPoint(const QString &title, double minutes, bool focus)
{
    AgendaPoint p;
    p.title = title;
    p.targetSeconds = qRound(minutes * 60);
    addSection(p, focus, focus);
    emit changed();
}

void AgendaEditor::addSection(const AgendaPoint &point, bool focus, bool expanded)
{
    auto *row = new SectionRow(this);
    row->title->setText(point.title);
    row->minutes->setSeconds(point.targetSeconds);
    row->summary->setText(point.summary);
    row->notes->setPlainText(point.sourceText);
    row->keyPoints->setPlainText(point.keyPoints.join(QLatin1Char('\n')));
    row->updateCount();
    row->setExpanded(expanded);
    m_rows->addWidget(row);

    connect(row->title, &QLineEdit::textChanged, this, &AgendaEditor::changed);
    connect(row->minutes, &DurationEdit::secondsChanged, this, &AgendaEditor::changed);
    connect(row->summary, &QLineEdit::textChanged, this, &AgendaEditor::changed);
    connect(row->keyPoints, &QPlainTextEdit::textChanged, this, &AgendaEditor::changed);
    connect(row->notes, &QPlainTextEdit::textChanged, this, &AgendaEditor::changed);
    connect(row->up, &QPushButton::clicked, this, [this, row] { move(row, -1); });
    connect(row->down, &QPushButton::clicked, this, [this, row] { move(row, +1); });
    connect(row->remove, &QPushButton::clicked, this, [this, row] {
        row->hide();
        row->deleteLater();
        QTimer::singleShot(0, this, [this] {
            renumber();
            emit changed();
        });
    });

    renumber();
    if (focus) {
        row->title->setFocus();
        row->title->selectAll();
    }
}

void AgendaEditor::setAgenda(const Agenda &agenda)
{
    while (QLayoutItem *item = m_rows->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    for (const AgendaPoint &p : agenda)
        addSection(p, false, false);
    emit changed();
}

void AgendaEditor::move(QWidget *row, int delta)
{
    const int from = m_rows->indexOf(row);
    const int to = from + delta;
    if (from < 0 || to < 0 || to >= m_rows->count())
        return;
    m_rows->removeWidget(row);
    m_rows->insertWidget(to, row);
    renumber();
    emit changed();
}

Agenda AgendaEditor::agenda() const
{
    Agenda result;
    for (int i = 0; i < m_rows->count(); ++i) {
        auto *row = static_cast<SectionRow *>(m_rows->itemAt(i)->widget());
        if (!row || row->isHidden())
            continue;
        AgendaPoint p;
        p.title = row->title->text().trimmed();
        if (p.title.isEmpty())
            continue;
        p.targetSeconds = row->minutes->seconds();
        p.summary = row->summary->text().trimmed();
        p.sourceText = row->notes->toPlainText().trimmed();
        p.keyPoints = row->points();
        result.append(p);
    }
    return result;
}

void AgendaEditor::renumber()
{
    int n = 1;
    const int total = m_rows->count();
    for (int i = 0; i < total; ++i) {
        auto *row = static_cast<SectionRow *>(m_rows->itemAt(i)->widget());
        if (!row || row->isHidden())
            continue;
        row->number->setText(QString::number(n++));
        row->up->setEnabled(i > 0);
        row->down->setEnabled(i < total - 1);
    }
}
