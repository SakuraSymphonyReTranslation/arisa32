#include "arisa32/log_screen.h"
#include "arisa32/resource.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
namespace {
int failures = 0;
#define CHECK(v) do { if (!(v)) { std::cerr << __LINE__ << ": " #v "\n"; ++failures; } } while (0)
void write(const fs::path& path, const std::vector<uint8_t>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    CHECK(bool(out));
}
void crx(const fs::path& path, const dc4scene::Image& image) {
    std::vector<uint8_t> bytes{'C','R','X','G',0,0,0,0};
    for (int word : {image.w, image.h, 1, 0, 1, 2}) {
        bytes.push_back(uint8_t(word)); bytes.push_back(uint8_t(word >> 8));
    }
    std::vector<uint8_t> pixels;
    for (size_t at = 0; at < image.rgba.size(); at += 4)
        for (int c : {3,2,1,0}) pixels.push_back(image.rgba[at+c]);
    for (size_t at = 0; at < pixels.size(); at += 8) {
        bytes.push_back(255);
        bytes.insert(bytes.end(), pixels.begin()+at, pixels.begin()+std::min(at+8,pixels.size()));
    }
    write(path, bytes);
}
dc4scene::Image bar() {
    dc4scene::Image image;
    image.w = 19; image.h = 22; image.rgba.assign(19u*22u*4u, 0);
    for (int y = 10; y <= 12; ++y) for (int x = 0; x < 19; ++x) {
        const size_t at = (size_t(y)*19+x)*4;
        const uint8_t color = y == 11 && x >= 2 && x <= 16 ? 255 : 0;
        image.rgba[at] = image.rgba[at+1] = image.rgba[at+2] = color;
        image.rgba[at+3] = 255;
    }
    const size_t edge = (9u*19u+1u)*4u;
    image.rgba[edge] = 200; image.rgba[edge+1] = 100;
    image.rgba[edge+2] = 50; image.rgba[edge+3] = 128;
    return image;
}
void script(const fs::path& root, const std::string& text, bool dalsegno = true) {
    std::vector<uint8_t> bytes = dalsegno ? std::vector<uint8_t>{0,0,0,0,0x9A,0x9F,0,0x61} :
        std::vector<uint8_t>{1,0,0,0,3,0,0,0,0,0,0xB6,0xAA,0,0x6B};
    for (unsigned char c : text)
        bytes.push_back(uint8_t((dalsegno && c == ' ' ? '$' : c)-0x20));
    bytes.insert(bytes.end(), {0,0});
    write(root / "MES/main.mes", bytes);
    if (dalsegno) {
        write(root / "MES/DS_1129B_CM.mes", {0,0,0,0,0x9A,0x9F,0,0});
        write(root / "MES/start.mes", {0,0,0,0,0x9A,0x9F,0,0});
    }
}
void start(dc4scene::SceneRunner& scene, const fs::path& root, const dc4scene::FontRenderer& font) {
    scene.set_font(&font);
    CHECK(scene.load_script((root / "MES/main.mes").string()));
    CHECK(scene.advance_to_next_text());
}
std::vector<uint8_t> render(const fs::path& root, const std::string& text,
                           const dc4scene::FontRenderer& font, int reveal = -1) {
    script(root, text);
    dc4scene::SceneRunner scene(root.string()); start(scene,root,font);
    scene.draw(reveal); return scene.frame();
}
void bitmap(const dc4scene::FontRenderer& font) {
    const auto image = bar();
    CHECK(font.glyph_advance(dc4scene::kInlineDash,32,true,true,7,&image) == 19);
    CHECK(font.text_width(u"\u2014\u2014W",22,false,false,0,&image) == 38+font.text_width(u"W",22));
    std::vector<uint8_t> actual(160u*64u*4u,0), expected = actual;
    font.rasterize_line(u"\u2014",10,32,&actual,160,64,22,0xFF0000,true,0x00FF00,true,7,0,1,true,&image);
    for (int y = 0; y < image.h; ++y) for (int x = 0; x < image.w; ++x) {
        const size_t src = (size_t(y)*image.w+x)*4, dst = (size_t(y+14)*160+x+10)*4;
        std::copy_n(image.rgba.begin()+src,4,expected.begin()+dst);
    }
    CHECK(actual == expected);
    font.rasterize_line(u"W",29,32,&expected,160,64,22);
    actual.assign(actual.size(),0);
    font.rasterize_line(u"\u2014W",10,32,&actual,160,64,22,0xFFFFFF,false,0,false,0,0,1,true,&image);
    CHECK(actual == expected);
    actual.assign(actual.size(),0); expected.assign(expected.size(),0);
    font.rasterize_line(u"\u2014\u2014",10,32,&actual,160,64,22,0xFFFFFF,false,0,false,0,0,1,true,&image);
    for (int x : {10,29}) font.rasterize_line(u"\u2014",x,32,&expected,160,64,22,0xFFFFFF,false,0,false,0,0,1,true,&image);
    CHECK(actual == expected);
    actual.assign(actual.size(),0); expected.assign(expected.size(),0);
    dc4scene::Image missing;
    font.rasterize_line(u"\u2014",10,32,&actual,160,64,22);
    font.rasterize_line(u"\u2014",10,32,&expected,160,64,22,0xFFFFFF,false,0,false,0,0,1,true,&missing);
    CHECK(actual == expected);
    std::vector<uint8_t> clipped(64u*32u*4u+16u,0);
    std::fill(clipped.end()-16,clipped.end(),123);
    font.rasterize_line(u"\u2014\u2014\u2014",-2,16,&clipped,64,32,22,0xFFFFFF,false,0,false,0,0,2,true,&image);
    CHECK(std::any_of(clipped.begin(),clipped.end()-16,[](uint8_t c){return c!=0;}));
    CHECK(std::all_of(clipped.end()-16,clipped.end(),[](uint8_t c){return c==123;}));
}
void dialogue_and_log(const fs::path& root, const dc4scene::FontRenderer& font) {
    crx(root / "GRP/BaR.cRx",bar());
    const std::string raw = "The island#h#b#h\n#q #H #";
    script(root,raw);
    dc4scene::SceneRunner scene(root.string()); start(scene,root,font);
    const std::u16string visible = u"The island\u2014\u2014\u2014\n#q #H #";
    CHECK(scene.current_line() == raw);
    CHECK(scene.visible_text() == visible);
    CHECK(scene.visible_text_length() == visible.size()-1);
    CHECK(dc4ui::capture_log_row(scene,0).text == visible);
    std::vector<uint8_t> state;
    CHECK(scene.capture_state(&state));
    dc4scene::SceneRunner restored(root.string()); restored.set_font(&font);
    CHECK(restored.restore_state(state));
    CHECK(restored.current_line() == raw && restored.visible_text() == visible);
    CHECK(render(root,"#h#bW",font,1) == render(root,"#h",font));
    CHECK(render(root,"#h#bW",font,2) == render(root,"#h#b",font));
    CHECK(render(root,"@cr#h@cwW",font) == render(root,"#hW",font));
    CHECK(render(root,"#hW",font) == render(root,"#bW",font));
    const auto other = root / "other";
    script(other,"#h#b",false);
    dc4scene::SceneRunner modern(other.string()); start(modern,other,font);
    CHECK(modern.visible_text() == u"#h#b" && modern.visible_text_length() == 4);

    dc4scene::Image background; background.w = 1280; background.h = 720;
    background.rgba.assign(1280u*720u*4u,0);
    crx(root / "GRP/SYSTEM/LOGBACK.CRX",background);
    background.w = 256; background.h = 160; background.rgba.assign(256u*160u*4u,0);
    crx(root / "GRP/SYSTEM/LOGCHIP.CRX",background);
    dc4ui::LogScreen log; CHECK(log.load(root.string()));
    dc4ui::LogRow row; row.text = u"\u2014\u2014W";
    std::vector<uint8_t> actual(1280u*720u*4u,0), expected = actual;
    log.draw({row},0,false,-1,-1,font,&actual);
    const auto image = bar();
    font.rasterize_line(row.text,310,109,&expected,1280,720,22,0xFFFFFF,false,0,false,0,0,1,true,&image);
    for (size_t at = 0; at < actual.size(); at += 4) for (int c = 0; c < 3; ++c)
        CHECK(actual[at+c] == int(expected[at+c])*expected[at+3]/255);
    row.text = std::u16string(40,dc4scene::kInlineDash);
    actual.assign(actual.size(),0);
    log.draw({row},0,false,-1,-1,font,&actual);
    expected.assign(expected.size(),0);
    font.rasterize_line(std::u16string(38,dc4scene::kInlineDash),310,109,&expected,1280,720,22,0xFFFFFF,false,0,false,0,0,1,true,&image);
    font.rasterize_line(u"\u2014\u2014",310,137,&expected,1280,720,22,0xFFFFFF,false,0,false,0,0,1,true,&image);
    for (size_t at = 0; at < actual.size(); at += 4) for (int c = 0; c < 3; ++c)
        CHECK(actual[at+c] == int(expected[at+c])*expected[at+3]/255);
}
}
int main(int argc, char** argv) {
    const auto root = fs::temp_directory_path() / ("arisa32-dalsegno-text-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    dc4::configure_resources(root.string(),"","windows-31j"); dc4::set_language(dc4::Language::English);
    dc4scene::FontRenderer font;
    if (argc < 2 || !font.load(argv[1],0,22,false)) return 1;
    bitmap(font); dialogue_and_log(root,font);
    dc4::configure_resources("",""); fs::remove_all(root);
    std::cout << (failures ? "Dal Segno text FAILED\n" : "Dal Segno text passed\n");
    return failures ? 1 : 0;
}
