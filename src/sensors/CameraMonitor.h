#pragma once

#include <QCameraDevice>
#include <QElapsedTimer>
#include <QImage>
#include <QMediaCaptureSession>
#include <QObject>
#include <QThread>
#include <QVideoFrame>
#include <QVideoSink>
#include <atomic>
#include <memory>

#include "vision/FaceFrame.h"

class ExpressionModel;
class FaceDetector;
class QCamera;

// Does the heavy work off the UI thread: converts camera frames for display
// and runs the local face tracker on them.
class FrameWorker : public QObject
{
    Q_OBJECT
public:
    explicit FrameWorker(bool trackFaces);
    ~FrameWorker() override;
    std::atomic<bool> busy{false};
    bool faceTrackingAvailable() const { return m_available; }

public slots:
    void process(const QVideoFrame &frame);
    void processImage(const QImage &image); // for tests and still images

signals:
    void imageReady(const QImage &image, float convertMs);
    void faceReady(const FaceFrame &face);

private:
    void detect(const QImage &image);

    const bool m_trackFaces;
    std::unique_ptr<FaceDetector> m_detector; // created on the worker thread
    std::unique_ptr<ExpressionModel> m_expression;
    QElapsedTimer m_lastExpression;
    FaceFrame m_lastExpressionFrame;
    std::atomic<bool> m_available{false};
    QElapsedTimer m_lastDetect;
    QElapsedTimer m_clock;
};

// Reads frames from the webcam. It only ever reads: SMILE never modifies or
// re-broadcasts your video. The camera runs while at least one part of the
// app has acquired it (preview, Pre-Meet check, a running session).
class CameraMonitor : public QObject
{
    Q_OBJECT

public:
    enum class Input { Coaching, LogiInsightInfrared };
    enum class DeviceKind { Standard, LogiInsightColor, LogiInsightInfrared, LogiInsightHello };

    explicit CameraMonitor(QObject *parent = nullptr, Input input = Input::Coaching);
    static DeviceKind deviceKind(const QCameraDevice &device);
    static QByteArray deviceKey(const QByteArray &id);
    static QList<QCameraDevice> videoInputs();
    static bool hasLogiInsightInfrared();
    ~CameraMonitor() override;

    void acquire();
    void release();
    bool isActive() const { return m_active; }
    QString errorString() const { return m_error; }
    bool faceTrackingAvailable() const;

    // Latest frame, mirrored like a selfie view. Null if none yet.
    QImage latestFrame() const { return m_latest; }
    FaceFrame latestFace() const { return m_face; }
    float framesPerSecond() const { return m_fps; }
    float convertMs() const { return m_convertMs; }
    QString formatDescription() const { return m_formatText; }

    // Feed a still image through the same pipeline as camera frames
    // (conversion thread + face tracker). Used by tests and demos.
    void processStillImage(const QImage &image);

    struct Lighting {
        qreal brightness = 0; // 0..1 average
        bool tooDark = false;
        bool backlit = false; // centre (face) much darker than the edges
    };
    static Lighting analyzeLighting(const QImage &frame);

signals:
    void frameReady(const QImage &frame); // every camera frame (up to ~30 fps)
    void faceReady(const FaceFrame &face); // ~15 per second
    void activeChanged(bool active);
    void errorOccurred(const QString &message);

private:
    void startCamera();
    void stopCamera();
    void onFrame(const QVideoFrame &frame);
    void fail(const QString &message);

    const Input m_input;
    int m_users = 0;
    bool m_active = false;
    bool m_still = false;
    QCamera *m_camera = nullptr;
    QMediaCaptureSession m_session;
    QVideoSink m_sink;
    QThread m_thread;
    FrameWorker *m_worker;

    QImage m_latest;
    FaceFrame m_face;
    QString m_error;
    QString m_formatText;
    float m_fps = 0;
    float m_convertMs = 0;
    int m_frameCount = 0;
    QElapsedTimer m_fpsTimer;
};
