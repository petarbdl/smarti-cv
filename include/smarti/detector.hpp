#pragma once

#include <vector>

#include <opencv2/core.hpp>

#include "smarti/types.hpp"

namespace smarti {

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

    // A knot lying on a frame boundary is split across two frames and detected
    // as two boxes that meet at the seam. When stitching a board's frames into
    // one coordinate space, boxes within this many pixels of each other are
    // merged so the knot is reported once. Also tolerates minor seam jitter.
    int seamMergePx = 3;
};

// Detect knots in a single decoded BGR frame. Returns axis-aligned bounding
// boxes in the pixel coordinate space of `image`.
std::vector<cv::Rect> detect_knots(const cv::Mat& image, const DetectorParams& params = {});

// Detect knots across every frame of a board and return their bounding boxes in
// the board's stitched coordinate space
std::vector<cv::Rect> detect_board_knots(const Board& board, const DetectorParams& params = {});

std::vector<cv::Rect> merge_boxes(std::vector<cv::Rect> boxes, int tol = 0);

} // namespace smarti
