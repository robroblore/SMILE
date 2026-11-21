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
    return qBound(5, value("ai/snapshotInterval", 10).toInt(), 120);
}

void AppSettings::setSnapshotIntervalSeconds(int seconds)
{
    store("ai/snapshotInterval", qBound(5, seconds, 120));
    emit changed();
}
