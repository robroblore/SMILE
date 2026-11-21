#include "ai/VisionTipSource.h"

#include <QJsonArray>
#include <algorithm>
#include <cmath>

#include "ai/AiClient.h"
#include "core/AppSettings.h"
#include "core/LiveSignals.h"
#include "sensors/CameraMonitor.h"

namespace {
constexpr int kTickMs = 250;
constexpr int kMinCloudSpacingMs = 1200;
constexpr float kYawLimit = 0.22f;   // eye-distances of nose offset
constexpr float kPitchLimit = 0.16f;

float median(QList<float> v)
{
    if (v.isEmpty())
        return 0;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

QString secs(qint64 ms)
{
    return QString::number(ms / 1000);
}
} // namespace

VisionTipSource::VisionTipSource(CameraMonitor *camera, AiClient *ai, QObject *parent)
    : TipSource(parent)
    , m_camera(camera)
    , m_ai(ai)
{
    m_tick.setInterval(kTickMs);
    connect(&m_tick, &QTimer::timeout, this, &VisionTipSource::onTick);
    connect(&m_cloudTimer, &QTimer::timeout, this, [this] { requestCloud(tr("regular check")); });
    connect(m_camera, &CameraMonitor::faceReady, this, &VisionTipSource::onFace);
}

QString VisionTipSource::analysisPrompt()
{
    return QStringLiteral(
        "You are a presentation coach looking at one webcam frame of a person in an online meeting. "
        "Judge only what you can see; if unsure, choose the neutral value. Reply with ONLY a JSON object:\n"
        "{\n"
        "  \"face_visible\": true|false,\n"
        "  \"smiling\": true|false,             // a clear, natural smile\n"
        "  \"expression\": \"warm\"|\"neutral\"|\"tense\"|\"distracted\",\n"
        "  \"looking_at_camera\": true|false,\n"
        "  \"posture\": \"upright\"|\"slouching\"|\"leaning_in\"|\"leaning_back\"|\"head_tilted\"|\"unknown\",\n"
        "  \"shoulders_tense\": true|false,\n"
        "  \"touching_face\": true|false,\n"
        "  \"lighting\": \"good\"|\"dark\"|\"backlit\"|\"uneven\",\n"
        "  \"clothing_messy\": true|false,     // crooked collar, untidy clothes\n"
        "  \"observation\": \"one short sentence: what you see about expression and posture\"\n"
        "}");
}

void VisionTipSource::start()
{
    m_running = true;
    m_inFlight = false;
    m_session.start();
    m_cloud = QJsonObject();
    m_cloudAt = -1;
    m_cloudHistory.clear();
    m_noSmileSince = m_lookAwaySince = m_faceMissingSince = m_postureSince = -1;
    m_posture.clear();
    m_prevSmile = -1;
    m_lastFaceAt = -1;
    m_face = FaceFrame();
    m_prevLooking = true;
    m_prevFace = true;
    m_smileSamples.clear();
    m_neutralSamples.clear();
    m_pitchInit = false;
    m_lastTip.clear();
    m_streak.clear();
    m_trackedMs = m_faceMs = m_eyeMs = m_smileKnownMs = m_smileMs = 0;
    m_engagementMs = 0;
    m_cloudChecks = m_cloudUpright = m_cloudFaces = 0;
    m_cloudLatencyTotal = 0;
    m_postures.clear();
    m_expressions.clear();
    m_lastError.clear();

    LiveSignals::instance().vision = LiveSignals::Vision();
    LiveSignals::instance().vision.localAvailable = m_camera->faceTrackingAvailable();

    if (!AppSettings::instance().useCamera())
        return;
    m_camera->acquire();
    m_acquired = true;
    m_tick.start();
    m_cloudTimer.start(AppSettings::instance().snapshotIntervalSeconds() * 1000);
    QTimer::singleShot(1500, this, [this] { requestCloud(tr("first look")); });
}

void VisionTipSource::stop()
{
    m_running = false;
    m_tick.stop();
    m_cloudTimer.stop();
    if (m_acquired) {
        m_camera->release();
        m_acquired = false;
    }
}

bool VisionTipSource::cloudFresh() const
{
    const int interval = AppSettings::instance().snapshotIntervalSeconds() * 1000;
    return m_cloudAt >= 0 && now() - m_cloudAt < qMax(6000, int(interval * 2.5));
}

// ------------------------------------------------------------ local tracker

void VisionTipSource::onFace(const FaceFrame &face)
{
    if (!m_running)
        return;
    m_face = face;
    m_lastFaceAt = now();
    LiveSignals::Vision &v = LiveSignals::instance().vision;
    v.face = face;
    v.cameraFps = m_camera->framesPerSecond();
    if (!face.found)
        return;

    // Pitch baseline follows the user's normal head position while facing the camera
    if (!m_pitchInit) {
        m_pitchBaseline = face.pitch;
        m_pitchInit = true;
    } else if (std::abs(face.yaw) < 0.15f) {
        m_pitchBaseline += 0.02f * (face.pitch - m_pitchBaseline);
    }
    v.pitchBaseline = m_pitchBaseline;
    v.lookingLocal = std::abs(face.yaw) < kYawLimit && std::abs(face.pitch - m_pitchBaseline) < kPitchLimit;

    // Local smile estimate, once the cloud has shown us both of your faces
    if (m_smileSamples.size() >= 2 && m_neutralSamples.size() >= 2) {
        const float smile = median(m_smileSamples);
        const float neutral = median(m_neutralSamples);
        if (smile > neutral + 0.03f) {
            v.smileRef = smile;
            v.neutralRef = neutral;
            v.smileThreshold = (smile + neutral) / 2;
            v.smileLocal = face.mouthRatio > v.smileThreshold ? 1 : 0;
        }
    }

    // A change spotted locally -> ask the cloud now instead of waiting
    if (v.smileLocal >= 0 && m_prevSmile >= 0 && v.smileLocal != m_prevSmile)
        requestCloud(v.smileLocal ? tr("local tracker saw a smile start") : tr("local tracker saw the smile fade"));
    m_prevSmile = v.smileLocal;
}

// ----------------------------------------------------------------- cloud AI

void VisionTipSource::requestCloud(const QString &trigger)
{
    if (!m_running || m_inFlight || !m_ai->isConfigured() || !AppSettings::instance().cloudAnalysis())
        return;
    if (m_sinceCloudRequest.isValid() && m_sinceCloudRequest.elapsed() < kMinCloudSpacingMs)
        return;
    const QImage frame = m_camera->latestFrame();
    if (frame.isNull())
        return;

    m_inFlight = true;
    m_sinceCloudRequest.restart();
    m_cloudRequestTimer.restart();
    LiveSignals::instance().vision.cloudTrigger = trigger;
    m_cloudTimer.start(); // restart the regular interval
    m_ai->analyzeImage(frame, analysisPrompt(), [this](const QJsonObject &result, const QString &error) {
        m_inFlight = false;
        LiveSignals::Vision &v = LiveSignals::instance().vision;
        v.cloudLatencyMs = int(m_cloudRequestTimer.elapsed());
        if (!m_running)
            return;
        if (!error.isEmpty()) {
            m_lastError = error;
            v.cloudError = error;
            return;
        }
        v.cloudError.clear();
        m_cloudLatencyTotal += v.cloudLatencyMs;
        onCloud(result);
    });
}

void VisionTipSource::onCloud(const QJsonObject &r)
{
    m_cloud = r;
    m_cloudAt = now();
    m_cloudHistory.prepend(r);
    while (m_cloudHistory.size() > 10)
        m_cloudHistory.removeLast();
    ++m_cloudChecks;

    LiveSignals::Vision &v = LiveSignals::instance().vision;
    v.cloud = r;
    v.cloudChecks = m_cloudChecks;

    const bool face = r.value(QLatin1String("face_visible")).toBool(true);
    if (!face)
        return;
    ++m_cloudFaces;
    const QString posture = r.value(QLatin1String("posture")).toString(QStringLiteral("unknown"));
    const QString expression = r.value(QLatin1String("expression")).toString(QStringLiteral("neutral"));
    ++m_postures[posture];
    ++m_expressions[expression];
    if (posture == QLatin1String("upright") || posture == QLatin1String("unknown"))
        ++m_cloudUpright;

    // Teach the local tracker what this person's smile and neutral face look like
    if (m_face.found && m_lastFaceAt >= 0 && now() - m_lastFaceAt < 1000) {
        QList<float> &bucket = r.value(QLatin1String("smiling")).toBool() ? m_smileSamples : m_neutralSamples;
        bucket.prepend(m_face.mouthRatio);
        while (bucket.size() > 9)
            bucket.removeLast();
    }

    auto streak = [this](const QString &key, bool condition) {
        const int n = condition ? m_streak.value(key) + 1 : 0;
        m_streak[key] = n;
        return n;
    };
    const AppSettings::Thresholds t = AppSettings::instance().thresholds();

    // Posture: time-based, the same bad posture across consecutive checks
    const bool bad = !(posture == QLatin1String("upright") || posture == QLatin1String("unknown"));
    if (bad && posture == m_posture) {
        // continuing
    } else if (bad) {
        m_posture = posture;
        m_postureSince = now();
        alert(tr("Posture changed: %1").arg(QString(posture).replace(QLatin1Char('_'), QLatin1Char(' '))),
              r.value(QLatin1String("observation")).toString());
    } else {
        if (!m_posture.isEmpty()) {
            alert(tr("Posture back to upright"), QString());
            if (m_lastTip.contains(QStringLiteral("posture")))
                tip(QStringLiteral("posture-fixed"), TipCategory::Posture, TipSeverity::Positive,
                    tr("Much better posture. Nice."), tr("The cloud AI now sees you sitting upright."));
        }
        m_posture.clear();
        m_postureSince = -1;
    }

    if (streak(QStringLiteral("tense"), expression == QLatin1String("tense")) >= 2)
        tip(QStringLiteral("tense"), TipCategory::Expression, TipSeverity::Info,
            tr("You look a little tense. Relax your face and take a breath."),
            tr("The cloud AI rated your expression \"tense\" in %1 checks in a row. It said: \"%2\"")
                .arg(m_streak.value(QStringLiteral("tense")))
                .arg(r.value(QLatin1String("observation")).toString()));
    if (streak(QStringLiteral("shoulders"), r.value(QLatin1String("shoulders_tense")).toBool()) >= 2)
        tip(QStringLiteral("shoulders"), TipCategory::Posture, TipSeverity::Info, tr("Drop your shoulders and relax."),
            tr("Raised, tense shoulders in %1 cloud checks in a row.").arg(m_streak.value(QStringLiteral("shoulders"))));
    if (streak(QStringLiteral("touching"), r.value(QLatin1String("touching_face")).toBool()) >= 2)
        tip(QStringLiteral("touching"), TipCategory::Posture, TipSeverity::Info,
            tr("Try to keep your hands away from your face."),
            tr("A hand near your face in %1 cloud checks in a row.").arg(m_streak.value(QStringLiteral("touching"))));
    const QString lighting = r.value(QLatin1String("lighting")).toString(QStringLiteral("good"));
    if (streak(QStringLiteral("lighting"), lighting == QLatin1String("dark") || lighting == QLatin1String("backlit")) >= 3)
        tip(QStringLiteral("lighting"), TipCategory::Appearance, TipSeverity::Info,
            lighting == QLatin1String("backlit") ? tr("You're backlit. Face the light, or close the blinds behind you.")
                                                 : tr("It's quite dark. Add some light in front of you."),
            tr("The cloud AI rated your lighting \"%1\" in %2 checks in a row.").arg(lighting).arg(m_streak.value(QStringLiteral("lighting"))));
    if (streak(QStringLiteral("clothing"), r.value(QLatin1String("clothing_messy")).toBool()) >= 2)
        tip(QStringLiteral("clothing"), TipCategory::Appearance, TipSeverity::Info,
            tr("Your collar looks out of place. Straighten it when you can."),
            tr("Untidy collar or clothing seen in %1 cloud checks in a row.").arg(m_streak.value(QStringLiteral("clothing"))));
    Q_UNUSED(t)
}

// ------------------------------------------------------- time-based decisions

void VisionTipSource::onTick()
{
    if (!m_running)
        return;
    LiveSignals::Vision &v = LiveSignals::instance().vision;
    const AppSettings::Thresholds t = AppSettings::instance().thresholds();
    const qint64 nowMs = now();
    const bool local = v.localAvailable;
    const bool localFresh = local && m_lastFaceAt >= 0 && nowMs - m_lastFaceAt < 1000;
    const bool fresh = cloudFresh();

    // --- Is the face there?
    int face = -1; // unknown
    if (localFresh)
        face = m_face.found ? 1 : 0;
    else if (fresh)
        face = m_cloud.value(QLatin1String("face_visible")).toBool(true) ? 1 : 0;

    // --- Looking at the camera?
    int looking = -1;
    if (localFresh && face == 1)
        looking = v.lookingLocal ? 1 : 0;
    else if (fresh && face == 1)
        looking = m_cloud.value(QLatin1String("looking_at_camera")).toBool(true) ? 1 : 0;

    // --- Smiling? The cloud is the judge; between cloud checks the calibrated
    // local estimate fills in.
    int smiling = -1;
    QString source = QStringLiteral("unknown");
    if (face == 1) {
        if (fresh && m_cloudAt >= 0 && (v.smileLocal < 0 || nowMs - m_cloudAt < 2500)) {
            smiling = m_cloud.value(QLatin1String("smiling")).toBool() ? 1 : 0;
            source = QStringLiteral("cloud");
        } else if (v.smileLocal >= 0) {
            smiling = v.smileLocal;
            source = QStringLiteral("local");
        } else if (fresh) {
            smiling = m_cloud.value(QLatin1String("smiling")).toBool() ? 1 : 0;
            source = QStringLiteral("cloud");
        }
    }
    v.smilingFused = smiling;
    v.smilingSource = source;
    v.cloudAgeMs = m_cloudAt >= 0 ? nowMs - m_cloudAt : -1;

    // --- Live alerts on changes (debug view)
    if (face == 0 && m_prevFace)
        alert(tr("Face lost"), local ? tr("The local tracker found no face in the frame.")
                                     : tr("The cloud AI couldn't see a face."));
    if (face == 1 && !m_prevFace)
        alert(tr("Face found again"), QString());
    if (face >= 0)
        m_prevFace = face == 1;
    if (looking == 0 && m_prevLooking)
        alert(tr("Eye contact dropped"),
              local ? tr("Head turned: yaw %1 (limit ±%2), pitch change %3 (limit ±%4).")
                          .arg(m_face.yaw, 0, 'f', 2)
                          .arg(kYawLimit, 0, 'f', 2)
                          .arg(m_face.pitch - m_pitchBaseline, 0, 'f', 2)
                          .arg(kPitchLimit, 0, 'f', 2)
                    : tr("The cloud AI says you're not looking at the camera."));
    if (looking == 1 && !m_prevLooking)
        alert(tr("Eye contact back"), QString());
    if (looking >= 0)
        m_prevLooking = looking == 1;

    // --- Durations
    auto track = [nowMs](qint64 &since, bool active) {
        if (active && since < 0)
            since = nowMs;
        else if (!active)
            since = -1;
        return since >= 0 ? nowMs - since : 0;
    };
    const qint64 missing = track(m_faceMissingSince, face == 0);
    const qint64 away = track(m_lookAwaySince, looking == 0);
    const bool wasSmiling = m_noSmileSince < 0;
    const qint64 noSmile = track(m_noSmileSince, smiling == 0);
    if (smiling == 0 && wasSmiling && m_trackedMs > 2000)
        alert(tr("Facial engagement dropped: smile gone"),
              source == QLatin1String("cloud") ? tr("The cloud AI saw no smile.")
                                               : tr("Mouth width %1 is below your smile threshold %2.")
                                                     .arg(m_face.mouthRatio, 0, 'f', 2)
                                                     .arg(v.smileThreshold, 0, 'f', 2));
    v.faceMissingSeconds = int(missing / 1000);
    v.lookAwaySeconds = int(away / 1000);
    v.noSmileSeconds = int(noSmile / 1000);

    if (missing >= t.faceMissingSeconds * 1000LL) {
        tip(QStringLiteral("noface"), TipCategory::Appearance, TipSeverity::Warning,
            tr("I can't see your face. Check your camera framing."),
            local ? tr("The local face tracker (15 checks a second) has found no face for %1 s (limit %2 s).")
                        .arg(secs(missing)).arg(t.faceMissingSeconds)
                  : tr("The cloud AI hasn't seen your face for %1 s.").arg(secs(missing)));
    }
    if (away >= t.lookAwaySeconds * 1000LL) {
        tip(QStringLiteral("eyes"), TipCategory::Expression, TipSeverity::Info,
            tr("Look at the camera now and then, not just at the screen."),
            local ? tr("Your head has been turned away for %1 s (limit %2 s). Yaw %3, pitch change %4; within ±%5 / ±%6 "
                       "counts as looking at the camera.")
                        .arg(secs(away)).arg(t.lookAwaySeconds)
                        .arg(m_face.yaw, 0, 'f', 2).arg(m_face.pitch - m_pitchBaseline, 0, 'f', 2)
                        .arg(kYawLimit, 0, 'f', 2).arg(kPitchLimit, 0, 'f', 2)
                  : tr("The cloud AI says you haven't looked at the camera for %1 s.").arg(secs(away)));
        m_lookAwaySince = nowMs; // need another full period before repeating
    }
    if (noSmile >= t.noSmileSeconds * 1000LL) {
        QString why = tr("No smile for %1 s (limit %2 s). Cloud checks, newest first: %3.")
                          .arg(secs(noSmile)).arg(t.noSmileSeconds).arg(cloudHistory("smiling", 6));
        if (v.smileLocal >= 0)
            why += QLatin1Char(' ') + tr("Local mouth width %1; your smile is about %2 and neutral about %3.")
                                          .arg(m_face.mouthRatio, 0, 'f', 2)
                                          .arg(v.smileRef, 0, 'f', 2)
                                          .arg(v.neutralRef, 0, 'f', 2);
        tip(QStringLiteral("nosmile"), TipCategory::Expression, TipSeverity::Info, tr("Try smiling more."), why);
        m_noSmileSince = nowMs;
    }
    if (m_postureSince >= 0 && nowMs - m_postureSince >= t.postureSeconds * 1000LL) {
        static const QHash<QString, const char *> texts = {
            {QStringLiteral("slouching"), QT_TR_NOOP("Sit up straight. You're slouching a little.")},
            {QStringLiteral("leaning_in"), QT_TR_NOOP("You're leaning towards the screen. Sit back a little.")},
            {QStringLiteral("leaning_back"), QT_TR_NOOP("You're leaning back. Sit up to look more engaged.")},
            {QStringLiteral("head_tilted"), QT_TR_NOOP("Your head is tilted. Level it with the camera.")},
        };
        if (texts.contains(m_posture))
            tip(QStringLiteral("posture"), TipCategory::Posture, TipSeverity::Warning, tr(texts.value(m_posture)),
                tr("The cloud AI has seen \"%1\" for %2 s (limit %3 s). It said: \"%4\"")
                    .arg(QString(m_posture).replace(QLatin1Char('_'), QLatin1Char(' ')))
                    .arg(secs(nowMs - m_postureSince))
                    .arg(t.postureSeconds)
                    .arg(m_cloud.value(QLatin1String("observation")).toString()));
        m_postureSince = nowMs;
    }

    // --- Time-weighted statistics
    if (face >= 0) {
        m_trackedMs += kTickMs;
        if (face == 1)
            m_faceMs += kTickMs;
        if (looking == 1)
            m_eyeMs += kTickMs;
        if (smiling >= 0) {
            m_smileKnownMs += kTickMs;
            if (smiling == 1)
                m_smileMs += kTickMs;
        }
        const bool centred = m_face.found && m_face.box.center().x() > 0.25 && m_face.box.center().x() < 0.75;
        const double engagement = 0.4 * (looking == 1) + 0.35 * (smiling == 1 ? 1.0 : (smiling < 0 ? 0.5 : 0.0))
                                  + 0.25 * (face == 1 && (centred || !local));
        v.engagement = float(engagement);
        m_engagementMs += engagement * kTickMs;
    }
    LiveSignals::instance().notifyChanged();
}

QString VisionTipSource::cloudHistory(const char *key, int count) const
{
    QStringList parts;
    for (int i = 0; i < qMin(count, int(m_cloudHistory.size())); ++i)
        parts << (m_cloudHistory[i].value(QLatin1String(key)).toBool() ? QStringLiteral("yes") : QStringLiteral("no"));
    return parts.isEmpty() ? tr("none yet") : parts.join(QStringLiteral(", "));
}

void VisionTipSource::alert(const QString &title, const QString &detail)
{
    LiveSignals::instance().addEvent(LiveSignals::Event::Alert, title, detail);
}

void VisionTipSource::tip(const QString &key, TipCategory category, TipSeverity severity, const QString &text,
                          const QString &reason)
{
    const int cooldown = AppSettings::instance().thresholds().repeatCooldownSeconds;
    QElapsedTimer &last = m_lastTip[key];
    if (last.isValid() && last.elapsed() < cooldown * 1000LL)
        return;
    last.restart();

    Tip t;
    t.category = category;
    t.severity = severity;
    t.text = text;
    t.reason = reason;
    t.source = name();
    emit tipRaised(t);
}

QVariantMap VisionTipSource::reportData() const
{
    if (m_trackedMs == 0 && m_cloudChecks == 0)
        return {};
    auto pct = [](qint64 n, qint64 of) { return of > 0 ? int(qRound(100.0 * double(n) / double(of))) : -1; };
    QVariantMap postures;
    for (auto it = m_postures.cbegin(); it != m_postures.cend(); ++it)
        postures.insert(it.key(), it.value());
    QVariantMap expressions;
    for (auto it = m_expressions.cbegin(); it != m_expressions.cend(); ++it)
        expressions.insert(it.key(), it.value());
    return {
        {QStringLiteral("snapshots"), m_cloudChecks},
        {QStringLiteral("trackedSeconds"), int(m_trackedMs / 1000)},
        {QStringLiteral("faceVisiblePct"), pct(m_faceMs, m_trackedMs)},
        {QStringLiteral("eyeContactPct"), pct(m_eyeMs, m_faceMs)},
        {QStringLiteral("smilingPct"), pct(m_smileMs, m_smileKnownMs)},
        {QStringLiteral("engagementPct"), m_trackedMs > 0 ? int(qRound(100.0 * m_engagementMs / double(m_trackedMs))) : -1},
        {QStringLiteral("uprightPct"), pct(m_cloudUpright, m_cloudFaces)},
        {QStringLiteral("postures"), postures},
        {QStringLiteral("expressions"), expressions},
        {QStringLiteral("localTracking"), m_camera->faceTrackingAvailable()},
        {QStringLiteral("avgCloudLatencyMs"), m_cloudChecks ? int(m_cloudLatencyTotal / m_cloudChecks) : 0},
        {QStringLiteral("lastError"), m_lastError},
    };
}
