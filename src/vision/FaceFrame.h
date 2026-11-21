#pragma once

#include <QMetaType>
#include <QPointF>
#include <QRectF>
#include <QSize>

#include "vision/FaceDetector.h"

// What the local face tracker sees in one frame. Coordinates are normalised
// (0..1) in the mirrored preview image, so overlays can be drawn at any size.
struct FaceFrame {
    bool found = false;
    qint64 timestampMs = 0;
    float score = 0;
    QRectF box;
    QPointF landmarks[5]; // eye, eye, nose tip, mouth corner, mouth corner
    float yaw = 0;        // nose offset from between the eyes, in eye-distances (0 = facing camera)
    float pitch = 0;      // nose drop below the eyes, in eye-distances (~0.6 when level)
    float mouthRatio = 0; // mouth width / eye distance (wider when smiling)
    float faceHeight = 0; // face height / frame height (distance to camera)
    float detectMs = 0;

    static FaceFrame from(const FaceDetector::Face &face, const QSize &frame, qint64 timestampMs, float detectMs);
};

Q_DECLARE_METATYPE(FaceFrame)
