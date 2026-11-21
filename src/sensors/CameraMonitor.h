#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QMediaCaptureSession>
#include <QObject>
#include <QVideoSink>

class QCamera;
class QVideoFrame;

// Reads frames from the webcam. It only ever reads: SMILE never modifies or
// re-broadcasts your video. The camera runs while at least one part of the
// app has acquired it (preview, Pre-Meet check, a running session).
class CameraMonitor : public QObject
{
    Q_OBJECT

public:
    explicit CameraMonitor(QObject *parent = nullptr);
    ~CameraMonitor() override;

    void acquire();
    void release();
    bool isActive() const { return m_active; }
    QString errorString() const { return m_error; }

    // Latest frame, already mirrored like a selfie view. Null if none yet.
    QImage latestFrame() const { return m_latest; }

    struct Lighting {
        qreal brightness = 0; // 0..1 average
        bool tooDark = false;
        bool backlit = false; // centre (face) much darker than the edges
    };
    static Lighting analyzeLighting(const QImage &frame);

signals:
    void frameReady(const QImage &frame); // ~10 per second
    void activeChanged(bool active);
    void errorOccurred(const QString &message);

private:
    void startCamera();
    void stopCamera();
    void onFrame(const QVideoFrame &frame);
    void fail(const QString &message);

    int m_users = 0;
    bool m_active = false;
    QCamera *m_camera = nullptr;
    QMediaCaptureSession m_session;
    QVideoSink m_sink;
    QImage m_latest;
    QElapsedTimer m_lastFrame;
    QString m_error;
};
