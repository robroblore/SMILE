#include "app/pages/TrainingPage.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "app/SessionRunWidget.h"
#include "core/SessionController.h"
#include "core/TimeFormat.h"

namespace {
QLabel *makeTitle(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    QFont f = label->font();
    f.setPointSizeF(f.pointSizeF() * 1.8);
    f.setBold(true);
    label->setFont(f);
    return label;
}
} // namespace

TrainingPage::TrainingPage(SessionController *controller, QWidget *parent)
    : QWidget(parent)
    , m_controller(controller)
    , m_table(new QTableWidget(0, 2, this))
    , m_total(new QLabel(this))
    , m_add(new QPushButton(tr("Add point"), this))
    , m_remove(new QPushButton(tr("Remove"), this))
    , m_start(new QPushButton(tr("Start rehearsal"), this))
    , m_run(new SessionRunWidget(controller, this))
{
    auto *intro = new QLabel(tr("List the points you want to get across and how long each should take. "
                                "This agenda is also used during the meeting."),
                             this);
    intro->setWordWrap(true);

    m_table->setHorizontalHeaderLabels({tr("Point to get across"), tr("Target (min)")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);

    m_start->setDefault(true);
    m_start->setMinimumHeight(36);

    auto *tableButtons = new QHBoxLayout;
    tableButtons->addWidget(m_add);
    tableButtons->addWidget(m_remove);
    tableButtons->addStretch();
    tableButtons->addWidget(m_total);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->addWidget(makeTitle(tr("Training"), this));
    layout->addWidget(intro);
    layout->addSpacing(8);
    layout->addWidget(m_table, 1);
    layout->addLayout(tableButtons);
    layout->addSpacing(8);
    layout->addWidget(m_start, 0, Qt::AlignLeft);
    layout->addWidget(m_run);

    connect(m_add, &QPushButton::clicked, this, [this] {
        addRow(tr("New point"), 2.0);
        m_table->editItem(m_table->item(m_table->rowCount() - 1, 0));
    });
    connect(m_remove, &QPushButton::clicked, this, &TrainingPage::removeSelectedRows);
    connect(m_table, &QTableWidget::itemChanged, this, &TrainingPage::commitAgenda);
    connect(m_start, &QPushButton::clicked, this, [this] {
        commitAgenda();
        m_controller->start(SessionMode::Training);
    });
    connect(m_controller, &SessionController::runningChanged, this, &TrainingPage::onRunningChanged);

    // A sample agenda so the app is usable straight away.
    addRow(tr("Introduce yourself and your role"), 1.0);
    addRow(tr("The problem we are solving"), 2.0);
    addRow(tr("Our proposal and its benefits"), 3.0);
    addRow(tr("Questions and next steps"), 2.0);
    commitAgenda();

    onRunningChanged(false);
}

void TrainingPage::addRow(const QString &title, double minutes)
{
    const QSignalBlocker blocker(m_table);
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, 0, new QTableWidgetItem(title));

    auto *spin = new QDoubleSpinBox(m_table);
    spin->setRange(0.0, 120.0);
    spin->setSingleStep(0.5);
    spin->setDecimals(1);
    spin->setSpecialValueText(tr("No limit"));
    spin->setValue(minutes);
    connect(spin, &QDoubleSpinBox::valueChanged, this, &TrainingPage::commitAgenda);
    m_table->setCellWidget(row, 1, spin);

    commitAgenda();
}

void TrainingPage::removeSelectedRows()
{
    QList<int> rows;
    for (const QModelIndex &index : m_table->selectionModel()->selectedRows())
        rows.append(index.row());
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int row : rows)
        m_table->removeRow(row);
    commitAgenda();
}

Agenda TrainingPage::readAgenda() const
{
    Agenda agenda;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QTableWidgetItem *item = m_table->item(row, 0);
        const QString title = item ? item->text().trimmed() : QString();
        if (title.isEmpty())
            continue;
        const auto *spin = qobject_cast<QDoubleSpinBox *>(m_table->cellWidget(row, 1));
        const int seconds = spin ? qRound(spin->value() * 60.0) : 0;
        agenda.append({title, seconds, 0, false});
    }
    return agenda;
}

void TrainingPage::commitAgenda()
{
    m_controller->setAgenda(readAgenda());
    updateTotal();
}

void TrainingPage::updateTotal()
{
    int total = 0;
    for (const AgendaPoint &point : m_controller->agenda())
        total += point.targetSeconds;
    m_total->setText(tr("%n point(s), ", nullptr, int(m_controller->agenda().size()))
                     + tr("target %1").arg(formatDuration(total)));
}

void TrainingPage::onRunningChanged(bool running)
{
    const bool mine = running && m_controller->mode() == SessionMode::Training;
    m_run->setVisible(mine);
    m_start->setVisible(!mine);
    m_start->setEnabled(!running);
    m_table->setEnabled(!running);
    m_add->setEnabled(!running);
    m_remove->setEnabled(!running);
}
