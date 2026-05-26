#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include "smarti/detector.hpp"
#include "smarti/frame_loader.hpp"
#include "smarti/label.hpp"
#include "smarti/viewer.hpp"

namespace {

void print_usage() {
    std::cout << "smarti-cv - wood-board knot tooling\n\n"
              << "Usage:\n"
              << "  smarti-cv view --dataset <dir> [--board <index>]\n"
              << "      Browse dataset frames with ground-truth knot overlays.\n"
              << "  smarti-cv detect --dataset <dir> [--board <index>] [--whole] [--save <dir>]\n"
              << "      Detect knots and print their bounding boxes. Per frame by default;\n"
              << "      with --whole, aggregate into each board's stitched coordinate space\n"
              << "      and report one box list per board. With --save, write detection\n"
              << "      overlays (red).\n"
              << "  smarti-cv test --dataset <dir> [--board <index>] [--whole] [--save <dir>]\n"
              << "      Like detect, but --save overlays also draw ground truth (green)\n"
              << "      alongside the detections (red) for comparison.\n";
}

// Minimal flag lookup: returns the value following `flag`, if present.
std::optional<std::string> get_opt(int argc, char** argv, const std::string& flag) {
    for (int i = 2; i + 1 < argc; ++i) {
        if (flag == argv[i]) {
            return std::string(argv[i + 1]);
        }
    }
    return std::nullopt;
}

// True if `flag` appears anywhere in the argument list.
bool has_flag(int argc, char** argv, const std::string& flag) {
    for (int i = 2; i < argc; ++i) {
        if (flag == argv[i]) {
            return true;
        }
    }
    return false;
}

int run_view(int argc, char** argv) {
    const auto dataset = get_opt(argc, argv, "--dataset");
    if (!dataset) {
        std::cerr << "error: view requires --dataset <dir>\n";
        return 2;
    }

    const auto boards = smarti::load_dataset(*dataset);

    std::size_t totalFrames = 0;
    std::size_t labelled = 0;
    std::size_t totalKnots = 0;
    for (const auto& board : boards) {
        totalFrames += board.frames.size();
        for (const auto& f : board.frames) {
            if (!f.labelPath.empty()) {
                ++labelled;
                totalKnots += smarti::load_yolo_labels(f.labelPath).size();
            }
        }
    }

    std::cout << "Loaded " << boards.size() << " boards, " << totalFrames << " frames (" << labelled
              << " with labels, " << totalKnots << " knots) from " << *dataset << "\n";

    int startBoard = -1;
    if (const auto board = get_opt(argc, argv, "--board")) {
        startBoard = std::stoi(*board);
    }

    // Resolve the starting board's position in the list.
    std::size_t bi = 0;
    if (startBoard >= 0) {
        for (std::size_t i = 0; i < boards.size(); ++i) {
            if (boards[i].index == startBoard) {
                bi = i;
                break;
            }
        }
    }

    const bool whole = has_flag(argc, argv, "--whole");
    // Boards stitch end-to-end along X (columns), so horizontal is the default.
    const bool vertical = get_opt(argc, argv, "--axis").value_or("h") == "v";
    const auto outDir = get_opt(argc, argv, "--save");

    // Headless: render to PNG(s) and exit (no window). Useful without a display.
    if (outDir) {
        const smarti::Board& board = boards[bi];
        if (whole) {
            const std::string out = *outDir + "/board" + std::to_string(board.index) + "_whole_" +
                                    (vertical ? "v" : "h") + ".png";
            cv::imwrite(out, smarti::render_board_composite(board, vertical));
            std::cout << "wrote " << out << "\n";
        } else {
            for (std::size_t fi = 0; fi < board.frames.size(); ++fi) {
                const std::string out = *outDir + "/board" + std::to_string(board.index) +
                                        "_frame" + std::to_string(board.frames[fi].frameNumber) +
                                        ".png";
                cv::imwrite(out, smarti::render_overlay(board, fi));
                std::cout << "wrote " << out << "\n";
            }
        }
        return 0;
    }

    return smarti::run_viewer(boards, startBoard);
}

// Aggregate a board's ground-truth knot boxes into board space
std::vector<cv::Rect> board_ground_truth(const smarti::Board& board, int seamMergePx) {
    std::vector<cv::Rect> boxes;
    int xOffset = 0;
    for (const auto& ref : board.frames) {
        const cv::Mat image = cv::imread(ref.imagePath, cv::IMREAD_COLOR);
        if (image.empty()) {
            continue;
        }
        for (const auto& yolo : smarti::load_yolo_labels(ref.labelPath)) {
            cv::Rect r = smarti::to_pixel_rect(yolo, image.size());
            r.x += xOffset;
            boxes.push_back(r);
        }
        xOffset += image.cols;
    }
    return smarti::merge_boxes(std::move(boxes), seamMergePx);
}

// Whole-board mode for detect or test mode
int run_detect_whole(const std::vector<smarti::Board>& boards, int onlyBoard,
                     const std::optional<std::string>& outDir, bool drawTruth) {
    const smarti::DetectorParams params;
    std::size_t totalDet = 0;
    for (const auto& board : boards) {
        if (onlyBoard >= 0 && board.index != onlyBoard) {
            continue;
        }

        const std::vector<cv::Rect> detections = smarti::detect_board_knots(board, params);
        totalDet += detections.size();

        std::cout << "board " << board.index << ": " << detections.size() << " knot(s)";
        for (const auto& r : detections) {
            std::cout << "  [" << r.x << "," << r.y << "," << r.width << "," << r.height << "]";
        }
        std::cout << "\n";

        if (outDir) {
            const cv::Mat canvas = smarti::stitch_board(board);
            std::string status = "board " + std::to_string(board.index) +
                                 "  det:" + std::to_string(detections.size());
            cv::Mat overlay;
            std::string suffix;
            if (drawTruth) {
                const std::vector<cv::Rect> gt = board_ground_truth(board, params.seamMergePx);
                status += "  truth:" + std::to_string(gt.size());
                overlay = smarti::render_evaluation(canvas, gt, detections, status);
                suffix = "_whole_test.png";
            } else {
                overlay = smarti::render_comparison(canvas, detections, status);
                suffix = "_whole_det.png";
            }
            const std::string out = *outDir + "/board" + std::to_string(board.index) + suffix;
            cv::imwrite(out, overlay);
            std::cout << "wrote " << out << "\n";
        }
    }

    std::cout << "detected " << totalDet << " knot(s) total across boards\n";
    return 0;
}

// Shared func for detect and test modes
int run_detect(int argc, char** argv, bool drawTruth) {
    const char* cmd = drawTruth ? "test" : "detect";
    const auto dataset = get_opt(argc, argv, "--dataset");
    if (!dataset) {
        std::cerr << "error: " << cmd << " requires --dataset <dir>\n";
        return 2;
    }

    const auto boards = smarti::load_dataset(*dataset);
    const auto outDir = get_opt(argc, argv, "--save");

    int onlyBoard = -1;
    if (const auto board = get_opt(argc, argv, "--board")) {
        onlyBoard = std::stoi(*board);
    }

    if (has_flag(argc, argv, "--whole")) {
        return run_detect_whole(boards, onlyBoard, outDir, drawTruth);
    }

    std::size_t totalDet = 0;
    for (const auto& board : boards) {
        if (onlyBoard >= 0 && board.index != onlyBoard) {
            continue;
        }
        for (std::size_t fi = 0; fi < board.frames.size(); ++fi) {
            const smarti::FrameRef& ref = board.frames[fi];
            const cv::Mat image = cv::imread(ref.imagePath, cv::IMREAD_COLOR);
            if (image.empty()) {
                std::cerr << "warning: failed to read " << ref.imagePath << "\n";
                continue;
            }

            const std::vector<cv::Rect> detections = smarti::detect_knots(image);
            totalDet += detections.size();

            std::cout << board.index << "_" << ref.frameNumber << ": " << detections.size()
                      << " knot(s)";
            for (const auto& r : detections) {
                std::cout << "  [" << r.x << "," << r.y << "," << r.width << "," << r.height << "]";
            }
            std::cout << "\n";

            if (outDir) {
                std::vector<cv::Rect> gt;
                for (const auto& box : smarti::load_yolo_labels(ref.labelPath)) {
                    gt.push_back(smarti::to_pixel_rect(box, image.size()));
                }
                const std::string status = std::to_string(board.index) + "_" +
                                           std::to_string(ref.frameNumber) +
                                           "  truth:" + std::to_string(gt.size()) +
                                           " det:" + std::to_string(detections.size());
                const std::string suffix = drawTruth ? "_test.png" : "_det.png";
                const std::string out = *outDir + "/board" + std::to_string(board.index) +
                                        "_frame" + std::to_string(ref.frameNumber) + suffix;
                cv::imwrite(out, drawTruth
                                     ? smarti::render_evaluation(image, gt, detections, status)
                                     : smarti::render_comparison(image, detections, status));
            }
        }
    }

    std::cout << "detected " << totalDet << " knot(s) total\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
        print_usage();
        return 0;
    }

    const std::string command = argv[1];
    try {
        if (command == "view") {
            return run_view(argc, argv);
        }
        if (command == "detect") {
            return run_detect(argc, argv, /*drawTruth=*/false);
        }
        if (command == "test") {
            return run_detect(argc, argv, /*drawTruth=*/true);
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    std::cerr << "unknown command: " << command << "\n\n";
    print_usage();
    return 2;
}
