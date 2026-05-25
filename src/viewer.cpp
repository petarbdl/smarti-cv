#include "smarti/viewer.hpp"

#include <algorithm>
#include <iostream>
#include <string>

#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "smarti/label.hpp"

namespace smarti {

namespace {

constexpr char kWindow[] = "smarti-cv viewer";
constexpr int kMinDisplayHeight = 360; // upscale small frames for readability

// Navigation is letter-key only (a/d/n/p/q)
constexpr int kEsc = 27;

} // namespace

cv::Mat render_overlay(const Board& board, std::size_t frameIdx) {
    const FrameRef& ref = board.frames[frameIdx];

    cv::Mat image = cv::imread(ref.imagePath, cv::IMREAD_COLOR);
    if (image.empty()) {
        image = cv::Mat(kMinDisplayHeight, 640, CV_8UC3, cv::Scalar(40, 40, 40));
        cv::putText(image, "failed to read " + ref.imagePath, {10, 30},
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, {0, 0, 255}, 1);
        return image;
    }

    const std::vector<YoloBox> boxes = load_yolo_labels(ref.labelPath);

    // Upscale small frames
    int scale = std::max(1, kMinDisplayHeight / std::max(1, image.rows));
    if (scale > 1) {
        cv::resize(image, image, {}, scale, scale, cv::INTER_NEAREST);
    }

    for (std::size_t i = 0; i < boxes.size(); ++i) {
        cv::Rect r = to_pixel_rect(boxes[i], image.size());
        cv::rectangle(image, r, cv::Scalar(0, 255, 0), 2);
        cv::putText(image, std::to_string(i), {r.x + 2, std::max(r.y - 4, 12)},
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, {0, 255, 0}, 1, cv::LINE_AA);
    }

    // Status banner across the top.
    const std::string status = "board " + std::to_string(board.index) + "  frame " +
                               std::to_string(ref.frameNumber) + "   [" +
                               std::to_string(frameIdx + 1) + "/" +
                               std::to_string(board.frames.size()) + "]   knots: " +
                               std::to_string(boxes.size());
    cv::rectangle(image, {0, 0}, {image.cols, 26}, {0, 0, 0}, cv::FILLED);
    cv::putText(image, status, {8, 18}, cv::FONT_HERSHEY_SIMPLEX, 0.55, {255, 255, 255}, 1,
                cv::LINE_AA);
    return image;
}

cv::Mat render_board_composite(const Board& board, bool vertical) {
    std::vector<cv::Mat> tiles;
    int maxW = 0;
    int maxH = 0;

    for (const auto& ref : board.frames) {
        cv::Mat image = cv::imread(ref.imagePath, cv::IMREAD_COLOR);
        if (image.empty()) {
            continue;
        }
        for (const auto& box : load_yolo_labels(ref.labelPath)) {
            cv::rectangle(image, to_pixel_rect(box, image.size()), cv::Scalar(0, 255, 0), 2);
        }
        maxW = std::max(maxW, image.cols);
        maxH = std::max(maxH, image.rows);
        tiles.push_back(std::move(image));
    }
    if (tiles.empty()) {
        return cv::Mat(64, 256, CV_8UC3, cv::Scalar(40, 40, 40));
    }

    // Pad each tile to the common cross-axis size and add a seam separator.
    const cv::Scalar seam(0, 0, 255);
    std::vector<cv::Mat> padded;
    for (std::size_t i = 0; i < tiles.size(); ++i) {
        const cv::Mat& t = tiles[i];
        const int w = vertical ? maxW : t.cols;
        const int h = vertical ? t.rows : maxH;
        cv::Mat tile(h, w, CV_8UC3, cv::Scalar(0, 0, 0));
        t.copyTo(tile(cv::Rect(0, 0, t.cols, t.rows)));
        if (i > 0) {
            if (vertical) {
                cv::line(tile, {0, 0}, {w - 1, 0}, seam, 1);
            } else {
                cv::line(tile, {0, 0}, {0, h - 1}, seam, 1);
            }
        }
        padded.push_back(std::move(tile));
    }

    cv::Mat composite;
    if (vertical) {
        cv::vconcat(padded, composite);
    } else {
        cv::hconcat(padded, composite);
    }
    return composite;
}

int run_viewer(const std::vector<Board>& boards, int startBoardIndex) {
    if (boards.empty()) {
        std::cerr << "no boards to view\n";
        return 1;
    }

    std::size_t bi = 0;
    if (startBoardIndex >= 0) {
        for (std::size_t i = 0; i < boards.size(); ++i) {
            if (boards[i].index == startBoardIndex) {
                bi = i;
                break;
            }
        }
    }
    std::size_t fi = 0;

    std::cout << "Viewer controls: a/d (frame), n/p (board), q/Esc (quit)\n";
    // WINDOW_GUI_NORMAL suppresses the Qt backend's navigation toolbar
    cv::namedWindow(kWindow, cv::WINDOW_AUTOSIZE | cv::WINDOW_GUI_NORMAL);

    bool running = true;
    bool dirty = true; // redraw the current frame on the next loop pass
    while (running) {
        if (dirty) {
            cv::imshow(kWindow, render_overlay(boards[bi], fi));
            std::cout << "Viewer shows: board " << boards[bi].index << ", frame "
                      << boards[bi].frames[fi].frameNumber << "\n";
            dirty = false;
        }

        const int key = cv::waitKeyEx(30);

        // Mask to the low byte so letter/Esc keys match on either backend:
        // Qt reports plain ASCII ('d' == 0x64), while GTK adds a 0x100000
        // keysym offset plus modifier state ('d' == 0x100064); both -> 0x64.
        const int action = (key == -1) ? -1 : (key & 0xFF);
        switch (action) {
        case -1:
            break; // poll timeout, no key pressed
        case 'q':
        case kEsc:
            running = false;
            break;
        case 'd': // next frame, rolling into the next board
            if (fi + 1 < boards[bi].frames.size()) {
                ++fi;
                dirty = true;
            } else if (bi + 1 < boards.size()) {
                ++bi;
                fi = 0;
                dirty = true;
            }
            break;
        case 'a': // previous frame, rolling into the previous board
            if (fi > 0) {
                --fi;
                dirty = true;
            } else if (bi > 0) {
                --bi;
                fi = boards[bi].frames.size() - 1;
                dirty = true;
            }
            break;
        case 'n': // next board
            if (bi + 1 < boards.size()) {
                ++bi;
                fi = 0;
                dirty = true;
            }
            break;
        case 'p': // previous board
            if (bi > 0) {
                --bi;
                fi = 0;
                dirty = true;
            }
            break;
        default:
            break;
        }

        // Window closed via the title-bar
        if (cv::getWindowProperty(kWindow, cv::WND_PROP_AUTOSIZE) < 0.0) {
            running = false;
        }
    }

    cv::destroyAllWindows();
    return 0;
}

} // namespace smarti
