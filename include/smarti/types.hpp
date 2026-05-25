#pragma once

#include <string>
#include <vector>

namespace smarti {

// A single frame on disk: the image and its (optional) matching label file.
// Image pixels are loaded lazily by the consumer to keep memory bounded across
// the thousands of frames in the dataset.
struct FrameRef {
    int boardIndex = 0;
    int frameNumber = 0;
    std::string imagePath;       // absolute path to {board}_{frame}.png
    std::string labelPath;       // matching .txt; empty if none exists
};

// All frames belonging to one board, ordered by frameNumber so they can be
// stitched end-to-end along the board's longitudinal axis.
struct Board {
    int index = 0;
    std::vector<FrameRef> frames;
};

} // namespace smarti
