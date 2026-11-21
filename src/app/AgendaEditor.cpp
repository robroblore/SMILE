#include "app/AgendaEditor.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "ui/Icons.h"
#include "ui/Theme.h"

namespace {

class AgendaRow : public QWidget
{
public:
    explicit AgendaRow(QWidget *parent)
        : QWidget(parent)
        , number(new QLabel(this))
        , title(new QLineEdit(this))
        , minutes(new QDoubleSpinBox(this))
        , remove(new QPushButton(this))
    {
        number->setFixedWidth(22);
        number->setAlignment(Qt::AlignCenter);
        number->setObjectName(QStringLiteral("secondary"));

        title->setPlaceholderText(AgendaEditor::tr("What do you want to get across?"));

        minutes->setRange(0.0, 120.0);
        minutes->setSingleStep(0.5);
        minutes->setDecimals(1);
        minutes->setSuffix(AgendaEditor::tr(" min"));
        minutes->setSpecialValueText(AgendaEditor::tr("No limit"));
        minutes->setButtonSymbols(QAbstractSpinBox::NoButtons);
        minutes->setAlignment(Qt::AlignRight);
        minutes->setFixedWidth(96);
        minutes->setToolTip(AgendaEditor::tr("Target time for this point. Scroll or type to change it."));

        remove->setObjectName(QStringLiteral("subtle"));
        remove->setIcon(Icons::icon(Icons::Icon::Close, Theme::colors().textSecondary, 14));
        remove->setToolTip(AgendaEditor::tr("Remove this point"));
        remove->setCursor(Qt::PointingHandCursor);
        remove->setFixedSize(32, 32);

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        layout->addWidget(number);
        layout->addWidget(title, 1);
        layout->addWidget(minutes);
        layout->addWidget(remove);
    }

    QLabel *number;
    QLineEdit *title;
    QDoubleSpinBox *minutes;
    QPushButton *remove;
};

} // namespace

AgendaEditor::AgendaEditor(QWidget *parent)
    : QWidget(parent)
    , m_rows(new QVBoxLayout(this))
{
    m_rows->setContentsMargins(0, 0, 0, 0);
    m_rows->setSpacing(6);
}

void AgendaEditor::addPoint(const QString &title, double minutes, bool focus)
{
    auto *row = new AgendaRow(this);
    row->title->setText(title);
    row->minutes->setValue(minutes);
    m_rows->addWidget(row);

    connect(row->title, &QLineEdit::textChanged, this, &AgendaEditor::changed);
    connect(row->minutes, &QDoubleSpinBox::valueChanged, this, &AgendaEditor::changed);
    connect(row->remove, &QPushButton::clicked, this, [this, row] {
        row->hide();
        row->deleteLater();
        // Wait until the row is gone before reporting the new agenda
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
    emit changed();
}

Agenda AgendaEditor::agenda() const
{
    Agenda result;
    for (int i = 0; i < m_rows->count(); ++i) {
        auto *row = static_cast<AgendaRow *>(m_rows->itemAt(i)->widget());
        if (!row || row->isHidden())
            continue;
        const QString title = row->title->text().trimmed();
        if (title.isEmpty())
            continue;
        result.append({title, qRound(row->minutes->value() * 60.0), 0, false});
    }
    return result;
}

void AgendaEditor::renumber()
{
    int n = 1;
    for (int i = 0; i < m_rows->count(); ++i) {
        auto *row = static_cast<AgendaRow *>(m_rows->itemAt(i)->widget());
        if (row && !row->isHidden())
            row->number->setText(QString::number(n++));
    }
}
