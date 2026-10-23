#include "arisa32/system_menu.h"
#include "arisa32/scene_audio.h"
#include "arisa32/names.h"
#include "arisa32/resource.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
int failures = 0;
void check(bool value, int line) {
    if (!value) {
        std::cerr << "line " << line << '\n';
        ++failures;
    }
}
#define CHECK(x) check(bool(x), __LINE__)
bool has(dc4ui::SystemMenu& menu, const std::string& tag) {
    for (const auto& control : menu.controls())
        if (control.enabled && control.tag == tag)
            return true;
    return false;
}
void capture(dc4ui::SystemMenu& menu, const std::string& name) {
    const char* output = std::getenv("DC2PC_MENU_CAPTURES");
    if (!output)
        return;
    dc4scene::FontRenderer font;
#ifdef _WIN32
    font.load("C:/Windows/Fonts/msgothic.ttc",dc4::is_latin()?2:0,16,false);
#endif
    std::vector<uint8_t> rgba;
    menu.draw(-1, -1, false, font, &rgba);
    CHECK(rgba.size() == 1280u * 720u * 4u);
    std::filesystem::create_directories(output);
    std::ofstream out(std::filesystem::path(output) / (name + ".ppm"), std::ios::binary);
    out << "P6\n1280 720\n255\n";
    for (size_t i = 0; i < rgba.size(); i += 4)
        out.write(reinterpret_cast<const char*>(rgba.data() + i), 3);
}
} // namespace
int main(int argc, char** argv) {
    const char* data = argc > 1 ? argv[1] : std::getenv("DC2PC_TEST_DATA");
    if (!data) {
        std::cout << "Installed DC2PC checks skipped (set DC2PC_TEST_DATA).\n";
        return 0;
    }
    if (argc > 2) dc4::configure_resources(data, argv[2]);
    using Page = dc4ui::SystemMenuPage;
    using Type = dc4ui::SystemMenuActionType;
    dc4ui::SystemMenu menu;
    std::string error;
    CHECK(menu.load(data, &error));
    CHECK(error.empty());
    if (!menu.loaded())
        return 1;
    CHECK(has(menu, "start"));
    auto start = menu.activate("start");
    if (menu.confirming()) start = menu.answer_confirmation(true);
    std::cout << "Start action: " << int(start.type) << " " << start.asset << " " << menu.error() << '\n';
    CHECK(start.type == Type::NewGame);
    CHECK(start.asset == "fst_1216_a1_cmn");
    CHECK(menu.title_music()=="music/025.pcm");
    CHECK(menu.scene_ui_script()=="start.mes");
    dc4::SceneAudio audio(data);
    CHECK(audio.load_names(&error));
    CHECK(audio.bgm_names().size()==90);
    CHECK(audio.bgm_names()[0]==audio.bgm_names()[1]);
    CHECK(audio.bgm_names()[25]=="music\\025");
    std::ifstream names_file(std::string(data)+"/DAT/names.dat",std::ios::binary);
    std::vector<uint8_t> names_bytes((std::istreambuf_iterator<char>(names_file)),{});
    dc4::GameNames names;
    CHECK(dc4::decode_game_names(names_bytes,&names));
    CHECK(names.cg.size()==1336 && names.scenes.size()==1434);
    const auto original_names=names.bgm;
    names_bytes.resize(names_bytes.size()-1);
    CHECK(!dc4::decode_game_names(names_bytes,&names));
    CHECK(names.bgm==original_names);
    dc4scene::SceneRunner opening(data);
    CHECK(opening.load_sheets(std::string(data)+"/MES/start.mes"));
    CHECK(opening.load_ui_script(std::string(data)+"/MES/"+menu.scene_ui_script()));
    CHECK(opening.show_message_window("SYSTEM/MESWIN.CRX"));
    CHECK(opening.load_script(std::string(data)+"/MES/"+start.asset+".mes",start.label));
    menu.initialize_scene(opening);
    bool reached=false;
    for(int i=0;i<100 && !reached;++i) { reached=opening.advance_to_next_text(); if(opening.waiting_for_time())opening.finish_script_wait(); }
    if(!reached)std::cerr<<opening.last_error()<<'\n';
    CHECK(reached);
    CHECK(!opening.current_line().empty());
    CHECK(opening.buttons().size()==7);
    for (const auto& button : opening.buttons()) {
        const auto* hit=opening.hit_test(button.x+button.w/2,button.y+button.h/2);
        CHECK(hit && *hit==button.action);
        CHECK(button.state[0].valid);
    }
    CHECK(!opening.hit_test(1270,710));
    opening.set_window_opacity(100);
    opening.set_message_visible(false);opening.draw(-1);
    const auto without_window=opening.frame();
    opening.set_message_visible(true);opening.draw(-1);
    size_t panel_pixels=0;
    for(int y=458;y<708;++y)for(int x=166;x<1048;++x) {
        const size_t pixel=(size_t(y)*1280+x)*4;
        if(!std::equal(opening.frame().begin()+pixel,opening.frame().begin()+pixel+3,
                       without_window.begin()+pixel))++panel_pixels;
    }
    CHECK(panel_pixels>10000);
    if(const char* output=std::getenv("DC2PC_MENU_CAPTURES")) {
        dc4scene::FontRenderer font;
#ifdef _WIN32
        font.load("C:/Windows/Fonts/msgothic.ttc",dc4::is_latin()?2:0,22,false);
#endif
        opening.set_font(&font);opening.draw(-1);
        std::ofstream out(std::filesystem::path(output)/"opening.ppm",std::ios::binary);
        out<<"P6\n1280 720\n255\n";
        for(size_t i=0;i<opening.frame().size();i+=4)
            out.write(reinterpret_cast<const char*>(opening.frame().data()+i),3);
        opening.set_font(nullptr);
        if (dc4::is_latin()) {
            for (int i=0;i<6;++i) {
                bool next=false;
                for(int j=0;j<100 && !next;++j) {
                    next=opening.advance_to_next_text();
                    if(opening.waiting_for_time())opening.finish_script_wait();
                }
                CHECK(next);
            }
            opening.set_font(&font);opening.draw(-1);
            std::ofstream sample(std::filesystem::path(output)/"opening-latin.ppm",std::ios::binary);
            sample<<"P6\n1280 720\n255\n";
            for(size_t i=0;i<opening.frame().size();i+=4)
                sample.write(reinterpret_cast<const char*>(opening.frame().data()+i),3);
            opening.set_font(nullptr);
        }
    }
    menu.open(Page::Title);
    CHECK(!has(menu, "dc"));
    CHECK(!has(menu, "alfine"));
    capture(menu, "fresh-title");
    menu.open(Page::Extras);
    CHECK(!has(menu, "talk"));
    capture(menu, "fresh-extra");
    dc4scene::SceneRunner scene(data);
    CHECK(scene.load_script(std::string(data) + "/MES/main.mes"));
    scene.set_variable(100, 1, 1);
    scene.set_variable(100, 2, 1);
    CHECK(menu.sync_progress(scene));
    menu.open(Page::Title);
    CHECK(has(menu, "dc"));
    CHECK(!has(menu, "alfine"));
    capture(menu, "dc-title");
    scene.set_variable(100, 7, 1);
    CHECK(menu.sync_progress(scene));
    menu.open(Page::Title);
    CHECK(has(menu, "dc"));
    CHECK(has(menu, "alfine"));
    capture(menu, "alfine-title");
    for (int i = 1; i <= 17; ++i)
        scene.set_variable(100, uint16_t(i), 1);
    CHECK(menu.sync_progress(scene));
    menu.open(Page::Title);
    menu.open(Page::Extras);
    CHECK(has(menu, "talk"));
    capture(menu, "interview-extra");
    const auto interview = menu.activate("talk");
    CHECK(interview.type == Type::NewGame);
    CHECK(interview.asset == "interview");
    const auto path = std::filesystem::temp_directory_path() / "arisa32-dc2pc-menu.profile";
    CHECK(menu.save_profile(path.string(), &error));
    dc4ui::SystemMenu restored;
    CHECK(restored.load(data, &error));
    CHECK(restored.load_profile(path.string(), &error));
    restored.open(Page::Title);
    CHECK(has(restored, "dc"));
    CHECK(has(restored, "alfine"));
    restored.open(Page::Extras);
    CHECK(has(restored, "talk"));
    std::filesystem::remove(path);
    menu.open(Page::Extras);
    for (const auto& entry : {std::pair<const char*,Page>{"cg",Page::Gallery},{"scene",Page::Scenes},{"bgm",Page::Music}}) {
        const auto out=menu.activate(entry.first);
        CHECK(out.type!=Type::StartScene);
        CHECK(menu.page()==entry.second);
        CHECK(has(menu,"return"));
        CHECK(menu.error().empty());
        capture(menu,entry.first);
        if(entry.second==Page::Music) {
            CHECK(has(menu,"music0")); CHECK(has(menu,"music1"));
            auto play=menu.activate("music1");
            CHECK(play.type==Type::PlayMusic); CHECK(play.asset=="music\\002");
            CHECK(menu.activate("rplay").type==Type::PlayMusic);
            CHECK(menu.variable(100,26)==0);
            CHECK(menu.activate("stop").type==Type::StopMusic);
            menu.scroll(2); CHECK(has(menu,"music4"));
            CHECK(menu.activate("music12").asset=="music\\058");
            CHECK(menu.activate("all_play_mode").type==Type::PlayMusic);
        }
        menu.activate("return"); CHECK(menu.page()==Page::Extras);
    }
    menu.open(Page::Config);
    CHECK(menu.activate("tr_off").type==Type::SettingsChanged);
    menu.pointer_down(160+int(600*1.2),int(107*1.2));
    CHECK(menu.pointer_move(160+int(672*1.2),int(107*1.2)).type==Type::SettingsChanged);
    menu.pointer_up(160+int(672*1.2),int(107*1.2));
    CHECK(menu.variable(99,7)==29);CHECK(menu.config().music_volume==100);
    // Text speed uses authored interval stops rather than a continuous thumb.
    dc4scene::FontRenderer config_font;
    std::vector<uint8_t> config_frame,previous_config;
    for(int value=0;value<4;++value) {
        const int stops[]={527,557,586,631,679};
        const int x=160+int(((stops[value]+stops[value+1])/2)*1.2),y=int(243*1.2);
        menu.pointer_down(x,y);CHECK(menu.pointer_up(x,y).type==Type::SettingsChanged);
        CHECK(menu.variable(99,13)==value);
        CHECK(menu.config().text_speed==((value==1 || value==3)?11:3));
        menu.draw(-1,-1,false,config_font,&config_frame);
        if(value)CHECK(config_frame!=previous_config);
        // No sprite may extend into the right pillarbox (the reported white bar).
        for(int py=270;py<310;++py)for(int px=1120;px<1280;++px) {
            const auto at=(size_t(py)*1280+px)*4;
            CHECK(config_frame[at]==0 && config_frame[at+1]==0 && config_frame[at+2]==0);
        }
        previous_config=config_frame;
    }
    for(const auto& control: {std::pair<int,int>{99,8},{99,9},{99,12},{99,11}}) {
        const int y=int((control.second==8?197:control.second==9?152:control.second==12?287:332)*1.2);
        const int left=160+int(528*1.2),right=160+int(666*1.2);
        menu.pointer_down(left,y);CHECK(menu.pointer_up(left,y).type==Type::SettingsChanged);
        CHECK(menu.variable(control.first,control.second)==0);
        menu.pointer_down(left,y);CHECK(menu.pointer_move(right,y).type==Type::SettingsChanged);
        menu.pointer_up(right,y);
        CHECK(menu.variable(control.first,control.second)==(control.second==12?3:control.second==11?15:29));
    }
    const auto config_path=std::filesystem::temp_directory_path()/"arisa32-dc2pc-config.profile";
    CHECK(menu.save_profile(config_path.string(),&error));
    dc4ui::SystemMenu config_restored;CHECK(config_restored.load(data,&error));
    CHECK(config_restored.load_profile(config_path.string(),&error));
    CHECK(config_restored.variable(99,13)==3);
    CHECK(config_restored.config().voice_volume==100);
    std::filesystem::remove(config_path);
    menu.activate("voiceSet");CHECK(!menu.controls().empty());capture(menu,"config-voice");menu.activate("menu");
    menu.activate("return"); CHECK(menu.page()==Page::Extras);
    menu.open(Page::Config);
    if(!menu.error().empty())std::cerr<<menu.error()<<'\n';
    CHECK(menu.error().empty());
    CHECK(!menu.controls().empty());
    capture(menu, "config");
    std::cout << "DC2PC Start dialogue, catalogue, audio, extras, settings, progression and profile persistence checked.\n";
    return failures ? 1 : 0;
}
