#pragma once

#include <QPoint>
#include <QQueue>
#include <QTimer>
#include <QWidget>
#include <optional>

#include "companion/SmileyPainter.h"
#include "core/Tip.h"

// The floating, always-on-top coach that sits beside the meeting window and
// shows tips in a speech bubble. It never touches the camera or video feed.
class CompanionWindow : public QWidget
{
    Q_OBJECT

public:
    explicit CompanionWindow(QWidget *parent = nullptr);

    void moveToDefaultPosition();

public slots:
    void showTip(const Tip &tip);
    void dismissTip();
    void setSessionActive(bool active);

signals:
    void openMainWindowRequested();
    void visibilityChanged(bool visible);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void showNextTip();
    QRectF faceRect() const;
    QRectF bubbleRect() const;
    QFont headerFont() const;
    QFont bodyFont() const;
    SmileyPainter::Mood currentMood() const;

    QQueue<Tip> m_queue;
    std::optional<Tip> m_current;
    QTimer m_tipTimer;
    QTimer m_blinkTimer;
    bool m_eyesClosed = false;
    bool m_sessionActive = false;

    QPoint m_pressPos;
    QPoint m_dragOffset;
    bool m_pressed = false;
    bool m_dragging = false;
};
