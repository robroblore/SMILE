#pragma once

#include <QTimer>

#include "core/TipSource.h"

// Placeholder that raises sample posture/smile/speech tips at random intervals,
// so the companion can be tried out before the real AI modules exist.
class DemoTipSource : public TipSource
{
    Q_OBJECT

public:
    explicit DemoTipSource(QObject *parent = nullptr);

    QString name() const override;
    void start() override;
    void stop() override;

private:
    void scheduleNext();
    void raiseRandomTip();

    QTimer m_timer;
};
