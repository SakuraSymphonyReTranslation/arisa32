#pragma once
#include "arisa32/crx.h"
#include "arisa32/resource.h"
#include <fstream>
#include <iterator>

namespace dc4ui {
inline std::vector<uint8_t> dc2dm_title_particle_sheet(const std::string& data) {
    std::ifstream file(dc4::resolve_asset(data + "/GRP/SYSTEM/EFF.CRX"), std::ios::binary);
    const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file), {}};
    std::vector<uint8_t> rgba;
    dc4::CrxInfo info;
    if (!dc4::crx_decode(bytes.data(), bytes.size(), &rgba, &info) || info.width != 800 || info.height != 32)
        return {};
    return rgba;
}
} // namespace dc4ui
