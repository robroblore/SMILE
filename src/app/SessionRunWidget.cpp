#include "app/SessionRunWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

#include "core/SessionController.h"
#include "core/TimeFormat.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

SessionRunWidget::SessionRunWidget(SessionController *controller, QWidget *parent)
    : QWidget(parent)
    , m_controller(controller)
    , m_caption(Ui::label(QString(), "caption", this))
    , m_point(Ui::label(QString(), "bigValue", this))
    , m_time(Ui::label(QString(), nullptr, this))
    , m_total(Ui::label(QString(), "secondary", this))
    , m_bar(new QProgressBar(this))
    , m_next(new QPushButton(tr("Next point"), this))
    , m_finish(new QPushButton(tr("Finish"), this))
{
    m_point->setWordWrap(true);
    m_bar->setTextVisible(false);

    QFont timeFont = m_time->font();
    timeFont.setPointSizeF(timeFont.pointSizeF() * 1.15);
    timeFont.setWeight(QFont::DemiBold);
    m_time->setFont(timeFont);

    m_next->setObjectName(QStringLiteral("accent"));
    m_next->setCursor(Qt::PointingHandCursor);
    m_finish->setCursor(Qt::PointingHandCursor);

    auto *timeRow = new QHBoxLayout;
    timeRow->addWidget(m_time);
    timeRow->addStretch();
    timeRow->addWidget(m_total);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    buttons->addWidget(m_next);
    buttons->addWidget(m_finish);
    buttons->addStretch();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(m_caption);
    layout->addWidget(m_point);
    layout->addSpacing(4);
    layout->addWidget(m_bar);
    layout->addLayout(timeRow);
    layout->addSpacing(6);
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

    m_caption->setText(tr("POINT %1 OF %2").arg(index + 1).arg(agenda.size()));
    m_point->setText(point.title);
    m_next->setText(last ? tr("Finish") : tr("Next point"));
    m_finish->setVisible(!last);
    m_finish->setText(tr("End early"));

    QString state = QStringLiteral("normal");
    if (point.targetSeconds > 0) {
        const bool over = elapsed > point.targetSeconds;
        state = over ? QStringLiteral("over") : state;
        m_bar->setRange(0, point.targetSeconds);
        m_bar->setValue(qMin(elapsed, point.targetSeconds));
        QString text = tr("%1 / %2").arg(formatDuration(elapsed), formatDuration(point.targetSeconds));
        if (over)
            text = tr("%1 over").arg(formatDuration(elapsed - point.targetSeconds));
        m_time->setText(text);
        m_time->setStyleSheet(over ? QStringLiteral("color: %1;").arg(Theme::css(Theme::colors().critical))
                                   : QString());
    } else {
        m_bar->setRange(0, 0); // busy indicator
        m_time->setText(formatDuration(elapsed));
        m_time->setStyleSheet(QString());
    }
    if (m_bar->property("state").toString() != state) {
        m_bar->setProperty("state", state);
        Theme::repolish(m_bar);
    }

    int totalTarget = 0;
    for (const AgendaPoint &p : agenda)
        totalTarget += p.targetSeconds;
    const QString total = formatDuration(m_controller->totalElapsed());
    m_total->setText(totalTarget > 0 ? tr("Total %1 of %2").arg(total, formatDuration(totalTarget))
                                     : tr("Total %1").arg(total));
}
