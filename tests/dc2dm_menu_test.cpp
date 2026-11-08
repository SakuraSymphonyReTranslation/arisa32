#include "arisa32/system_menu.h"
#include "arisa32/resource.h"
#include "arisa32/crx.h"
#include "arisa32/names.h"
#include "arisa32/scene.h"
#include "arisa32/dc2pc_menu_transition.h"
#include "arisa32/dc2dm_title_particles.h"
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
    CHECK(!modern.is_dc2dm());
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
    CHECK(menu.is_dc2dm());
    CHECK(menu.page_voice_event()==2);
    CHECK(menu.page_voice_event(Page::Extras)==0);
    const std::pair<int,int> voice_events[] = {{2,2},{7,3},{8,7},{10,8},{13,7},
        {14,13},{15,10},{16,11},{17,9},{18,16},{19,12},{20,20},{21,21},{22,14},{23,15}};
    for (int style=0; style<4; ++style) {
        menu.set_variable(100,56,style);
        for (const auto& event : voice_events) {
            char asset[64];
            std::snprintf(asset,sizeof(asset),"VOICE/DM_SYS/DM_SYS%02d%c",event.second,'A'+style);
            CHECK(menu.system_voice(event.first) == (style==3 ? "" : asset));
        }
        CHECK(menu.system_voice(11).empty());
        CHECK(menu.system_voice(12).empty());
        CHECK(menu.system_voice(99).empty());
    }
    menu.set_variable(100,56,0);
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
    for (int style : {1,2,3,0}) {
        const auto selected=click(menu,"sys_v0"+std::to_string(style));
        CHECK(menu.variable(100,56)==style);
        if (style==3) CHECK(selected.type!=Type::PlayVoice);
        else {
            CHECK(selected.type==Type::PlayVoice);
            CHECK(selected.asset==std::string("VOICE\\DM_SYS\\DM_SYS16")+char('A'+style));
        }
        CHECK(!has(menu,"sys_v0"+std::to_string(style)));
    }
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
    CHECK(menu.page_voice_event(Page::Title)==10);
    CHECK(menu.page_voice_event(Page::Gallery)==0);
    CHECK(menu.page_voice_event(Page::Config)==0);
    CHECK(has(menu,"cgmode")); CHECK(has(menu,"scemode")); CHECK(has(menu,"musmode"));
    capture(menu,"extras");
    menu.set_variable(100,20,1);
    const auto minigame=click(menu,"minigame");
    CHECK(minigame.type==Type::NewGame && minigame.voice_event==19);
    menu.open(Page::Extras);
    click(menu,"cgmode"); CHECK(menu.page() == Page::Gallery); CHECK(menu.error().empty());
    CHECK(menu.page_voice_event(Page::Extras)==15);
    CHECK(menu.page_voice_event(Page::Config)==0);
    CHECK(menu.variable(3,2) == 1);
    CHECK(!has(menu,"cg0"));
    menu.set_variable(97,0,1); CHECK(has(menu,"cg0"));
    capture(menu,"gallery");
    menu.set_variable(97,1,1); menu.set_variable(97,10,1); menu.set_variable(97,145,1);
    menu.set_variable(97,146,1);
    dc4scene::FontRenderer gallery_font;
    const auto gallery_menu=frame(menu,gallery_font);
    click(menu,"cg0");
    CHECK(menu.gallery_preview_active());
    CHECK(frame(menu,gallery_font)==gallery_menu);
    menu.update(160);
    const auto entering=frame(menu,gallery_font);
    CHECK(entering!=gallery_menu);
    CHECK(frame(menu,gallery_font)==entering);
    menu.update(160);
    const auto first_cg=frame(menu,gallery_font);
    CHECK(first_cg!=entering);
    capture(menu,"gallery-view");
    std::vector<uint8_t> native_cg, high_cg;
    CHECK(menu.draw_gallery(1024,576,&native_cg));
    CHECK(menu.draw_gallery(1920,1080,&high_cg));
    CHECK(high_cg.size()==1920u*1080u*4u);
    CHECK(!menu.draw_gallery(1920,1200,&high_cg));
    std::ifstream names_file(std::filesystem::path(data)/"GRP/NAMES.DAT",std::ios::binary);
    std::vector<uint8_t> names_bytes(std::istreambuf_iterator<char>(names_file),{});
    dc4::GameNames names; CHECK(dc4::decode_game_names(names_bytes,&names));
    std::ifstream cg_file(std::filesystem::path(data)/"GRP"/(names.cg[0]+".CRX"),std::ios::binary);
    std::vector<uint8_t> cg_bytes(std::istreambuf_iterator<char>(cg_file),{}), decoded;
    dc4::CrxInfo info;
    CHECK(dc4::crx_decode(cg_bytes.data(),cg_bytes.size(),&decoded,&info));
    CHECK(info.width==1024 && info.height==576);
    CHECK(native_cg==decoded);
    bool filtered=false;
    for(int y=100;y<110;++y) for(int x=100;x<110;++x) {
        const size_t dst=(size_t(y)*1920+x)*4, nearest=(size_t(y*576/1080)*1024+x*1024/1920)*4;
        for(int c=0;c<3;++c) if(high_cg[dst+c]!=decoded[nearest+c]) filtered=true;
    }
    CHECK(filtered);
    menu.scroll(1); CHECK(menu.variable(99,100)==1);
    CHECK(frame(menu,gallery_font)==first_cg);
    menu.update(160); const auto between=frame(menu,gallery_font);
    CHECK(between!=first_cg); CHECK(frame(menu,gallery_font)==between);
    menu.scroll(1); CHECK(menu.variable(99,100)==1);
    const auto second_cg=frame(menu,gallery_font); CHECK(second_cg!=between);
    menu.scroll(1); CHECK(menu.variable(99,100)==10);
    menu.update(320);
    menu.scroll(1); CHECK(menu.variable(99,100)==145);
    menu.update(320); menu.scroll(1); CHECK(menu.variable(99,100)==0);
    menu.update(320); menu.scroll(-1); CHECK(menu.variable(99,100)==145);
    menu.update(320); menu.back();
    CHECK(menu.page()==Page::Gallery && !menu.gallery_preview_active());
    CHECK(menu.variable(3,6)==1);
    click(menu,"page--");
    menu.set_variable(99,10,0); click(menu,"cg0");
    CHECK(frame(menu,gallery_font)==first_cg);
    menu.scroll(1); CHECK(menu.variable(99,100)==1);
    CHECK(frame(menu,gallery_font)==second_cg);
    menu.back(); menu.set_variable(99,10,1);
    CHECK(menu.page() == Page::Gallery);
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
    {
        const auto sheet=dc4ui::dc2dm_title_particle_sheet(data);
        CHECK(sheet.size()==800u*32u*4u);
        dc4scene::FontRenderer font;
        auto outgoing=frame(menu,font);
        if(const char* source=std::getenv("DC2DM_TRANSITION_RGBA")) {
            std::ifstream input(source,std::ios::binary);
            outgoing.assign(std::istreambuf_iterator<char>(input),{});
            CHECK(outgoing.size()==1280u*720u*4u);
        }
        dc4ui::Dc2pcMenuTransition transition;
        const auto lit_pixels = [](const std::vector<uint8_t>& pixels) {
            size_t count=0;
            for(size_t i=0;i<pixels.size();i+=4)if(pixels[i])++count;
            return count;
        };
        std::vector<uint8_t> solid(1280u*720u*4u,255), masked;
        transition.start_dc2dm_title(solid,sheet);
        transition.update(240);transition.draw(&masked);
        const auto first_area=lit_pixels(masked);
        CHECK(first_area>0 && first_area<1280u*720u);
        transition.update(360);transition.draw(&masked);
        CHECK(lit_pixels(masked)<first_area/2);
        transition.update(400);transition.draw(&masked);
        CHECK(lit_pixels(masked)==0);
        transition.start_dc2dm_title(outgoing,sheet);
        transition.update(200);
        for(int step=0;step<25;++step) {
            std::vector<uint8_t> pixels;
            transition.update(40);transition.draw(&pixels);
            CHECK(pixels.size()==outgoing.size());
            if(const char* output=std::getenv("DC2DM_MENU_CAPTURES")) {
                std::ofstream file(std::filesystem::path(output)/("particles-"+std::to_string(step)+".ppm"),std::ios::binary);
                file << "P6\n1280 720\n255\n";
                for(size_t i=0;i<pixels.size();i+=4)file.write(reinterpret_cast<const char*>(pixels.data()+i),3);
            }
            if(step==24)for(size_t i=0;i<pixels.size();i+=4)CHECK(!pixels[i]&&!pixels[i+1]&&!pixels[i+2]);
        }
    }
    const auto temporary = std::filesystem::temp_directory_path()/
        ("dc2dm-menu-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    {
        std::ofstream native(temporary,std::ios::binary);
        const std::string bytes(0x4B80,'\1');
        native.write(bytes.data(),bytes.size());
    }
    const int progress = menu.variable(100,1);
    CHECK(!menu.load_system_dat(temporary.string(),&error));
    CHECK(error.find("Unsupported or truncated DC2DM system data") != std::string::npos);
    CHECK(menu.variable(100,1) == progress);
    {
        std::string bytes(0x3D64,'\0');
        bytes[0xE5]=1;bytes[0xE6]=1;bytes[0xF4]=3;
        bytes[0x148]=1;bytes[0x17A]=1;bytes[0x210]=1;
        std::ofstream native(temporary,std::ios::binary);
        native.write(bytes.data(),bytes.size());
    }
    CHECK(menu.load_system_dat(temporary.string(),&error));
    CHECK(menu.variable(100,1)==1 && menu.variable(100,2)==1 && menu.variable(100,16)==3);
    CHECK(menu.variable(10,1)==1 && menu.variable(96,0)==1 && menu.variable(97,0)==1 && menu.variable(98,0)==1);
    CHECK(menu.variable(100,100)==0);
    menu.set_variable(100,1,1);
    CHECK(menu.save_profile(temporary.string(),&error));
    menu.set_variable(100,1,0);
    CHECK(menu.load_profile(temporary.string(),&error));
    CHECK(menu.variable(100,1) == 1 && menu.music_seen("music/001"));
    std::vector<uint8_t> previous_title;
    dc4scene::FontRenderer title_font;
    for(int stage=0;stage<3;++stage) {
        Menu stage_menu;CHECK(stage_menu.load(data,&error));
        std::vector<uint8_t> native(0x3D64,0);
        native[0xE5]=stage>=1;native[0xE6]=stage>=2;
        native[0x148]=1;native[0x17A]=1;native[0x210]=1;
        write_bytes(temporary,native);
        CHECK(stage_menu.load_system_dat(temporary.string(),&error));
        const auto title=frame(stage_menu,title_font);
        if(stage)CHECK(title!=previous_title);
        previous_title=title;
        capture(stage_menu,"title-stage-"+std::to_string(stage));
        auto stage_start = click(stage_menu,"sce_start");
        CHECK(stage_menu.confirming() == (stage == 0));
        if (stage == 0) {
            CHECK(stage_start.type == Type::None);
            CHECK(stage_menu.answer_confirmation(false).type == Type::None);
            CHECK(!stage_menu.confirming() && stage_menu.variable(100,1) == 0);
            CHECK(click(stage_menu,"sce_start").type == Type::None);
            CHECK(stage_menu.confirming());
            stage_start = stage_menu.answer_confirmation(true);
        }
        CHECK(stage_start.type == Type::NewGame && stage_start.asset == "SCESELECT");
        if (stage > 0) {
            dc4scene::SceneRunner selector(data);
            CHECK(selector.load_ui_script(std::string(data)+"/MES/start.mes"));
            CHECK(selector.load_script(std::string(data)+"/MES/sceselect.mes"));
            stage_menu.initialize_scene(selector,false);
            CHECK(!selector.advance_to_next_text() && selector.waiting_for_time());
            const auto choice_present = [&](const std::string& action) {
                return std::any_of(selector.buttons().begin(),selector.buttons().end(),
                    [&](const auto& button) { return button.action == action; });
            };
            CHECK(choice_present("sce01") && choice_present("sce02"));
            CHECK(choice_present("sce03") == (stage == 2));
        }
        CHECK(stage_menu.save_profile(temporary.string(),&error));
        Menu restarted;CHECK(restarted.load(data,&error));
        CHECK(restarted.load_profile(temporary.string(),&error));
        CHECK(frame(restarted,title_font)==title);
        CHECK(restarted.variable(96,0)==1 && restarted.variable(97,0)==1 && restarted.variable(98,0)==1);
        dc4scene::SceneRunner old_save(data);
        CHECK(old_save.load_script(std::string(data)+"/MES/main.mes"));
        old_save.set_variable(100,1,0);old_save.set_variable(100,2,0);
        restarted.restore_scene_progress(old_save);
        CHECK(old_save.variable(10,1)==(stage>=1) && old_save.variable(10,2)==(stage>=2));
    }
    std::filesystem::remove(temporary);
    return failures ? 1 : 0;
}
