#pragma once

#include <QtGlobal>

// Spots when you've been holding the floor too long without letting anyone in.
// A "turn" is everything you say between real breaks: a silence of a few seconds
// is where someone else could speak (a question, an answer). Short breaths and
// pauses between sentences don't end your turn.
//
// It fires once you've talked for most of `limitMs` without such a break, then
// stays quiet until you either give others the floor or talk for another full
// stretch, so it never nags.
class TalkTurn
{
public:
    static constexpr qint64 kBreakMs = 4000;   // silence that hands the floor over
    static constexpr double kMinShare = 0.6;   // you spoke at least this much of the turn

    void reset(qint64 nowMs, qint64 speechMs)
    {
        m_turnStart = nowMs;
        m_speechAtStart = speechMs;
        m_silentSince = -1;
    }

    // Call regularly. speechMs is your total speaking time so far.
    // Returns true when it's time to suggest pausing for questions.
    bool update(qint64 nowMs, bool speaking, qint64 speechMs, qint64 limitMs)
    {
        if (m_turnStart < 0)
            reset(nowMs, speechMs);
        if (speaking) {
            m_silentSince = -1;
        } else {
            if (m_silentSince < 0)
                m_silentSince = nowMs;
            if (nowMs - m_silentSince >= kBreakMs) {
                reset(nowMs, speechMs); // you let others in: a new turn starts
                m_silentSince = nowMs;
                return false;
            }
        }
        const qint64 length = nowMs - m_turnStart;
        const qint64 spoken = speechMs - m_speechAtStart;
        if (length >= limitMs && spoken >= kMinShare * double(length)) {
            m_lastTurnLengthMs = length;
            reset(nowMs, speechMs); // a second reminder only after another full stretch
            return true;
        }
        return false;
    }

    qint64 turnMs(qint64 nowMs) const { return m_turnStart < 0 ? 0 : nowMs - m_turnStart; }
    qint64 lastTurnLengthMs() const { return m_lastTurnLengthMs; }

private:
    qint64 m_turnStart = -1;
    qint64 m_speechAtStart = 0;
    qint64 m_silentSince = -1;
    qint64 m_lastTurnLengthMs = 0;
};
