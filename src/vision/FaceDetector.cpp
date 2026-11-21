#include "vision/FaceDetector.h"

#include <QFile>
#include <algorithm>
#include <cmath>

#ifdef SMILE_HAVE_ONNX
#include <onnxruntime_cxx_api.h>
#endif

namespace {
constexpr int kInput = 640; // YuNet 2023mar has a fixed 640x640 input
constexpr float kNms = 0.3f;

float iou(const QRectF &a, const QRectF &b)
{
    const QRectF i = a.intersected(b);
    if (i.isEmpty())
        return 0;
    const float inter = float(i.width() * i.height());
    return inter / float(a.width() * a.height() + b.width() * b.height() - inter);
}
} // namespace

#ifdef SMILE_HAVE_ONNX

struct FaceDetector::Impl {
    Ort::Env env{ORT_LOGGING_LEVEL_ERROR, "smile-face"};
    std::unique_ptr<Ort::Session> session;
    QByteArray model;
    std::vector<float> input = std::vector<float>(size_t(3) * kInput * kInput);
};

FaceDetector::FaceDetector()
    : d(std::make_unique<Impl>())
{
    QFile file(QStringLiteral(":/models/face_detection_yunet_2023mar.onnx"));
    if (!file.open(QIODevice::ReadOnly)) {
        m_error = QStringLiteral("Face model missing from the build.");
        return;
    }
    d->model = file.readAll();
    try {
        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(2); // leave CPU for the camera and UI
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        d->session = std::make_unique<Ort::Session>(d->env, d->model.constData(), size_t(d->model.size()), options);
    } catch (const Ort::Exception &e) {
        m_error = QString::fromUtf8(e.what());
    }
}

FaceDetector::~FaceDetector() = default;

bool FaceDetector::isAvailable() const
{
    return d && d->session;
}

QList<FaceDetector::Face> FaceDetector::detect(const QImage &image, float minScore)
{
    QList<Face> faces;
    if (!isAvailable() || image.isNull())
        return faces;

    // Letterbox into 640x640 (top-left), keeping the aspect ratio.
    const float scale = std::min(float(kInput) / image.width(), float(kInput) / image.height());
    const QImage scaled = image.scaled(int(image.width() * scale), int(image.height() * scale), Qt::IgnoreAspectRatio,
                                       Qt::FastTransformation)
                              .convertToFormat(QImage::Format_RGB888);
    std::fill(d->input.begin(), d->input.end(), 0.0f);
    float *b = d->input.data();
    float *g = b + kInput * kInput;
    float *r = g + kInput * kInput;
    for (int y = 0; y < scaled.height(); ++y) {
        const uchar *row = scaled.constScanLine(y);
        for (int x = 0; x < scaled.width(); ++x) {
            const int i = y * kInput + x;
            r[i] = row[x * 3 + 0]; // the model expects BGR, 0..255
            g[i] = row[x * 3 + 1];
            b[i] = row[x * 3 + 2];
        }
    }

    const std::array<int64_t, 4> shape{1, 3, kInput, kInput};
    Ort::MemoryInfo memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value tensor = Ort::Value::CreateTensor<float>(memory, d->input.data(), d->input.size(), shape.data(), shape.size());

    static const char *inputNames[] = {"input"};
    static const char *outputNames[] = {"cls_8", "cls_16", "cls_32", "obj_8", "obj_16", "obj_32",
                                        "bbox_8", "bbox_16", "bbox_32", "kps_8", "kps_16", "kps_32"};
    std::vector<Ort::Value> out;
    try {
        out = d->session->Run(Ort::RunOptions{nullptr}, inputNames, &tensor, 1, outputNames, 12);
    } catch (const Ort::Exception &e) {
        m_error = QString::fromUtf8(e.what());
        return faces;
    }

    // Decode (same as OpenCV's FaceDetectorYN)
    QList<Face> candidates;
    const int strides[3] = {8, 16, 32};
    for (int si = 0; si < 3; ++si) {
        const int stride = strides[si];
        const int cols = kInput / stride;
        const int rows = kInput / stride;
        const float *cls = out[size_t(si)].GetTensorData<float>();
        const float *obj = out[size_t(3 + si)].GetTensorData<float>();
        const float *box = out[size_t(6 + si)].GetTensorData<float>();
        const float *kps = out[size_t(9 + si)].GetTensorData<float>();
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                const int idx = row * cols + col;
                const float score = std::sqrt(std::clamp(cls[idx], 0.f, 1.f) * std::clamp(obj[idx], 0.f, 1.f));
                if (score < minScore)
                    continue;
                const float cx = (col + box[idx * 4 + 0]) * stride;
                const float cy = (row + box[idx * 4 + 1]) * stride;
                const float w = std::exp(box[idx * 4 + 2]) * stride;
                const float h = std::exp(box[idx * 4 + 3]) * stride;
                Face f;
                f.score = score;
                f.box = QRectF((cx - w / 2) / scale, (cy - h / 2) / scale, w / scale, h / scale);
                for (int n = 0; n < 5; ++n)
                    f.landmarks[n] = QPointF((kps[idx * 10 + 2 * n] + col) * stride / scale,
                                             (kps[idx * 10 + 2 * n + 1] + row) * stride / scale);
                candidates.append(f);
            }
        }
    }

    // Non-maximum suppression
    std::sort(candidates.begin(), candidates.end(), [](const Face &a, const Face &b) { return a.score > b.score; });
    for (const Face &c : std::as_const(candidates)) {
        bool keep = true;
        for (const Face &k : std::as_const(faces)) {
            if (iou(c.box, k.box) > kNms) {
                keep = false;
                break;
            }
        }
        if (keep)
            faces.append(c);
    }
    return faces;
}

#else // no ONNX Runtime: detector unavailable

struct FaceDetector::Impl {};
FaceDetector::FaceDetector()
    : m_error(QStringLiteral("This build of SMILE has no local face tracking."))
{
}
FaceDetector::~FaceDetector() = default;
bool FaceDetector::isAvailable() const
{
    return false;
}
QList<FaceDetector::Face> FaceDetector::detect(const QImage &, float)
{
    return {};
}

#endif
