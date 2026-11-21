#include "sensors/CameraMonitor.h"

#include <QCamera>
#include <QCameraDevice>
#include <QCoreApplication>
#include <QMediaDevices>
#include <QtGlobal>

#if QT_CONFIG(permissions)
#include <QPermissions>
#endif

#include "core/AppSettings.h"
#include "vision/FaceDetector.h"

namespace {
constexpr int kMaxWidth = 960;      // plenty for preview and analysis
constexpr int kDetectEveryMs = 66;  // ~15 face detections per second
} // namespace

// ---------------------------------------------------------------- FrameWorker

FrameWorker::FrameWorker()
{
    m_clock.start();
}

FrameWorker::~FrameWorker() = default;

void FrameWorker::process(const QVideoFrame &frame)
{
    QElapsedTimer t;
    t.start();
    QImage image = frame.toImage();
    if (!image.isNull()) {
        if (image.width() > kMaxWidth)
            image = image.scaledToWidth(kMaxWidth, Qt::SmoothTransformation);
        image = image.mirrored(true, false).convertToFormat(QImage::Format_RGB32);
        emit imageReady(image, float(t.nsecsElapsed() / 1e6));
        detect(image);
    }
    busy = false;
}

void FrameWorker::processImage(const QImage &image)
{
    emit imageReady(image, 0);
    m_lastDetect.invalidate();
    detect(image);
    busy = false;
}

void FrameWorker::detect(const QImage &image)
{
    if (m_lastDetect.isValid() && m_lastDetect.elapsed() < kDetectEveryMs)
        return;
    m_lastDetect.restart();
    if (!m_detector) {
        m_detector = std::make_unique<FaceDetector>();
        m_available = m_detector->isAvailable();
    }
    if (!m_available)
        return;

    QElapsedTimer t;
    t.start();
    const QList<FaceDetector::Face> faces = m_detector->detect(image);
    const float ms = float(t.nsecsElapsed() / 1e6);
    FaceFrame f;
    if (!faces.isEmpty())
        f = FaceFrame::from(faces.first(), image.size(), m_clock.elapsed(), ms);
    f.timestampMs = m_clock.elapsed();
    f.detectMs = ms;
    emit faceReady(f);
}

// --------------------------------------------------------------- CameraMonitor

CameraMonitor::CameraMonitor(QObject *parent)
    : QObject(parent)
    , m_worker(new FrameWorker)
{
    qRegisterMetaType<FaceFrame>();
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread.setObjectName(QStringLiteral("SMILE camera"));
    m_thread.start();

    m_session.setVideoSink(&m_sink);
    connect(&m_sink, &QVideoSink::videoFrameChanged, this, &CameraMonitor::onFrame);
    connect(m_worker, &FrameWorker::imageReady, this, [this](const QImage &image, float ms) {
        if (!m_camera && !m_still)
            return; // a late frame after stopping
        m_latest = image;
        m_convertMs = ms;
        ++m_frameCount;
        if (!m_fpsTimer.isValid())
            m_fpsTimer.start();
        if (m_fpsTimer.elapsed() >= 1000) {
            m_fps = m_frameCount * 1000.0f / float(m_fpsTimer.elapsed());
            m_frameCount = 0;
            m_fpsTimer.restart();
        }
        emit frameReady(image);
    });
    connect(m_worker, &FrameWorker::faceReady, this, [this](const FaceFrame &face) {
        if (!m_camera && !m_still)
            return;
        m_face = face;
        emit faceReady(face);
    });
}

CameraMonitor::~CameraMonitor()
{
    stopCamera();
    m_thread.quit();
    m_thread.wait();
}

bool CameraMonitor::faceTrackingAvailable() const
{
#ifdef SMILE_HAVE_ONNX
    return true;
#else
    return false;
#endif
}

void CameraMonitor::acquire()
{
    if (m_users++ == 0)
        startCamera();
}

void CameraMonitor::release()
{
    if (m_users == 0)
        return;
    if (--m_users == 0)
        stopCamera();
}

void CameraMonitor::fail(const QString &message)
{
    m_error = message;
    emit errorOccurred(message);
}

void CameraMonitor::startCamera()
{
    if (m_camera || m_still || !AppSettings::instance().useCamera())
        return; // still-image mode never opens a real device

#if QT_CONFIG(permissions)
    QCameraPermission permission;
    switch (qApp->checkPermission(permission)) {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(permission, this, [this](const QPermission &p) {
            if (p.status() == Qt::PermissionStatus::Granted && m_users > 0)
                startCamera();
            else if (p.status() != Qt::PermissionStatus::Granted)
                fail(tr("Camera access was denied. Allow it in your system privacy settings."));
        });
        return;
    case Qt::PermissionStatus::Denied:
        fail(tr("Camera access is blocked. Allow it in your system privacy settings."));
        return;
    case Qt::PermissionStatus::Granted:
        break;
    }
#endif

    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    if (cameras.isEmpty()) {
        fail(tr("No camera found."));
        return;
    }
    QCameraDevice device = QMediaDevices::defaultVideoInput();
    const QByteArray wanted = AppSettings::instance().cameraId();
    for (const QCameraDevice &d : cameras) {
        if (!wanted.isEmpty() && d.id() == wanted)
            device = d;
    }

    m_camera = new QCamera(device, this);

    // Smooth video matters more than resolution: prefer ~30 fps at up to 720p.
    auto score = [](const QCameraFormat &f) {
        const QSize s = f.resolution();
        const int area = s.width() * s.height();
        if (area > 1280 * 720 || s.width() < 320)
            return -1.0;
        const double fps = qMin<double>(f.maxFrameRate(), 30.0);
        return fps * 1000.0 + area / 1000.0; // frame rate first, then size
    };
    QCameraFormat best;
    double bestScore = -1;
    for (const QCameraFormat &format : device.videoFormats()) {
        const double s = score(format);
        if (s > bestScore) {
            bestScore = s;
            best = format;
        }
    }
    if (!best.isNull()) {
        m_camera->setCameraFormat(best);
        m_formatText = QStringLiteral("%1×%2 @ %3 fps")
                           .arg(best.resolution().width())
                           .arg(best.resolution().height())
                           .arg(best.maxFrameRate(), 0, 'f', 0);
    }

    connect(m_camera, &QCamera::errorOccurred, this, [this](QCamera::Error, const QString &message) {
        fail(message.isEmpty() ? tr("The camera stopped unexpectedly.") : message);
    });
    connect(m_camera, &QCamera::activeChanged, this, [this](bool active) {
        m_active = active;
        emit activeChanged(active);
    });

    m_error.clear();
    m_fps = 0;
    m_frameCount = 0;
    m_fpsTimer.invalidate();
    m_session.setCamera(m_camera);
    m_camera->start();
}

void CameraMonitor::stopCamera()
{
    if (!m_camera)
        return;
    m_camera->stop();
    m_session.setCamera(nullptr);
    m_camera->deleteLater();
    m_camera = nullptr;
    m_latest = QImage();
    m_face = FaceFrame();
    m_fps = 0;
    if (m_active) {
        m_active = false;
        emit activeChanged(false);
    }
}

void CameraMonitor::processStillImage(const QImage &image)
{
    m_still = true;
    const QImage copy = image.convertToFormat(QImage::Format_RGB32);
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, copy] { worker->processImage(copy); }, Qt::QueuedConnection);
}

void CameraMonitor::onFrame(const QVideoFrame &frame)
{
    // Hand the frame to the worker unless it is still busy with the previous
    // one: dropping a frame is better than building up lag.
    if (!frame.isValid() || m_worker->busy.exchange(true))
        return;
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, frame] { worker->process(frame); }, Qt::QueuedConnection);
}

CameraMonitor::Lighting CameraMonitor::analyzeLighting(const QImage &frame)
{
    Lighting result;
    if (frame.isNull())
        return result;

    const QImage small = frame.scaled(64, 48, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    double total = 0, centre = 0, edge = 0;
    int centreCount = 0, edgeCount = 0;
    for (int y = 0; y < small.height(); ++y) {
        for (int x = 0; x < small.width(); ++x) {
            const double l = qGray(small.pixel(x, y)) / 255.0;
            total += l;
            const bool inCentre = x > small.width() * 0.3 && x < small.width() * 0.7 && y > small.height() * 0.2
                                  && y < small.height() * 0.8;
            if (inCentre) {
                centre += l;
                ++centreCount;
            } else {
                edge += l;
                ++edgeCount;
            }
        }
    }
    result.brightness = total / (small.width() * small.height());
    const double c = centreCount ? centre / centreCount : 0;
    const double e = edgeCount ? edge / edgeCount : 0;
    result.tooDark = result.brightness < 0.18;
    result.backlit = e > 0.55 && c < e - 0.22;
    return result;
}
