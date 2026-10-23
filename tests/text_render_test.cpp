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
    std::filesystem::remove_all(root);
    dc4::configure_resources("", "");
    std::cout << (failures ? "Text rendering FAILED\n" : "Text rendering passed\n");
    return failures ? 1 : 0;
}
