#include "arisa32/crm.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
namespace {
int failures = 0;
void check(bool value, int line) {
    if (!value) { std::cerr << "line " << line << '\n'; ++failures; }
}
#define CHECK(x) check(bool(x), __LINE__)
void word(std::vector<uint8_t>& bytes, uint16_t value) {
    bytes.push_back(uint8_t(value)); bytes.push_back(uint8_t(value >> 8));
}
void dword(std::vector<uint8_t>& bytes, uint32_t value) {
    word(bytes, uint16_t(value)); word(bytes, uint16_t(value >> 16));
}
// Literal-only CRXG, with RGBA converted to the file's raw channel order.
std::vector<uint8_t> image(int x, int width, int mode,
                           const std::array<uint8_t,4>& rgba, bool rgb = false) {
    std::vector<uint8_t> bytes{'C','R','X','G'};
    word(bytes, uint16_t(x)); word(bytes, 10); word(bytes, uint16_t(width));
    word(bytes, 1); word(bytes, 1); word(bytes, 0); word(bytes, rgb ? 0 : 1);
    word(bytes, uint16_t(mode));
    const int ps = rgb ? 3 : 4;
    std::vector<uint8_t> raw(size_t((width * ps + 3) & ~3), 0);
    for (int i = 0; i < width; ++i) {
        auto* p = raw.data() + i * ps;
        if (!rgb) *p++ = mode == 0 ? uint8_t(rgba[3] ^ 255) : rgba[3];
        *p++ = rgba[2]; *p++ = rgba[1]; *p = rgba[0];
    }
    for (size_t i = 0; i < raw.size(); i += 8) {
        bytes.push_back(255);
        bytes.insert(bytes.end(), raw.begin() + i, raw.begin() + std::min(i + 8, raw.size()));
    }
    return bytes;
}
std::vector<uint8_t> wrapper(uint32_t base, const std::vector<uint8_t>& diff) {
    std::vector<uint8_t> bytes{'C','R','X','D'};
    dword(bytes, 0); dword(bytes, base); bytes.resize(32, 0);
    bytes.insert(bytes.end(), diff.begin(), diff.end()); return bytes;
}
void fixture(const fs::path& root, int base_mode, int diff_mode, bool rgb = false) {
    // The patch starts before the base and intersects only its first two
    // pixels. Transparency 0 minus delta 1 wraps to opaque 255, as in DC2PC.
    const auto base = image(100, 3, base_mode, {220,210,200,0}, rgb);
    const uint32_t base_off = 16 + 3 * 32;
    const uint32_t diff_off = base_off + uint32_t(base.size());
    const auto diff = wrapper(base_off, image(99, 3, diff_mode, {30,30,30,1}, rgb));
    std::vector<uint8_t> jump{'C','R','X','J'};
    dword(jump, 0); dword(jump, diff_off);
    const auto linked = wrapper(base_off, jump);
    std::vector<uint8_t> bytes{'C','R','X','B'};
    dword(bytes, 1); dword(bytes, 3); word(bytes, 800); word(bytes, 600);
    const uint32_t offsets[]{base_off, diff_off, diff_off + uint32_t(diff.size())};
    const char* names[]{"BASE.CRX", "TEST_0A_000.CRX", "TEST_0A_000_B.CRX"};
    for (int i = 0; i < 3; ++i) {
        dword(bytes, offsets[i]); dword(bytes, uint32_t(i));
        const size_t start = bytes.size();
        for (const char* p = names[i]; *p; ++p) bytes.push_back(uint8_t(*p));
        bytes.resize(start + 24, 0);
    }
    for (const auto* part : {&base, &diff, &linked}) bytes.insert(bytes.end(), part->begin(), part->end());
    const auto path = root / "test_0a.CRM";
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size())); out.close();
    dc4::CrmArchive archive; std::string error;
    CHECK(archive.open(path.string(), &error));
    for (const char* name : {"TEST_0A_000", "TEST_0A_000_B"}) {
        std::vector<uint8_t> rgba; dc4::CrxInfo info;
        CHECK(dc4::crxd_decode(archive, archive.find(name)->offset, &rgba, &info, &error));
        CHECK(info.offset_x == 100 && info.offset_y == 10 && info.width == 3);
        CHECK(rgba == std::vector<uint8_t>({250,240,230,255,250,240,230,255,
                                          220,210,200,uint8_t(rgb ? 255 : 0)}));
        std::vector<uint8_t> resolved; dc4::CrxInfo resolved_info;
        CHECK(dc4::bst_load(root.string(), std::string("bst\\") + name,
                           &resolved, &resolved_info, &error, true));
        CHECK(resolved == rgba);
    }
}
void installed() {
    const char* data = std::getenv("DC2PC_CRM_DATA");
    if (!data) return;
    int archives = 0, variants = 0;
    for (const auto& file : fs::directory_iterator(fs::path(data) / "GRP/BST")) {
        auto extension = file.path().extension().string();
        if (extension != ".CRM" && extension != ".crm") continue;
        dc4::CrmArchive archive; std::string error;
        CHECK(archive.open(file.path().string(), &error)); ++archives;
        for (const auto& entry : archive.entries()) {
            std::vector<uint8_t> rgba; dc4::CrxInfo info;
            const bool ok = dc4::crxd_decode(archive, entry.offset, &rgba, &info, &error);
            if (!ok) std::cerr << file.path() << ':' << entry.name << ": " << error << '\n';
            CHECK(ok); CHECK(rgba.size() == size_t(info.width) * info.height * 4); ++variants;
            if (!entry.name.empty() && entry.name[0] != '_') {
                const auto ref = dc4::bst_parse_name("bst\\" + entry.name, true);
                CHECK(ref.entry == entry.name);
                std::string expected = file.path().stem().string(), actual = ref.archive;
                for (auto& c : expected) c = char(std::tolower(static_cast<unsigned char>(c)));
                for (auto& c : actual) c = char(std::tolower(static_cast<unsigned char>(c)));
                CHECK(actual == expected);
                CHECK(archive.find(ref.entry) == &entry);
            }
            if (entry.name == "SGN_0A_000") {
                // Real skin pixel in the face patch used to be almost transparent.
                const size_t at = (size_t(155) * info.width + 104) * 4;
                CHECK(at + 4 <= rgba.size());
                if (at + 4 <= rgba.size()) {
                    CHECK(rgba[at] == 255 && rgba[at+1] == 249 && rgba[at+2] == 224 && rgba[at+3] == 255);
                }
            }
        }
    }
    CHECK(archives > 0 && variants > 0);
    std::cout << "Decoded " << variants << " variants in " << archives << " BST archives.\n";
}
}
int main() {
    const auto root = fs::temp_directory_path() / ("arisa32-crm-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    try {
        CHECK(dc4::bst_parse_name("BST\\TEST_0A_000_B", true).archive == "TEST_0A");
        CHECK(dc4::bst_parse_name("bst\\test_0a_000_b", true).entry == "test_0a_000_b");
        CHECK(dc4::bst_parse_name("BST\\TEST_0A_000_B,BST\\TEST_0A").entry == "TEST_0A_000");
        for (int base : {0,1,2}) for (int diff : {0,1,2}) fixture(root, base, diff);
        fixture(root, 0, 0, true); installed();
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; ++failures; }
    std::error_code ignored; fs::remove_all(root, ignored);
    std::cout << (failures ? "CRM regressions FAILED\n" : "CRM regressions passed\n");
    return failures ? 1 : 0;
}
