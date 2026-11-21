#pragma once

#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>
#include <functional>

#include "companion/SmileyPainter.h"

// The SMILE character at full size, as a meeting participant: listens,
// thinks, speaks with lip sync, and reacts with a mood.
class AvatarWidget : public QWidget
{
    Q_OBJECT

public:
    enum class State { Idle, Listening, Thinking, Speaking };

    explicit AvatarWidget(QWidget *parent = nullptr);

    void setState(State state);
    State state() const { return m_state; }
    // neutral, friendly, impressed, skeptical, confused, concerned
    void setMood(const QString &mood);
    // While speaking, the mouth follows this (0..1)
    void setLevelSource(std::function<qreal()> source) { m_level = std::move(source); }
    // While listening, the avatar reacts a little to the user's voice
    void setUserSpeaking(bool speaking) { m_userSpeaking = speaking; }

    QSize sizeHint() const override { return {260, 260}; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void tick();
    SmileyPainter::Face target() const;

    State m_state = State::Idle;
    QString m_mood = QStringLiteral("friendly");
    std::function<qreal()> m_level;
    bool m_userSpeaking = false;
    SmileyPainter::Face m_face;
    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_nextBlink = 2500;
    qint64 m_blinkUntil = 0;
};
