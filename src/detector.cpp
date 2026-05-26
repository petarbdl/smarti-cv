#include "smarti/detector.hpp"

#include <algorithm>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace smarti {

namespace {

// Force an odd, >=1 kernel side (OpenCV structuring elements want odd sizes).
int odd(int v) {
    v = std::max(1, v);
    return (v % 2 == 0) ? v + 1 : v;
}

cv::Rect inflate(const cv::Rect& r, int pad) {
    return cv::Rect(std::max(0, r.x - pad), std::max(0, r.y - pad), r.width + 2 * pad,
                    r.height + 2 * pad);
}

} // namespace

std::vector<cv::Rect> merge_boxes(std::vector<cv::Rect> boxes, int tol) {
    bool merged = true;
    while (merged) {
        merged = false;
        for (std::size_t i = 0; i < boxes.size(); ++i) {
            for (std::size_t j = i + 1; j < boxes.size(); ++j) {
                if ((inflate(boxes[i], tol) & inflate(boxes[j], tol)).area() > 0) {
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

    return merge_boxes(std::move(knots));
}

std::vector<cv::Rect> detect_board_knots(const Board& board, const DetectorParams& params) {
    std::vector<cv::Rect> boardBoxes;
    int xOffset = 0; // running start of the current frame along the board's X axis

    for (const auto& ref : board.frames) {
        const cv::Mat image = cv::imread(ref.imagePath, cv::IMREAD_COLOR);
        if (image.empty()) {
            continue; // skip unreadable frames; offsets resume from a known width below
        }
        for (cv::Rect box : detect_knots(image, params)) {
            box.x += xOffset; // frame-local X -> board-space X
            boardBoxes.push_back(box);
        }
        xOffset += image.cols;
    }

    // Stitch the per-frame detections, joining knots split across a seam.
    return merge_boxes(std::move(boardBoxes), params.seamMergePx);
}

} // namespace smarti
