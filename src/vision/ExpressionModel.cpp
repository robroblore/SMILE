#include "vision/ExpressionModel.h"

#include <QFile>
#include <cmath>

#include "vision/FaceDetector.h"

#ifdef SMILE_HAVE_ONNX
#define ORT_API_MANUAL_INIT
#include <onnxruntime_cxx_api.h>

struct ExpressionModel::Impl {
    std::unique_ptr<Ort::Env> env;
    std::unique_ptr<Ort::Session> session;
    QByteArray model;
    std::vector<float> input = std::vector<float>(64 * 64);
};

ExpressionModel::ExpressionModel()
    : d(std::make_unique<Impl>())
{
    // FaceDetector loads ONNX Runtime (and initialises its API) on first use
    if (!FaceDetector::libraryAvailable())
        return;
    QFile file(QStringLiteral(":/models/emotion-ferplus-8.onnx"));
    if (!file.open(QIODevice::ReadOnly))
        return; // built without the expression model
    d->model = file.readAll();
    try {
        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(1);
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        d->env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_ERROR, "smile-expression");
        d->session = std::make_unique<Ort::Session>(*d->env, d->model.constData(), size_t(d->model.size()), options);
    } catch (const Ort::Exception &) {
        d->session.reset();
    }
}

ExpressionModel::~ExpressionModel() = default;

bool ExpressionModel::isAvailable() const
{
    return d && d->session;
}

bool ExpressionModel::classify(const QImage &image, const QRectF &faceBox, Probabilities *out)
{
    if (!isAvailable() || image.isNull() || faceBox.isEmpty())
        return false;
    // Square crop around the face, a little wider than the detector's box
    const qreal side = qMax(faceBox.width(), faceBox.height()) * 1.15;
    const QRect crop = QRectF(faceBox.center().x() - side / 2, faceBox.center().y() - side / 2, side, side)
                           .toAlignedRect()
                           .intersected(image.rect());
    if (crop.width() < 24 || crop.height() < 24)
        return false;
    const QImage gray = image.copy(crop).scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                            .convertToFormat(QImage::Format_Grayscale8);
    for (int y = 0; y < 64; ++y) {
        const uchar *row = gray.constScanLine(y);
        for (int x = 0; x < 64; ++x)
            d->input[size_t(y * 64 + x)] = row[x];
    }
    const std::array<int64_t, 4> shape{1, 1, 64, 64};
    Ort::MemoryInfo memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value tensor = Ort::Value::CreateTensor<float>(memory, d->input.data(), d->input.size(), shape.data(), shape.size());
    static const char *inputNames[] = {"Input3"};
    static const char *outputNames[] = {"Plus692_Output_0"};
    try {
        auto result = d->session->Run(Ort::RunOptions{nullptr}, inputNames, &tensor, 1, outputNames, 1);
        const float *logits = result[0].GetTensorData<float>();
        float peak = logits[0];
        for (int i = 1; i < Count; ++i)
            peak = std::max(peak, logits[i]);
        float sum = 0;
        for (int i = 0; i < Count; ++i) {
            (*out)[size_t(i)] = std::exp(logits[i] - peak);
            sum += (*out)[size_t(i)];
        }
        for (float &p : *out)
            p /= sum;
        return true;
    } catch (const Ort::Exception &) {
        return false;
    }
}

#else

struct ExpressionModel::Impl {};
ExpressionModel::ExpressionModel() = default;
ExpressionModel::~ExpressionModel() = default;
bool ExpressionModel::isAvailable() const
{
    return false;
}
bool ExpressionModel::classify(const QImage &, const QRectF &, Probabilities *)
{
    return false;
}

#endif
