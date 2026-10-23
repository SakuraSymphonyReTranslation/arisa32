#include "arisa32/names.h"
#include <cstring>

namespace dc4 {
bool decode_game_names(const std::vector<uint8_t>& bytes, GameNames* out) {
    if (!out || bytes.size() < 4) return false;
    auto u32 = [&](size_t p) {
        return uint32_t(bytes[p]) | uint32_t(bytes[p+1]) << 8 |
               uint32_t(bytes[p+2]) << 16 | uint32_t(bytes[p+3]) << 24;
    };
    const auto version = u32(0);
    // v6: six 128-byte script names, BGM 192, SE 132, CG 128,
    // scenario 188. v7 has a 224-byte header and shorter records.
    size_t at = version == 6 ? 772 : 228;
    if (version != 6 && version != 7) return false;
    GameNames result;
    auto table = [&](size_t stride, size_t name_size, unsigned limit,
                     std::vector<std::string>& names) {
        if (at > bytes.size() || bytes.size() - at < 4) return false;
        const auto count = u32(at); at += 4;
        if (count > limit || count > (bytes.size()-at)/stride) return false;
        for (unsigned i=0; i<count; ++i, at+=stride) {
            const char* first = reinterpret_cast<const char*>(bytes.data()+at);
            const char* end = static_cast<const char*>(std::memchr(first, 0, name_size));
            if (!end) return false;
            names.emplace_back(first,end);
        }
        return true;
    };
    if (!table(version==6?192:48, version==6?128:32, 400, result.bgm) ||
        !table(version==6?132:36, version==6?128:32, 600, result.se) ||
        !table(version==6?128:32, version==6?128:32, 2000, result.cg) ||
        !table(version==6?188:32, version==6?128:32, 2000, result.scenes)) return false;
    *out = std::move(result);
    return true;
}
}
