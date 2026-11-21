#include "vision/FaceFrame.h"

#include <QLineF>

FaceFrame FaceFrame::from(const FaceDetector::Face &face, const QSize &frame, qint64 timestampMs, float detectMs)
{
    FaceFrame f;
    f.found = true;
    f.timestampMs = timestampMs;
    f.score = face.score;
    f.detectMs = detectMs;
    const qreal w = frame.width();
    const qreal h = frame.height();
    f.box = QRectF(face.box.x() / w, face.box.y() / h, face.box.width() / w, face.box.height() / h);
    for (int i = 0; i < 5; ++i)
        f.landmarks[i] = QPointF(face.landmarks[i].x() / w, face.landmarks[i].y() / h);

    // Geometry in pixels (so ratios aren't distorted by the aspect ratio)
    const QPointF eyeA = face.landmarks[0];
    const QPointF eyeB = face.landmarks[1];
    const QPointF nose = face.landmarks[2];
    const QPointF eyeMid = (eyeA + eyeB) / 2.0;
    const qreal eyeDist = qMax<qreal>(1.0, QLineF(eyeA, eyeB).length());
    f.yaw = float((nose.x() - eyeMid.x()) / eyeDist);
    f.pitch = float((nose.y() - eyeMid.y()) / eyeDist);
    f.mouthRatio = float(QLineF(face.landmarks[3], face.landmarks[4]).length() / eyeDist);
    f.faceHeight = float(face.box.height() / h);
    return f;
}
