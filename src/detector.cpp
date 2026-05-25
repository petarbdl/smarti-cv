#include "smarti/detector.hpp"

#include <algorithm>

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

} // namespace smarti
