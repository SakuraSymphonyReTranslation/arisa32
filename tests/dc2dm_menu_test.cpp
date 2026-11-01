#include "arisa32/system_menu.h"
#include "arisa32/resource.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
int failures = 0;
void check(bool value, int line) {
    if (!value) { std::cerr << "line " << line << '\n'; ++failures; }
}
#define CHECK(value) check(bool(value), __LINE__)
using Menu = dc4ui::SystemMenu;
using Page = dc4ui::SystemMenuPage;
using Type = dc4ui::SystemMenuActionType;
void write_bytes(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));
    CHECK(bool(out));
}
struct MenuScript {
    std::vector<uint8_t> bytes{1,0,0,0,3,0,0,0,0,0,0x72,0x9D,0};
    explicit MenuScript(bool dm) { if (!dm) { bytes[10]=0xB6; bytes[11]=0xAA; } }
    void words(uint8_t opcode, std::initializer_list<uint16_t> values) {
        bytes.push_back(opcode);
        for (auto value : values) { bytes.push_back(uint8_t(value)); bytes.push_back(uint8_t(value>>8)); }
    }
    void string(uint8_t opcode, const std::string& value) {
        bytes.push_back(opcode); bytes.insert(bytes.end(),value.begin(),value.end()); bytes.push_back(0);
    }
    void text(uint8_t opcode, const std::string& value) {
        bytes.push_back(opcode);
        for (unsigned char ch : value) bytes.push_back(uint8_t(ch-0x20));
        bytes.push_back(0);
    }
};
void preview_fixture(const std::filesystem::path& root, const std::string& text,
                     bool animated = true, bool dm = true, bool backdrop = false) {
    MenuScript main(dm);
    main.string(dm ? 0x34 : 0x4B,dm ? "start_main" : "main"); main.bytes.push_back(0);
    write_bytes(root/"MES/main.mes",main.bytes);
    MenuScript option(dm);
    option.string(dm ? 0x34 : 0x4B,"menu1");
    option.bytes.push_back(dm ? 0x2B : 0x3C); option.bytes.push_back(12);
    const std::string sheet="system/preview";
    option.bytes.insert(option.bytes.end(),sheet.begin(),sheet.end()); option.bytes.push_back(0);
    if (backdrop) {
        option.words(dm ? 0x5A : 0x7A,{20,20,0,0});
        option.words(dm ? 0x57 : 0x77,{8,8,140,1});
    }
    option.words(dm ? 0x67 : 0x87,{370,424,29,135});
    option.text(dm ? 0x50 : 0x69,text);
    if (animated && dm) option.words(0x5B,{0x110C,1,0,9999});
    if (dm) {
        option.words(0x54,{100,100,201,121});
        option.string(0x3D,"speed");
        option.words(0x55,{100,200,12,99});
        option.words(0x57,{26,0,12,5});
    }
    option.bytes.push_back(0);
    write_bytes(root/(dm ? "MES/option.mes" : "MES/config.mes"),option.bytes);
    std::vector<uint8_t> names(244,0); names[0]=7;
    write_bytes(root/"GRP/NAMES.DAT",names);
    std::vector<uint8_t> white{'C','R','X','G',0,0,0,0,8,0,8,0,1,0,0,0,0,0,0,0};
    for (int i=0; i<8*8*3; i+=8) { white.push_back(255); white.insert(white.end(),8,255); }
    write_bytes(root/"GRP/system/preview.crx",white);
    if (!dm) {
        MenuScript music(false); music.bytes.push_back(0);
        write_bytes(root/"MES/music.mes",music.bytes);
        for (const char* name : {"titleback02","titlechip01","configback01","configchip01","exchip"})
            write_bytes(root/(std::string("GRP/system/")+name+".crx"),white);
    }
}
std::vector<uint8_t> frame(Menu& menu, const dc4scene::FontRenderer& font) {
    std::vector<uint8_t> pixels;
    menu.draw(-1,-1,false,font,&pixels);
    return pixels;
}
std::vector<uint8_t> expected_preview(const dc4scene::FontRenderer& font,
                                    const std::u16string& text, bool translated = true) {
    std::vector<uint8_t> native(1024u*576u*4u,0), expected(1280u*720u*4u,0);
    size_t start=0;
    int baseline=442;
    while (start<text.size()) {
        const auto end=text.find(u'\n',start);
        font.rasterize_line(text.substr(start,end-start),370,baseline,&native,1024,576,
                            22,0xFFFFFF,false,0,true,-1,translated ? 22*15.0f/19 : 0);
        if (end==std::u16string::npos) break;
        start=end+1; baseline+=29;
    }
    for (int y=0; y<720; ++y) for (int x=0; x<1280; ++x) {
        const size_t src=(size_t(y*576/720)*1024+x*1024/1280)*4;
        const size_t dst=(size_t(y)*1280+x)*4;
        for (int channel=0; channel<3; ++channel)
            expected[dst+channel]=uint8_t(int(native[src+channel])*native[src+3]/255);
        expected[dst+3]=255;
    }
    return expected;
}
void preview_regressions(const std::filesystem::path& root, const char* font_path) {
    preview_fixture(root/"opacity","AB_CD\nEF.",true,true,true);
    Menu menu;
    std::string error;
    CHECK(menu.load((root/"opacity").string(),&error)); CHECK(error.empty());
    menu.open(Page::Config);
    dc4scene::FontRenderer font;
    menu.set_variable(99,11,0);
    const auto opaque=frame(menu,font);
    CHECK(menu.config().window_opacity==100);
    menu.set_variable(99,11,8);
    const auto half=frame(menu,font);
    CHECK(menu.config().window_opacity==50);
    menu.set_variable(99,11,14);
    const auto transparent=frame(menu,font);
    const size_t sample=(26u*1280u+26u)*4;
    CHECK(transparent[sample]==31); CHECK(half[sample]==127); CHECK(opaque[sample]==255);
    CHECK(menu.config().window_opacity==12);
    if (!font_path || !*font_path) {
        std::cout << "DC2DM_MENU_FONT unset; preview font checks skipped\n";
        return;
    }
    CHECK(font.load(font_path,0,22,false));
    preview_fixture(root/"dm","AB_CD\nEF.");
    CHECK(menu.load((root/"dm").string(),&error)); CHECK(error.empty());
    menu.open(Page::Config);
    menu.set_variable(99,11,0); menu.set_variable(99,29,20); menu.set_variable(99,26,11);
    const auto first=expected_preview(font,u"A"), full=expected_preview(font,u"AB CD\nEF.");
    CHECK(frame(menu,font)==first);
    menu.update(29); CHECK(frame(menu,font)==first);
    menu.update(1); CHECK(frame(menu,font)==full);
    menu.update(199); CHECK(frame(menu,font)==full);
    menu.update(1); CHECK(frame(menu,font)==first);
    menu.set_variable(99,26,0);
    menu.update(200); CHECK(frame(menu,font)==expected_preview(font,u"AB"));
    menu.update(480); CHECK(frame(menu,font)==expected_preview(font,u"AB CD\nE"));
    menu.update(272); CHECK(frame(menu,font)==full);
    menu.open(Page::Config); CHECK(frame(menu,font)==first);
    menu.set_variable(99,26,11); menu.set_variable(99,29,0);
    menu.update(10229); CHECK(frame(menu,font)==full);
    menu.update(1); CHECK(frame(menu,font)==first);
    for (int speed=8; speed<=10; ++speed) {
        menu.set_variable(99,29,20);
        menu.set_variable(99,26,speed);
        const size_t batch=size_t(speed-7);
        CHECK(frame(menu,font)==expected_preview(font,std::u16string(u"AB ").substr(0,batch)));
        menu.update(uint32_t(((8+batch-1)/batch-1)*30)); CHECK(frame(menu,font)==full);
        const size_t overshoot=(8+batch-1)/batch*batch-8;
        const uint32_t hold=uint32_t(std::max<size_t>(1,2-overshoot)*100);
        menu.update(hold-1); CHECK(frame(menu,font)==full);
        menu.update(1);
        CHECK(frame(menu,font)==expected_preview(font,std::u16string(u"AB ").substr(0,batch)));
    }
    menu.set_variable(99,26,8); menu.update(90);
    const auto partial=expected_preview(font,u"AB C");
    CHECK(frame(menu,font)==partial);
    menu.pointer_down(125,137); menu.pointer_up(125,137);
    CHECK(menu.variable(99,26)==0); CHECK(frame(menu,font)==partial);
    menu.update(135); CHECK(frame(menu,font)==partial);
    menu.update(1); CHECK(frame(menu,font)==expected_preview(font,u"AB CD"));
    preview_fixture(root/"static","AB_CD\nEF.",false);
    Menu fixed; CHECK(fixed.load((root/"static").string(),&error)); fixed.open(Page::Config);
    fixed.set_variable(99,11,0);
    CHECK(frame(fixed,font)==full); fixed.update(100000); CHECK(frame(fixed,font)==full);
    preview_fixture(root/"japanese","\x83\x65\x83\x58\x83\x67\x81\x5B\x81\x42_A",false);
    Menu japanese; CHECK(japanese.load((root/"japanese").string(),&error)); japanese.open(Page::Config);
    japanese.set_variable(99,11,0);
    CHECK(frame(japanese,font)==expected_preview(font,u"\u30c6\u30b9\u30c8\u30fc\u3002_A",false));
    preview_fixture(root/"modern","AB_CD\nEF.",true,false,true);
    Menu modern; CHECK(modern.load((root/"modern").string(),&error)); modern.open(Page::Config);
    const auto modern_text=frame(modern,font);
    std::vector<uint8_t> generic(1280u*720u*4u,0);
    for (size_t i=3; i<generic.size(); i+=4) generic[i]=255;
    font.rasterize_line(u"AB_CD",370,453,&generic,1280,720,29);
    font.rasterize_line(u"EF.",370,486,&generic,1280,720,29);
    CHECK(modern_text==generic);
    modern.update(100000); CHECK(frame(modern,font)==modern_text);
    modern.set_variable(99,11,0); CHECK(frame(modern,font)==modern_text);
}
bool has(Menu& menu, const std::string& tag) {
    const auto controls = menu.controls();
    return std::any_of(controls.begin(), controls.end(), [&](const auto& control) {
        return control.tag == tag && control.enabled;
    });
}
dc4ui::SystemMenuAction click(Menu& menu, const std::string& tag) {
    for (const auto& control : menu.controls()) {
        if (control.tag != tag || !control.enabled) continue;
        const int x = control.x + control.w/2, y = control.y + control.h/2;
        menu.pointer_down(x,y);
        return menu.pointer_up(x,y);
    }
    std::cerr << "missing control: " << tag << '\n';
    ++failures;
    return {};
}
void capture(Menu& menu, const std::string& name) {
    static dc4scene::FontRenderer font;
    static bool font_checked = false;
    if (!font_checked) {
        font_checked = true;
        if (const char* path = std::getenv("DC2DM_MENU_FONT")) CHECK(font.load(path,0,20,false));
    }
    std::vector<uint8_t> frame;
    menu.draw(-1,-1,false,font,&frame);
    CHECK(frame.size() == 1280u*720u*4u);
    const auto colored = [&] (int x, int y) {
        const size_t at = (size_t(y)*1280+x)*4;
        return frame[at] || frame[at+1] || frame[at+2];
    };
    CHECK(colored(100,100)); CHECK(colored(1180,620));
    if (const char* output = std::getenv("DC2DM_MENU_CAPTURES")) {
        std::filesystem::create_directories(output);
        std::ofstream out(std::filesystem::path(output)/(name+".ppm"),std::ios::binary);
        out << "P6\n1280 720\n255\n";
        for (size_t at=0; at<frame.size(); at+=4)
            out.write(reinterpret_cast<const char*>(frame.data()+at),3);
    }
}
}

int main(int argc, char** argv) {
    const auto fixtures=std::filesystem::temp_directory_path()/
        ("dc2dm-menu-fixtures-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    dc4::configure_resources("","","windows-31j");
    preview_regressions(fixtures,argc>1 ? argv[1] : std::getenv("DC2DM_MENU_FONT"));
    std::filesystem::remove_all(fixtures);
    const char* data = std::getenv("DC2DM_DATA");
    if (!data || !*data) { std::cout << "DC2DM_DATA unset; real-data checks skipped\n"; return failures ? 1 : 0; }
    Menu menu;
    std::string error;
    CHECK(menu.load(data,&error));
    if (!error.empty()) { std::cerr << error << '\n'; return 1; }
    CHECK(menu.page() == Page::Title);
    CHECK(has(menu,"sce_start")); CHECK(has(menu,"load")); CHECK(has(menu,"extra"));
    CHECK(has(menu,"config")); CHECK(has(menu,"end")); CHECK(!has(menu,"qload"));
    for (const auto& control : menu.controls()) {
        if (control.tag != "sce_start") continue;
        CHECK(control.x == 550 && control.y == 270 && control.w == 180 && control.h == 60);
    }
    capture(menu,"title");
    CHECK(click(menu,"load").type == Type::Load);
    auto start = click(menu,"sce_start");
    if (menu.confirming()) start = menu.answer_confirmation(true);
    CHECK(start.type == Type::NewGame && start.asset == "SCESELECT");
    menu.set_save_available(true,true);
    CHECK(has(menu,"qload"));
    auto latest = click(menu,"qload");
    if (menu.confirming()) latest = menu.answer_confirmation(true);
    CHECK(latest.type == Type::LatestLoad);
    click(menu,"config"); CHECK(menu.page() == Page::Config); CHECK(menu.error().empty());
    capture(menu,"config-sound");
    const int music = menu.variable(99,7);
    menu.activate("*music_vol--");
    CHECK(menu.variable(99,7) == music-1);
    CHECK(menu.config().music_volume == (music-1)*100/29);
    menu.activate("*music_vol++"); CHECK(menu.variable(99,7) == music);
    menu.pointer_down(625,225); menu.pointer_up(625,225);
    CHECK(menu.variable(99,7) == 14);
    menu.set_variable(99,7,music);
    click(menu,"voice"); CHECK(!has(menu,"v00")); CHECK(has(menu,"v01"));
    capture(menu,"config-voice");
    click(menu,"v01"); CHECK(menu.variable(3,0) == 1); CHECK(has(menu,"v00"));
    click(menu,"v00");
    CHECK(menu.variable(3,0) == 0);
    click(menu,"char_voice_set");
    CHECK(menu.variable(95,0) == 0 && menu.config().character_muted[0]);
    click(menu,"char_voice_set");
    CHECK(menu.variable(95,0) == 1 && !menu.config().character_muted[0]);
    click(menu,"vret"); CHECK(has(menu,"voice"));
    menu.activate("text"); CHECK(menu.variable(3,17) == 3); CHECK(menu.error().empty());
    capture(menu,"config-text-start");
    menu.update(1000);
    capture(menu,"config-text-mid");
    menu.update(1500);
    capture(menu,"config-text");
    const int text = menu.config().text_speed;
    menu.activate("mspeed--"); CHECK(menu.config().text_speed == text-1);
    menu.activate("system"); CHECK(menu.variable(3,17) == 4); CHECK(menu.error().empty());
    capture(menu,"config-system");
    menu.back(); CHECK(menu.page() == Page::Title);
    click(menu,"extra"); CHECK(menu.page() == Page::Extras); CHECK(menu.error().empty());
    CHECK(has(menu,"cgmode")); CHECK(has(menu,"scemode")); CHECK(has(menu,"musmode"));
    capture(menu,"extras");
    click(menu,"cgmode"); CHECK(menu.page() == Page::Gallery); CHECK(menu.error().empty());
    CHECK(menu.variable(3,2) == 1);
    CHECK(!has(menu,"cg0"));
    menu.set_variable(97,0,1); CHECK(has(menu,"cg0"));
    capture(menu,"gallery");
    click(menu,"cg0"); capture(menu,"gallery-view");
    menu.back(); CHECK(menu.page() == Page::Gallery);
    click(menu,"page++"); CHECK(menu.variable(3,6) == 1);
    click(menu,"page--"); CHECK(menu.variable(3,6) == 0);
    click(menu,"sin"); CHECK(menu.variable(3,5) == 1); CHECK(menu.error().empty());
    menu.back(); CHECK(menu.page() == Page::Extras);
    click(menu,"scemode"); CHECK(menu.page() == Page::Scenes); CHECK(menu.error().empty());
    CHECK(menu.variable(3,2) == 3);
    CHECK(!has(menu,"sce0"));
    menu.set_variable(98,0,1); CHECK(has(menu,"sce0"));
    capture(menu,"scenes");
    auto replay = click(menu,"sce0");
    if (menu.confirming()) replay = menu.answer_confirmation(true);
    CHECK(replay.type == Type::StartScene && replay.asset == "MAR_0101_F1");
    dc4scene::SceneRunner replay_scene(data);
    CHECK(replay_scene.load_script(std::string(data)+"/MES/"+replay.asset+".mes",replay.label));
    menu.initialize_scene(replay_scene,true);
    CHECK(replay_scene.variable(9,7) == 1 && !replay_scene.save_load_allowed());
    menu.initialize_scene(replay_scene,false);
    CHECK(replay_scene.variable(9,7) == 0 && replay_scene.save_load_allowed());
    menu.resume_scenes(); CHECK(menu.page() == Page::Scenes); CHECK(has(menu,"sce0"));
    click(menu,"sin"); CHECK(menu.variable(3,5) == 1 && menu.variable(3,2) == 7);
    capture(menu,"scenes-married-life");
    click(menu,"suc"); CHECK(menu.variable(3,5) == 2 && menu.variable(3,2) == 5);
    capture(menu,"scenes-dear-to");
    menu.back(); CHECK(menu.page() == Page::Extras);
    menu.set_variable(99,7,0); menu.set_variable(99,14,0);
    click(menu,"musmode"); CHECK(menu.page() == Page::Music); CHECK(menu.error().empty());
    CHECK(!has(menu,"#music:0"));
    menu.mark_music_seen("music/001"); menu.mark_music_seen("music/002");
    menu.open(Page::Music); CHECK(has(menu,"#music:0")); CHECK(has(menu,"#music:1"));
    CHECK(click(menu,"#music:0").asset == "music\\001");
    CHECK(click(menu,"next").asset == "music\\002");
    CHECK(click(menu,"back").asset == "music\\001");
    CHECK(click(menu,"stop").type == Type::StopMusic);
    click(menu,"#dm:loop2"); CHECK(menu.variable(100,26) == 2);
    capture(menu,"music");
    menu.mark_music_seen("music/112"); menu.mark_music_seen("music/117");
    menu.mark_music_seen("music/118");
    menu.scroll(1000); CHECK(!has(menu,"#music:0"));
    CHECK(click(menu,"#music:67").asset == "music\\112");
    CHECK(click(menu,"#music:72").asset == "music\\117");
    CHECK(!has(menu,"#music:73"));
    menu.scroll(-1000); CHECK(has(menu,"#music:0"));
    click(menu,"return"); CHECK(menu.page() == Page::Extras);
    CHECK(menu.variable(99,7) == 0 && menu.variable(99,14) == 0);
    menu.back(); CHECK(menu.page() == Page::Title);
    click(menu,"end");
    if (menu.confirming()) CHECK(menu.answer_confirmation(false).type == Type::None);
    CHECK(menu.page() == Page::Title);
    CHECK(menu.title_music().find("dc4") == std::string::npos);
    CHECK(menu.system_voice(14) == "VOICE/DM_SYS/DM_SYS13A");
    CHECK(menu.scene_ui_script() == "start.mes");
    CHECK(menu.error().empty());
    const auto temporary = std::filesystem::temp_directory_path()/
        ("dc2dm-menu-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    {
        std::ofstream native(temporary,std::ios::binary);
        const std::string bytes(0x4B80,'\1');
        native.write(bytes.data(),bytes.size());
    }
    const int progress = menu.variable(100,1);
    CHECK(!menu.load_system_dat(temporary.string(),&error));
    CHECK(error.find("Native DC2DM system.dat import is not supported") != std::string::npos);
    CHECK(menu.variable(100,1) == progress);
    menu.set_variable(100,1,1);
    CHECK(menu.save_profile(temporary.string(),&error));
    menu.set_variable(100,1,0);
    CHECK(menu.load_profile(temporary.string(),&error));
    CHECK(menu.variable(100,1) == 1 && menu.music_seen("music/001"));
    std::filesystem::remove(temporary);
    return failures ? 1 : 0;
}
