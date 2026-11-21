#include "ai/AiClient.h"

#include <QBuffer>
#include <QHttpMultiPart>
#include <QImage>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "core/AppSettings.h"

namespace {
constexpr int kTimeoutMs = 45000;

QNetworkRequest makeRequest(const QString &path)
{
    const AppSettings &s = AppSettings::instance();
    QNetworkRequest request(QUrl(s.apiBaseUrl() + path));
    request.setRawHeader("Authorization", "Bearer " + s.apiKey().toUtf8());
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("SMILE/" SMILE_VERSION));
    request.setTransferTimeout(kTimeoutMs);
    return request;
}

QString messageText(const QJsonObject &response)
{
    const QJsonObject message = response.value(QLatin1String("choices")).toArray().at(0).toObject()
                                    .value(QLatin1String("message")).toObject();
    return message.value(QLatin1String("content")).toString();
}
} // namespace

AiClient::AiClient(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
}

bool AiClient::isConfigured() const
{
    return !AppSettings::instance().apiKey().isEmpty();
}

QString AiClient::errorFrom(QNetworkReply *reply, const QByteArray &body)
{
    const QJsonObject json = QJsonDocument::fromJson(body).object();
    QString message = json.value(QLatin1String("error")).toObject().value(QLatin1String("message")).toString();
    if (message.isEmpty())
        message = json.value(QLatin1String("message")).toString();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 401)
        return tr("The API key was rejected. Check it on the Settings page.");
    if (message.isEmpty())
        message = reply->errorString();
    return status ? tr("AI service error %1: %2").arg(status).arg(message) : message;
}

QNetworkReply *AiClient::post(const QString &path, const QJsonObject &body)
{
    QNetworkRequest request = makeRequest(path);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    return m_net->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
}

void AiClient::chat(const QString &system, const QString &user, int maxTokens, TextCallback done)
{
    if (!isConfigured()) {
        done(QString(), tr("No AI key set."));
        return;
    }
    const QString model = AppSettings::instance().chatModel();
    QJsonObject body{
        {QStringLiteral("model"), model},
        {QStringLiteral("max_tokens"), maxTokens},
        {QStringLiteral("temperature"), 0.4},
        {QStringLiteral("messages"),
         QJsonArray{QJsonObject{{QStringLiteral("role"), QStringLiteral("system")}, {QStringLiteral("content"), system}},
                    QJsonObject{{QStringLiteral("role"), QStringLiteral("user")}, {QStringLiteral("content"), user}}}},
    };
    if (model.contains(QLatin1String("gpt-oss")))
        body.insert(QStringLiteral("reasoning_effort"), QStringLiteral("low"));

    QNetworkReply *reply = post(QStringLiteral("/chat/completions"), body);
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        reply->deleteLater();
        const QByteArray data = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            done(QString(), errorFrom(reply, data));
            return;
        }
        const QString text = messageText(QJsonDocument::fromJson(data).object()).trimmed();
        done(text, text.isEmpty() ? tr("The AI returned an empty answer.") : QString());
    });
}

void AiClient::analyzeImage(const QImage &image, const QString &prompt, JsonCallback done)
{
    if (!isConfigured()) {
        done({}, tr("No AI key set."));
        return;
    }
    // Small JPEG: enough detail for posture and expression, fast and cheap.
    const QImage scaled = image.width() > 640 ? image.scaledToWidth(640, Qt::SmoothTransformation) : image;
    QByteArray jpeg;
    QBuffer buffer(&jpeg);
    buffer.open(QIODevice::WriteOnly);
    scaled.save(&buffer, "JPEG", 75);
    const QString dataUrl = QStringLiteral("data:image/jpeg;base64,") + QString::fromLatin1(jpeg.toBase64());

    const QJsonObject body{
        {QStringLiteral("model"), AppSettings::instance().visionModel()},
        {QStringLiteral("max_tokens"), 400},
        {QStringLiteral("temperature"), 0.1},
        // Snapshot checks need a quick answer, not deliberation: with reasoning
        // on, the model spends its whole budget thinking (and takes 3x longer).
        {QStringLiteral("reasoning"), QJsonObject{{QStringLiteral("enabled"), false}}},
        {QStringLiteral("messages"),
         QJsonArray{QJsonObject{
             {QStringLiteral("role"), QStringLiteral("user")},
             {QStringLiteral("content"),
              QJsonArray{QJsonObject{{QStringLiteral("type"), QStringLiteral("text")}, {QStringLiteral("text"), prompt}},
                         QJsonObject{{QStringLiteral("type"), QStringLiteral("image_url")},
                                     {QStringLiteral("image_url"), QJsonObject{{QStringLiteral("url"), dataUrl}}}}}},
         }}},
    };

    QNetworkReply *reply = post(QStringLiteral("/chat/completions"), body);
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        reply->deleteLater();
        const QByteArray data = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            done({}, errorFrom(reply, data));
            return;
        }
        const QJsonObject json = extractJson(messageText(QJsonDocument::fromJson(data).object()));
        done(json, json.isEmpty() ? tr("Could not read the AI's answer.") : QString());
    });
}

void AiClient::transcribe(const QByteArray &wav, TextCallback done)
{
    if (!isConfigured()) {
        done(QString(), tr("No AI key set."));
        return;
    }
    auto *multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    auto field = [multi](const QString &name, const QByteArray &value) {
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QStringLiteral("form-data; name=\"%1\"").arg(name));
        part.setBody(value);
        multi->append(part);
    };
    field(QStringLiteral("model"), AppSettings::instance().transcribeModel().toUtf8());
    field(QStringLiteral("language"), "en");
    // Whisper tends to tidy away disfluencies; a prompt full of them keeps them.
    field(QStringLiteral("prompt"), "Umm, so, uh, I was like, hmm, you know... Okay, um, here's what I mean.");
    QHttpPart file;
    file.setHeader(QNetworkRequest::ContentDispositionHeader,
                   QStringLiteral("form-data; name=\"file\"; filename=\"speech.wav\""));
    file.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("audio/wav"));
    file.setBody(wav);
    multi->append(file);

    QNetworkReply *reply = m_net->post(makeRequest(QStringLiteral("/audio/transcriptions")), multi);
    multi->setParent(reply);
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        reply->deleteLater();
        const QByteArray data = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            done(QString(), errorFrom(reply, data));
            return;
        }
        done(QJsonDocument::fromJson(data).object().value(QLatin1String("text")).toString().trimmed(), QString());
    });
}

void AiClient::speak(const QString &text, const QString &voice, AudioCallback done)
{
    if (!isConfigured()) {
        done({}, tr("No AI key set."));
        return;
    }
    const QJsonObject body{
        {QStringLiteral("model"), AppSettings::instance().speechModel()},
        {QStringLiteral("input"), text},
        {QStringLiteral("voice"), voice},
        {QStringLiteral("response_format"), QStringLiteral("wav")},
        {QStringLiteral("sample_rate"), 24000},
    };
    QNetworkReply *reply = post(QStringLiteral("/audio/speech"), body);
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        reply->deleteLater();
        const QByteArray data = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            done({}, errorFrom(reply, data));
            return;
        }
        done(data, data.startsWith("RIFF") ? QString() : tr("Unexpected audio format from the AI."));
    });
}

void AiClient::chatMessages(const QJsonArray &messages, int maxTokens, TextCallback done)
{
    if (!isConfigured()) {
        done(QString(), tr("No AI key set."));
        return;
    }
    const QString model = AppSettings::instance().chatModel();
    QJsonObject body{{QStringLiteral("model"), model},
                     {QStringLiteral("max_tokens"), maxTokens},
                     {QStringLiteral("temperature"), 0.8},
                     {QStringLiteral("messages"), messages}};
    if (model.contains(QLatin1String("gpt-oss")))
        body.insert(QStringLiteral("reasoning_effort"), QStringLiteral("low"));
    QNetworkReply *reply = post(QStringLiteral("/chat/completions"), body);
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        reply->deleteLater();
        const QByteArray data = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            done(QString(), errorFrom(reply, data));
            return;
        }
        const QString text = messageText(QJsonDocument::fromJson(data).object()).trimmed();
        done(text, text.isEmpty() ? tr("The AI returned an empty answer.") : QString());
    });
}

void AiClient::testConnection(TextCallback done)
{
    chat(QStringLiteral("You are a connection test."), QStringLiteral("Reply with the single word: ready"), 200,
         [done](const QString &text, const QString &error) {
             done(error.isEmpty() ? tr("Connected. The AI answered: \"%1\"").arg(text.left(40)) : QString(), error);
         });
}

QJsonObject AiClient::extractJson(const QString &text)
{
    const int start = text.indexOf(QLatin1Char('{'));
    const int end = text.lastIndexOf(QLatin1Char('}'));
    if (start < 0 || end <= start)
        return {};
    return QJsonDocument::fromJson(text.mid(start, end - start + 1).toUtf8()).object();
}
