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

QString AppSettings::visionModel() const
{
    return value("ai/visionModel", QStringLiteral("nvidia/NVIDIA-Nemotron-3.5-Super-VL")).toString();
}

QString AppSettings::chatModel() const
{
    return value("ai/chatModel", QStringLiteral("openai/gpt-oss-120b")).toString();
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
    store("sensors/camera", on);
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
    store("sensors/cameraId", id);
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
    return qBound(2, value("ai/snapshotSeconds", 3).toInt(), 30);
}

void AppSettings::setSnapshotIntervalSeconds(int seconds)
{
    store("ai/snapshotSeconds", qBound(2, seconds, 30));
    emit changed();
}

AppSettings::Sensitivity AppSettings::sensitivity() const
{
    return Sensitivity(qBound(0, value("coach/sensitivity", 1).toInt(), 2));
}

void AppSettings::setSensitivity(Sensitivity s)
{
    store("coach/sensitivity", int(s));
    emit changed();
}

AppSettings::Thresholds AppSettings::thresholds() const
{
    switch (sensitivity()) {
    case Sensitivity::Relaxed:
        return {40, 10, 8, 15, 185, 6, 180, 120, 40, 20};
    case Sensitivity::Responsive:
        return {10, 3, 3, 4, 165, 3, 90, 30, 6, 4};
    case Sensitivity::Balanced:
        break;
    }
    return {20, 6, 5, 8, 175, 4, 120, 60, 15, 8};
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

bool AppSettings::debugMode() const
{
    return value("debug/enabled", false).toBool();
}

void AppSettings::setDebugMode(bool on)
{
    store("debug/enabled", on);
    emit changed();
}

bool AppSettings::hapticFeedbackEnabled() const
{
    return value("haptics/enabled", false).toBool();
}

void AppSettings::setHapticFeedbackEnabled(bool on)
{
    store("haptics/enabled", on);
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
