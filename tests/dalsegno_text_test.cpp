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
    dc4scene::SceneRunner scene(root.string());
    CHECK(scene.load_ui_script((root / "MES/start.mes").string()));
    start(scene,root,font);
    scene.draw(reveal); return scene.frame();
}
std::vector<std::pair<int,int>> ink_bands(const std::vector<uint8_t>& frame) {
    std::vector<std::pair<int,int>> bands;
    for (int y = 0; y < dc4scene::kScreenH; ++y) {
        for (int x = 390; x < 1190; ++x) {
            if (frame[(size_t(y) * dc4scene::kScreenW + x) * 4] < 100) continue;
            if (!bands.empty() && bands.back().second == y - 1) bands.back().second = y;
            else bands.emplace_back(y, y);
            break;
        }
    }
    return bands;
}
void translated_saves(const fs::path& root) {
    const auto base = root / "AdvData", patch = root / "id_Data";
    const auto scenario = fs::path("MES") / "DS_1129B_CM.mes";
    const auto story = [&](const fs::path& folder, std::string first, std::string who, std::string line) {
        std::vector<uint8_t> code{0x9A,0x9F,0}, table{3,0,0,0};
        const auto text = [&](uint8_t op, const std::string& value) {
            code.push_back(op);
            for (char ch : value) code.push_back(uint8_t((ch == ' ' ? '$' : ch) - 0x20));
            code.push_back(0);
        };
        for (int index = 0; index < 3; ++index) {
            code.insert(code.end(), {3,uint8_t(index),0});
            uint32_t offset = uint32_t(code.size());
            for (int i = 0; i < 4; ++i) table.push_back(uint8_t(offset >> (8*i)));
            if (index == 1) text(0x60,who);
            text(0x61,index == 0 ? first : index == 1 ? line : "Still in English");
            code.insert(code.end(), {6,0,0});
        }
        table.insert(table.end(), {11,0,12,0,13,0});
        table.insert(table.end(),code.begin(),code.end()); table.push_back(0);
        write(folder / scenario,table);
    };
    script(base,"Title");
    story(base,"One","Visitor","Second line");
    story(patch,"Baris pertama","Tamu","Baris kedua");
    const auto save = (root / "slot.dat").string();
    const auto configure = [&](const fs::path& overlay) {
        dc4::configure_resources(base.string(),overlay.string());
    };
    configure({});
    dc4scene::SceneRunner en(base.string());
    CHECK(en.load_script((base / scenario).string()));
    CHECK(en.advance_to_next_text() && en.advance_to_next_text());
    en.set_variable(2,9,37);
    CHECK(en.save_state(save));
    const auto check_load = [&](const std::string& who, const std::string& line, bool resave) {
        dc4scene::SceneRunner scene(base.string());
        CHECK(scene.load_script((base / "MES/main.mes").string()));
        CHECK(scene.load_state(save));
        CHECK(scene.speaker() == who && scene.current_line() == line && scene.variable(2,9) == 37);
        if (resave) CHECK(scene.save_state(save));
        CHECK(scene.advance_to_next_text() && scene.current_line() == "Still in English");
    };
    configure(patch);
    check_load("Tamu","Baris kedua",true);
    // Some PC saves use offsets from the installed translation itself.
    {
        std::ifstream file(save,std::ios::binary);
        std::vector<uint8_t> imported((std::istreambuf_iterator<char>(file)),{});
        dc4::MesFile translated;
        if (imported.size() == 0x11AF8 && translated.load_file((base / scenario).string()) &&
            translated.labels.size() == 3) {
            dc4::MesStream stream(translated);
            stream.seek(translated.labels[1].offset);
            dc4::MesCommand command;
            auto field = [&](size_t at, uint32_t value) {
                for (int i = 0; i < 4; ++i) imported[at+i] = uint8_t(value >> (8*i));
            };
            while (stream.next(&command)) {
                if (command.op == 0x61) field(0x848,command.pos);
                if (command.op == 6) { field(0x83C,stream.pos()); break; }
            }
            configure({});
            std::string error;
            CHECK(en.restore_pc_save(imported,&error));
            CHECK(en.speaker() == "Visitor" && en.current_line() == "Second line");
            CHECK(en.advance_to_next_text() && en.current_line() == "Still in English");
        } else CHECK(false);
    }
    const std::string revision = "Dialog pengganti " + std::string(280,'x');
    story(patch,"Revisi awal yang lebih panjang","Pengunjung",revision);
    configure(patch);
    check_load("Pengunjung",revision,true);
    fs::remove(root / "slot.arisa.dat");
    check_load("Pengunjung",revision,false);
    configure({});
    check_load("Visitor","Second line",false);
    fs::remove(patch / scenario);
    configure(patch);
    check_load("Visitor","Second line",false);
    std::ifstream file(save,std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
    if (bytes.size() == 0x11AF8) {
        std::string error;
        for (size_t field : {0x83C,0x840,0x844,0x848}) {
            auto damaged = bytes;
            std::fill_n(damaged.begin()+field,4,0xFF);
            CHECK(!en.restore_pc_save(damaged,&error));
        }
        bytes.resize(0x1000); CHECK(!en.restore_pc_save(bytes,&error));
    } else CHECK(false);
}
void multiline_dialogue(const fs::path& root, const dc4scene::FontRenderer& font) {
    const std::string word = "gantung";
    const auto one = ink_bands(render(root,word,font));
    const auto two = ink_bands(render(root,word+"\n"+word,font));
    const auto three = ink_bands(render(root,word+"\n"+word+"\n"+word,font));
    CHECK(one.size() == 1 && two.size() == 2 && three.size() == 3);
    if (one.size() != 1 || two.size() != 2 || three.size() != 3) return;
    CHECK(one[0].first >= 632 && one[0].second >= 656 && one[0].second < 668);
    CHECK(two[0] == one[0] && two[1].first == one[0].first + 32);
    CHECK(three[0].first > 602 && three[0].first < one[0].first);
    CHECK(three[2].second < 712);
    for (int i = 0; i < 3; ++i) {
        CHECK(three[i].first == three[0].first + 32 * i);
        CHECK(three[i].second - three[i].first == one[0].second - one[0].first);
    }
    std::string wrapped = word;
    while (font.text_width(std::u16string(wrapped.begin(),wrapped.end()) + u" gantung",22) <= 800)
        wrapped += " " + word;
    const std::string paragraph = wrapped + " " + wrapped + " " + word;
    CHECK(ink_bands(render(root,paragraph,font)) == three);
    const auto partial = ink_bands(render(root,paragraph,font,int(word.size())));
    CHECK(partial.size() == 1 && partial[0] == three[0]);
    const auto four = ink_bands(render(root,paragraph+"\n"+word,font));
    CHECK(four.size() == 4 && four.front().first > 602 && four.back().second < 712);
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
    dc4scene::Image note;
    note.w = 14; note.h = 22; note.rgba.assign(14u*22u*4u, 0);
    const size_t note_pixel = (10u * note.w + 6) * 4;
    note.rgba[note_pixel] = note.rgba[note_pixel+3] = 255;
    crx(root / "GRP/OnP.cRx", note);
    const std::string raw = "The island#h#b#h#o\n#q #H #";
    script(root,raw);
    dc4scene::SceneRunner scene(root.string()); start(scene,root,font);
    const std::u16string visible = u"The island\u2014\u2014\u2014\u266a\n#q #H #";
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
    CHECK(render(root,"#oW",font,1) == render(root,"#o",font));
    CHECK(render(root,"#o",font) != render(root,"#o",font,0));
    const auto other = root / "other";
    script(other,"#h#b",false);
    dc4scene::SceneRunner modern(other.string()); start(modern,other,font);
    CHECK(modern.visible_text() == u"#h#b" && modern.visible_text_length() == 4);

    dc4scene::Image background; background.w = 1280; background.h = 720;
    background.rgba.assign(1280u*720u*4u,0);
    crx(root / "GRP/SYSTEM/LOGBACK.CRX",background);
    dc4scene::Image chips;
    chips.w = 248; chips.h = 140; chips.rgba.assign(248u*140u*4u, 0);
    const size_t back_pixel = (103u * chips.w + 4) * 4;
    chips.rgba[back_pixel] = chips.rgba[back_pixel+3] = 255;
    chips.rgba[back_pixel+48*4+1] = chips.rgba[back_pixel+48*4+3] = 255;
    crx(root / "GRP/SYSTEM/LOGCHIP.CRX", chips);
    dc4ui::LogScreen log; CHECK(log.load(root.string()));
    CHECK(log.back_hit(1260, 695) && !log.back_hit(1210, 654));
    dc4ui::LogRow row; row.text = u"\u2014\u266aW";
    std::vector<uint8_t> actual(1280u*720u*4u,0), expected = actual;
    const size_t back_position = (679u * 1280 + 1238) * 4;
    log.draw({row},0,false,-1,-1,font,&actual);
    const auto image = bar();
    font.rasterize_line(row.text,310,109,&expected,1280,720,22,0xFFFFFF,false,0,false,0,0,1,true,&image,&note);
    expected[back_position] = expected[back_position+3] = 255;
    for (size_t at = 0; at < actual.size(); at += 4) for (int c = 0; c < 3; ++c)
        CHECK(actual[at+c] == int(expected[at+c])*expected[at+3]/255);
    actual.assign(actual.size(),0);
    log.draw({row},0,false,1260,695,font,&actual);
    CHECK(actual[back_position] == 0 && actual[back_position+1] == 255);
    row.text = std::u16string(40,dc4scene::kInlineDash);
    actual.assign(actual.size(),0);
    log.draw({row},0,false,-1,-1,font,&actual);
    expected.assign(expected.size(),0);
    font.rasterize_line(std::u16string(38,dc4scene::kInlineDash),310,109,&expected,1280,720,22,0xFFFFFF,false,0,false,0,0,1,true,&image);
    font.rasterize_line(u"\u2014\u2014",310,137,&expected,1280,720,22,0xFFFFFF,false,0,false,0,0,1,true,&image);
    expected[back_position] = expected[back_position+3] = 255;
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
    bitmap(font); dialogue_and_log(root,font); multiline_dialogue(root,font);
    translated_saves(root / "translation");
    dc4::configure_resources("",""); fs::remove_all(root);
    std::cout << (failures ? "Dal Segno text FAILED\n" : "Dal Segno text passed\n");
    return failures ? 1 : 0;
}
