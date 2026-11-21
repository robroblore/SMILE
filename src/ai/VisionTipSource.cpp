#include "ai/VisionTipSource.h"

#include <QJsonArray>
#include <QLineF>
#include <algorithm>
#include <cmath>

#include "ai/AiClient.h"
#include "core/AppSettings.h"
#include "core/LiveSignals.h"
#include "core/TimeFormat.h"
#include "sensors/CameraMonitor.h"

namespace {
constexpr int kTickMs = 250;
constexpr int kMinCloudSpacingMs = 1200;
constexpr float kYawLimit = 0.24f;      // eye-distances of nose offset (side)
constexpr float kPitchDownLimit = 0.16f; // looking down at notes
constexpr qint64 kBriefGlanceMs = 3000;  // glances shorter than this never count against you
constexpr double kMinConfidence = 0.6;   // ignore cloud judgements below this
constexpr double kRestless = 0.55;       // face-heights per second, sustained

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

QString pretty(QString s)
{
    return s.replace(QLatin1Char('_'), QLatin1Char(' '));
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
        "You are an experienced communication coach looking at one webcam frame of a professional in an online "
        "meeting. Judge only what is clearly visible, be fair and realistic: a calm neutral face is normal and good; "
        "nobody smiles all the time. If unsure, choose the neutral value and a low confidence. Reply with ONLY JSON:\n"
        "{\n"
        "  \"face_visible\": true|false,\n"
        "  \"expression\": \"warm\"|\"positive\"|\"neutral_relaxed\"|\"neutral_flat\"|\"tense\"|\"unapproachable\",\n"
        "      // warm = clear friendly smile; positive = pleasant, soft smile or bright engaged eyes;\n"
        "      // neutral_relaxed = calm and at ease (perfectly fine); neutral_flat = blank, low energy;\n"
        "      // tense = tight lips/jaw, furrowed brow, stressed; unapproachable = frowning, cold, annoyed or bored\n"
        "  \"smiling\": true|false,\n"
        "  \"relaxed\": true|false,               // facial muscles relaxed\n"
        "  \"gaze\": \"camera\"|\"screen\"|\"notes_down\"|\"away_side\"|\"eyes_closed\",\n"
        "  \"posture\": \"upright\"|\"slouching\"|\"leaning_in\"|\"leaning_back\"|\"head_tilted\"|\"unknown\",\n"
        "  \"shoulders_tense\": true|false,\n"
        "  \"hands\": \"not_visible\"|\"resting\"|\"natural_gesture\"|\"distracting\"|\"inappropriate\",\n"
        "      // distracting = fidgeting, playing with objects, hands covering the face;\n"
        "      // inappropriate = rude or offensive gestures only\n"
        "  \"hands_note\": \"what the hands are doing, a few words\",\n"
        "  \"touching_face\": true|false,\n"
        "  \"face_covered\": true|false,        // hand, object or hair hiding part of the face\n"
        "  \"uncomfortable\": true|false,         // visibly uneasy: hunched, rubbing neck, squirming\n"
        "  \"lighting\": \"good\"|\"dark\"|\"backlit\"|\"uneven\",\n"
        "  \"clothing_messy\": true|false,\n"
        "  \"confidence\": {\"expression\": 0-1, \"gaze\": 0-1, \"posture\": 0-1, \"hands\": 0-1},\n"
        "  \"observation\": \"one short sentence: what you see\"\n"
        "}");
}

int VisionTipSource::expressionScore(const QString &e)
{
    if (e == QLatin1String("warm"))
        return 100;
    if (e == QLatin1String("positive"))
        return 90;
    if (e == QLatin1String("neutral_relaxed") || e == QLatin1String("neutral"))
        return 75;
    if (e == QLatin1String("neutral_flat"))
        return 60;
    if (e == QLatin1String("tense"))
        return 35;
    if (e == QLatin1String("unapproachable"))
        return 15;
    return 70;
}

void VisionTipSource::start()
{
    m_running = true;
    m_inFlight = false;
    m_session.start();
    m_cloud = QJsonObject();
    m_cloudAt = -1;
    m_cloudHistory.clear();
    m_coldSince = m_flatSince = m_sideSince = m_notesSince = m_faceMissingSince = m_postureSince = m_restlessSince = -1;
    m_coveredSince = -1;
    m_sideSeen = m_notesSeen = m_missingSeen = m_coveredSeen = m_lastFoundAt = -1;
    m_noSmileMs = 0;
    m_negativeEma = m_happyEma = 0;
    m_neutralEma = 1;
    m_coveredMs = m_smileScoreKnownMs = 0;
    m_smileScoreMs = 0;
    m_heights.clear();
    m_centres.clear();
    m_posture.clear();
    m_prevExpression.clear();
    m_prevSmile = -1;
    m_prevGaze = 0;
    m_prevFace = true;
    m_lastFaceAt = -1;
    m_face = FaceFrame();
    m_smileSamples.clear();
    m_neutralSamples.clear();
    m_pitchInit = false;
    m_positions.clear();
    m_lastTip.clear();
    m_streak.clear();
    m_trackedMs = m_faceMs = m_lookingMs = m_awayCountedMs = m_currentAwayMs = 0;
    m_smileKnownMs = m_smileMs = m_expressionKnownMs = m_relaxedMs = m_restlessMs = 0;
    m_expressionScoreMs = m_engagementMs = 0;
    m_notesGlances = 0;
    m_cloudChecks = m_cloudUpright = m_cloudFaces = 0;
    m_cloudLatencyTotal = 0;
    m_postures.clear();
    m_expressions.clear();
    m_hands.clear();
    m_discomfortChecks = 0;
    m_events.clear();
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

double VisionTipSource::confidence(const char *key) const
{
    const QJsonObject c = m_cloud.value(QLatin1String("confidence")).toObject();
    return c.contains(QLatin1String(key)) ? c.value(QLatin1String(key)).toDouble(0.7) : 0.7;
}

void VisionTipSource::note(const QString &kind, const QString &detail)
{
    m_events.append({{QStringLiteral("at"), formatDuration(int(now() / 1000))},
                     {QStringLiteral("kind"), kind},
                     {QStringLiteral("detail"), detail}});
    while (m_events.size() > 60)
        m_events.removeFirst();
}

// ------------------------------------------------------------ local tracker

void VisionTipSource::onFace(const FaceFrame &face)
{
    if (!m_running)
        return;
    const bool wasFound = m_face.found;
    m_face = face;
    m_lastFaceAt = now();
    if (face.found) {
        m_lastFoundYaw = face.yaw;
        m_lastFoundPitch = face.pitch;
        m_lastFoundAt = now();
    } else if (wasFound) {
        // Lost the face: covered, turned away or gone? Ask the cloud straight away
        requestCloud(tr("the face tracker lost your face"));
    }
    LiveSignals::Vision &v = LiveSignals::instance().vision;
    v.face = face;
    v.cameraFps = m_camera->framesPerSecond();
    if (!face.found)
        return;

    // Pitch baseline follows the user's normal head position while facing the camera
    if (!m_pitchInit) {
        m_pitchBaseline = face.pitch;
        m_pitchInit = true;
    } else if (std::abs(face.yaw) < 0.15f && std::abs(face.pitch - m_pitchBaseline) < 0.1f) {
        m_pitchBaseline += 0.02f * (face.pitch - m_pitchBaseline);
    }
    v.pitchBaseline = m_pitchBaseline;
    v.lookingLocal = std::abs(face.yaw) < kYawLimit && face.pitch - m_pitchBaseline < kPitchDownLimit
                     && face.pitch - m_pitchBaseline > -kPitchDownLimit;

    // Local expression (FER+), smoothed over ~1 s
    if (face.expressionValid) {
        m_happyEma += 0.25f * (face.happiness - m_happyEma);
        m_neutralEma += 0.25f * (face.neutral - m_neutralEma);
        m_negativeEma += 0.25f * (face.negative - m_negativeEma);
    }
    // Framing consistency samples (about 2 per second)
    if (m_heights.isEmpty() || now() % 500 < 70) {
        m_heights.append(float(face.box.height()));
        m_centres.append(float(face.box.center().x()));
    }

    // Movement: how far the face centre travels, in face heights per second
    m_faceHeight = 0.9f * m_faceHeight + 0.1f * qMax(0.05f, float(face.box.height()));
    // Ignore detector glitches: a face can't jump a quarter of its height within a few
    // milliseconds. A real move is accepted once it's still there a moment later.
    const QPointF centre = face.box.center();
    const bool glitch = !m_positions.isEmpty() && now() - m_positions.last().first < 150
                        && QLineF(m_positions.last().second, centre).length() > 0.25 * qMax(0.05f, m_faceHeight);
    if (!glitch)
        m_positions.append({now(), centre});
    while (!m_positions.isEmpty() && now() - m_positions.first().first > 4000)
        m_positions.removeFirst();

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
    m_cloudTimer.start();
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
    v.hands = r.value(QLatin1String("hands")).toString();

    if (!r.value(QLatin1String("face_visible")).toBool(true))
        return;
    ++m_cloudFaces;
    const QString posture = r.value(QLatin1String("posture")).toString(QStringLiteral("unknown"));
    const QString expression = r.value(QLatin1String("expression")).toString(QStringLiteral("neutral_relaxed"));
    const QString hands = r.value(QLatin1String("hands")).toString(QStringLiteral("not_visible"));
    const QString observation = r.value(QLatin1String("observation")).toString();
    ++m_postures[posture];
    ++m_expressions[expression];
    ++m_hands[hands];
    if (posture == QLatin1String("upright") || posture == QLatin1String("unknown"))
        ++m_cloudUpright;

    if (expression != m_prevExpression && !m_prevExpression.isEmpty())
        alert(tr("Expression: %1").arg(pretty(expression)),
              tr("%1 (confidence %2). %3").arg(pretty(m_prevExpression) + QStringLiteral(" → ") + pretty(expression))
                  .arg(confidence("expression"), 0, 'f', 2)
                  .arg(observation));
    m_prevExpression = expression;

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

    // Posture: the same uncomfortable posture across consecutive checks
    const bool bad = !(posture == QLatin1String("upright") || posture == QLatin1String("unknown"))
                     && confidence("posture") >= kMinConfidence;
    if (bad && posture == m_posture) {
        // continuing
    } else if (bad) {
        m_posture = posture;
        m_postureSince = now();
        alert(tr("Posture: %1").arg(pretty(posture)), tr("Confidence %1. %2").arg(confidence("posture"), 0, 'f', 2).arg(observation));
    } else {
        if (!m_posture.isEmpty()) {
            alert(tr("Posture back to upright"), QString());
            if (m_lastTip.contains(QStringLiteral("posture")))
                tip(QStringLiteral("posture-fixed"), TipCategory::Posture, TipSeverity::Positive, tr("Much better posture. Nice."),
                    tr("The cloud AI now sees you sitting upright."));
        }
        m_posture.clear();
        m_postureSince = -1;
    }

    // Hands: only with confident, repeated evidence
    const double handsConf = confidence("hands");
    const QString handsNote = r.value(QLatin1String("hands_note")).toString();
    if (streak(QStringLiteral("hands-inappropriate"), hands == QLatin1String("inappropriate") && handsConf >= 0.75) >= 2) {
        note(QStringLiteral("gesture"), tr("Possibly inappropriate gesture: %1").arg(handsNote));
        tip(QStringLiteral("hands-inappropriate"), TipCategory::Posture, TipSeverity::Warning,
            tr("That gesture may come across as unprofessional."),
            tr("Seen in %1 checks in a row with confidence %2: \"%3\".")
                .arg(m_streak.value(QStringLiteral("hands-inappropriate")))
                .arg(handsConf, 0, 'f', 2)
                .arg(handsNote));
    }
    if (streak(QStringLiteral("hands-distracting"), hands == QLatin1String("distracting") && handsConf >= kMinConfidence) >= 3) {
        note(QStringLiteral("hands"), tr("Distracting hands: %1").arg(handsNote));
        tip(QStringLiteral("hands"), TipCategory::Posture, TipSeverity::Info,
            tr("Your hands are a little distracting. Rest them, or use calm gestures."),
            tr("Distracting hand movement in %1 checks in a row (confidence %2): \"%3\".")
                .arg(m_streak.value(QStringLiteral("hands-distracting")))
                .arg(handsConf, 0, 'f', 2)
                .arg(handsNote));
    }
    if (r.value(QLatin1String("uncomfortable")).toBool())
        ++m_discomfortChecks;
    if (streak(QStringLiteral("uncomfortable"), r.value(QLatin1String("uncomfortable")).toBool()) >= 3)
        tip(QStringLiteral("uncomfortable"), TipCategory::Posture, TipSeverity::Info,
            tr("You look a little uncomfortable. Sit back, drop your shoulders and breathe."),
            tr("Signs of discomfort in %1 checks in a row. The AI said: \"%2\"")
                .arg(m_streak.value(QStringLiteral("uncomfortable")))
                .arg(observation));
    if (streak(QStringLiteral("touching"), r.value(QLatin1String("touching_face")).toBool()) >= 3)
        tip(QStringLiteral("touching"), TipCategory::Posture, TipSeverity::Info, tr("Try to keep your hands away from your face."),
            tr("A hand near your face in %1 checks in a row.").arg(m_streak.value(QStringLiteral("touching"))));
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
}

// ------------------------------------------------------- time-based decisions

void VisionTipSource::onTick()
{
    if (!m_running)
        return;
    LiveSignals::Vision &v = LiveSignals::instance().vision;
    const AppSettings::Thresholds t = AppSettings::instance().thresholds();
    const qint64 nowMs = now();
    const bool localFresh = v.localAvailable && m_lastFaceAt >= 0 && nowMs - m_lastFaceAt < 1000;
    const bool fresh = cloudFresh();

    // --- Is the face there?
    int face = -1;
    if (localFresh)
        face = m_face.found ? 1 : 0;
    else if (fresh)
        face = m_cloud.value(QLatin1String("face_visible")).toBool(true) ? 1 : 0;

    // --- Where is the user looking? 0 camera, 1 notes (down), 2 side/away
    int gaze = -1;
    // The tracker loses a face turned far away: that's looking away, not a covered face
    const bool turnedAway = localFresh && face == 0 && m_lastFoundAt >= 0 && nowMs - m_lastFoundAt < 8000
                            && (std::abs(m_lastFoundYaw) >= kYawLimit * 0.9f
                                || m_lastFoundPitch - m_pitchBaseline <= -kPitchDownLimit * 0.9f);
    if (localFresh && face == 1) {
        const float down = m_face.pitch - m_pitchBaseline;
        gaze = std::abs(m_face.yaw) >= kYawLimit || down <= -kPitchDownLimit ? 2 : (down >= kPitchDownLimit ? 1 : 0);
    } else if (turnedAway) {
        gaze = 2;
    } else if (fresh && face == 1) {
        const QString g = m_cloud.value(QLatin1String("gaze")).toString(QStringLiteral("camera"));
        gaze = (g == QLatin1String("camera") || g == QLatin1String("screen")) ? 0 : (g == QLatin1String("notes_down") ? 1 : 2);
    }
    v.gaze = gaze == 0 ? tr("camera") : gaze == 1 ? tr("notes (down)") : gaze == 2 ? tr("away") : QString();

    // --- Expression: the local model reads it continuously; the cloud adds
    // nuance (relaxed vs flat, tense vs cold). Keep the last answer while fresh.
    const bool localExpr = localFresh && face == 1 && m_face.expressionValid;
    QString expression = fresh && face == 1 ? m_cloud.value(QLatin1String("expression")).toString() : QString();
    double exprConf = confidence("expression");
    if (localExpr) {
        QString local = m_happyEma > 0.5f ? QStringLiteral("warm")
                        : m_happyEma > 0.25f ? QStringLiteral("positive")
                        : m_negativeEma > 0.45f ? QStringLiteral("tense")
                                                : QStringLiteral("neutral_relaxed");
        // The cloud can refine a neutral read (flat) or a negative one (cold)
        if (local == QLatin1String("neutral_relaxed") && expression == QLatin1String("neutral_flat"))
            local = expression;
        if (local == QLatin1String("tense") && expression == QLatin1String("unapproachable"))
            local = expression;
        expression = local;
        exprConf = qMax(m_happyEma, qMax(m_neutralEma, m_negativeEma));
    }
    v.expression = expression;
    v.expressionConfidence = float(exprConf);
    v.smileScore = localExpr ? float(qBound(0.0, 100.0 * m_happyEma + 45.0 * m_neutralEma + 15.0 * m_negativeEma, 100.0)) : -1.f;
    int smiling = -1;
    QString source = QStringLiteral("unknown");
    if (face == 1 && localExpr) {
        smiling = m_happyEma > 0.4f ? 1 : 0;
        source = QStringLiteral("local model");
    } else if (face == 1) {
        if (fresh && (v.smileLocal < 0 || nowMs - m_cloudAt < 2500)) {
            smiling = m_cloud.value(QLatin1String("smiling")).toBool() ? 1 : 0;
            source = QStringLiteral("cloud");
        } else if (v.smileLocal >= 0) {
            smiling = v.smileLocal;
            source = QStringLiteral("local");
        }
    }
    v.smilingFused = smiling;
    v.smilingSource = source;
    v.cloudAgeMs = m_cloudAt >= 0 ? nowMs - m_cloudAt : -1;

    // --- Movement (local)
    double movement = 0;
    if (m_positions.size() >= 4) {
        double path = 0;
        for (int i = 1; i < m_positions.size(); ++i)
            path += QLineF(m_positions[i - 1].second, m_positions[i].second).length();
        const double span = (m_positions.last().first - m_positions.first().first) / 1000.0;
        if (span > 1.0)
            movement = path / span / qMax(0.05f, m_faceHeight);
    }
    v.movement = float(movement);

    // --- Live alerts on changes (debug view)
    if (face == 0 && m_prevFace)
        alert(tr("Face lost"), localFresh ? tr("The local tracker found no face in the frame.") : tr("The cloud AI couldn't see a face."));
    if (face == 1 && !m_prevFace)
        alert(tr("Face found again"), QString());
    if (face >= 0)
        m_prevFace = face == 1;
    if (gaze >= 0 && gaze != m_prevGaze) {
        if (gaze == 2)
            alert(tr("Looking away"), tr("Head turned: yaw %1 (limit ±%2).").arg(m_face.yaw, 0, 'f', 2).arg(kYawLimit, 0, 'f', 2));
        else if (gaze == 1)
            alert(tr("Looking down (notes?)"), tr("Fine for a while: reading notes is normal."));
        else
            alert(tr("Back to the camera"), QString());
        m_prevGaze = gaze;
    }

    // --- Durations
    auto track = [nowMs](qint64 &since, bool active) {
        if (active && since < 0)
            since = nowMs;
        else if (!active)
            since = -1;
        return since >= 0 ? nowMs - since : 0;
    };
    // Like track(), but a short flicker (tracker noise, one odd cloud answer) doesn't restart the clock
    auto steady = [nowMs](qint64 &since, qint64 &seen, bool active, qint64 graceMs) {
        if (active) {
            if (since < 0)
                since = nowMs;
            seen = nowMs;
        } else if (since >= 0 && nowMs - seen > graceMs) {
            since = -1;
        }
        return since >= 0 ? nowMs - since : 0;
    };
    const bool coldNow = (expression == QLatin1String("tense") || expression == QLatin1String("unapproachable"))
                         && (localExpr || exprConf >= kMinConfidence);
    const bool flatNow = expression == QLatin1String("neutral_flat") && exprConf >= kMinConfidence;
    const qint64 notesBefore = m_notesSince >= 0 ? nowMs - m_notesSince : 0;
    const qint64 side = steady(m_sideSince, m_sideSeen, gaze == 2, 800);
    const qint64 notes = steady(m_notesSince, m_notesSeen, gaze == 1, 800);
    if (gaze != 1 && notesBefore > 600)
        ++m_notesGlances;
    const qint64 cold = track(m_coldSince, coldNow);
    const qint64 flat = track(m_flatSince, flatNow);
    const qint64 restless = track(m_restlessSince, movement > kRestless);
    // Face covered: the cloud sees a hand/object over it, or the local tracker
    // lost the face while the cloud still sees the person
    const bool cloudCovered = fresh && m_cloud.value(QLatin1String("face_covered")).toBool();
    const bool trackerLost = localFresh && face == 0 && !turnedAway && fresh && m_cloud.value(QLatin1String("face_visible")).toBool(true);
    const bool coveredNow = cloudCovered || trackerLost;
    const bool wasCovered = m_coveredSince >= 0;
    steady(m_coveredSince, m_coveredSeen, coveredNow, 2500);
    // Once the cloud confirms, count from the moment the tracker lost the face
    if (!wasCovered && trackerLost && m_lastFoundAt >= 0 && nowMs - m_lastFoundAt < 8000)
        m_coveredSince = qMin(m_coveredSince, m_lastFoundAt);
    const qint64 covered = m_coveredSince >= 0 ? nowMs - m_coveredSince : 0;
    const qint64 missing = steady(m_faceMissingSince, m_missingSeen, face == 0 && !coveredNow && !turnedAway, 1000);
    v.faceMissingSeconds = int(missing / 1000);
    v.lookAwaySeconds = int(qMax(side, notes) / 1000);

    if (covered >= t.faceCoveredSeconds * 1000LL && nowMs - m_coveredSeen < 1000) {
        tip(QStringLiteral("covered"), TipCategory::Appearance, TipSeverity::Warning, tr("Your face is covered."),
            cloudCovered ? tr("The cloud AI sees something over your face (%1). Covered for %2 s (limit %3 s).")
                               .arg(m_cloud.value(QLatin1String("hands_note")).toString())
                               .arg(secs(covered))
                               .arg(t.faceCoveredSeconds)
                         : tr("The face tracker lost your face while the cloud AI still sees you: something is in "
                              "front of it. %1 s (limit %2 s).")
                               .arg(secs(covered))
                               .arg(t.faceCoveredSeconds));
        note(QStringLiteral("covered"), tr("Face covered for %1 s").arg(secs(covered)));
        m_coveredSince = nowMs; // count again from here if it stays covered
    }
    if (missing >= t.faceMissingSeconds * 1000LL && face == 0)
        tip(QStringLiteral("noface"), TipCategory::Appearance, TipSeverity::Warning, tr("I can't see your face. Check your camera framing."),
            localFresh ? tr("The local face tracker (15 checks a second) has found no face for %1 s (limit %2 s).")
                             .arg(secs(missing)).arg(t.faceMissingSeconds)
                       : tr("The cloud AI hasn't seen your face for %1 s.").arg(secs(missing)));

    // Eye contact, in context: side looks are flagged after the limit; looking
    // down at notes is allowed for much longer.
    if (side >= t.lookAwaySeconds * 1000LL && gaze == 2) {
        tip(QStringLiteral("eyes"), TipCategory::Expression, TipSeverity::Info, tr("Bring your eyes back to the camera."),
            tr("You've been looking to the side for %1 s (limit %2 s; brief glances are ignored). Head yaw %3, limit ±%4.")
                .arg(secs(side)).arg(t.lookAwaySeconds).arg(m_face.yaw, 0, 'f', 2).arg(kYawLimit, 0, 'f', 2));
        note(QStringLiteral("eye contact"), tr("Looked away for %1 s").arg(secs(side)));
        m_sideSince = nowMs;
    }
    const qint64 notesLimit = qMax<qint64>(12000, t.lookAwaySeconds * 3000LL);
    if (notes >= notesLimit && gaze == 1) {
        tip(QStringLiteral("notes"), TipCategory::Expression, TipSeverity::Info,
            tr("Look up from your notes and connect with the camera."),
            tr("Looking down for %1 s. Reading notes is fine, but after %2 s it starts to feel like reading aloud.")
                .arg(secs(notes)).arg(notesLimit / 1000));
        note(QStringLiteral("eye contact"), tr("Read notes for %1 s without looking up").arg(secs(notes)));
        m_notesSince = nowMs;
    }

    // Expression: never punish a calm face. Only a tense or cold look that
    // lasts, seen confidently by the cloud AI, gets a tip.
    if (cold >= t.noSmileSeconds * 1000LL) {
        const bool tense = expression == QLatin1String("tense");
        tip(QStringLiteral("cold"), TipCategory::Expression, TipSeverity::Info,
            tense ? tr("You look a little tense. Relax your face and breathe.")
                  : tr("Soften your expression. A slight smile makes you more approachable."),
            localExpr ? tr("The expression model on your computer has read \"%1\" for %2 s (limit %3 s): negative %4, "
                           "smile %5, neutral %6.")
                            .arg(pretty(expression)).arg(secs(cold)).arg(t.noSmileSeconds)
                            .arg(m_negativeEma, 0, 'f', 2).arg(m_happyEma, 0, 'f', 2).arg(m_neutralEma, 0, 'f', 2)
                      : tr("The cloud AI has rated your expression \"%1\" for %2 s (limit %3 s, confidence %4). It said: \"%5\"")
                            .arg(pretty(expression)).arg(secs(cold)).arg(t.noSmileSeconds)
                            .arg(exprConf, 0, 'f', 2).arg(m_cloud.value(QLatin1String("observation")).toString()));
        note(QStringLiteral("expression"), tr("Looked %1 for %2 s").arg(pretty(expression)).arg(secs(cold)));
        m_coldSince = nowMs;
    }
    // Not smiling for a long time: counted only while you're on camera, reset by any smile
    if (face == 1 && smiling == 0)
        m_noSmileMs += kTickMs;
    else if (smiling == 1)
        m_noSmileMs = 0;
    v.noSmileSeconds = int(m_noSmileMs / 1000);
    const qint64 noSmileLimit = t.noSmileSeconds * 2000LL;
    if (m_noSmileMs >= noSmileLimit) {
        tip(QStringLiteral("nosmile"), TipCategory::Expression, TipSeverity::Info, tr("Try smiling a little more."),
            tr("No smile for %1 s on camera (limit %2 s). %3")
                .arg(m_noSmileMs / 1000)
                .arg(noSmileLimit / 1000)
                .arg(localExpr ? tr("Smile reading from the expression model on your computer: %1.").arg(m_happyEma, 0, 'f', 2)
                               : tr("Smile reading from the cloud AI.")));
        note(QStringLiteral("expression"), tr("No smile for %1 s").arg(m_noSmileMs / 1000));
        m_noSmileMs = 0;
    }
    Q_UNUSED(flat);

    if (restless >= 6000) {
        tip(QStringLiteral("restless"), TipCategory::Posture, TipSeverity::Info,
            tr("You're moving around quite a lot. Try to stay settled."),
            tr("Your head has moved %1 face-heights per second for %2 s (limit %3, local tracker).")
                .arg(movement, 0, 'f', 2).arg(secs(restless)).arg(kRestless, 0, 'f', 2));
        note(QStringLiteral("movement"), tr("Restless for %1 s").arg(secs(restless)));
        m_restlessSince = nowMs;
    }

    if (m_postureSince >= 0 && nowMs - m_postureSince >= t.postureSeconds * 1000LL) {
        static const QHash<QString, const char *> texts = {
            {QStringLiteral("slouching"), QT_TR_NOOP("Sit up straight. You're slouching a little.")},
            {QStringLiteral("leaning_in"), QT_TR_NOOP("You're leaning towards the screen. Sit back a little.")},
            {QStringLiteral("leaning_back"), QT_TR_NOOP("You're leaning back. Sit up to look more engaged.")},
            {QStringLiteral("head_tilted"), QT_TR_NOOP("Your head is tilted. Level it with the camera.")},
        };
        if (texts.contains(m_posture)) {
            tip(QStringLiteral("posture"), TipCategory::Posture, TipSeverity::Warning, tr(texts.value(m_posture)),
                tr("The cloud AI has seen \"%1\" for %2 s (limit %3 s, confidence %4). It said: \"%5\"")
                    .arg(pretty(m_posture)).arg(secs(nowMs - m_postureSince)).arg(t.postureSeconds)
                    .arg(confidence("posture"), 0, 'f', 2).arg(m_cloud.value(QLatin1String("observation")).toString()));
            note(QStringLiteral("posture"), tr("%1 for %2 s").arg(pretty(m_posture)).arg(secs(nowMs - m_postureSince)));
        }
        m_postureSince = nowMs;
    }

    // --- Time-weighted statistics
    if (face >= 0) {
        m_trackedMs += kTickMs;
        if (face == 1)
            m_faceMs += kTickMs;
        // Eye contact: brief glances (and reading notes up to the limit) don't count against you
        if (gaze == 0) {
            m_lookingMs += kTickMs;
            m_currentAwayMs = 0;
        } else if (gaze > 0) {
            m_currentAwayMs += kTickMs;
            const qint64 allowance = gaze == 1 ? notesLimit : kBriefGlanceMs;
            if (m_currentAwayMs > allowance)
                m_awayCountedMs += kTickMs;
            else
                m_lookingMs += kTickMs; // benefit of the doubt
        }
        if (smiling >= 0) {
            m_smileKnownMs += kTickMs;
            if (smiling == 1)
                m_smileMs += kTickMs;
        }
        if (!expression.isEmpty()) {
            m_expressionKnownMs += kTickMs;
            m_expressionScoreMs += expressionScore(expression) * double(kTickMs);
            if (m_cloud.value(QLatin1String("relaxed")).toBool(true))
                m_relaxedMs += kTickMs;
        }
        if (movement > kRestless)
            m_restlessMs += kTickMs;
        if (covered > 0)
            m_coveredMs += kTickMs;
        // Smile score: a real smile ~100, a neutral engaged face ~45, and a
        // little lower when you look away for long
        if (localExpr) {
            double score = 100.0 * m_happyEma + 45.0 * m_neutralEma + 15.0 * m_negativeEma;
            score += 50.0 * qMax(0.0, 1.0 - m_happyEma - m_neutralEma - m_negativeEma); // surprise
            if (gaze == 2)
                score *= 0.85;
            m_smileScoreMs += qBound(0.0, score, 100.0) * kTickMs;
            m_smileScoreKnownMs += kTickMs;
        } else if (!expression.isEmpty()) {
            m_smileScoreMs += (expression == QLatin1String("warm") ? 95.0 : expression == QLatin1String("positive") ? 75.0
                               : expression.startsWith(QLatin1String("neutral")) ? 45.0 : 20.0) * kTickMs;
            m_smileScoreKnownMs += kTickMs;
        }
        // Facial engagement in context: a good expression, attention on the
        // camera (or notes), and being in frame. No smile is required.
        const double exprPart = expression.isEmpty() ? 0.75 : expressionScore(expression) / 100.0;
        const double gazePart = gaze == 0 ? 1.0 : (gaze == 1 ? 0.8 : 0.3);
        const double engagement = face == 1 ? 0.45 * exprPart + 0.4 * gazePart + 0.15 : 0.0;
        v.engagement = float(engagement);
        m_engagementMs += engagement * kTickMs;
    }
    LiveSignals::instance().notifyChanged();
}

void VisionTipSource::alert(const QString &title, const QString &detail)
{
    LiveSignals::instance().addEvent(LiveSignals::Event::Alert, title, detail);
}

bool VisionTipSource::isWarningActive(const QString &warningId) const
{
    if (!m_running || !m_camera->isActive() || !AppSettings::instance().useCamera())
        return false;
    if (warningId == QLatin1String("noface"))
        return m_faceMissingSince >= 0;
    if (warningId == QLatin1String("covered"))
        return m_coveredSince >= 0;
    if (!AppSettings::instance().cloudAnalysis() || !cloudFresh())
        return false;
    if (warningId == QLatin1String("posture"))
        return !m_posture.isEmpty();
    return warningId == QLatin1String("hands-inappropriate") && m_streak.value(warningId) >= 2;
}

void VisionTipSource::tip(const QString &key, TipCategory category, TipSeverity severity, const QString &text,
                          const QString &reason)
{
    QElapsedTimer &last = m_lastTip[key];
    if (last.isValid() && last.elapsed() < repeatCooldownMs(key))
        return;
    last.restart();

    Tip t;
    t.category = category;
    t.severity = severity;
    t.text = text;
    t.reason = reason;
    t.key = key;
    t.source = name();
    if (severity == TipSeverity::Warning)
        t.warningId = key;
    emit tipRaised(t);
}

QVariantMap VisionTipSource::reportData() const
{
    if (m_trackedMs == 0 && m_cloudChecks == 0)
        return {};
    auto pct = [](qint64 n, qint64 of) { return of > 0 ? int(qRound(100.0 * double(n) / double(of))) : -1; };
    auto toMap = [](const QMap<QString, int> &m) {
        QVariantMap out;
        for (auto it = m.cbegin(); it != m.cend(); ++it)
            out.insert(it.key(), it.value());
        return out;
    };
    QVariantList events;
    for (const QVariantMap &e : m_events)
        events << e;
    // Framing consistency: steady distance (face size) and position
    int framing = -1;
    if (m_heights.size() >= 10) {
        auto stats = [](const QList<float> &v, double *mean) {
            double m = 0, var = 0;
            for (float x : v)
                m += x;
            m /= v.size();
            for (float x : v)
                var += (x - m) * (x - m);
            *mean = m;
            return std::sqrt(var / v.size());
        };
        double meanH = 0, meanX = 0;
        const double sdH = stats(m_heights, &meanH);
        const double sdX = stats(m_centres, &meanX);
        const double cv = meanH > 0 ? sdH / meanH : 0;
        framing = int(qBound(0.0, 100.0 - cv * 250.0 - sdX * 200.0 - std::abs(meanX - 0.5) * 80.0, 100.0));
    }
    return {
        {QStringLiteral("snapshots"), m_cloudChecks},
        {QStringLiteral("trackedSeconds"), int(m_trackedMs / 1000)},
        {QStringLiteral("faceVisiblePct"), pct(m_faceMs, m_trackedMs)},
        {QStringLiteral("eyeContactPct"), pct(m_lookingMs, m_lookingMs + m_awayCountedMs)},
        {QStringLiteral("notesGlances"), m_notesGlances},
        {QStringLiteral("smilingPct"), pct(m_smileMs, m_smileKnownMs)},
        {QStringLiteral("expressionScore"), m_expressionKnownMs > 0 ? int(qRound(m_expressionScoreMs / double(m_expressionKnownMs))) : -1},
        {QStringLiteral("relaxedPct"), pct(m_relaxedMs, m_expressionKnownMs)},
        {QStringLiteral("engagementPct"), m_trackedMs > 0 ? int(qRound(100.0 * m_engagementMs / double(m_trackedMs))) : -1},
        {QStringLiteral("uprightPct"), pct(m_cloudUpright, m_cloudFaces)},
        {QStringLiteral("restlessPct"), pct(m_restlessMs, m_faceMs)},
        {QStringLiteral("coveredSeconds"), int(m_coveredMs / 1000)},
        {QStringLiteral("coveredPct"), pct(m_coveredMs, m_trackedMs)},
        {QStringLiteral("smileScore"), m_smileScoreKnownMs > 0 ? int(qRound(m_smileScoreMs / double(m_smileScoreKnownMs))) : -1},
        {QStringLiteral("framingScore"), framing},
        {QStringLiteral("localExpression"), m_smileScoreKnownMs > 0 && m_camera->faceTrackingAvailable()},
        {QStringLiteral("discomfortChecks"), m_discomfortChecks},
        {QStringLiteral("postures"), toMap(m_postures)},
        {QStringLiteral("expressions"), toMap(m_expressions)},
        {QStringLiteral("hands"), toMap(m_hands)},
        {QStringLiteral("events"), events},
        {QStringLiteral("localTracking"), m_camera->faceTrackingAvailable()},
        {QStringLiteral("avgCloudLatencyMs"), m_cloudChecks ? int(m_cloudLatencyTotal / m_cloudChecks) : 0},
        {QStringLiteral("lastError"), m_lastError},
    };
}
