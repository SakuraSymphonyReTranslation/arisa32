#include "arisa32/system_menu.h"
#include "arisa32/scene_audio.h"
#include "arisa32/names.h"
#include "arisa32/resource.h"
#include "arisa32/save_screen.h"
#include <algorithm>
#include <chrono>
#include <cmath>
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
void capture_save(dc4ui::SaveScreen& screen,const std::vector<dc4ui::SaveSlot>& slots,const std::string& name) {
    const char* output=std::getenv("DC2PC_MENU_CAPTURES");
    if(!output)return;
    dc4scene::FontRenderer font;
#ifdef _WIN32
    font.load("C:/Windows/Fonts/msgothic.ttc",0,22,false);
#endif
    std::vector<uint8_t> frame;screen.draw(slots,-1,-1,false,font,&frame);
    std::filesystem::create_directories(output);
    std::ofstream out(std::filesystem::path(output)/(name+".ppm"),std::ios::binary);
    out<<"P6\n1280 720\n255\n";
    for(size_t i=0;i<frame.size();i+=4)out.write(reinterpret_cast<const char*>(frame.data()+i),3);
}
void save_interactions(const char* data) {
    using A=dc4ui::SaveActionType;
    dc4ui::SaveScreen screen;std::string error;
    CHECK(screen.load(data,&error));CHECK(screen.dc2pc());CHECK(error.empty());
    std::vector<dc4ui::SaveSlot> slots(dc4ui::SaveScreen::kSlotCount);
    auto px=[](int x){return 160+int(std::lround(x*1.2));};
    auto py=[](int y){return int(std::lround(y*1.2));};
    auto click=[&](int x,int y){screen.pointer_down(px(x),py(y));return screen.pointer_up(px(x),py(y));};
    capture_save(screen,slots,"save-empty");
    CHECK(click(588,303).type==A::None);CHECK(screen.choosing_icon());
    capture_save(screen,slots,"save-icons");
    screen.pointer_down(px(108),py(219));screen.cancel_pointer();
    CHECK(screen.pointer_up(px(108),py(219)).type==A::None);CHECK(!screen.confirming());
    screen.pointer_down(px(108),py(219));
    CHECK(screen.pointer_up(px(172),py(219)).type==A::None);CHECK(!screen.confirming());
    CHECK(click(108,219).type==A::None);CHECK(screen.confirming());
    capture_save(screen,slots,"save-confirm");
    CHECK(screen.pointer_up(578,397).type==A::None);CHECK(screen.confirming());
    CHECK(screen.answer_confirmation(false).type==A::None);
    CHECK(!screen.choosing_icon());CHECK(screen.selected_icon()==-1);CHECK(!slots[0].occupied);
    for(int icon=0;icon<16;++icon) {
        CHECK(click(588,303).type==A::None);
        CHECK(click(108+(icon%4)*64,219+(icon/4)*64).type==A::None);
        CHECK(screen.confirming());CHECK(screen.selected_icon()==icon);
        screen.pointer_down(578,397);const auto saved=screen.pointer_up(578,397);
        CHECK(saved.type==A::Slot && saved.index==0);CHECK(screen.selected_icon()==icon);
    }
    slots[0].occupied=true;slots[0].character_icon=0;
    slots[0].timestamp=u"2026 10/24 20:30:00";slots[0].excerpt=u"Saved scene";
    screen.set_slot_occupied(0,true);screen.set_special_slots({},slots[0]);
    capture_save(screen,slots,"save-filled");
    CHECK(click(588,303).type==A::None);CHECK(click(289,547).type==A::None);
    CHECK(!screen.choosing_icon());CHECK(screen.selected_icon()==-1);
    CHECK(click(588,133).type==A::None);CHECK(screen.choosing_icon());
    CHECK(click(108,219).type==A::None);CHECK(screen.answer_confirmation(true).type==A::QuickSave);
    CHECK(click(316,457).type==A::Page);CHECK(screen.page()==1);
    CHECK(screen.slot_hit(px(588),py(303))==3);
    screen.set_page(9);CHECK(screen.slot_hit(px(588),py(473))==29);
    CHECK(click(316,457).type==A::Page);CHECK(screen.page()==0);
    screen.set_mode(dc4ui::SaveMode::Load);screen.set_shortcuts(true,false,false);
    CHECK(click(588,388).type==A::None);CHECK(!screen.confirming());
    CHECK(click(588,133).type==A::None);CHECK(!screen.confirming());
    capture_save(screen,slots,"load-filled");
    CHECK(click(588,218).type==A::None);CHECK(screen.confirming());
    capture_save(screen,slots,"load-confirm");
    CHECK(screen.answer_confirmation(true).type==A::LatestLoad);
    CHECK(click(588,303).type==A::None);screen.set_slot_occupied(0,false);
    CHECK(screen.answer_confirmation(true).type==A::None);
}
bool has(dc4ui::SystemMenu& menu, const std::string& tag) {
    for (const auto& control : menu.controls())
        if (control.enabled && control.tag == tag)
            return true;
    return false;
}
void capture_scene(dc4scene::SceneRunner& scene, const std::string& name) {
    const char* output = std::getenv("DC2PC_MENU_CAPTURES");
    if (!output) return;
    std::filesystem::create_directories(output);
    dc4scene::FontRenderer font;
#ifdef _WIN32
    font.load("C:/Windows/Fonts/msgothic.ttc",dc4::is_latin()?2:0,22,false);
#endif
    scene.set_font(&font); scene.draw(-1);
    std::ofstream out(std::filesystem::path(output) / (name + ".ppm"), std::ios::binary);
    out << "P6\n1280 720\n255\n";
    for (size_t i = 0; i < scene.frame().size(); i += 4)
        out.write(reinterpret_cast<const char*>(scene.frame().data() + i), 3);
    scene.set_font(nullptr);
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
void sprite_scenarios(const char* data) {
    for (const auto& script : {"fst_1216_b1_cmn", "fst_1216_b11_cmn", "fst_1216_b2_cmn", "fst_1216_c1_cmn"}) {
        dc4scene::SceneRunner scene(data);
        CHECK(scene.load_ui_script(std::string(data)+"/MES/start.mes"));
        CHECK(scene.show_message_window("SYSTEM/MESWIN.CRX"));
        CHECK(scene.load_script(std::string(data)+"/MES/"+script+".mes"));
        std::string expected[5];
        scene.set_observer([&](const dc4::MesCommand& c, const std::string&) {
            if (c.op == 0x30) for (int i=0;i<4;++i) expected[i].clear();
            if (c.op == 0x64 && c.words[2] == 255) std::swap(expected[0],expected[1]);
            if (c.op != 0x2B) return;
            if (c.p1 < 5) {
                if (c.str == "-") expected[c.p1].clear();
                else if (c.str != "--") expected[c.p1] = c.str;
            } else {
                const unsigned masks[]{0x10,0x20,0x40,0x80,0x08};
                for(int i=0;i<5;++i)if(c.p1&masks[i])expected[i].clear();
            }
        });
        bool target=false, multiple=false, boys=false, girls=false, trio=false, empty_room=false;
        int retained_lines=0;
        for(int i=0;i<2000 && !scene.scene_ended() && !scene.waiting_for_choice();++i) {
            if(!scene.advance_to_next_text()) {
                if(scene.waiting_for_time())scene.finish_script_wait();
                continue;
            }
            if (scene.script_name() != std::string(script)+".mes") break;
            int visible=0;
            for(int index=0;index<5;++index) {
                const auto& sprite=scene.sprite(index);
                if(expected[index].empty()) { CHECK(!sprite.visible); continue; }
                if(!sprite.visible || !sprite.img.valid())std::cerr<<script<<": missing "<<expected[index]<<'\n';
                CHECK(sprite.visible && sprite.img.valid()); ++visible;
            }
            multiple |= visible>1;
            const auto& sprite=scene.sprite(0);
            const bool closeup=sprite.visible && sprite.image.find("mya_0a_001_b")!=std::string::npos;
            const bool cave=sprite.visible && sprite.image.find("sgn_0a_100@")!=std::string::npos;
            const auto& right=scene.sprite(1);
            if(!boys && sprite.visible && right.visible && sprite.image.find("sgn_0a_000")!=std::string::npos && right.image.find("wtr_0a_110")!=std::string::npos) {
                CHECK(sprite.cur_x==-240 && right.cur_x==160);
                capture_scene(scene,"suginami-wataru"); boys=true;
            }
            if(!girls && sprite.visible && right.visible && sprite.image.find("anz_0a_000")!=std::string::npos && right.image.find("akn_0a_000")!=std::string::npos) {
                CHECK(sprite.cur_x==-240 && right.cur_x==160);
                capture_scene(scene,"anzu-akane"); girls=true;
            }
            const auto& third=scene.sprite(2);
            if(!trio && sprite.visible && right.visible && third.visible && sprite.image.find("kko_0a_a012")!=std::string::npos && third.image.find("akn_0a_103")!=std::string::npos) {
                CHECK(sprite.cur_x==-40 && sprite.scale==.75f);
                CHECK(right.cur_x==-240 && third.cur_x==160);
                scene.draw(-1);
                for(int y=0;y<720;++y)for(int x=1120;x<1280;++x) {
                    const auto at=(size_t(y)*1280+x)*4;
                    CHECK(scene.frame()[at]==0 && scene.frame()[at+1]==0 && scene.frame()[at+2]==0);
                }
                capture_scene(scene,"anzu-koko-akane"); trio=true;
            }
            if(!empty_room && std::string(script)=="fst_1216_b2_cmn" && scene.last_voice().find("hrm01")!=std::string::npos) {
                CHECK(visible==0); capture_scene(scene,"an-room-no-maya"); empty_room=true;
            }
            if(!target && (closeup || cave)) {
                CHECK(sprite.visible); CHECK(sprite.scale==1);
                CHECK(sprite.canvas_w==(closeup?1260:882));
                CHECK(sprite.canvas_h==(closeup?1080:756));
                capture_scene(scene,closeup?"maya-closeup":"suginami-cave");
                scene.draw(-1);const auto frame=scene.frame();
                std::vector<uint8_t> state;std::string error;
                CHECK(scene.capture_state(&state));CHECK(scene.restore_state(state,&error));
                scene.draw(-1);CHECK(scene.frame()==frame);
                target=true;
            }
            if(cave)++retained_lines;
        }
        if(std::string(script)=="fst_1216_b1_cmn") { CHECK(target); CHECK(multiple); CHECK(boys && girls); }
        else if(std::string(script)=="fst_1216_b11_cmn")CHECK(trio);
        else if(std::string(script)=="fst_1216_b2_cmn")CHECK(empty_room);
        else { CHECK(target); CHECK(retained_lines>5); }
    }
}
void native_pc_saves(const char* data) {
    using Page=dc4ui::SystemMenuPage;
    using Type=dc4ui::SystemMenuActionType;
    const auto root=std::filesystem::temp_directory_path()/("arisa32-dc2pc-import-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    const auto file=root/"renamed-system.bin", profile=root/"android.profile";
    std::vector<uint8_t> bytes(0xCF4,0);
    const auto write=[&] { std::ofstream out(file,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size())); };
    dc4ui::SystemMenu menu;
    std::string error;
    CHECK(menu.load(data,&error)); CHECK(menu.save_profile(profile.string(),&error));
    bytes[0x74+1]=bytes[0x74+2]=1; write();
    CHECK(menu.load_system_dat(file.string(),&error));
    menu.open(Page::Title);CHECK(has(menu,"dc") && !has(menu,"alfine"));
    CHECK(menu.load_profile(profile.string(),&error)); // old Android zeros cannot erase imported clears
    menu.open(Page::Title);CHECK(has(menu,"dc") && !has(menu,"alfine"));
    bytes[0x74+7]=1;
    bytes.resize(0xCF4+32,0xDA);write(); // known layout with extension is still supported
    CHECK(menu.load_system_dat(file.string(),&error));
    menu.open(Page::Title);CHECK(has(menu,"dc") && has(menu,"alfine"));
    for(const char* tag:{"dc","alfine"}) {
        menu.open(Page::Title);
        auto action=menu.activate(tag);
        if(menu.confirming())action=menu.answer_confirmation(true);
        CHECK(action.type==Type::NewGame && !action.asset.empty());
        dc4scene::SceneRunner scene(data);menu.initialize_scene(scene,false);
        CHECK(scene.load_script(std::string(data)+"/MES/"+action.asset+".mes"));
        CHECK(scene.advance_to_next_text()); CHECK(!scene.current_line().empty());
        CHECK(scene.variable(9,7)==0 && scene.variable(3,30)==0);
    }
    bytes.resize(0xCF4-1);write();CHECK(!menu.load_system_dat(file.string(),&error));
    CHECK(menu.variable(100,7)==1);
    bytes.resize(0xCF4,0);bytes[0xD8]=99;write();CHECK(!menu.load_system_dat(file.string(),&error));
    CHECK(menu.variable(100,1)==1);
    bytes[0xD8]=0;bytes[0x74+3]=255;write();CHECK(!menu.load_system_dat(file.string(),&error));
    CHECK(menu.variable(100,3)==0);
    CHECK(menu.save_profile(profile.string(),&error));
    dc4ui::SystemMenu restored;CHECK(restored.load(data,&error));CHECK(restored.load_profile(profile.string(),&error));
    restored.open(Page::Title);CHECK(has(restored,"dc") && has(restored,"alfine"));
    if(const char* saves=std::getenv("DC2PC_PC_SAVES")) {
        CHECK(menu.load_system_dat(std::string(saves)+"/system.dat",&error));
        menu.open(Page::Title);CHECK(has(menu,"dc") && has(menu,"alfine"));capture(menu,"pc-completed-title");
        for(const char* slot:{"00","01","02","03","04","08"}) {
            dc4scene::SceneRunner scene(data);menu.initialize_scene(scene,false);
            CHECK(scene.load_ui_script(std::string(data)+"/MES/start.mes"));
            // Reproduce a runner that previously executed Interview controls.
            CHECK(scene.load_script(std::string(data)+"/MES/interview.mes"));
            scene.advance_to_next_text();
            const auto started=std::chrono::steady_clock::now();
            const bool loaded=scene.load_state(std::string(saves)+"/DC2PC"+slot+".dat");
            const auto imported=std::chrono::steady_clock::now();
            if(!loaded)std::cerr<<"PC save "<<slot<<": "<<scene.last_error()<<'\n';
            CHECK(loaded);CHECK(!scene.current_line().empty());CHECK(scene.variable(100,7)==1);
            CHECK(scene.buttons().size()==7);
            for(const auto& button:scene.buttons())CHECK(button.action!="return" && button.action!="page");
            std::vector<uint8_t> snapshot;CHECK(scene.capture_state(&snapshot,&error));
            CHECK(scene.load_script(std::string(data)+"/MES/interview.mes"));
            scene.advance_to_next_text();
            const auto restoring=std::chrono::steady_clock::now();
            CHECK(scene.restore_state(snapshot,&error));
            CHECK(scene.buttons().size()==7);
            const auto restored=std::chrono::steady_clock::now();
            const auto save_path=root/(std::string("slot-")+slot+".dc4sav");
            CHECK(scene.save_state(save_path.string()));
            const auto saved=std::chrono::steady_clock::now();
            auto ms=[](auto a,auto b){return std::chrono::duration<double,std::milli>(b-a).count();};
            std::cout<<"Save "<<slot<<" PC load="<<ms(started,imported)<<"ms snapshot restore="
                     <<ms(restoring,restored)<<"ms write="<<ms(restored,saved)<<"ms\n";
            if(std::string(slot)=="00")capture_scene(scene,"load-after-interview");
            if(std::string(slot)=="04") {
                CHECK(scene.waiting_for_choice() && scene.choices().size()==3);
                CHECK(scene.choose(1));CHECK(scene.advance_to_next_text());
                CHECK(scene.script_name()=="fst_1216_b12_cmn.mes");
            } else { CHECK(!scene.waiting_for_choice());CHECK(scene.advance_to_next_text()); }
            if(std::string(slot)=="01" || std::string(slot)=="08") {
                CHECK(scene.sprite(0).img.valid());
            }
            for(size_t i=1;i<5;++i)CHECK(!scene.sprite(i).visible);
        }
    }
    std::filesystem::remove_all(root);
}
void legacy_quick_save(const char* data) {
    const char* path=std::getenv("DC2PC_LEGACY_SAVE");
    if(!path)return;
    dc4scene::SceneRunner scene(data);
    CHECK(scene.load_ui_script(std::string(data)+"/MES/start.mes"));
    CHECK(scene.load_state(path));
    CHECK(scene.script_name()=="fst_1216_b1_cmn.mes");
    CHECK(!scene.sprite(0).visible && !scene.sprite(1).visible);
    CHECK(scene.sprite(0).cur_x==-240 && scene.sprite(1).cur_x==160);
    CHECK(scene.advance_to_next_text());
    CHECK(scene.sprite(0).visible && scene.sprite(1).visible);
    CHECK(scene.sprite(0).image=="bst\\sgn_0a_000" && scene.sprite(1).image=="bst\\wtr_0a_101");
    CHECK(scene.sprite(0).cur_x==-240 && scene.sprite(1).cur_x==160);
    capture_scene(scene,"recorded-load-fixed");
    std::vector<uint8_t> repaired;std::string error;
    CHECK(scene.capture_state(&repaired));scene.draw(-1);const auto frame=scene.frame();
    CHECK(scene.restore_state(repaired,&error));scene.draw(-1);CHECK(scene.frame()==frame);
}
} // namespace
int main(int argc, char** argv) {
    const char* data = argc > 1 ? argv[1] : std::getenv("DC2PC_TEST_DATA");
    if (!data) {
        std::cout << "Installed DC2PC checks skipped (set DC2PC_TEST_DATA).\n";
        return 0;
    }
    if (argc > 2) dc4::configure_resources(data, argv[2]);
    native_pc_saves(data);
    legacy_quick_save(data);
    save_interactions(data);
    sprite_scenarios(data);
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
    dc4::VoiceArchive voices;
    CHECK(voices.open(std::string(data)+"/VOICE.DAT",&error));
    CHECK(voices.entries().size()==49474);
    dc4::AudioEngine mixer; audio.set_engine(&mixer,&voices);
    const auto* menu_voice=voices.find("VOICE\\sys\\sys_otm_02");
    CHECK(menu_voice);
    std::vector<uint8_t> voice_bytes;
    std::vector<int16_t> pcm; uint32_t rate=0; uint16_t channels=0;
    if(menu_voice) {
        CHECK(voices.read_entry(*menu_voice,&voice_bytes,&error));
        CHECK(dc4::decode_xpcm_pcm(voice_bytes.data(),voice_bytes.size(),&pcm,&rate,&channels));
        CHECK(rate>0 && channels>0 && !pcm.empty());
        CHECK(std::any_of(pcm.begin(),pcm.end(),[](int16_t sample){return sample!=0;}));
    }
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
    CHECK(opening.speaker().empty());
    std::vector<uint8_t> live;
    CHECK(opening.capture_state(&live));
    CHECK(opening.set_sidebar_visible(true)); CHECK(opening.sidebar_visible());
    for (const auto& row : {std::pair<int,const char*>{89,":sidebarwin"}, {119,":ds"},
                           {149,":dl"}, {179,":option"}, {209,":aatitle"}, {238,":sidebarclose"}}) {
        const int row_y = int(std::lround(row.first*1.2));
        const auto* hit = opening.hit_test(160+int(670*1.2),row_y);
        CHECK(hit && *hit == row.second);
        CHECK(!opening.hit_test(1124,row_y));
    }
    CHECK(!opening.hit_test(300,550)); CHECK(!opening.advance_to_next_text());
    CHECK(!opening.browse_dialogue(-1));
    capture_scene(opening,"sidebar");
    opening.update(1000);
    std::vector<uint8_t> sidebar_state; CHECK(opening.capture_state(&sidebar_state)); CHECK(sidebar_state == live);
    CHECK(opening.restore_state(live,&error)); CHECK(!opening.sidebar_visible());
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
    // Narration keeps the complete floral frame; cropping the first 39 rows
    // used to cut both flowers and the nameplate off at the body boundary.
    size_t empty_plate_pixels=0;
    for(int y=459;y<503;++y)for(int x=175;x<480;++x) {
        const auto at=(size_t(y)*1280+x)*4;
        if(!std::equal(opening.frame().begin()+at,opening.frame().begin()+at+3,without_window.begin()+at))++empty_plate_pixels;
    }
    CHECK(empty_plate_pixels>500);
    capture_scene(opening,"monologue-frame");
    dc4scene::SceneRunner dialogue(data);
    CHECK(dialogue.load_ui_script(std::string(data)+"/MES/start.mes"));
    CHECK(dialogue.show_message_window("SYSTEM/MESWIN.CRX"));
    CHECK(dialogue.load_script(std::string(data)+"/MES/fst_1216_b1_cmn.mes"));
    bool named=false;
    for(int i=0;i<100 && !named;++i) {
        dialogue.advance_to_next_text();
        if(dialogue.waiting_for_time())dialogue.finish_script_wait();
        named=!dialogue.speaker().empty() && !dialogue.current_line().empty();
    }
    CHECK(named); CHECK(dialogue.speaker()!=dialogue.current_line());
    CHECK(dialogue.browse_dialogue(-1)); CHECK(dialogue.displayed_speaker().empty());
    dialogue.return_to_dialogue();
    capture_scene(dialogue,"nameplate");
    bool captured_sprites=false;
    for (int i=0;i<1500 && !dialogue.waiting_for_choice() && !dialogue.scene_ended();++i) {
        dialogue.advance_to_next_text();
        if(dialogue.waiting_for_time())dialogue.finish_script_wait();
        if(!captured_sprites && dialogue.sprite(0).visible && dialogue.sprite(0).img.valid()) {
            capture_scene(dialogue,"sprites");captured_sprites=true;
        }
    }
    CHECK(dialogue.waiting_for_choice()); CHECK(dialogue.choices().size()==3);
    CHECK(dialogue.choice_at(500,260)==0); CHECK(dialogue.choice_at(500,330)==1);
    CHECK(dialogue.choice_at(500,400)==2); CHECK(dialogue.choice_at(500,550)==-1);
    capture_scene(dialogue,std::string("choices-")+std::filesystem::path(data).filename().string());
    dialogue.draw(-1); const auto normal_choices=dialogue.frame();
    dialogue.set_pointer(500,330,false); dialogue.draw(-1);
    size_t hover_pixels=0;
    for(int y=308;y<355;++y)for(int x=324;x<898;++x) {
        const auto at=(size_t(y)*1280+x)*4;
        if(!std::equal(dialogue.frame().begin()+at,dialogue.frame().begin()+at+3,normal_choices.begin()+at))++hover_pixels;
    }
    CHECK(hover_pixels>1000);
    const auto hovered_choices=dialogue.frame();
    dialogue.set_pointer(500,330,true); dialogue.draw(-1);
    CHECK(dialogue.frame()==hovered_choices); // flower stays visible during touch
    capture_scene(dialogue,std::string("choices-hover-")+std::filesystem::path(data).filename().string());
    std::vector<uint8_t> choice_state; CHECK(dialogue.capture_state(&choice_state));
    CHECK(dialogue.set_sidebar_visible(true)); CHECK(dialogue.choice_at(500,330)==-1);
    CHECK(!dialogue.choose(1)); CHECK(dialogue.set_sidebar_visible(false));
    for(size_t i=0;i<3;++i) {
        const bool restored=dialogue.restore_state(choice_state,&error);
        if(!restored)std::cerr<<"choice restore: "<<error<<'\n';
        CHECK(restored); CHECK(dialogue.choose(i));
        dialogue.advance_to_next_text();
        CHECK(dialogue.script_name()==std::string("fst_1216_b1")+char('1'+i)+"_cmn.mes");
        CHECK(dialogue.last_error().empty());
    }
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
    CHECK(interview.type == Type::None);
    CHECK(menu.page()==Page::Interview);
    CHECK(has(menu,"otome") && has(menu,"anzu") && has(menu,"page") && has(menu,"return"));
    CHECK(menu.controls().size()==7);
    capture(menu,"interview-first");
    const char* actors[]={"otome","yume","koko","minatu","anzu","akane","aisia","mahiru","sinobu","rino"};
    for(int actor=0;actor<10;++actor) {
        if(actor==5) {menu.activate("page");CHECK(has(menu,"akane") && !has(menu,"otome"));}
        const auto play=menu.activate(actors[actor]);
        CHECK(play.type==Type::PlayMusic && !play.asset.empty());
        CHECK(menu.page()==Page::Interview && menu.controls().size()==6);
        CHECK(!has(menu,actors[actor])); // active actor is an authored static chip
        capture(menu,std::string("interview-")+actors[actor]);
    }
    menu.back();CHECK(menu.page()==Page::Extras && has(menu,"talk"));
    CHECK(menu.error().empty());capture(menu,"interview-return");
    const auto path = std::filesystem::temp_directory_path() / ("arisa32-dc2pc-menu-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".profile");
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
    if(const char* saves=std::getenv("DC2PC_PC_SAVES"))
        CHECK(menu.load_system_dat(std::string(saves)+"/system.dat",&error));
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
        if(entry.second==Page::Scenes) {
            menu.activate("nanaka");
            CHECK(menu.page()==Page::Scenes);
            const auto before=menu.controls();
            const auto bar=std::find_if(before.begin(),before.end(),[](const auto& c){return c.tag=="#catalogue:scroll";});
            CHECK(bar!=before.end());
            if(bar!=before.end()) {
                const int x=bar->x+bar->w/2,top=bar->y+bar->w/2,bottom=bar->y+bar->h-bar->w/2;
                menu.pointer_down(x,top);menu.pointer_move(x,bottom);
                CHECK(menu.variable(206,80)>0);
                capture(menu,"scene-slider-bottom");
                menu.pointer_move(x,top);CHECK(menu.variable(206,80)==0);
                menu.pointer_up(x,top);
                capture(menu,"scene-slider-top");
                // A pixel move within the same row still moves the authored thumb.
                dc4scene::FontRenderer font;
                std::vector<uint8_t> first,second;
                menu.pointer_down(x,top);menu.draw(-1,-1,false,font,&first);
                bool subrow_motion=false;
                int previous=menu.variable(206,80);
                for(int pixel=1;pixel<=12;++pixel) {
                    menu.pointer_move(x,top+pixel);menu.draw(-1,-1,false,font,&second);
                    const int value=menu.variable(206,80);
                    subrow_motion|=value==previous && first!=second;
                    previous=value;first.swap(second);
                }
                CHECK(subrow_motion);
                menu.pointer_up(x,top);CHECK(menu.variable(206,80)==0);
                for(int pass=0;pass<3;++pass) {
                    const auto controls=menu.controls();
                    const auto down=std::find_if(controls.begin(),controls.end(),[](const auto& c){return c.tag=="sel_statdown";});
                    CHECK(down!=controls.end());
                    if(down!=controls.end()) {
                        menu.pointer_down(down->x+down->w/2,down->y+down->h/2);
                        menu.pointer_up(down->x+down->w/2,down->y+down->h/2);
                        CHECK(menu.variable(206,80)==(pass+1)*2);
                    }
                }
                auto controls=menu.controls();
                const auto up=std::find_if(controls.begin(),controls.end(),[](const auto& c){return c.tag=="sel_statup";});
                CHECK(up!=controls.end());
                if(up!=controls.end()) {
                    menu.pointer_down(up->x+up->w/2,up->y+up->h/2);
                    menu.pointer_up(up->x+up->w/2,up->y+up->h/2);
                    CHECK(menu.variable(206,80)==4);
                }
                for(int i=0;i<100;++i)menu.activate("sel_statdown");
                const int maximum=menu.variable(206,80);CHECK(maximum>4);
                menu.activate("sel_statdown");CHECK(menu.variable(206,80)==maximum);
                for(int i=0;i<100;++i)menu.activate("sel_statup");
                CHECK(menu.variable(206,80)==0);
                menu.activate("sel_statup");CHECK(menu.variable(206,80)==0);
                CHECK(menu.controls().size()==before.size());
            }
        }
        menu.activate("return"); CHECK(menu.page()==Page::Extras);
        CHECK(has(menu,"cg") && has(menu,"talk"));
        menu.activate(entry.first);menu.back();CHECK(menu.page()==Page::Extras);
        CHECK(has(menu,"cg") && has(menu,"talk"));
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
    const auto config_path=std::filesystem::temp_directory_path()/("arisa32-dc2pc-config-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".profile");
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
