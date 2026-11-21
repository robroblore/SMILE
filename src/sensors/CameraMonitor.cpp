#include "sensors/CameraMonitor.h"

#include <QCamera>
#include <QCameraDevice>
#include <QCoreApplication>
#include <QMediaDevices>
#include <QVideoFrame>
#include <QtGlobal>

#if QT_CONFIG(permissions)
#include <QPermissions>
#endif

#include "core/AppSettings.h"

CameraMonitor::CameraMonitor(QObject *parent)
    : QObject(parent)
{
    m_session.setVideoSink(&m_sink);
    connect(&m_sink, &QVideoSink::videoFrameChanged, this, &CameraMonitor::onFrame);
}

CameraMonitor::~CameraMonitor()
{
    stopCamera();
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
    if (m_camera || !AppSettings::instance().useCamera())
        return;

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

    // A modest resolution is plenty for coaching and keeps CPU use low.
    QCameraFormat best;
    for (const QCameraFormat &format : device.videoFormats()) {
        const QSize s = format.resolution();
        if (s.width() * s.height() > 1280 * 720)
            continue;
        if (best.isNull() || s.width() * s.height() > best.resolution().width() * best.resolution().height())
            best = format;
    }
    if (!best.isNull())
        m_camera->setCameraFormat(best);

    connect(m_camera, &QCamera::errorOccurred, this, [this](QCamera::Error, const QString &message) {
        fail(message.isEmpty() ? tr("The camera stopped unexpectedly.") : message);
    });
    connect(m_camera, &QCamera::activeChanged, this, [this](bool active) {
        m_active = active;
        emit activeChanged(active);
    });

    m_error.clear();
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
    if (m_active) {
        m_active = false;
        emit activeChanged(false);
    }
}

void CameraMonitor::onFrame(const QVideoFrame &frame)
{
    // Converting every frame is wasteful; ~10 fps is enough for preview and AI.
    if (m_lastFrame.isValid() && m_lastFrame.elapsed() < 100)
        return;
    m_lastFrame.restart();

    QImage image = frame.toImage();
    if (image.isNull())
        return;
    if (image.width() > 960)
        image = image.scaledToWidth(960, Qt::SmoothTransformation);
    m_latest = image.mirrored(true, false).convertToFormat(QImage::Format_RGB32);
    emit frameReady(m_latest);
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
