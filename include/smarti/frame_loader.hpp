#pragma once

#include <string>
#include <vector>

#include "smarti/types.hpp"

namespace smarti {

// Scan a WoodDataset directory and group its frames into boards.
//
// Expected layout:
//   <datasetRoot>/images/{board}_{frame}.png
//   <datasetRoot>/labels/{board}_{frame}.txt   (optional, matched per frame)
//
// Returns boards sorted by index, each with frames sorted by frameNumber.
// Throws std::runtime_error if the images directory does not exist.
std::vector<Board> load_dataset(const std::string& datasetRoot);

} // namespace smarti
