#include "app/pages/PreMeetPage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "ui/Theme.h"
#include "ui/Widgets.h"

PreMeetPage::PreMeetPage(QWidget *parent)
    : QWidget(parent)
    , m_progressText(Ui::label(QString(), "secondary"))
    , m_progress(new QProgressBar)
{
    struct Item {
        QString title;
        QString hint;
    };
    const QList<Item> items = {
        {tr("Camera at eye level"), tr("Your face centred, with a little space above your head.")},
        {tr("Good lighting"), tr("Light in front of you, not behind. No harsh shadows.")},
        {tr("Collar and clothes straight"), tr("A quick look in the camera preview.")},
        {tr("Nothing distracting in view"), tr("Notes, clutter and screens out of the frame.")},
        {tr("Sitting upright"), tr("Feet flat, shoulders relaxed, back against the chair.")},
        {tr("Notifications muted"), tr("Phone silent, chat and email notifications off.")},
        {tr("Agenda ready"), tr("Your points are listed on the Training page.")},
    };

    auto *checklist = new Card(tr("Readiness checklist"));
    for (const Item &item : items) {
        auto *check = new CheckCircle;
        checklist->body()->addWidget(new OptionRow(item.title, item.hint, check, true));
        connect(check, &QAbstractButton::toggled, this, &PreMeetPage::updateProgress);
        m_items.append(check);
    }
    checklist->body()->setSpacing(2);

    // Camera check (placeholder until the vision module exists)
    auto *camera = new Card(tr("Automatic camera check"));
    auto *cameraRow = new QHBoxLayout;
    QLabel *cameraText = Ui::label(tr("Soon SMILE will look through your camera and check framing, lighting, "
                                      "your collar and visible notes for you. It will never change your video."),
                                   "secondary");
    cameraText->setWordWrap(true);
    auto *cameraButton = new QPushButton(tr("Coming soon"));
    cameraButton->setEnabled(false);
    cameraRow->addWidget(cameraText, 1);
    cameraRow->addSpacing(16);
    cameraRow->addWidget(cameraButton);
    camera->body()->addLayout(cameraRow);

    // Footer: progress + go
    auto *ready = new Card;
    auto *readyRow = new QHBoxLayout;
    auto *progressCol = new QVBoxLayout;
    progressCol->setSpacing(8);
    progressCol->addWidget(m_progressText);
    m_progress->setTextVisible(false);
    progressCol->addWidget(m_progress);
    auto *go = new QPushButton(tr("Go to Meet"));
    go->setObjectName(QStringLiteral("accent"));
    go->setIcon(Icons::icon(Icons::Icon::Next, Theme::colors().onAccent, 14));
    go->setCursor(Qt::PointingHandCursor);
    readyRow->addLayout(progressCol, 1);
    readyRow->addSpacing(24);
    readyRow->addWidget(go);
    ready->body()->addLayout(readyRow);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(36, 28, 36, 28);
    layout->setSpacing(12);
    layout->addWidget(Ui::pageHeader(tr("Pre-Meet"), tr("A one-minute check so you look your best before you join.")));
    layout->addWidget(checklist);
    layout->addWidget(camera);
    layout->addWidget(ready);
    layout->addStretch();

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(Ui::scrollPage(content));

    connect(go, &QPushButton::clicked, this, &PreMeetPage::goToMeetRequested);
    updateProgress();
}

void PreMeetPage::updateProgress()
{
    int done = 0;
    for (const QAbstractButton *item : std::as_const(m_items))
        done += item->isChecked() ? 1 : 0;
    const int total = int(m_items.size());
    m_progressText->setText(done == total ? tr("All set. You're ready to join.")
                                          : tr("%1 of %2 ready").arg(done).arg(total));
    m_progress->setRange(0, total);
    m_progress->setValue(done);
    const QString state = done == total ? QStringLiteral("done") : QStringLiteral("normal");
    if (m_progress->property("state").toString() != state) {
        m_progress->setProperty("state", state);
        Theme::repolish(m_progress);
    }
}
