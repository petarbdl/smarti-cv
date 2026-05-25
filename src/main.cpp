#include <iostream>
#include <string>

#include <opencv2/core.hpp>

namespace {

void print_usage() {
    std::cout << "smarti-cv - wood-board knot tooling\n\n"
              << "Usage:\n"
              << "  smarti-cv view --dataset <dir> [--board <index>]\n"
              << "      Browse dataset frames with ground-truth knot overlays.\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
        print_usage();
        return 0;
    }

    std::cout << "smarti-cv (OpenCV " << CV_VERSION << ")\n";
    print_usage();
    return 0;
}
