#pragma once

#include <string>
#include <vector>

#include <opencv2/core.hpp>

namespace smarti {

// One annotation in YOLO format: class id plus a box whose center and size are
// normalized to [0, 1] relative to the image dimensions.
struct YoloBox {
    int classId = 0;
    float cx = 0.0f; // center x, normalized
    float cy = 0.0f; // center y, normalized
    float w = 0.0f;  // width, normalized
    float h = 0.0f;  // height, normalized
};

// Read a YOLO label file (one "class cx cy w h" per line). Blank lines are
// ignored; malformed lines are skipped. A missing path yields an empty vector
// (a frame with no annotated knots).
std::vector<YoloBox> load_yolo_labels(const std::string& path);

// Convert a normalized YOLO box to a pixel rectangle, clamped to the image.
cv::Rect to_pixel_rect(const YoloBox& box, const cv::Size& imageSize);

// Inverse of to_pixel_rect: convert a pixel rectangle to a normalized YOLO box
// relative to `imageSize`. `classId` defaults to 0 (the dataset's only class).
YoloBox to_yolo_box(const cv::Rect& rect, const cv::Size& imageSize, int classId = 0);

// Write YOLO labels (one "class cx cy w h" line each) to `path`, overwriting it.
// Throws std::runtime_error if the file cannot be opened.
void write_yolo_labels(const std::string& path, const std::vector<YoloBox>& boxes);

} // namespace smarti
