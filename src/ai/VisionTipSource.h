#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QMap>
#include <QTimer>

#include "core/TipSource.h"
#include "vision/FaceFrame.h"

class AiClient;
class CameraMonitor;

// Watches the camera during a session using two AIs in tandem:
//  * the local face tracker (on this computer, ~15x per second) follows your
//    face, head direction and mouth shape, and notices changes instantly;
//  * the cloud vision model (every few seconds, and immediately when the local
//    tracker sees a change) judges smile, posture, tension and appearance.
// Cloud answers also calibrate the local smile estimate to your face.
// Tips fire when a condition has lasted for the configured time; each one
// carries a plain-language reason.
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
    void onFace(const FaceFrame &face);
    void onTick();
    void requestCloud(const QString &trigger);
    void onCloud(const QJsonObject &result);
    void tip(const QString &key, TipCategory category, TipSeverity severity, const QString &text,
             const QString &reason);
    void alert(const QString &title, const QString &detail);
    bool cloudFresh() const;
    qint64 now() const { return m_session.elapsed(); }
    QString cloudHistory(const char *key, int count) const;

    CameraMonitor *m_camera;
    AiClient *m_ai;
    QTimer m_tick;
    QTimer m_cloudTimer;
    bool m_running = false;
    bool m_acquired = false;
    bool m_inFlight = false;
    QElapsedTimer m_session;
    QElapsedTimer m_sinceCloudRequest;
    QElapsedTimer m_cloudRequestTimer;

    // Latest views
    FaceFrame m_face;
    qint64 m_lastFaceAt = -1; // session time of the last local tracker result
    QJsonObject m_cloud;
    qint64 m_cloudAt = -1;
    QList<QJsonObject> m_cloudHistory; // newest first, last ~10

    // When each condition started (session ms), -1 if not active
    qint64 m_noSmileSince = -1;
    qint64 m_lookAwaySince = -1;
    qint64 m_faceMissingSince = -1;
    qint64 m_postureSince = -1;
    QString m_posture;
    int m_prevSmile = -1;
    bool m_prevLooking = true;
    bool m_prevFace = true;

    // Local calibration
    QList<float> m_smileSamples;
    QList<float> m_neutralSamples;
    float m_pitchBaseline = 0.6f;
    bool m_pitchInit = false;

    QHash<QString, QElapsedTimer> m_lastTip;
    QHash<QString, int> m_streak;

    // Time-weighted statistics (ms)
    qint64 m_trackedMs = 0, m_faceMs = 0, m_eyeMs = 0, m_smileKnownMs = 0, m_smileMs = 0;
    double m_engagementMs = 0;
    int m_cloudChecks = 0, m_cloudUpright = 0, m_cloudFaces = 0;
    qint64 m_cloudLatencyTotal = 0;
    QMap<QString, int> m_postures;
    QMap<QString, int> m_expressions;
    QString m_lastError;
};
