#pragma once

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace snapshot_fixture {
inline void strip_idle_v11_motion(std::vector<uint8_t>& state) {
    // These legacy fixtures have default sprite state and no queued dialogue events.
    constexpr size_t sprite_bytes = 61;
    std::vector<uint8_t> expected(16 + 16 * sprite_bytes + 11 * 4, 0);
    expected[8] = 60;
    for (size_t i = 0; i < 16; ++i) expected[16 + i * sprite_bytes] = 255;
    if (state.size() < 24 + expected.size() || state[8] != 11 ||
        state[9] || state[10] || state[11])
        throw std::runtime_error("legacy fixture requires a v11 snapshot");
    const size_t start = state.size() - expected.size();
    // The clock is irrelevant once all queues are empty; validate every other field.
    if (!std::equal(expected.begin() + 8, expected.end(), state.begin() + start + 8))
        throw std::runtime_error("legacy fixture contains non-default sprite motion or pending events");
    state.resize(start);
}
}
