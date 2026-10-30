#include "arisa32/system_menu.h"
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

int main() {
    const char* data = std::getenv("DC2DM_DATA");
    if (!data || !*data) { std::cout << "DC2DM_DATA unset; real-data checks skipped\n"; return 0; }
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
