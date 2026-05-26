#include "smarti/frame_loader.hpp"

#include <algorithm>
#include <filesystem>
#include <map>
#include <stdexcept>

namespace fs = std::filesystem;

namespace smarti {

namespace {

// Parse a "{board}_{frame}" stem into its two integer parts.
// Returns false if the stem is not in that exact form.
bool parse_stem(const std::string& stem, int& board, int& frame) {
    const auto underscore = stem.find('_');
    if (underscore == std::string::npos || underscore == 0 || underscore + 1 >= stem.size()) {
        return false;
    }
    try {
        size_t consumed = 0;
        board = std::stoi(stem.substr(0, underscore), &consumed);
        if (consumed != underscore) {
            return false;
        }
        const std::string frameStr = stem.substr(underscore + 1);
        frame = std::stoi(frameStr, &consumed);
        if (consumed != frameStr.size()) {
            return false;
        }
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

} // namespace

std::vector<Board> load_dataset(const std::string& datasetRoot) {
    const fs::path root(datasetRoot);
    const fs::path imagesDir = root / "images";
    const fs::path labelsDir = root / "labels";

    if (!fs::is_directory(imagesDir)) {
        throw std::runtime_error("images directory not found: " + imagesDir.string());
    }

    std::map<int, Board> boards; // keyed by board index, keeps them sorted

    for (const auto& entry : fs::directory_iterator(imagesDir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".png") {
            continue;
        }
        int board = 0;
        int frame = 0;
        if (!parse_stem(entry.path().stem().string(), board, frame)) {
            continue; // skip files that don't match {board}_{frame}.png
        }

        FrameRef ref;
        ref.boardIndex = board;
        ref.frameNumber = frame;
        ref.imagePath = entry.path().string();

        const fs::path label = labelsDir / (entry.path().stem().string() + ".txt");
        if (fs::is_regular_file(label)) {
            ref.labelPath = label.string();
        }

        boards[board].index = board;
        boards[board].frames.push_back(std::move(ref));
    }

    std::vector<Board> result;
    result.reserve(boards.size());
    for (auto& [index, board] : boards) {
        std::sort(
            board.frames.begin(), board.frames.end(),
            [](const FrameRef& a, const FrameRef& b) { return a.frameNumber < b.frameNumber; });
        result.push_back(std::move(board));
    }
    return result;
}

} // namespace smarti
