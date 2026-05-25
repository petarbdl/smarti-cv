#pragma once

#include <vector>

#include <opencv2/core.hpp>

#include "smarti/types.hpp"

namespace smarti {

// Render one frame with its ground-truth knot boxes and a status banner,
// upscaling small frames for readability.
cv::Mat render_overlay(const Board& board, std::size_t frameIdx);

// Stitch all of a board's frames (in frameNumber order) into one composite
// image with ground-truth boxes drawn, to visualize the assembled board.
// Frames stitch end-to-end along X (the board's longitudinal axis = image
// columns), so the default places them left-to-right; `vertical` instead
// stacks them top-to-bottom for comparison. Frames are padded to a common
// cross-axis size, and a thin red separator marks each frame seam.
cv::Mat render_board_composite(const Board& board, bool vertical);

// Draw a detection-vs-ground-truth comparison over a copy of `image` (upscaling
// small frames for readability): ground-truth boxes in green, detected boxes in
// red, plus a status banner. Used by the `detect` command's --save output.
cv::Mat render_comparison(const cv::Mat& image, const std::vector<cv::Rect>& groundTruth,
                          const std::vector<cv::Rect>& detections, const std::string& status);

// Open an interactive highgui window to browse the dataset frame by frame,
// overlaying ground-truth knot boxes. `startBoardIndex` selects which board to
// open on (-1 = first). Blocks until the user quits; returns a process exit code.
//
// Keys:
//   right / d : next frame        left / a : previous frame
//   down  / n : next board        up   / p : previous board
//   q / Esc   : quit
int run_viewer(const std::vector<Board>& boards, int startBoardIndex = -1);

} // namespace smarti
