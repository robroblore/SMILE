#include "ai/VisionTipSource.h"

#include "ai/AiClient.h"
#include "core/AppSettings.h"
#include "sensors/CameraMonitor.h"

VisionTipSource::VisionTipSource(CameraMonitor *camera, AiClient *ai, QObject *parent)
    : TipSource(parent)
    , m_camera(camera)
    , m_ai(ai)
{
    connect(&m_timer, &QTimer::timeout, this, &VisionTipSource::analyze);
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
        "  \"clothing_messy\": true|false      // crooked collar, untidy clothes\n"
        "}");
}

void VisionTipSource::start()
{
    m_running = true;
    m_busy = false;
    m_streak.clear();
    m_lastTip.clear();
    m_postureTipped.clear();
    m_samples = m_faces = m_smiles = m_eyeContact = m_upright = 0;
    m_postures.clear();
    m_expressions.clear();
    m_lastError.clear();

    if (!AppSettings::instance().useCamera())
        return;
    m_camera->acquire();
    m_acquired = true;
    m_timer.start(AppSettings::instance().snapshotIntervalSeconds() * 1000);
    QTimer::singleShot(4000, this, &VisionTipSource::analyze); // first look soon after starting
}

void VisionTipSource::stop()
{
    m_running = false;
    m_timer.stop();
    if (m_acquired) {
        m_camera->release();
        m_acquired = false;
    }
}

void VisionTipSource::analyze()
{
    if (!m_running || m_busy)
        return;
    const QImage frame = m_camera->latestFrame();
    if (frame.isNull())
        return;

    const bool cloud = m_ai->isConfigured() && AppSettings::instance().cloudAnalysis();
    if (!cloud) {
        handleLightingOnly();
        return;
    }

    m_busy = true;
    m_ai->analyzeImage(frame, analysisPrompt(), [this](const QJsonObject &result, const QString &error) {
        m_busy = false;
        if (!m_running)
            return;
        if (!error.isEmpty()) {
            m_lastError = error;
            handleLightingOnly();
            return;
        }
        handle(result);
    });
}

void VisionTipSource::handleLightingOnly()
{
    const CameraMonitor::Lighting light = CameraMonitor::analyzeLighting(m_camera->latestFrame());
    const bool bad = light.tooDark || light.backlit;
    m_streak[QStringLiteral("lighting")] = bad ? m_streak.value(QStringLiteral("lighting")) + 1 : 0;
    if (m_streak.value(QStringLiteral("lighting")) >= 3) {
        tip(QStringLiteral("lighting"), TipCategory::Appearance, TipSeverity::Info,
            light.backlit ? tr("You're backlit. Face the light, or close the blinds behind you.")
                          : tr("It's quite dark. Add some light in front of you."),
            300);
    }
}

void VisionTipSource::handle(const QJsonObject &r)
{
    auto streak = [this](const QString &key, bool condition) {
        const int n = condition ? m_streak.value(key) + 1 : 0;
        m_streak[key] = n;
        return n;
    };

    ++m_samples;
    const bool face = r.value(QLatin1String("face_visible")).toBool(true);
    if (streak(QStringLiteral("noface"), !face) >= 3)
        tip(QStringLiteral("noface"), TipCategory::Appearance, TipSeverity::Warning,
            tr("I can't see your face. Check your camera framing."), 180);
    if (!face)
        return;
    ++m_faces;

    // Expression
    const bool smiling = r.value(QLatin1String("smiling")).toBool();
    const QString expression = r.value(QLatin1String("expression")).toString(QStringLiteral("neutral"));
    m_smiles += smiling ? 1 : 0;
    ++m_expressions[expression];
    if (streak(QStringLiteral("nosmile"), !smiling) >= 4) {
        tip(QStringLiteral("nosmile"), TipCategory::Expression, TipSeverity::Info, tr("Try smiling more."), 150);
        m_streak[QStringLiteral("nosmile")] = 0;
    }
    if (streak(QStringLiteral("tense"), expression == QLatin1String("tense")) >= 2)
        tip(QStringLiteral("tense"), TipCategory::Expression, TipSeverity::Info,
            tr("You look a little tense. Relax your face and take a breath."), 180);

    const bool eyes = r.value(QLatin1String("looking_at_camera")).toBool(true);
    m_eyeContact += eyes ? 1 : 0;
    if (streak(QStringLiteral("eyes"), !eyes) >= 3)
        tip(QStringLiteral("eyes"), TipCategory::Expression, TipSeverity::Info,
            tr("Look at the camera now and then, not just at the screen."), 180);

    // Posture
    const QString posture = r.value(QLatin1String("posture")).toString(QStringLiteral("unknown"));
    ++m_postures[posture];
    const bool upright = posture == QLatin1String("upright") || posture == QLatin1String("unknown");
    m_upright += upright ? 1 : 0;
    if (streak(QStringLiteral("posture:") + posture, !upright) >= 2) {
        static const QHash<QString, const char *> texts = {
            {QStringLiteral("slouching"), QT_TR_NOOP("Sit up straight. You're slouching a little.")},
            {QStringLiteral("leaning_in"), QT_TR_NOOP("You're leaning towards the screen. Sit back a little.")},
            {QStringLiteral("leaning_back"), QT_TR_NOOP("You're leaning back. Sit up to look more engaged.")},
            {QStringLiteral("head_tilted"), QT_TR_NOOP("Your head is tilted. Level it with the camera.")},
        };
        if (texts.contains(posture)) {
            tip(QStringLiteral("posture"), TipCategory::Posture, TipSeverity::Warning, tr(texts.value(posture)), 120);
            m_postureTipped = posture;
        }
    }
    if (upright && !m_postureTipped.isEmpty()) {
        tip(QStringLiteral("posture-fixed"), TipCategory::Posture, TipSeverity::Positive,
            tr("Much better posture. Nice."), 60);
        m_postureTipped.clear();
    }
    if (streak(QStringLiteral("shoulders"), r.value(QLatin1String("shoulders_tense")).toBool()) >= 2)
        tip(QStringLiteral("shoulders"), TipCategory::Posture, TipSeverity::Info,
            tr("Drop your shoulders and relax."), 180);
    if (streak(QStringLiteral("touching"), r.value(QLatin1String("touching_face")).toBool()) >= 2)
        tip(QStringLiteral("touching"), TipCategory::Posture, TipSeverity::Info,
            tr("Try to keep your hands away from your face."), 180);

    // Appearance
    const QString lighting = r.value(QLatin1String("lighting")).toString(QStringLiteral("good"));
    if (streak(QStringLiteral("lighting"), lighting == QLatin1String("dark") || lighting == QLatin1String("backlit")) >= 3)
        tip(QStringLiteral("lighting"), TipCategory::Appearance, TipSeverity::Info,
            lighting == QLatin1String("backlit") ? tr("You're backlit. Face the light, or close the blinds behind you.")
                                                 : tr("It's quite dark. Add some light in front of you."),
            300);
    if (streak(QStringLiteral("clothing"), r.value(QLatin1String("clothing_messy")).toBool()) >= 2)
        tip(QStringLiteral("clothing"), TipCategory::Appearance, TipSeverity::Info,
            tr("Your collar looks out of place. Straighten it when you can."), 600);
}

void VisionTipSource::tip(const QString &key, TipCategory category, TipSeverity severity, const QString &text,
                          int cooldownS)
{
    QElapsedTimer &last = m_lastTip[key];
    if (last.isValid() && last.elapsed() < cooldownS * 1000LL)
        return;
    last.restart();

    Tip t;
    t.category = category;
    t.severity = severity;
    t.text = text;
    t.source = name();
    emit tipRaised(t);
}

QVariantMap VisionTipSource::reportData() const
{
    if (m_samples == 0)
        return {};
    auto pct = [](int n, int of) { return of > 0 ? qRound(100.0 * n / of) : 0; };
    QVariantMap postures;
    for (auto it = m_postures.cbegin(); it != m_postures.cend(); ++it)
        postures.insert(it.key(), it.value());
    QVariantMap expressions;
    for (auto it = m_expressions.cbegin(); it != m_expressions.cend(); ++it)
        expressions.insert(it.key(), it.value());
    return {
        {QStringLiteral("snapshots"), m_samples},
        {QStringLiteral("faceVisiblePct"), pct(m_faces, m_samples)},
        {QStringLiteral("smilingPct"), pct(m_smiles, m_faces)},
        {QStringLiteral("eyeContactPct"), pct(m_eyeContact, m_faces)},
        {QStringLiteral("uprightPct"), pct(m_upright, m_faces)},
        {QStringLiteral("postures"), postures},
        {QStringLiteral("expressions"), expressions},
        {QStringLiteral("lastError"), m_lastError},
    };
}
