#pragma once

#include <QObject>
#include <QVariantMap>

#include "core/Tip.h"

// Anything that watches the user and produces tips: the future posture model,
// smile detector, speech analyser and so on. Implement this interface, register
// the source with SessionController::addTipSource(), and its tips flow to the
// companion and into the session report.
class TipSource : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    virtual QString name() const = 0;

    // Called by SessionController when a session starts or stops.
    virtual void start() = 0;
    virtual void stop() = 0;

    // Numbers and text for the Post-Meet report, collected after stop().
    virtual QVariantMap reportData() const { return {}; }

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

signals:
    void tipRaised(const Tip &tip);

private:
    bool m_enabled = true;
};
