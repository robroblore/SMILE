#pragma once

#include <QImage>
#include <QRectF>
#include <array>
#include <memory>

// Local facial-expression model: FER+ (ONNX Model Zoo) run with ONNX Runtime
// on the face crop from the face tracker. Gives a continuous, on-device read
// of the expression several times a second.
class ExpressionModel
{
public:
    enum Emotion { Neutral, Happiness, Surprise, Sadness, Anger, Disgust, Fear, Contempt, Count };
    using Probabilities = std::array<float, Count>;

    ExpressionModel();
    ~ExpressionModel();

    bool isAvailable() const;
    // faceBox in image pixels. Returns false if it couldn't run.
    bool classify(const QImage &image, const QRectF &faceBox, Probabilities *out);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
