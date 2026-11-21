#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QPointF>
#include <QTimer>

#include "core/TipSource.h"
#include "vision/FaceFrame.h"

class AiClient;
class CameraMonitor;

// Watches the camera during a session using two AIs in tandem:
//  * the local face tracker (on this computer, ~15x per second) follows your
//    face, head direction and movement, and notices changes instantly;
//  * the cloud vision model (every few seconds, and immediately when the local
//    tracker sees a change) judges expression, posture, hands and appearance,
//    each with a confidence.
// Evaluation is contextual: a neutral, relaxed face is good; glancing at notes
// is fine; only persistent, confident evidence leads to a tip, and every tip
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
    bool isWarningActive(const QString &warningId) const override;

    static QString analysisPrompt();
    // 0..100: warm 100, positive 90, neutral & relaxed 75, flat 60, tense 35, cold 15
    static int expressionScore(const QString &expression);

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
    double confidence(const char *key) const;
    void note(const QString &kind, const QString &detail); // event for the report

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
    qint64 m_lastFaceAt = -1;
    QJsonObject m_cloud;
    qint64 m_cloudAt = -1;
    QList<QJsonObject> m_cloudHistory; // newest first

    // When each condition started (session ms), -1 if not active
    qint64 m_coldSince = -1;     // tense or unapproachable expression
    qint64 m_flatSince = -1;     // blank expression (gentle nudge only)
    qint64 m_sideSince = -1;     // looking away to the side
    qint64 m_notesSince = -1;    // looking down (notes)
    qint64 m_faceMissingSince = -1;
    qint64 m_postureSince = -1;
    qint64 m_restlessSince = -1;
    qint64 m_coveredSince = -1;   // hand/object over the face
    float m_negativeEma = 0;      // local expression model, smoothed
    float m_happyEma = 0;
    float m_neutralEma = 0;
    QString m_posture;
    QString m_prevExpression;
    int m_prevSmile = -1;
    int m_prevGaze = 0; // 0 camera, 1 notes, 2 side
    bool m_prevFace = true;

    // Local calibration
    QList<float> m_smileSamples;
    QList<float> m_neutralSamples;
    float m_pitchBaseline = 0.6f;
    bool m_pitchInit = false;
    QList<QPair<qint64, QPointF>> m_positions; // recent face centres for movement
    float m_faceHeight = 0.3f;

    QHash<QString, QElapsedTimer> m_lastTip;
    QHash<QString, int> m_streak;

    // Time-weighted statistics (ms)
    qint64 m_trackedMs = 0, m_faceMs = 0;
    qint64 m_lookingMs = 0, m_awayCountedMs = 0, m_currentAwayMs = 0;
    qint64 m_smileKnownMs = 0, m_smileMs = 0;
    qint64 m_expressionKnownMs = 0;
    double m_expressionScoreMs = 0;
    qint64 m_relaxedMs = 0;
    qint64 m_restlessMs = 0;
    qint64 m_coveredMs = 0;
    double m_smileScoreMs = 0;   // local smile score, time-weighted
    qint64 m_smileScoreKnownMs = 0;
    QList<float> m_heights;      // face size samples, for framing consistency
    QList<float> m_centres;
    double m_engagementMs = 0;
    int m_notesGlances = 0;
    int m_cloudChecks = 0, m_cloudUpright = 0, m_cloudFaces = 0;
    qint64 m_cloudLatencyTotal = 0;
    QMap<QString, int> m_postures;
    QMap<QString, int> m_expressions;
    QMap<QString, int> m_hands;
    int m_discomfortChecks = 0;
    QList<QVariantMap> m_events; // notable moments for the report
    QString m_lastError;
};
