#pragma once

#include <QImage>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <memory>

// Local, on-device face detector: YuNet (OpenCV Zoo, MIT licence) run with
// ONNX Runtime. Finds faces with 5 landmarks in ~10 ms, so SMILE can track you
// many times a second without sending anything anywhere.
// If SMILE was built without ONNX Runtime, isAvailable() is false.
class FaceDetector
{
public:
    struct Face {
        QRectF box;           // pixels in the input image
        QPointF landmarks[5]; // eye, eye, nose tip, mouth corner, mouth corner
        float score = 0;
    };

    FaceDetector();
    ~FaceDetector();

    bool isAvailable() const;

    // Whether the ONNX Runtime library could be loaded on this computer, and
    // if not, why. Safe to call from any thread.
    static bool libraryAvailable();
    static QString libraryError();
    QString errorString() const { return m_error; }

    // Detect faces, best first. Thread-affine: use from one thread only.
    QList<Face> detect(const QImage &image, float minScore = 0.6f);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
    QString m_error;
};
