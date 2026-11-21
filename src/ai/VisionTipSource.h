#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QMap>
#include <QTimer>

#include "core/TipSource.h"

class AiClient;
class CameraMonitor;

// Watches the camera during a session. Every few seconds a snapshot is
// analysed (by the AI vision model if allowed, otherwise only lighting is
// checked locally). Tips are raised only for patterns that persist across
// several snapshots, and each kind of tip has its own cooldown.
class VisionTipSource : public TipSource
{
    Q_OBJECT

public:
    VisionTipSource(CameraMonitor *camera, AiClient *ai, QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("vision"); }
    void start() override;
    void stop() override;
    QVariantMap reportData() const override;

    static QString analysisPrompt();

private:
    void analyze();
    void handle(const QJsonObject &result);
    void handleLightingOnly();
    void tip(const QString &key, TipCategory category, TipSeverity severity, const QString &text, int cooldownS = 120);

    CameraMonitor *m_camera;
    AiClient *m_ai;
    QTimer m_timer;
    bool m_running = false;
    bool m_busy = false;
    bool m_acquired = false;

    // How many snapshots in a row showed each issue
    QHash<QString, int> m_streak;
    QHash<QString, QElapsedTimer> m_lastTip;
    QString m_postureTipped; // posture we last warned about, to praise the fix

    // Report statistics
    int m_samples = 0;
    int m_faces = 0;
    int m_smiles = 0;
    int m_eyeContact = 0;
    int m_upright = 0;
    QMap<QString, int> m_postures;
    QMap<QString, int> m_expressions;
    QString m_lastError;
};
