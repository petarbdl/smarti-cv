#include "smarti/label.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace smarti {

std::vector<YoloBox> load_yolo_labels(const std::string& path) {
    std::vector<YoloBox> boxes;
    if (path.empty()) {
        return boxes;
    }

    std::ifstream in(path);
    if (!in) {
        return boxes;
    }

    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        YoloBox box;
        if (ss >> box.classId >> box.cx >> box.cy >> box.w >> box.h) {
            boxes.push_back(box);
        }
        // lines that don't parse to five fields are silently skipped
    }
    return boxes;
}

cv::Rect to_pixel_rect(const YoloBox& box, const cv::Size& imageSize) {
    const float fx = box.cx * imageSize.width;
    const float fy = box.cy * imageSize.height;
    const float fw = box.w * imageSize.width;
    const float fh = box.h * imageSize.height;

    int x = static_cast<int>(std::lround(fx - fw / 2.0f));
    int y = static_cast<int>(std::lround(fy - fh / 2.0f));
    int w = static_cast<int>(std::lround(fw));
    int h = static_cast<int>(std::lround(fh));

    // Clamp to image bounds so downstream drawing/IoU stays valid.
    x = std::clamp(x, 0, imageSize.width);
    y = std::clamp(y, 0, imageSize.height);
    w = std::clamp(w, 0, imageSize.width - x);
    h = std::clamp(h, 0, imageSize.height - y);
    return cv::Rect(x, y, w, h);
}

} // namespace smarti
