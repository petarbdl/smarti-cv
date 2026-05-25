#pragma once

#include <vector>

#include <opencv2/core.hpp>

#include "smarti/types.hpp"

namespace smarti {

// Render one frame with its ground-truth knot boxes and a status banner,
// upscaling small frames for readability.
cv::Mat render_overlay(const Board& board, std::size_t frameIdx);

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
