#include "sensors/CameraMonitor.h"

#include <QCamera>
#include <QCameraDevice>
#include <QCoreApplication>
#include <QMediaDevices>
#include <QVideoFrameFormat>
#include <QtGlobal>
#include <algorithm>

#if QT_CONFIG(permissions)
#include <QPermissions>
#endif

#include "core/AppSettings.h"
#include "vision/ExpressionModel.h"
#include "vision/FaceDetector.h"

namespace {
constexpr int kMaxWidth = 960;      // plenty for preview and analysis
constexpr int kDetectEveryMs = 66;  // ~15 face detections per second
constexpr int kExpressionEveryMs = 180; // ~5 expression reads per second
constexpr char kLogiInsightUsbId[] = "usb#vid_046d&pid_094f&mi_";
constexpr char kSensorInterface[] = "#{24e552d7-6523-47f7-a647-d3465bf1f5ca}";
} // namespace

// ---------------------------------------------------------------- FrameWorker

FrameWorker::FrameWorker(bool trackFaces)
    : m_trackFaces(trackFaces)
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
        if (m_trackFaces)
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
    if (!faces.isEmpty()) {
        f = FaceFrame::from(faces.first(), image.size(), m_clock.elapsed(), ms);
        // Expression, a few times a second (kept between reads)
        if (!m_expression)
            m_expression = std::make_unique<ExpressionModel>();
        if (m_expression->isAvailable()) {
            if (!m_lastExpression.isValid() || m_lastExpression.elapsed() >= kExpressionEveryMs) {
                m_lastExpression.restart();
                ExpressionModel::Probabilities p{};
                if (m_expression->classify(image, faces.first().box, &p)) {
                    f.expressionValid = true;
                    f.neutral = p[ExpressionModel::Neutral];
                    f.happiness = p[ExpressionModel::Happiness];
                    f.surprise = p[ExpressionModel::Surprise];
                    f.negative = p[ExpressionModel::Sadness] + p[ExpressionModel::Anger] + p[ExpressionModel::Disgust]
                                 + p[ExpressionModel::Fear] + p[ExpressionModel::Contempt];
                    m_lastExpressionFrame = f;
                }
            } else if (m_lastExpressionFrame.expressionValid) {
                f.expressionValid = true;
                f.neutral = m_lastExpressionFrame.neutral;
                f.happiness = m_lastExpressionFrame.happiness;
                f.surprise = m_lastExpressionFrame.surprise;
                f.negative = m_lastExpressionFrame.negative;
            }
        }
    }
    f.timestampMs = m_clock.elapsed();
    f.detectMs = ms;
    emit faceReady(f);
}

// --------------------------------------------------------------- CameraMonitor

CameraMonitor::CameraMonitor(QObject *parent, Input input)
    : QObject(parent)
    , m_input(input)
    , m_worker(new FrameWorker(input == Input::Coaching))
{
    qRegisterMetaType<FaceFrame>();
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread.setObjectName(QStringLiteral("SMILE camera"));
    m_thread.start();

    if (m_input == Input::Coaching) {
        connect(&AppSettings::instance(), &AppSettings::cameraSettingsChanged, this, [this] {
            stopCamera();
            if (m_users > 0)
                startCamera();
        });
    }

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

bool CameraMonitor::isUsingLogiInsightColor() const
{
    return m_active && m_camera && deviceKind(m_camera->cameraDevice()) == DeviceKind::LogiInsightColor;
}

bool CameraMonitor::faceTrackingAvailable() const
{
    return FaceDetector::libraryAvailable();
}

CameraMonitor::DeviceKind CameraMonitor::deviceKind(const QCameraDevice &device)
{
    const QByteArray id = device.id().toLower();
    if (id.contains(QByteArrayLiteral("usb#vid_046d&pid_094f&mi_00#")))
        return DeviceKind::LogiInsightColor;
    if (id.contains(QByteArrayLiteral("usb#vid_046d&pid_094f&mi_04#")))
        return DeviceKind::LogiInsightInfrared;
    if (id.contains(QByteArrayLiteral("usb#vid_046d&pid_094f&mi_02#")))
        return DeviceKind::LogiInsightHello;

    const QString name = device.description().simplified();
    if (name.compare(QLatin1String("LVNext Camera"), Qt::CaseInsensitive) == 0)
        return DeviceKind::LogiInsightColor;
    if (name.compare(QLatin1String("LVNext CPD Camera"), Qt::CaseInsensitive) == 0)
        return DeviceKind::LogiInsightInfrared;
    if (name.compare(QLatin1String("LVNext Hello Camera"), Qt::CaseInsensitive) == 0)
        return DeviceKind::LogiInsightHello;
    return DeviceKind::Standard;
}

QByteArray CameraMonitor::deviceKey(const QByteArray &id)
{
    const QByteArray lower = id.toLower();
    const auto category = lower.indexOf("#{");
    // Windows can advertise the same LogiInsight USB interface under both
    // video capture and sensor camera categories.
    return category >= 0 && lower.contains(kLogiInsightUsbId) ? lower.left(category) : id;
}

QList<QCameraDevice> CameraMonitor::videoInputs()
{
    QList<QCameraDevice> cameras;
    for (const QCameraDevice &camera : QMediaDevices::videoInputs()) {
        const QByteArray key = deviceKey(camera.id());
        const auto existing = std::find_if(cameras.begin(), cameras.end(), [&](const QCameraDevice &entry) {
            return deviceKey(entry.id()) == key;
        });
        if (existing == cameras.end()) {
            cameras.append(camera);
        } else if (existing->id().toLower().contains(kSensorInterface)) {
            *existing = camera;
        }
    }
    return cameras;
}

bool CameraMonitor::hasLogiInsightInfrared()
{
    const auto cameras = videoInputs();
    return std::any_of(cameras.cbegin(), cameras.cend(), [](const QCameraDevice &camera) {
        return deviceKind(camera) == DeviceKind::LogiInsightInfrared;
    });
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

    const bool infrared = m_input == Input::LogiInsightInfrared;
    const QByteArray defaultKey = deviceKey(QMediaDevices::defaultVideoInput().id());
    const QByteArray preferred = deviceKey(AppSettings::instance().cameraId());
    const auto cameras = videoInputs();
    QCameraDevice device;
    QCameraDevice fallback;
    for (const QCameraDevice &candidate : cameras) {
        const auto kind = deviceKind(candidate);
        if (infrared) {
            if (kind == DeviceKind::LogiInsightInfrared) {
                device = candidate;
                break;
            }
            continue;
        }
        if (kind == DeviceKind::LogiInsightInfrared || kind == DeviceKind::LogiInsightHello)
            continue;

        const QByteArray key = deviceKey(candidate.id());
        if (key == preferred) {
            device = candidate;
            break;
        }
        if (key == defaultKey)
            device = candidate;
        if (fallback.isNull() || kind == DeviceKind::LogiInsightColor)
            fallback = candidate;
    }
    if (device.isNull())
        device = fallback;
    if (device.isNull()) {
        fail(infrared ? tr("The Logitech infrared camera is unavailable.") : tr("No camera found."));
        return;
    }

    m_camera = new QCamera(device, this);
    const bool logiColor = deviceKind(device) == DeviceKind::LogiInsightColor;
    auto score = [infrared, logiColor](const QCameraFormat &format) {
        const QSize size = format.resolution();
        const double fps = qMin<double>(format.maxFrameRate(), 30.0);
        if (infrared) {
            if (size.height() <= size.width())
                return -1.0;
            // The SDK samples use 512×640. Prefer that or a lower bandwidth mode.
            const double sizeScore = size == QSize(512, 640) ? 200.0
                                   : size.height() < 640 ? 100.0 : 0.0;
            return fps * 1000.0 + sizeScore;
        }
        const int area = size.width() * size.height();
        if (area > 1280 * 720 || size.width() < 320)
            return -1.0;
        // These formats leave bandwidth for the Logitech's second stream.
        const bool logiFormat = logiColor && (format.pixelFormat() == QVideoFrameFormat::Format_NV12
                                             || format.pixelFormat() == QVideoFrameFormat::Format_YUYV);
        return fps * 1000.0 + (logiFormat ? 1000.0 : 0.0) + area / 1000.0;
    };
    m_formatText.clear();
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
