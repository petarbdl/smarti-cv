#pragma once

#include <memory>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

namespace smarti {

// Abstract knot detector: takes one decoded BGR frame and returns axis-aligned
// bounding boxes in that frame's pixel coordinates. Implementations are
// interchangeable so the rest of the pipeline (board stitching, scoring) does
// not care whether boxes came from classical CV or a neural network.
class KnotDetector {
  public:
    virtual ~KnotDetector() = default;
    // Not const: the ONNX backend mutates its cv::dnn::Net during forward().
    virtual std::vector<cv::Rect> detect(const cv::Mat& image) = 0;
};

// --- Classical (OpenCV morphology) detector -------------------------------

// Tunable parameters for the classical (non-learned) knot detector. Defaults are
// calibrated for the ~640x235 board frames in the provided dataset; sizes that
// scale with the image are expressed as fractions so they transfer across
// resolutions.
struct DetectorParams {
    // Black-hat structuring-element size as a fraction of the frame's shorter
    // side. Must comfortably exceed the largest knot so the morphological
    // closing fills knots (making them pop in the black-hat response) while
    // leaving wide dark regions (edge shadows) untouched.
    double kernelFrac = 0.6;

    // Reject blobs smaller than this fraction of the frame area (speckle/noise)
    // or larger than this fraction (shadows, mis-segmentation spanning a frame).
    double minAreaFrac = 0.0015;
    double maxAreaFrac = 0.45;

    // Knots are roughly round; grain lines and seams are long and thin. Reject
    // bounding boxes whose longer/shorter side ratio exceeds this.
    double maxAspect = 4.0;

    // Contour area / bounding-box area. Compact blobs fill their box; stringy
    // grain artefacts do not. Reject anything below this.
    double minExtent = 0.30;

    // Denoise / consolidate kernel sizes (pixels) applied to the blob mask.
    int openSize = 3;
    int closeSize = 7;
};

// Detect knots in a single decoded BGR frame using classical morphology.
// Exposed as a free function so it can be called directly; also wrapped by the
// KnotDetector returned from make_classical_detector.
std::vector<cv::Rect> detect_knots(const cv::Mat& image, const DetectorParams& params = {});

std::unique_ptr<KnotDetector> make_classical_detector(const DetectorParams& params = {});

// --- ONNX (OpenCV DNN) detector -------------------------------------------

// Configuration for the YOLOv5-format ONNX detector. Everything that varies
// between models lives here, so swapping in a knot-trained model is a matter of
// changing modelPath (and, if needed, the thresholds) -- no code change. The
// number of classes is read from the model's output tensor at load time.
struct OnnxParams {
    std::string modelPath;       // path to a YOLOv5-export .onnx
    int inputSize = 640;         // square network input side
    float confThreshold = 0.25f; // min objectness*class confidence to keep
    float nmsThreshold = 0.45f;  // IoU threshold for non-max suppression
    // Keep only detections of this class id; -1 keeps every class. A
    // single-class knot model uses class 0, so the default of 0 is correct for
    // the eventual knot model and harmless to override for the COCO placeholder.
    int keepClassId = 0;
};

// Construct a detector backed by an ONNX model run through OpenCV's DNN module.
// Throws std::runtime_error if the model cannot be loaded.
std::unique_ptr<KnotDetector> make_onnx_detector(const OnnxParams& params);

} // namespace smarti
