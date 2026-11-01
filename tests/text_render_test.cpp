#include "arisa32/scene.h"
#include "arisa32/resource.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
int failures = 0;
void check(bool ok, int line) {
    if (!ok) { std::cerr << "text rendering line " << line << '\n'; ++failures; }
}
#define CHECK(x) check(bool(x), __LINE__)
void script(const std::filesystem::path& path, const std::string& text) {
    std::vector<uint8_t> bytes{1,0,0,0,3,0,0,0,0,0,0xB6,0xAA,0,0x6B};
    for (unsigned char ch : text) bytes.push_back(uint8_t(ch - 0x20));
    bytes.push_back(0); bytes.push_back(0);
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}
std::vector<uint8_t> render(const std::filesystem::path& root, const std::string& text,
                            dc4scene::FontRenderer& font, int reveal = -1) {
    script(root / "text.mes", text);
    dc4scene::SceneRunner scene(root.string());
    scene.set_font(&font);
    CHECK(scene.load_script((root / "text.mes").string()));
    CHECK(scene.advance_to_next_text());
    scene.draw(reveal);
    return scene.frame();
}
struct DmScript {
    std::vector<uint8_t> bytes{1,0,0,0,3,0,0,0,0,0,0x72,0x9D,0};
    void words(uint8_t op, std::initializer_list<uint16_t> values) {
        bytes.push_back(op);
        for (auto value : values) { bytes.push_back(uint8_t(value)); bytes.push_back(uint8_t(value >> 8)); }
    }
    void text(uint8_t op, const std::string& value) {
        bytes.push_back(op);
        for (auto ch : value) bytes.push_back(uint8_t(uint8_t(ch) - 0x20));
        bytes.push_back(0);
    }
    void save(const std::filesystem::path& path) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
};
void dc2dm_rendering(const std::filesystem::path& root, const char* font_path) {
    dc4scene::FontRenderer font;
    CHECK(font.load(font_path, 0, 22, false));
    CHECK(font.text_width(u"iiii", 22, false, true, -1) == 40);
    CHECK(font.text_width(u"WWWW", 22, false, true, -1) == 40);
    CHECK(font.text_width(u"i\u3042", 22, false, true, -1) == 31);
    DmScript ui;
    ui.bytes.push_back(0);
    ui.save(root / "MES/start.mes");
    DmScript dialogue;
    dialogue.text(0x4E, "Yoshiyuki");
    dialogue.text(0x4F, "Oleh_suara_jam_weker_yang_berbunyi_di_samping_\nbantal,_kesadaranku_mulai_terbangun_perlahan.");
    dialogue.bytes.push_back(0);
    dialogue.save(root / "MES/dialogue.mes");
    dc4scene::SceneRunner scene(root.string());
    scene.set_font(&font);
    CHECK(scene.load_ui_script((root / "MES/start.mes").string()));
    CHECK(scene.load_script((root / "MES/dialogue.mes").string()));
    CHECK(scene.advance_to_next_text());
    scene.draw(-1);
    const auto& frame = scene.frame();
    int first_top = 720, first_bottom = -1, first_right = -1;
    for (int y = 565; y < 598; ++y) for (int x = 380; x < 1200; ++x) {
        const auto at = (size_t(y) * 1280 + x) * 4;
        if (frame[at] < 100 || frame[at+1] < 100 || frame[at+2] < 100) continue;
        first_top = std::min(first_top, y); first_bottom = std::max(first_bottom, y);
        first_right = std::max(first_right, x);
    }
    // Native 22px cells use a 10px Latin advance and 17px translated glyphs.
    CHECK(first_bottom - first_top >= 16);
    CHECK(first_right >= 940 && first_right < 990);
    CHECK(first_top >= 570 && first_top < 580);
    int name_left = 1280, name_top = 720;
    for (int y = 520; y < 563; ++y) for (int x = 300; x < 520; ++x) {
        const auto at = (size_t(y) * 1280 + x) * 4;
        if (frame[at] < 100 || frame[at+1] < 100 || frame[at+2] < 100) continue;
        name_left = std::min(name_left, x); name_top = std::min(name_top, y);
    }
    // Native centers the nine-byte name in five 22px columns at x=264.
    CHECK(name_left >= 335 && name_left <= 340);
    CHECK(name_top >= 532 && name_top <= 540);

    std::filesystem::create_directories(root / "GRP/SYSTEM");
    std::vector<uint8_t> crx{'C','R','X','G',0,0,0,0,16,0,16,0,1,0,0,0,0,0,0,0};
    for (int i = 0; i < 16*16*3; i += 8) {
        crx.push_back(255);
        for (int n = 0; n < 8; ++n) crx.push_back(255);
    }
    std::ofstream image(root / "GRP/SYSTEM/date.CRX", std::ios::binary);
    image.write(reinterpret_cast<const char*>(crx.data()), crx.size()); image.close();
    DmScript date;
    date.bytes.insert(date.bytes.end(), {0x2B,10});
    const std::string tile = "SYSTEM\\date";
    date.bytes.insert(date.bytes.end(), tile.begin(), tile.end()); date.bytes.push_back(0);
    date.words(0x68, {12,440,0,0});
    date.words(0x5A, {36,0,0,0}); date.words(0x57, {16,16,10,1});
    date.words(0x68, {40,uint16_t(-32),1,0});
    date.words(0x68, {8,0,2,0});
    date.words(0x5A, {36,0,0,0}); date.words(0x57, {16,16,10,1});
    date.words(0x62, {3,0,1,0});
    date.words(0x68, {0,8,3,0x0300});
    date.words(0x5A, {70,0,0,0}); date.words(0x57, {16,16,10,1});
    date.words(0x68, {0,0,2,0}); date.words(0x68, {0,0,0,0});
    date.words(0x5A, {8,8,0,0}); date.words(0x57, {16,16,10,1});
    date.text(0x4F, ""); date.bytes.push_back(0);
    date.save(root / "MES/date.mes");
    dc4scene::SceneRunner dated(root.string());
    CHECK(dated.load_script((root / "MES/date.mes").string()));
    dated.advance_to_next_text(); dated.draw(-1);
    auto lit = [&](int x, int y) { return dated.frame()[(size_t(y)*1280+x)*4] == 255; };
    CHECK(lit(65, 555));
    CHECK(!lit(65, 5));
    CHECK(lit(114, 514));
    CHECK(!lit(125, 514));
    CHECK(lit(160, 514));
    CHECK(!lit(160, 525));
    CHECK(lit(15, 15));
    CHECK(!lit(30, 565));
}
}
int main(int argc, char** argv) {
    const auto root = std::filesystem::temp_directory_path() / ("arisa32-text-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    dc4::configure_resources(root.string(), "", "windows-31j");
    dc4::set_language(dc4::Language::Indonesian);
    // Shift-JIS fullwidth Latin and period from a translated MES.
    const std::string fullwidth = "\x82\x72\x82\x85\x82\x8f\x82\x8c\x82\x81\x82\x88\x81\x44";
    script(root / "text.mes", fullwidth);
    dc4scene::SceneRunner decoded(root.string());
    CHECK(decoded.load_script((root / "text.mes").string()));
    CHECK(decoded.advance_to_next_text());
#ifdef _WIN32
    CHECK(decoded.visible_text() == u"Seolah.");
    dc4::set_language(dc4::Language::Japanese);
    CHECK(decoded.visible_text() == u"\uff33\uff45\uff4f\uff4c\uff41\uff48\uff0e");
    dc4::set_language(dc4::Language::Indonesian);
#endif
    // Force the Android stb path even on Windows, using the bundled PGothic face.
    dc4scene::FontRenderer font;
    if (argc < 2 || !font.load(argv[1], 2, 20, false)) {
        std::cerr << "Pass the bundled MSGOTHIC.TTC for portable render checks.\n";
        return 1;
    }
    CHECK(font.text_width(u"iiii", 20) < font.text_width(u"WWWW", 20));
    CHECK(font.text_width(u"iW", 20, true) ==
          font.glyph_advance(u'i', 20, true) + font.glyph_advance(u'W', 20, true));
#ifdef _WIN32
    CHECK(render(root, fullwidth, font) == render(root, "Seolah.", font));
#endif
    // Narrow letters fit one line by measured width, although the old grid
    // measured them as 1000px and wrapped halfway through the word.
    const std::string narrow(100, 'i');
    CHECK(font.text_width(std::u16string(100, u'i'), 20) < 750);
    const auto actual = render(root, narrow, font);
    std::vector<uint8_t> expected(1280u * 720u * 4u, 0);
    font.rasterize_line(std::u16string(100, u'i'), 283, 594, &expected, 1280, 720, 20);
    // Composite onto the same black background as the scene frame.
    for (size_t i = 0; i < expected.size(); i += 4)
        for (int k = 0; k < 3; ++k) expected[i+k] = uint8_t(int(expected[i+k]) * expected[i+3] / 255);
    // Ignore alpha: the scene background is opaque.
    for (size_t i = 0; i < expected.size(); i += 4)
        if (!std::equal(actual.begin()+i, actual.begin()+i+3, expected.begin()+i)) {
            CHECK(false); break;
        }
    // A word that crosses the right edge must move intact to the next line.
    std::string prefix;
    while (font.text_width(std::u16string(prefix.begin(), prefix.end()) + u"dunia ", 20) <= 750)
        prefix += "dunia ";
    CHECK(render(root, prefix + "dunia memutih", font) ==
          render(root, prefix + "\ndunia memutih", font));
    // Different colour runs use the same advances as a continuous run.
    const auto plain = render(root, "iiiWWW", font);
    const auto styled = render(root, "@criii@cwWWW", font);
    for (int y = 550; y < 610; ++y) for (int x = 283 + font.text_width(u"iii", 20); x < 380; ++x) {
        const size_t i = (size_t(y) * 1280 + x) * 4;
        CHECK(plain[i] == styled[i] && plain[i+1] == styled[i+1] && plain[i+2] == styled[i+2]);
    }
    CHECK(render(root, "iiiWWW", font, 3) == render(root, "iii", font));
    dc2dm_rendering(root / "dc2dm", argv[1]);
    std::filesystem::remove_all(root);
    dc4::configure_resources("", "");
    std::cout << (failures ? "Text rendering FAILED\n" : "Text rendering passed\n");
    return failures ? 1 : 0;
}
