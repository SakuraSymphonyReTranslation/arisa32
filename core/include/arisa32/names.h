#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace dc4 {
struct GameNames {
    std::vector<std::string> bgm, se, cg, scenes;
};
// Decode the indexed catalogues without losing empty/duplicate entries.
bool decode_game_names(const std::vector<uint8_t>& bytes, GameNames* out);
}
