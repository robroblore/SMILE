#include "app/pages/PreMeetPage.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

PreMeetPage::PreMeetPage(QWidget *parent)
    : QWidget(parent)
    , m_progress(new QLabel(this))
    , m_autoCheck(new QPushButton(tr("Run automatic check"), this))
    , m_goToMeet(new QPushButton(tr("I'm ready: go to Meet"), this))
{
    auto *title = new QLabel(tr("Pre-Meet"), this);
    QFont f = title->font();
    f.setPointSizeF(f.pointSizeF() * 1.8);
    f.setBold(true);
    title->setFont(f);

    auto *intro = new QLabel(tr("Check how you'll look before you join. Automatic checks will use your "
                                "camera once the vision module is added; for now, tick each item yourself."),
                             this);
    intro->setWordWrap(true);

    auto *group = new QGroupBox(tr("Readiness checklist"), this);
    auto *groupLayout = new QVBoxLayout(group);
    const QStringList items = {
        tr("Camera at eye level, face centred in the frame"),
        tr("Face evenly lit, no strong light behind you"),
        tr("Collar and clothing straight"),
        tr("No notes or clutter visible on camera"),
        tr("Sitting upright, shoulders relaxed"),
        tr("Notifications muted"),
        tr("Agenda ready on the Training page"),
    };
    for (const QString &text : items) {
        auto *box = new QCheckBox(text, group);
        connect(box, &QCheckBox::toggled, this, &PreMeetPage::updateProgress);
        groupLayout->addWidget(box);
        m_items.append(box);
    }

    m_autoCheck->setEnabled(false);
    m_autoCheck->setToolTip(tr("Coming soon: needs the camera and vision module."));
    m_goToMeet->setMinimumHeight(36);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_autoCheck);
    buttons->addStretch();
    buttons->addWidget(m_progress);
    buttons->addWidget(m_goToMeet);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->addWidget(title);
    layout->addWidget(intro);
    layout->addSpacing(8);
    layout->addWidget(group);
    layout->addStretch();
    layout->addLayout(buttons);

    connect(m_goToMeet, &QPushButton::clicked, this, &PreMeetPage::goToMeetRequested);
    updateProgress();
}

void PreMeetPage::updateProgress()
{
    int done = 0;
    for (const QCheckBox *box : std::as_const(m_items))
        done += box->isChecked() ? 1 : 0;
    m_progress->setText(tr("%1 of %2 ready").arg(done).arg(m_items.size()));
}
