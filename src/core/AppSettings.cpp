#include "core/AppSettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>

namespace {
const char *kEnvKey = "TOGETHER_API_KEY";

QVariant value(const char *key, const QVariant &fallback)
{
    return QSettings().value(QLatin1String(key), fallback);
}

void store(const char *key, const QVariant &v)
{
    QSettings().setValue(QLatin1String(key), v);
}
} // namespace

AppSettings &AppSettings::instance()
{
    static AppSettings settings;
    return settings;
}

QString AppSettings::keyFromDotEnv() const
{
    // Look in the working directory and next to the executable (and its parent,
    // for build folders inside the project).
    const QStringList dirs = {QDir::currentPath(), QCoreApplication::applicationDirPath(),
                              QCoreApplication::applicationDirPath() + QStringLiteral("/..")};
    for (const QString &dir : dirs) {
        QFile file(dir + QStringLiteral("/.env"));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        while (!file.atEnd()) {
            const QString line = QString::fromUtf8(file.readLine()).trimmed();
            if (line.startsWith(QLatin1String(kEnvKey) + QLatin1Char('='))) {
                QString v = line.mid(int(qstrlen(kEnvKey)) + 1).trimmed();
                if (v.size() >= 2 && (v.startsWith(QLatin1Char('"')) || v.startsWith(QLatin1Char('\''))))
                    v = v.mid(1, v.size() - 2);
                if (!v.isEmpty())
                    return v;
            }
        }
    }
    return QString();
}

QString AppSettings::apiKey() const
{
    const QString stored = value("ai/apiKey", QString()).toString();
    if (!stored.isEmpty())
        return stored;
    const QString env = qEnvironmentVariable(kEnvKey);
    if (!env.isEmpty())
        return env;
    return keyFromDotEnv();
}

AppSettings::KeySource AppSettings::apiKeySource() const
{
    if (!value("ai/apiKey", QString()).toString().isEmpty())
        return KeySource::Settings;
    if (!qEnvironmentVariable(kEnvKey).isEmpty())
        return KeySource::Environment;
    if (!keyFromDotEnv().isEmpty())
        return KeySource::DotEnv;
    return KeySource::None;
}

void AppSettings::setApiKey(const QString &key)
{
    if (key.trimmed().isEmpty())
        QSettings().remove(QStringLiteral("ai/apiKey"));
    else
        store("ai/apiKey", key.trimmed());
    emit changed();
}

QString AppSettings::apiBaseUrl() const
{
    return value("ai/baseUrl", QStringLiteral("https://api.together.xyz/v1")).toString();
}

// Models per intelligence level. Standard is the fastest and cheapest; higher
// levels use larger models for vision, reviews and structuring. The live
// simulation conversation always uses a fast model so replies stay quick.
QString AppSettings::visionModel() const
{
    static const char *models[] = {"nvidia/NVIDIA-Nemotron-3.5-Super-VL", "moonshotai/Kimi-K3", "moonshotai/Kimi-K3.1-Pro"};
    return value("ai/visionModel", QString::fromLatin1(models[intelligence()])).toString();
}

QString AppSettings::chatModel() const
{
    static const char *models[] = {"openai/gpt-oss-120b", "moonshotai/Kimi-K3", "moonshotai/Kimi-K3.1-Pro"};
    return value("ai/chatModel", QString::fromLatin1(models[intelligence()])).toString();
}

QString AppSettings::dialogueModel() const
{
    return value("ai/dialogueModel", QStringLiteral("openai/gpt-oss-120b")).toString();
}

QString AppSettings::transcribeModel() const
{
    return value("ai/transcribeModel", QStringLiteral("openai/whisper-large-v3")).toString();
}

bool AppSettings::useCamera() const
{
    return value("sensors/camera", true).toBool();
}

void AppSettings::setUseCamera(bool on)
{
    if (useCamera() == on)
        return;
    store("sensors/camera", on);
    emit cameraSettingsChanged();
    emit changed();
}

bool AppSettings::useMicrophone() const
{
    return value("sensors/microphone", true).toBool();
}

void AppSettings::setUseMicrophone(bool on)
{
    store("sensors/microphone", on);
    emit changed();
}

bool AppSettings::cloudAnalysis() const
{
    return value("ai/cloudAnalysis", true).toBool();
}

void AppSettings::setCloudAnalysis(bool on)
{
    store("ai/cloudAnalysis", on);
    emit changed();
}

QByteArray AppSettings::cameraId() const
{
    return value("sensors/cameraId", QByteArray()).toByteArray();
}

void AppSettings::setCameraId(const QByteArray &id)
{
    if (cameraId() == id)
        return;
    store("sensors/cameraId", id);
    emit cameraSettingsChanged();
    emit changed();
}

QByteArray AppSettings::microphoneId() const
{
    return value("sensors/microphoneId", QByteArray()).toByteArray();
}

void AppSettings::setMicrophoneId(const QByteArray &id)
{
    store("sensors/microphoneId", id);
    emit changed();
}

int AppSettings::snapshotIntervalSeconds() const
{
    return qBound(1, value("ai/snapshotSeconds", 2).toInt(), 30);
}

void AppSettings::setSnapshotIntervalSeconds(int seconds)
{
    store("ai/snapshotSeconds", qBound(1, seconds, 30));
    emit changed();
}

int AppSettings::liveLevel() const
{
    return qBound(1, value("coach/liveLevel", 3).toInt(), 5);
}

void AppSettings::setLiveLevel(int level)
{
    store("coach/liveLevel", qBound(1, level, 5));
    emit changed();
}

int AppSettings::trainingLevel() const
{
    return qBound(1, value("coach/trainingLevel", 4).toInt(), 5);
}

void AppSettings::setTrainingLevel(int level)
{
    store("coach/trainingLevel", qBound(1, level, 5));
    emit changed();
}

int AppSettings::intelligence() const
{
    return qBound(0, value("ai/intelligence", 0).toInt(), 2);
}

void AppSettings::setIntelligence(int level)
{
    store("ai/intelligence", qBound(0, level, 2));
    emit changed();
}

int AppSettings::avatarIntervention() const
{
    return qBound(0, value("simulation/intervention", 2).toInt(), 3);
}

void AppSettings::setAvatarIntervention(int level)
{
    store("simulation/intervention", qBound(0, level, 3));
    emit changed();
}

AppSettings::Thresholds AppSettings::thresholdsFor(int level)
{
    const int i = qBound(1, level, 5) - 1;
    //                         relaxed  …          strict
    static const int cold[] = {45, 30, 20, 10, 3};   // no smile for twice this: 90…40 (medium)…6 s
    static const int look[] = {12, 8, 6, 4, 3};
    static const int face[] = {10, 6, 4, 3, 2};
    static const int posture[] = {20, 12, 8, 5, 4};
    static const int wpm[] = {190, 180, 175, 168, 162};
    static const int fillers[] = {8, 6, 4, 3, 2};
    static const int monologue[] = {300, 240, 180, 120, 90}; // talking without letting others in
    static const int repeat[] = {150, 90, 60, 40, 25};
    static const int meetingGap[] = {90, 60, 40, 25, 15}; // level 3: about one tip per 40 s
    static const int trainingGap[] = {40, 25, 15, 10, 6};
    static const int covered[] = {5, 3, 2, 2, 1};
    return {cold[i],      look[i],       face[i],       posture[i],        wpm[i],   fillers[i],
            monologue[i], repeat[i],     meetingGap[i], trainingGap[i],    covered[i]};
}

AppSettings::Thresholds AppSettings::thresholds() const
{
    return thresholdsFor(m_trainingActive ? trainingLevel() : liveLevel());
}

bool AppSettings::sectionTimer() const
{
    return value("coach/sectionTimer", false).toBool();
}

void AppSettings::setSectionTimer(bool on)
{
    store("coach/sectionTimer", on);
    emit changed();
}

bool AppSettings::autoAdvance() const
{
    return value("coach/autoAdvance", true).toBool();
}

void AppSettings::setAutoAdvance(bool on)
{
    store("coach/autoAdvance", on);
    emit changed();
}

int AppSettings::themeMode() const
{
    return qBound(0, value("appearance/theme", 0).toInt(), 2);
}

void AppSettings::setThemeMode(int mode)
{
    store("appearance/theme", mode);
    emit changed();
}

QString AppSettings::avatarVoice() const
{
    return value("simulation/voice", QStringLiteral("am_michael")).toString();
}

void AppSettings::setAvatarVoice(const QString &voice)
{
    store("simulation/voice", voice);
    emit changed();
}

QString AppSettings::speechModel() const
{
    return value("ai/speechModel", QStringLiteral("hexgrad/Kokoro-82M")).toString();
}

bool AppSettings::tipsMuted() const
{
    return value("coach/tipsMuted", false).toBool();
}

void AppSettings::setTipsMuted(bool muted)
{
    if (muted == tipsMuted())
        return;
    store("coach/tipsMuted", muted);
    emit changed();
}

bool AppSettings::debugMode() const
{
    return value("debug/enabled", false).toBool();
}

void AppSettings::setDebugMode(bool on)
{
    store("debug/enabled", on);
    emit changed();
}

bool AppSettings::physicalHapticsEnabled() const
{
    return value("haptics/physicalEnabled", false).toBool();
}

void AppSettings::setPhysicalHapticsEnabled(bool on)
{
    store("haptics/physicalEnabled", on);
    emit changed();
}

int AppSettings::hapticDurationSeconds() const
{
    return qBound(0, value("haptics/durationSeconds", 0).toInt(), 120);
}

void AppSettings::setHapticDurationSeconds(int seconds)
{
    store("haptics/durationSeconds", qBound(0, seconds, 120));
    emit changed();
}

int AppSettings::hapticIntensity() const
{
    return qBound(0, value("haptics/strength", 1).toInt(), 2);
}

void AppSettings::setHapticIntensity(int intensity)
{
    store("haptics/strength", qBound(0, intensity, 2));
    emit changed();
}

int AppSettings::hapticPulseIntervalMs() const
{
    const int milliseconds = value("haptics/pulseIntervalMs", 1000).toInt();
    return milliseconds == 500 || milliseconds == 3000 ? milliseconds : 1000;
}

void AppSettings::setHapticPulseIntervalMs(int milliseconds)
{
    if (milliseconds != 500 && milliseconds != 1000 && milliseconds != 3000)
        return;
    store("haptics/pulseIntervalMs", milliseconds);
    emit changed();
}

QString AppSettings::speechText() const
{
    return value("training/speech", QString()).toString();
}

void AppSettings::setSpeechText(const QString &text)
{
    store("training/speech", text);
}

bool AppSettings::speechAsBullets() const
{
    return value("training/bullets", false).toBool();
}

void AppSettings::setSpeechAsBullets(bool bullets)
{
    store("training/bullets", bullets);
}

int AppSettings::targetMinutes() const
{
    return qBound(1, value("training/targetMinutes", 8).toInt(), 180);
}

void AppSettings::setTargetMinutes(int minutes)
{
    store("training/targetMinutes", minutes);
}

int AppSettings::targetSeconds() const
{
    return qBound(30, value("training/targetSeconds", targetMinutes() * 60).toInt(), 6 * 3600);
}

void AppSettings::setTargetSeconds(int seconds)
{
    store("training/targetSeconds", seconds);
}

QString AppSettings::presentationType() const
{
    return value("training/type", QString()).toString();
}

void AppSettings::setPresentationType(const QString &type)
{
    store("training/type", type);
}

QByteArray AppSettings::agendaJson() const
{
    return value("training/agenda", QByteArray()).toByteArray();
}

void AppSettings::setAgendaJson(const QByteArray &json)
{
    store("training/agenda", json);
}
