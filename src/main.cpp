#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include <opencv2/core.hpp>

#include "smarti/frame_loader.hpp"
#include "smarti/label.hpp"

namespace {

void print_usage() {
    std::cout << "smarti-cv - wood-board knot tooling\n\n"
              << "Usage:\n"
              << "  smarti-cv view --dataset <dir> [--board <index>]\n"
              << "      Browse dataset frames with ground-truth knot overlays.\n";
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

    // TODO(phase1 commit 4): open a highgui window and render frames + overlays.
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
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    std::cerr << "unknown command: " << command << "\n\n";
    print_usage();
    return 2;
}
