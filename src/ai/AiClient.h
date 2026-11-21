#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <functional>

class QImage;
class QNetworkAccessManager;
class QNetworkReply;

// Talks to an OpenAI-compatible API (Together AI by default) for the three
// things SMILE needs: describe a camera snapshot, transcribe speech, and write
// feedback. Uses the key from AppSettings; nothing is sent without one.
class AiClient : public QObject
{
    Q_OBJECT

public:
    explicit AiClient(QObject *parent = nullptr);

    bool isConfigured() const;

    using TextCallback = std::function<void(const QString &text, const QString &error)>;
    using JsonCallback = std::function<void(const QJsonObject &json, const QString &error)>;

    // Free-form chat completion.
    void chat(const QString &system, const QString &user, int maxTokens, TextCallback done);

    // Ask the vision model about an image; the reply is parsed as JSON.
    // fast: always the quick standard model with a short timeout (setup check),
    // whatever the intelligence setting
    void analyzeImage(const QImage &image, const QString &prompt, JsonCallback done, bool fast = false);

    // Transcribe a WAV file (16 kHz mono).
    void transcribe(const QByteArray &wav, TextCallback done);

    // Text to speech (WAV, 16-bit mono 24 kHz) for the simulation avatar.
    using AudioCallback = std::function<void(const QByteArray &wav, const QString &error)>;
    void speak(const QString &text, const QString &voice, AudioCallback done);

    // Chat with a full message history (role/content objects).
    void chatMessages(const QJsonArray &messages, int maxTokens, TextCallback done);

    // Quick check that the key works.
    void testConnection(TextCallback done);

    // Pull the first {...} object out of a model reply (handles ```json fences).
    static QJsonObject extractJson(const QString &text);

private:
    QNetworkReply *post(const QString &path, const QJsonObject &body);
    static QString errorFrom(QNetworkReply *reply, const QByteArray &body);

    QNetworkAccessManager *m_net;
};
