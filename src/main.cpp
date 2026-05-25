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
              << "  smarti-cv detect --dataset <dir> [--board <index>] [--save <dir>]\n"
              << "      Detect knots in each frame and print their bounding boxes.\n"
              << "      With --save, write comparison overlays (green=truth, red=detected).\n";
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

    std::cout << "Loaded " << boards.size() << " boards, " << totalFrames << " frames ("
              << labelled << " with labels, " << totalKnots << " knots) from " << *dataset
              << "\n";

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
            const std::string out = *outDir + "/board" + std::to_string(board.index) +
                                    "_whole_" + (vertical ? "v" : "h") + ".png";
            cv::imwrite(out, smarti::render_board_composite(board, vertical));
            std::cout << "wrote " << out << "\n";
        } else {
            for (std::size_t fi = 0; fi < board.frames.size(); ++fi) {
                const std::string out = *outDir + "/board" + std::to_string(board.index) +
                                        "_frame" +
                                        std::to_string(board.frames[fi].frameNumber) + ".png";
                cv::imwrite(out, smarti::render_overlay(board, fi));
                std::cout << "wrote " << out << "\n";
            }
        }
        return 0;
    }

    return smarti::run_viewer(boards, startBoard);
}

int run_detect(int argc, char** argv) {
    const auto dataset = get_opt(argc, argv, "--dataset");
    if (!dataset) {
        std::cerr << "error: detect requires --dataset <dir>\n";
        return 2;
    }

    const auto boards = smarti::load_dataset(*dataset);
    const auto outDir = get_opt(argc, argv, "--save");

    int onlyBoard = -1;
    if (const auto board = get_opt(argc, argv, "--board")) {
        onlyBoard = std::stoi(*board);
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
                std::cout << "  [" << r.x << "," << r.y << "," << r.width << "," << r.height
                          << "]";
            }
            std::cout << "\n";

            if (outDir) {
                std::vector<cv::Rect> gt;
                for (const auto& box : smarti::load_yolo_labels(ref.labelPath)) {
                    gt.push_back(smarti::to_pixel_rect(box, image.size()));
                }
                const std::string status =
                    std::to_string(board.index) + "_" + std::to_string(ref.frameNumber) +
                    "  truth:" + std::to_string(gt.size()) +
                    " det:" + std::to_string(detections.size());
                const std::string out = *outDir + "/board" + std::to_string(board.index) +
                                        "_frame" + std::to_string(ref.frameNumber) + "_det.png";
                cv::imwrite(out, smarti::render_comparison(image, gt, detections, status));
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
            return run_detect(argc, argv);
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    std::cerr << "unknown command: " << command << "\n\n";
    print_usage();
    return 2;
}
