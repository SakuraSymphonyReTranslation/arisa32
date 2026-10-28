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
                           const std::array<uint8_t,4>& rgba, bool rgb = false,
                           int y = 10, int height = 1) {
    std::vector<uint8_t> bytes{'C','R','X','G'};
    word(bytes, uint16_t(x)); word(bytes, uint16_t(y)); word(bytes, uint16_t(width));
    word(bytes, uint16_t(height)); word(bytes, 1); word(bytes, 0); word(bytes, rgb ? 0 : 1);
    word(bytes, uint16_t(mode));
    const int ps = rgb ? 3 : 4;
    const int stride = (width * ps + 3) & ~3;
    std::vector<uint8_t> raw(size_t(stride) * height, 0);
    for (int row = 0; row < height; ++row) for (int i = 0; i < width; ++i) {
        auto* p = raw.data() + size_t(row) * stride + i * ps;
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
fs::path write_archive(const fs::path& root, const std::vector<uint8_t>& base,
                       const std::vector<uint8_t>& patch) {
    const uint32_t base_off = 16 + 3 * 32;
    const uint32_t diff_off = base_off + uint32_t(base.size());
    const auto diff = wrapper(base_off, patch);
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
    return path;
}
void fixture(const fs::path& root, int base_mode, int diff_mode, bool rgb = false) {
    // The patch starts before the base and intersects only its first two
    // pixels. Transparency 0 minus delta 1 wraps to opaque 255, as in DC2PC.
    const auto path = write_archive(root, image(100, 3, base_mode, {220,210,200,0}, rgb),
                                   image(99, 3, diff_mode, {30,30,30,1}, rgb));
    dc4::CrmArchive archive; std::string error;
    CHECK(archive.open(path.string(), &error));
    for (const char* name : {"TEST_0A_000", "TEST_0A_000_B"}) {
        std::vector<uint8_t> rgba; dc4::CrxInfo info;
        CHECK(dc4::crxd_decode(archive, archive.find(name)->offset, &rgba, &info, &error));
        CHECK(info.offset_x == (rgb ? 100 : 99) && info.offset_y == 10 &&
              info.width == (rgb ? 3 : 4) && info.height == 1);
        std::vector<uint8_t> expected{250,240,230,255,250,240,230,255,
                                      220,210,200,uint8_t(rgb ? 255 : 0)};
        if (!rgb) expected.insert(expected.begin(), {250,240,230,255});
        CHECK(rgba == expected);
        std::vector<uint8_t> resolved; dc4::CrxInfo resolved_info;
        CHECK(dc4::bst_load(root.string(), std::string("bst\\") + name,
                           &resolved, &resolved_info, &error, true));
        CHECK(resolved == rgba);
    }
}
void expanded_bounds(const fs::path& root, int base_mode, int diff_mode) {
    // A hair/arm patch extends above and on both sides of the body, but ends
    // before the body's bottom. The two uncovered lower corners stay clear.
    const auto path = write_archive(root,
        image(100, 3, base_mode, {220,210,200,128}, false, 10, 3),
        image(99, 5, diff_mode, {30,30,30,129}, false, 9, 3));
    dc4::CrmArchive archive; std::string error;
    CHECK(archive.open(path.string(), &error));
    const std::array<uint8_t,4> transparent{0,0,0,0}, base{220,210,200,128},
        added{250,240,230,255}, patch{30,30,30,127};
    std::vector<uint8_t> expected;
    for (const auto& pixel : {patch,patch,patch,patch,patch,
                             patch,added,added,added,patch,
                             patch,added,added,added,patch,
                             transparent,base,base,base,transparent})
        expected.insert(expected.end(), pixel.begin(), pixel.end());
    for (const char* name : {"TEST_0A_000", "TEST_0A_000_B"}) {
        std::vector<uint8_t> rgba; dc4::CrxInfo info;
        CHECK(dc4::crxd_decode(archive, archive.find(name)->offset, &rgba, &info, &error));
        CHECK(info.offset_x == 99 && info.offset_y == 9 && info.width == 5 && info.height == 4);
        CHECK(rgba == expected);
    }
}
void transparent_background(const fs::path& root, int base_mode, int diff_mode) {
    const auto path = write_archive(root, image(100, 2, base_mode, {255,0,255,0}),
                                    image(99, 2, diff_mode, {0,237,237,1}, false, 9, 2));
    dc4::CrmArchive archive; std::string error;
    CHECK(archive.open(path.string(), &error));
    for (const char* name : {"TEST_0A_000", "TEST_0A_000_B"}) {
        std::vector<uint8_t> rgba; dc4::CrxInfo info;
        CHECK(dc4::crxd_decode(archive, archive.find(name)->offset, &rgba, &info, &error));
        CHECK(info.offset_x == 99 && info.offset_y == 9 && info.width == 3 && info.height == 2);
        const std::vector<uint8_t> expected{255,237,236,255, 255,237,236,255, 255,0,255,0,
                                            255,237,236,255, 255,237,236,255, 255,0,255,0};
        CHECK(rgba == expected);
    }
}
void sparse_bounds(const fs::path& root) {
    // Non-overlapping patches still contribute pixels, including below the
    // base, while the space between the two rectangles remains transparent.
    auto path = write_archive(root, image(100, 1, 0, {80,90,100,255}),
                              image(102, 1, 2, {20,30,40,1}, false, 12));
    dc4::CrmArchive archive; std::string error;
    CHECK(archive.open(path.string(), &error));
    std::vector<uint8_t> rgba; dc4::CrxInfo info;
    CHECK(dc4::crxd_decode(archive, archive.find("TEST_0A_000")->offset, &rgba, &info, &error));
    CHECK(info.offset_x == 100 && info.offset_y == 10 && info.width == 3 && info.height == 3);
    std::vector<uint8_t> expected(3 * 3 * 4, 0);
    const std::array<uint8_t,4> first{80,90,100,255}, last{20,30,40,255};
    std::copy(first.begin(), first.end(), expected.begin());
    std::copy(last.begin(), last.end(), expected.end() - 4);
    CHECK(rgba == expected);

    path = write_archive(root, image(100, 1, 0, first), image(0, 0, 2, last, false, 0, 0));
    CHECK(archive.open(path.string(), &error));
    CHECK(dc4::crxd_decode(archive, archive.find("TEST_0A_000")->offset, &rgba, &info, &error));
    CHECK(info.offset_x == 100 && info.offset_y == 10 && info.width == 1 && info.height == 1);
    CHECK(rgba == std::vector<uint8_t>(first.begin(), first.end()));

    path = write_archive(root, image(-32768, 1, 0, first), image(32767, 1, 2, last));
    CHECK(archive.open(path.string(), &error));
    CHECK(!dc4::crxd_decode(archive, archive.find("TEST_0A_000")->offset, &rgba, &info, &error));
    CHECK(error == "CRXD combined bounds out of range");
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
            if (entry.name == "OTM_0B_000") {
                CHECK(info.offset_x == 265 && info.offset_y == 44 && info.width == 398 && info.height == 712);
                const size_t at = (size_t(297 - info.offset_y) * info.width + 590 - info.offset_x) * 4;
                CHECK(at + 4 <= rgba.size());
                if (at + 4 <= rgba.size())
                    CHECK(rgba[at] == 255 && rgba[at+1] == 237 && rgba[at+2] == 236 && rgba[at+3] == 255);
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
        for (int base : {0,1,2}) for (int diff : {0,1,2}) {
            fixture(root, base, diff); expanded_bounds(root, base, diff); transparent_background(root, base, diff);
        }
        fixture(root, 0, 0, true); sparse_bounds(root); installed();
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; ++failures; }
    std::error_code ignored; fs::remove_all(root, ignored);
    std::cout << (failures ? "CRM regressions FAILED\n" : "CRM regressions passed\n");
    return failures ? 1 : 0;
}
