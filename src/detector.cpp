#include "smarti/detector.hpp"

#include <algorithm>
#include <stdexcept>

#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>

namespace smarti {

namespace {

// Force an odd, >=1 kernel side (OpenCV structuring elements want odd sizes).
int odd(int v) {
    v = std::max(1, v);
    return (v % 2 == 0) ? v + 1 : v;
}

// Merge boxes that overlap into their bounding union. A single knot whose ring
// breaks into pieces during thresholding can yield several overlapping blobs;
// collapsing them gives one box per knot. Repeats until no pair overlaps, since
// a merge can create a box that now overlaps a third.
std::vector<cv::Rect> merge_overlapping(std::vector<cv::Rect> boxes) {
    bool merged = true;
    while (merged) {
        merged = false;
        for (std::size_t i = 0; i < boxes.size(); ++i) {
            for (std::size_t j = i + 1; j < boxes.size(); ++j) {
                if ((boxes[i] & boxes[j]).area() > 0) {
                    boxes[i] |= boxes[j]; // bounding union
                    boxes.erase(boxes.begin() + static_cast<std::ptrdiff_t>(j));
                    merged = true;
                    break;
                }
            }
            if (merged) {
                break;
            }
        }
    }
    return boxes;
}

} // namespace

std::vector<cv::Rect> detect_knots(const cv::Mat& image, const DetectorParams& params) {
    std::vector<cv::Rect> knots;
    if (image.empty()) {
        return knots;
    }

    cv::Mat gray;
    if (image.channels() == 1) {
        gray = image;
    } else {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    }

    const int shortSide = std::min(gray.rows, gray.cols);
    const double frameArea = static_cast<double>(gray.rows) * gray.cols;

    // Black-hat: closing(gray) - gray. The closing fills dark blobs smaller than
    // the structuring element, so the difference isolates exactly those compact
    // dark features (knots) while ignoring smooth illumination gradients and
    // dark regions wider than the kernel (e.g. the shadowed board edge).
    const int k = odd(static_cast<int>(shortSide * params.kernelFrac));
    const cv::Mat se = cv::getStructuringElement(cv::MORPH_ELLIPSE, {k, k});
    cv::Mat blackhat;
    cv::morphologyEx(gray, blackhat, cv::MORPH_BLACKHAT, se);

    // Otsu picks the dark/bright split automatically per frame, adapting to
    // varying wood tone and exposure.
    cv::Mat mask;
    cv::threshold(blackhat, mask, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    // Clean up: open removes isolated speckle, close merges a knot's broken
    // pieces (ring gaps) into one blob.
    const cv::Mat openSe =
        cv::getStructuringElement(cv::MORPH_ELLIPSE, {odd(params.openSize), odd(params.openSize)});
    const cv::Mat closeSe = cv::getStructuringElement(
        cv::MORPH_ELLIPSE, {odd(params.closeSize), odd(params.closeSize)});
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, openSe);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, closeSe);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    for (const auto& contour : contours) {
        const double area = cv::contourArea(contour);
        if (area < params.minAreaFrac * frameArea || area > params.maxAreaFrac * frameArea) {
            continue;
        }

        const cv::Rect box = cv::boundingRect(contour);

        // Aspect ratio: reject long thin shapes (grain lines, seams).
        const double longSide = std::max(box.width, box.height);
        const double shortSideBox = std::max(1, std::min(box.width, box.height));
        if (longSide / shortSideBox > params.maxAspect) {
            continue;
        }

        // Extent: compact blobs fill their bounding box; stringy artefacts don't.
        const double extent = area / std::max(1, box.width * box.height);
        if (extent < params.minExtent) {
            continue;
        }

        knots.push_back(box);
    }

    return merge_overlapping(std::move(knots));
}

// --- Classical detector wrapper -------------------------------------------

namespace {

class ClassicalDetector : public KnotDetector {
  public:
    explicit ClassicalDetector(DetectorParams params) : params_(params) {}
    std::vector<cv::Rect> detect(const cv::Mat& image) override {
        return detect_knots(image, params_);
    }

  private:
    DetectorParams params_;
};

} // namespace

std::unique_ptr<KnotDetector> make_classical_detector(const DetectorParams& params) {
    return std::make_unique<ClassicalDetector>(params);
}

// --- ONNX (OpenCV DNN) detector -------------------------------------------

namespace {

// Runs a YOLOv5-format ONNX model via OpenCV's DNN module. The decode is generic
// over the number of classes (read from the output tensor), so the same code
// serves both the 80-class COCO placeholder and a future single-class knot
// model -- swapping the model file is all that's required.
class OnnxDetector : public KnotDetector {
  public:
    explicit OnnxDetector(const OnnxParams& params) : params_(params) {
        net_ = cv::dnn::readNetFromONNX(params.modelPath);
        if (net_.empty()) {
            throw std::runtime_error("failed to load ONNX model: " + params.modelPath);
        }
        net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    }

    std::vector<cv::Rect> detect(const cv::Mat& image) override {
        if (image.empty()) {
            return {};
        }

        // Letterbox into a square by padding (not stretching), so aspect ratio
        // is preserved; the network input is that square scaled to inputSize.
        // With the image pinned at the top-left, mapping boxes back to original
        // pixels is a single uniform scale factor -- no padding offset.
        const int side = std::max(image.cols, image.rows);
        cv::Mat square = cv::Mat::zeros(side, side, CV_8UC3);
        image.copyTo(square(cv::Rect(0, 0, image.cols, image.rows)));
        const float factor = static_cast<float>(side) / params_.inputSize;

        cv::Mat blob =
            cv::dnn::blobFromImage(square, 1.0 / 255.0, {params_.inputSize, params_.inputSize},
                                   cv::Scalar(), /*swapRB=*/true, /*crop=*/false);
        net_.setInput(blob);
        std::vector<cv::Mat> outputs;
        net_.forward(outputs, net_.getUnconnectedOutLayersNames());

        // YOLOv5 exposes the combined detections as a 3-D tensor
        // [1, N, 5 + numClasses]; some exports also expose the three raw
        // per-scale heads (4-D). Pick the 3-D one rather than assuming index 0.
        const cv::Mat* combined = nullptr;
        for (const auto& o : outputs) {
            if (o.dims == 3) {
                combined = &o;
                break;
            }
        }
        if (combined == nullptr) {
            throw std::runtime_error("ONNX model has no [1, N, 5+C] detection output");
        }

        // Rows of [cx, cy, w, h, objectness, class_0 ... class_k] in inputSize px.
        const cv::Mat& det = *combined;
        const int rows = det.size[1];
        const int stride = det.size[2];
        const int numClasses = stride - 5;
        const float* data = reinterpret_cast<const float*>(det.data);

        std::vector<cv::Rect> boxes;
        std::vector<float> scores;
        std::vector<int> classes;
        for (int i = 0; i < rows; ++i, data += stride) {
            const float objectness = data[4];
            if (objectness < params_.confThreshold) {
                continue;
            }
            // Best class for this anchor.
            const float* classScores = data + 5;
            int bestClass = 0;
            float bestScore = classScores[0];
            for (int c = 1; c < numClasses; ++c) {
                if (classScores[c] > bestScore) {
                    bestScore = classScores[c];
                    bestClass = c;
                }
            }
            if (params_.keepClassId >= 0 && bestClass != params_.keepClassId) {
                continue;
            }
            const float confidence = objectness * bestScore;
            if (confidence < params_.confThreshold) {
                continue;
            }

            const float cx = data[0];
            const float cy = data[1];
            const float w = data[2];
            const float h = data[3];
            const int x = static_cast<int>((cx - w / 2.0f) * factor);
            const int y = static_cast<int>((cy - h / 2.0f) * factor);
            boxes.emplace_back(x, y, static_cast<int>(w * factor), static_cast<int>(h * factor));
            scores.push_back(confidence);
            classes.push_back(bestClass);
        }

        std::vector<int> keep;
        cv::dnn::NMSBoxes(boxes, scores, params_.confThreshold, params_.nmsThreshold, keep);

        std::vector<cv::Rect> result;
        result.reserve(keep.size());
        const cv::Rect frame(0, 0, image.cols, image.rows);
        for (int idx : keep) {
            result.push_back(boxes[idx] & frame); // clamp to image bounds
        }
        return result;
    }

  private:
    OnnxParams params_;
    cv::dnn::Net net_;
};

} // namespace

std::unique_ptr<KnotDetector> make_onnx_detector(const OnnxParams& params) {
    return std::make_unique<OnnxDetector>(params);
}

} // namespace smarti
