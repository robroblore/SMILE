#include "app/SessionRunWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

#include "core/SessionController.h"
#include "core/TimeFormat.h"

SessionRunWidget::SessionRunWidget(SessionController *controller, QWidget *parent)
    : QWidget(parent)
    , m_controller(controller)
    , m_point(new QLabel(this))
    , m_time(new QLabel(this))
    , m_total(new QLabel(this))
    , m_bar(new QProgressBar(this))
    , m_next(new QPushButton(tr("Next point"), this))
    , m_finish(new QPushButton(tr("Finish"), this))
{
    QFont big = m_point->font();
    big.setPointSizeF(big.pointSizeF() * 1.35);
    big.setBold(true);
    m_point->setFont(big);
    m_point->setWordWrap(true);

    m_bar->setTextVisible(false);
    m_bar->setFixedHeight(10);

    auto *timeRow = new QHBoxLayout;
    timeRow->addWidget(m_time);
    timeRow->addStretch();
    timeRow->addWidget(m_total);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(m_next);
    buttons->addWidget(m_finish);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 8, 0, 0);
    layout->addWidget(m_point);
    layout->addWidget(m_bar);
    layout->addLayout(timeRow);
    layout->addLayout(buttons);

    connect(m_next, &QPushButton::clicked, m_controller, &SessionController::nextPoint);
    connect(m_finish, &QPushButton::clicked, m_controller, &SessionController::stop);
    connect(m_controller, &SessionController::ticked, this, &SessionRunWidget::refresh);
    connect(m_controller, &SessionController::currentPointChanged, this, &SessionRunWidget::refresh);
}

void SessionRunWidget::refresh()
{
    if (!m_controller->isRunning())
        return;

    const Agenda &agenda = m_controller->activeAgenda();
    const int index = m_controller->currentIndex();
    if (index < 0 || index >= agenda.size())
        return;

    const AgendaPoint &point = agenda.at(index);
    const int elapsed = m_controller->elapsedOnCurrent();
    const bool last = index == agenda.size() - 1;

    m_point->setText(tr("Point %1 of %2: %3").arg(index + 1).arg(agenda.size()).arg(point.title));
    m_next->setText(last ? tr("Finish last point") : tr("Next point"));

    if (point.targetSeconds > 0) {
        const bool over = elapsed > point.targetSeconds;
        m_bar->setVisible(true);
        m_bar->setRange(0, point.targetSeconds);
        m_bar->setValue(qMin(elapsed, point.targetSeconds));
        m_bar->setStyleSheet(over ? QStringLiteral("QProgressBar::chunk { background: #D9534F; }") : QString());
        QString text = tr("%1 / %2").arg(formatDuration(elapsed), formatDuration(point.targetSeconds));
        if (over)
            text += tr("  (%1 over)").arg(formatDuration(elapsed - point.targetSeconds));
        m_time->setText(text);
    } else {
        m_bar->setVisible(false);
        m_time->setText(formatDuration(elapsed));
    }

    int totalTarget = 0;
    for (const AgendaPoint &p : agenda)
        totalTarget += p.targetSeconds;
    const QString total = formatDuration(m_controller->totalElapsed());
    m_total->setText(totalTarget > 0 ? tr("Total %1 / %2").arg(total, formatDuration(totalTarget))
                                     : tr("Total %1").arg(total));
}
