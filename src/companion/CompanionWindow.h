#pragma once

#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QTimer>
#include <QVariantAnimation>
#include <QWidget>
#include <optional>

#include "companion/SmileyPainter.h"
#include "core/Tip.h"

// The floating coach. It is invisible most of the time: when a tip arrives it
// pops up in the corner of the screen, says one short thing, and slips away
// again. Tips are rate-limited so it never nags. It never touches the camera
// or video feed.
// Live timing shown in the small card next to the avatar (optional).
struct SectionTimerInfo {
    bool running = false;
    QString section;
    int index = 0;
    int count = 0;
    int elapsedSeconds = 0;
    int targetSeconds = 0;
    int scheduleDeltaSeconds = 0; // + behind schedule, - ahead
    QString status;               // "Listening · Live feedback active"
};

class CompanionWindow : public QWidget
{
    Q_OBJECT

public:
    explicit CompanionWindow(QWidget *parent = nullptr);

    // Minimum quiet time between two pop-ups (default 45 s).
    void setMinimumGap(int milliseconds) { m_gapMs = milliseconds; }

public slots:
    void showTip(const Tip &tip);    // respects the quiet time and snooze
    void previewTip(const Tip &tip); // shows immediately (for "Preview a tip")
    void dismiss();
    void setSessionActive(bool active);
    void resetPosition();
    // Docked: the avatar stays visible with the section timer beside it.
    void setTimerVisible(bool visible);
    void setTimerInfo(const SectionTimerInfo &info);

signals:
    void liveTipPresented(const Tip &tip);
    void liveTipClosed();
    void openMainWindowRequested();
    void nextPointRequested();
    void endSessionRequested();
    void debugViewRequested();
    void muteRequested(bool muted);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void moveEvent(QMoveEvent *event) override;

private:
    enum class State { Hidden, Entering, Shown, Leaving };

    void popUp(const Tip &tip, bool preview);
    void setCurrentTip(const Tip &tip);
    void startLeaving();
    void closeLiveTip();
    void onFrame();
    void onAppearFinished();
    void placeOnScreen();
    void scheduleBlink();
    SmileyPainter::Face faceFor(const Tip &tip) const;
    QRectF faceRect() const;
    QRectF timerRect() const;
    void updateLayout();
    void updateMask();
    void paintTimer(QPainter &p);

    State m_state = State::Hidden;
    std::optional<Tip> m_current;
    std::optional<Tip> m_pending;
    QElapsedTimer m_pendingAge;
    QElapsedTimer m_sinceHidden;
    QDeadlineTimer m_snoozedUntil;
    int m_gapMs = 45000;
    bool m_sessionActive = false;
    bool m_livePresentationPending = false;
    bool m_liveTipOnScreen = false;

    QVariantAnimation m_appear; // 0 hidden .. 1 fully shown
    QTimer m_frame;
    QTimer m_hold;
    QTimer m_pendingTimer;
    QTimer m_blinkTimer;
    QTimer m_lookTimer;
    QElapsedTimer m_clock;

    SmileyPainter::Face m_face;
    SmileyPainter::Face m_target;
    QRectF m_bubble;
    bool m_hover = false;
    bool m_showReason = false;
    bool m_docked = false;
    SectionTimerInfo m_timer;

    QPoint m_pressPos;
    bool m_pressed = false;
    bool m_dragged = false;
    bool m_placing = false;
};
