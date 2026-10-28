#include "arisa32/system_menu.h"
#include "arisa32/mesvm.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>

namespace {
int failures=0;
void check(bool result,const char* expr,int line){if(!result){std::cerr<<line<<": "<<expr<<'\n';++failures;}}
#define CHECK(x) check(bool(x),#x,__LINE__)
using Menu=dc4ui::SystemMenu;
using Page=dc4ui::SystemMenuPage;
using Type=dc4ui::SystemMenuActionType;
std::string lower(std::string text){for(char& c:text)c=char(std::tolower(static_cast<unsigned char>(c)));return text;}
bool has(Menu& m,const std::string& tag){for(const auto& c:m.controls())if(c.tag==tag&&c.enabled)return true;return false;}
dc4ui::SystemMenuAction click(Menu& m,const std::string& tag){
    for(const auto& c:m.controls())if(c.tag==tag&&c.enabled){m.pointer_down(c.x+c.w/2,c.y+c.h/2);auto action=m.pointer_up(c.x+c.w/2,c.y+c.h/2);return m.confirming()?m.answer_confirmation(true):action;}return {};
}
void bmp(const std::filesystem::path& path,const std::vector<uint8_t>& rgba){
    std::ofstream o(path,std::ios::binary);if(!o)return;
    auto u16=[&](uint16_t v){o.put(char(v));o.put(char(v>>8));};
    auto u32=[&](uint32_t v){u16(uint16_t(v));u16(uint16_t(v>>16));};
    o.put('B');o.put('M');u32(54+1280*720*4);u32(0);u32(54);u32(40);u32(1280);u32(uint32_t(-720));u16(1);u16(32);u32(0);u32(1280*720*4);u32(0);u32(0);u32(0);u32(0);
    for(size_t i=0;i<rgba.size();i+=4){o.put(char(rgba[i+2]));o.put(char(rgba[i+1]));o.put(char(rgba[i]));o.put(char(rgba[i+3]));}
}
void profile(){
    const auto path=std::filesystem::temp_directory_path()/"dc4engine-system-menu-test.profile";
    Menu first;
    CHECK(first.config().master_volume==100);
    first.set_variable(99,7,46);first.mark_cg_seen("BG\\DC4_EV_ASA07B.CRX");first.mark_scene_seen("DC4_MID20190301A1.mes");first.mark_music_seen("Music\\001.PCM");
    std::string error;CHECK(first.save_profile(path.string(),&error));
    Menu second;CHECK(second.load_profile(path.string(),&error));CHECK(second.config().music_volume==46);
    CHECK(second.cg_seen("bg/dc4_ev_asa07b"));CHECK(second.scene_seen("dc4_mid20190301a1"));CHECK(second.music_seen("music/001"));
    {std::ofstream bad(path);bad<<"DC4ENGINE_PROFILE 1\nv 99 7 99\nc \"../escape\"\n";}
    CHECK(!second.load_profile(path.string(),&error));CHECK(second.config().music_volume==46);
    std::filesystem::remove(path);
}
struct ReplayControl { const char* action; int x; };
constexpr ReplayControl replay_controls[]={{":qs",760},{":ql",880},{":ds",972},{":dl",1046}};
std::vector<uint8_t> read_bytes(const std::filesystem::path& path){
    std::ifstream in(path,std::ios::binary);
    return {(std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>()};
}
void write_bytes(const std::filesystem::path& path,const std::vector<uint8_t>& bytes){
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));
    CHECK(bool(out));
}
void check_replay_controls(dc4scene::SceneRunner& scene,bool disabled){
    for(const auto& control:replay_controls){
        const auto* action=scene.hit_test(control.x,705);
        CHECK(action && *action==(disabled?":disabled":control.action));
    }
    const auto* voice=scene.hit_test(440,705);
    CHECK(voice && *voice==":voice");
}
void check_replay_storage(dc4scene::SceneRunner& scene,const std::filesystem::path& scratch){
    CHECK(scene.save_load_allowed());
    const auto story=scratch/"story.dc4sav";
    CHECK(scene.save_state(story.string()));
    scene.set_variable(9,7,1);scene.set_variable(3,30,47);
    CHECK(!scene.save_load_allowed());
    std::vector<uint8_t> replay,after;
    CHECK(scene.capture_state(&replay));
    CHECK(!scene.load_state(story.string()));
    CHECK(scene.last_error()=="Save/Load unavailable during scene replay");
    CHECK(scene.capture_state(&after));CHECK(after==replay);
    // Both snapshot saves and native files/companions must survive rejected
    // writes unchanged. All files here are disposable test data.
    const std::vector<uint8_t> sentinel={'k','e','e','p'};
    for(const char* filename:{"existing.dc4sav","existing.dat"}){
        const auto target=scratch/filename;
        write_bytes(target,sentinel);write_bytes(target.string()+".arisa",sentinel);
        CHECK(!scene.save_state(target.string()));
        CHECK(scene.last_error()=="Save/Load unavailable during scene replay");
        CHECK(read_bytes(target)==sentinel);
        CHECK(read_bytes(target.string()+".arisa")==sentinel);
        CHECK(!std::filesystem::exists(target.string()+".tmp"));
    }
    // Rendering/history checkpoints are internal snapshots, still usable.
    scene.set_variable(9,7,0);
    CHECK(scene.restore_state(replay));CHECK(!scene.save_load_allowed());
    scene.finish_replay();
    CHECK(scene.save_load_allowed());CHECK(scene.variable(9,7)==0);
    CHECK(scene.variable(3,30)==47);
    CHECK(scene.load_state(story.string()));
    CHECK(scene.save_state(story.string()));
    // Native DC4 controls test !=0, not ==1.
    scene.set_variable(9,7,2);CHECK(!scene.save_load_allowed());
    scene.finish_replay();CHECK(scene.save_load_allowed());
}
std::vector<uint8_t> policy_script(uint16_t version){
    if(version==0x9969)return {1,0,0,0,2,0,0,0,0x69,0x99,0};
    return {1,0,0,0,3,0,0,0,0,0,uint8_t(version),uint8_t(version>>8),0,0};
}
void replay_policy_fixtures(const std::filesystem::path& root){
    // The Android app id and legacy test directory both contain "dc4";
    // neither that path nor a shared modern MES version identifies the game.
    for(const auto version:{uint16_t(0xAAB6),uint16_t(0xABB6)}){
        for(const bool identified:{false,true}){
            const auto data=root/"org.dc4engine"/(std::to_string(version)+(identified?"-known":"-other"));
            const auto main=policy_script(version);
            write_bytes(data/"MES/main.mes",main);
            if(identified)write_bytes(data/"MES/DC4_KYO20190204A1.MES",main);
            auto ui=policy_script(version);ui.pop_back();
            auto word=[&](int n){ui.push_back(uint8_t(n));ui.push_back(uint8_t(n>>8));};
            auto button=[&](int left,int right,const char* tag){
                ui.push_back(0x72);word(left);word(695);word(right);word(720);
                ui.push_back(0x54);while(*tag)ui.push_back(uint8_t(*tag++));ui.push_back(0);
            };
            button(708,822,":qs");button(822,937,":ql");
            button(937,1010,":ds");button(1010,1082,":dl");button(400,470,":voice");ui.push_back(0);
            write_bytes(data/"MES/win_size.mes",ui);
            dc4scene::SceneRunner scene(data.string());
            CHECK(scene.load_script((data/"MES/main.mes").string()));
            CHECK(scene.load_ui_script((data/"MES/win_size.mes").string()));
            CHECK(scene.save_load_allowed());check_replay_controls(scene,false);
            scene.set_variable(9,7,1);scene.set_variable(3,30,47);
            CHECK(scene.save_load_allowed()!=identified);check_replay_controls(scene,identified);
            if(identified){
                scene.finish_replay();CHECK(scene.variable(3,30)==47);
                check_replay_storage(scene,data/"saves");
                check_replay_controls(scene,false);
            }else{
                scene.finish_replay();CHECK(scene.variable(9,7)==1 && scene.variable(3,30)==47);
                CHECK(scene.save_state((data/"saves/ordinary.dc4sav").string()));
                CHECK(scene.load_state((data/"saves/ordinary.dc4sav").string()));
            }
        }
    }
    const auto classic=root/"dc2pc";
    write_bytes(classic/"MES/main.mes",policy_script(0x9969));
    dc4scene::SceneRunner scene(classic.string());
    CHECK(scene.load_script((classic/"MES/main.mes").string()));
    scene.set_variable(9,7,1);scene.set_variable(3,30,1);
    CHECK(!scene.save_load_allowed());scene.finish_replay();
    CHECK(scene.save_load_allowed());CHECK(scene.variable(9,7)==0 && scene.variable(3,30)==0);
    scene.set_variable(9,7,2);scene.set_variable(3,30,47);
    CHECK(scene.save_load_allowed());scene.finish_replay();
    CHECK(scene.variable(9,7)==2 && scene.variable(3,30)==47);
}
void replay_policy_assets(const std::string& data,const std::filesystem::path& scratch){
    Menu menu;std::string error;CHECK(menu.load(data,&error));
    dc4scene::SceneRunner scene(data);
    CHECK(scene.load_sheets(data+"/MES/start.mes"));
    CHECK(scene.load_ui_script(data+"/MES/win_size.mes"));
    CHECK(scene.load_script(data+"/MES/main.mes"));
    menu.initialize_scene(scene,false);check_replay_controls(scene,false);
    scene.set_pointer(-1,-1,false);scene.draw(-1);const auto ordinary=scene.frame();
    menu.initialize_scene(scene,true);CHECK(!scene.save_load_allowed());
    check_replay_controls(scene,true);
    for(const auto& control:replay_controls){
        for(const bool down:{false,true}){
            scene.set_pointer(control.x,705,down);scene.draw(-1);
            CHECK(scene.frame()==ordinary);
        }
    }
    if(const char* capture=std::getenv("DC4_MENU_CAPTURES")){
        std::filesystem::create_directories(capture);
        bmp(std::filesystem::path(capture)/"replay-disabled-toolbar.bmp",scene.frame());
    }
    menu.initialize_scene(scene,false);CHECK(scene.save_load_allowed());
    check_replay_controls(scene,false);
    for(const auto& control:replay_controls){
        scene.set_pointer(control.x,705,false);scene.draw(-1);
        CHECK(scene.frame()!=ordinary);
    }
    check_replay_storage(scene,scratch);
    std::cout<<"Verified replay controls, native hover artwork, storage protection and story restoration.\n";
}
// NAMES.DAT is the independent unlock-index -> chapter catalogue. Route menu
// scripts use different local selection indexes, especially outside Asa.
std::vector<std::string> scene_catalogue(const std::string& data){
    std::ifstream in(std::filesystem::path(data)/"GRP/NAMES.DAT",std::ios::binary);
    auto u32=[&](){unsigned char b[4]{};in.read(reinterpret_cast<char*>(b),4);return uint32_t(b[0])|uint32_t(b[1])<<8|uint32_t(b[2])<<16|uint32_t(b[3])<<24;};
    in.seekg(228);
    for(int stride:{48,36,32}){auto count=u32();in.seekg(std::streamoff(count)*stride,std::ios::cur);}
    std::vector<std::string> names;
    auto count=u32();CHECK(count==469);if(count>2000)return names;
    for(uint32_t i=0;i<count;++i){char name[33]{};in.read(name,32);names.emplace_back(name);}
    CHECK(bool(in));return names;
}
void render_replay(const std::string& data,Menu& menu,const dc4ui::SystemMenuAction& selected,const std::string& tag){
    dc4scene::SceneRunner scene(data);
    dc4scene::FontRenderer font;
#ifdef _WIN32
    font.load("C:/Windows/Fonts/msgothic.ttc",0,22);scene.set_font(&font);
#endif
    CHECK(scene.load_sheets(data+"/MES/start.mes"));
    CHECK(scene.load_ui_script(data+"/MES/win_size.mes"));
    CHECK(scene.load_script(data+"/MES/"+selected.asset+".mes",selected.label));
    menu.initialize_scene(scene,true);
    CHECK(scene.variable(9,7)==1);
    CHECK(!scene.save_load_allowed());
    bool ready=false;
    for(int yields=0;yields<32;++yields){
        ready=scene.advance_to_next_text();if(ready)break;
        if(!scene.pending_movie().empty())scene.finish_movie();
        else if(scene.waiting_for_time())scene.finish_script_wait();
        else break;
    }
    CHECK(ready);CHECK(!scene.current_line().empty());CHECK(scene.last_error().empty());
    scene.draw(-1);
    if(const char* capture=std::getenv("DC4_MENU_CAPTURES")){
        std::filesystem::create_directories(capture);bmp(std::filesystem::path(capture)/(tag+"-replay.bmp"),scene.frame());
    }
    if(tag=="sce0"){
        int advances=0;
        for(;advances<1000&&!scene.scene_ended()&&scene.last_error().empty();++advances){
            if(!scene.advance_to_next_text())break;
        }
        CHECK(advances<1000);CHECK(scene.scene_ended());CHECK(scene.last_error().empty());
        CHECK(lower(scene.next_script())=="sce_end");
        CHECK(lower(scene.script_name())==lower(selected.asset)+".mes");
    }
    menu.initialize_scene(scene,false);CHECK(scene.variable(9,7)==0);CHECK(scene.save_load_allowed());
}
void scene_selections(const std::string& data){
    Menu m;std::string error;CHECK(m.load(data,&error));if(!m.loaded())return;
    auto names=scene_catalogue(data);
    for(const auto& name:names)m.mark_scene_seen(name);
    std::set<int> checked;
    size_t replay_lines=0;
    for(const char* route:{"asa","nin","sor","hyr","sii","miu","tiy","cmn"}){
        m.open(Page::Scenes);m.activate(route);
        bool rendered=false;
        const int pages=m.variable(3,2)+1;
        for(int page=0;page<pages;++page){
            auto controls=m.controls();
            for(const auto& control:controls){
                const auto& tag=control.tag;
                if(!control.enabled||tag.size()<4||tag.rfind("sce",0)!=0||!std::isdigit(static_cast<unsigned char>(tag[3])))continue;
                const int id=std::stoi(tag.substr(3));
                CHECK(id>=0&&size_t(id)<names.size());if(id<0||size_t(id)>=names.size())continue;
                auto selected=click(m,tag);
                if(selected.type!=Type::StartScene||lower(selected.asset)!=names[size_t(id)])
                    std::cerr<<route<<"/"<<page<<" "<<tag<<": expected "<<names[size_t(id)]<<", got "<<selected.asset<<" ("<<m.error()<<")\n";
                CHECK(selected.type==Type::StartScene);CHECK(lower(selected.asset)==names[size_t(id)]);
                checked.insert(id);
                if(selected.type==Type::StartScene){
                    dc4::MesFile mes;
                    CHECK(mes.load_file(data+"/MES/"+selected.asset+".mes",&error));
                    dc4::MesVm vm(mes);vm.write_variable(9,7,1);
                    if(!selected.label.empty())CHECK(vm.jump(selected.label));
                    dc4::MesCommand command;int choices=0;
                    std::string destination;int steps=0,transfers=0;
                    for(;steps<100000;++steps){
                        if(vm.next(&command)){
                            if(command.op==0x6B)++replay_lines;
                            if(command.op==0x4F||command.op==0x50){
                                destination=lower(command.str);
                                if(destination=="sce_end")break;
                                if(++transfers>16)break;
                                if(!mes.load_file(data+"/MES/"+command.str+".mes",&error)||!vm.bind(mes))break;
                            }
                        }else if(vm.waiting_for_choice())CHECK(vm.choose(choices++%vm.choices().size()));
                        else break;
                    }
                    if(!vm.error().empty()||destination!="sce_end")
                        std::cerr<<tag<<" replay: "<<vm.error()<<" destination="<<destination<<'\n';
                    CHECK(vm.error().empty());CHECK(destination=="sce_end");CHECK(steps<100000);
                    // Exercise the same shared startup used by both hosts with
                    // one real rendered chapter per character/common tab.
                    if(!rendered){render_replay(data,m,selected,tag);rendered=true;}
                }
                m.resume_scenes();
                CHECK(m.page()==Page::Scenes);CHECK(m.variable(3,6)==page);CHECK(has(m,tag));
            }
            m.scroll(1);
        }
    }
    CHECK(checked.size()==names.size());
    std::cout<<"Scene Mode selections checked: "<<checked.size()<<", replay dialogue commands: "<<replay_lines<<'\n';
}
void movie_modal_assets(const std::string& data){
    Menu m;std::string error;CHECK(m.load(data,&error));if(!m.loaded()){std::cerr<<error<<'\n';return;}
    m.mark_music_seen("music/001");m.mark_music_seen("music/002");m.open(Page::Music);
    CHECK(m.activate("#music:1").type==Type::PlayMusic);
    m.scroll(3);
    Menu::Control close{};
    for(const auto& control:m.controls())
        if(control.enabled && control.x>1000 && control.y>600 &&
           (control.tag=="return" || control.tag=="rmain" || control.tag=="retchk"))close=control;
    CHECK(!close.tag.empty());if(close.tag.empty())return;
    const int x=close.x+close.w/2,y=close.y+close.h/2;
    dc4scene::FontRenderer font;std::vector<uint8_t> music,modal,hover,restored;
    m.draw(-1,-1,false,font,&music);
    const int selected=m.variable(99,187),playlist=m.variable(100,21),volume=m.config().music_volume;
    auto open_movie=[&](){
        CHECK(m.activate("selmovie").type==Type::None);CHECK(m.page()==Page::Music);
        for(const char* tag:{"movie01","movie02","movie03"})CHECK(has(m,tag));
        CHECK(!has(m,"#music:3"));CHECK(!has(m,"play"));CHECK(!has(m,"allon"));
        bool close_active=false;
        for(const auto& c:m.controls())
            if(c.enabled && c.tag==close.tag && c.x==close.x && c.y==close.y && c.w==close.w && c.h==close.h)close_active=true;
        CHECK(close_active);
    };
    open_movie();
    m.draw(-1,-1,false,font,&modal);m.draw(x,y,false,font,&hover);CHECK(hover!=modal);
    // Releasing a drag outside the authored button must not close the popup.
    m.pointer_down(x,y);CHECK(m.pointer_up(1279,719).type==Type::None);CHECK(has(m,"movie01"));
    // The underlying list scrollbar stays inert while the popup is open.
    m.pointer_down(1080,590);CHECK(m.pointer_up(1080,590).type==Type::None);CHECK(has(m,"movie01"));
    CHECK(m.variable(99,187)==selected && m.variable(100,21)==playlist && m.config().music_volume==volume);
    m.pointer_down(x,y);CHECK(m.pointer_up(x,y).type==Type::None);
    CHECK(m.page()==Page::Music);CHECK(!has(m,"movie01"));CHECK(has(m,"selmovie"));
    m.draw(-1,-1,false,font,&restored);CHECK(restored==music);
    if(const char* capture=std::getenv("DC4_MENU_CAPTURES")){
        std::filesystem::create_directories(capture);
        bmp(std::filesystem::path(capture)/"movie-modal.bmp",modal);
        bmp(std::filesystem::path(capture)/"movie-back-hover.bmp",hover);
        bmp(std::filesystem::path(capture)/"movie-back-music.bmp",restored);
    }
    // Platform Back retains the same one-level dismissal, and the regular
    // Music return button still reaches Extras after the popup is gone.
    open_movie();CHECK(m.back().type==Type::None);CHECK(m.page()==Page::Music);CHECK(!has(m,"movie01"));
    m.pointer_down(x,y);CHECK(m.pointer_up(x,y).type==Type::None);CHECK(m.page()==Page::Extras);
    std::cout<<"Verified Movie popup return hit target, native hover artwork and Music state restoration.\n";
}
void assets(const std::string& data){
    Menu m;std::string error;
    CHECK(m.load(data,&error));if(!m.loaded()){std::cerr<<error<<'\n';return;}
    dc4scene::FontRenderer font;
#ifdef _WIN32
    font.load("C:/Windows/Fonts/msgothic.ttc",0,22);
#endif
    std::vector<uint8_t> frame;
    const char* capture=std::getenv("DC4_MENU_CAPTURES");
    auto shot=[&](const std::string& name){m.draw(-1,-1,false,font,&frame);CHECK(frame.size()==1280u*720u*4u);if(capture){std::filesystem::create_directories(capture);bmp(std::filesystem::path(capture)/(name+".bmp"),frame);}};
    CHECK(has(m,"prologue"));CHECK(has(m,"sce_start"));CHECK(!has(m,"qload"));
    CHECK(m.activate("prologue").type==Type::None);CHECK(m.confirming());shot("confirm-new-game");
    CHECK(m.activate("extra").type==Type::None);CHECK(m.page()==Page::Title);
    CHECK(m.answer_confirmation(false).type==Type::None);CHECK(!m.confirming());
    auto start=click(m,"prologue");CHECK(start.type==Type::NewGame);CHECK(start.asset=="DC4_SEN20181213B");
    m.pointer_down(1100,220);CHECK(m.pointer_up(1100,280).type==Type::None);
    m.pointer_down(1100,220);m.cancel_pointer();CHECK(m.pointer_up(1100,220).type==Type::None);
    shot("title");
    click(m,"config");CHECK(m.page()==Page::Config);
    for(int i=0;i<7;++i){m.set_config_page(i);CHECK(m.config_page()==i);CHECK(m.controls().size()>=8);CHECK(m.error().empty());shot("config"+std::to_string(i+1));}
    m.set_config_page(2);CHECK(has(m,"music_off"));CHECK(click(m,"music_off").type==Type::SettingsChanged);CHECK(!m.config().music_enabled);
    CHECK(click(m,"music_on").type==Type::SettingsChanged);CHECK(m.config().music_enabled);
    // Native BGM slider at y=278; test all slider banks by reading state after
    // a captured drag, including release outside the original hit rectangle.
    m.pointer_down(160,296);m.pointer_move(254,296);m.pointer_up(400,296);CHECK(m.config().music_volume>=90);
    m.set_config_page(1);m.activate("mspeed--");CHECK(m.config().text_speed==7);
    CHECK(m.activate("defaultparts").type==Type::None);CHECK(m.confirming());shot("confirm-reset");
    CHECK(m.config().text_speed==7);CHECK(m.back().type==Type::None);CHECK(m.config().text_speed==7);
    m.activate("defaultparts");CHECK(m.answer_confirmation(true).type==Type::SettingsChanged);CHECK(m.config().text_speed==8);
    m.set_variable(100,109,0); // Reset Settings above restores this feature's default to enabled.
    m.open(Page::Extras);CHECK(has(m,"cgmode"));CHECK(has(m,"scemode"));CHECK(!has(m,"musmode"));
    m.set_variable(100,109,1);CHECK(has(m,"musmode"));shot("extras");
    click(m,"cgmode");CHECK(m.page()==Page::Gallery);CHECK(!has(m,"cg0"));shot("gallery-locked");
    m.mark_cg_seen("bg/dc4_ev_asa07b");m.mark_cg_seen("bg/dc4_ev_asa07a");m.open(Page::Gallery);CHECK(has(m,"cg0"));shot("gallery-unlocked");
    click(m,"cg0");shot("gallery-view");m.scroll(1);shot("gallery-variant");m.back();CHECK(has(m,"cg0"));
    for(const char* route:{"nin","sor","hyr","sii","miu","tiy","cmn","asa"}){m.activate(route);CHECK(m.page()==Page::Gallery);CHECK(m.error().empty());m.scroll(1);CHECK(m.error().empty());}
    m.open(Page::Scenes);CHECK(!has(m,"sce0"));shot("scenes-locked");
    m.mark_scene_seen("dc4_mid20190301a1");m.open(Page::Scenes);CHECK(has(m,"sce0"));shot("scenes-unlocked");
    const int selector=m.variable(99,100);
    CHECK(m.activate("sce0").type==Type::None);CHECK(m.confirming());shot("confirm-scene");
    m.scroll(1);CHECK(m.variable(3,6)==0);CHECK(m.variable(99,100)==selector);
    CHECK(m.answer_confirmation(false).type==Type::None);CHECK(has(m,"sce0"));
    auto scene=click(m,"sce0");CHECK(scene.type==Type::StartScene);CHECK(scene.asset=="DC4_MID20190301A1");
    m.open(Page::Music);CHECK(!has(m,"#music:0"));shot("music-locked");
    m.mark_music_seen("music/001");m.mark_music_seen("music/002");m.open(Page::Music);CHECK(has(m,"#music:0"));
    auto music=click(m,"#music:0");CHECK(music.type==Type::PlayMusic);CHECK(music.index==0);CHECK(music.asset=="music\\001");
    CHECK(m.activate("next").index==1);CHECK(m.activate("stop").type==Type::StopMusic);shot("music-unlocked");
    m.scroll(36);CHECK(m.error().empty());shot("music-last-page");
    m.activate("selmovie");shot("movies");CHECK(m.activate("movie01").type==Type::None);CHECK(m.confirming());shot("confirm-movie");CHECK(m.answer_confirmation(true).asset=="DC4_OP1.MPG");
}
}
int main(int argc,char** argv){
    profile();
    const auto parent=std::filesystem::canonical(std::filesystem::temp_directory_path());
    const auto scratch=parent/("arisa32-replay-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    replay_policy_fixtures(scratch);
    const char* installed=argc>1?argv[1]:std::getenv("DC4_TEST_DATA");
    if(installed){
        const std::string mode=argc>2?argv[2]:"";
        if(mode!="--movie-modal")replay_policy_assets(installed,scratch/"installed");
        if(mode!="--replay-policy")movie_modal_assets(installed);
        if(mode!="--replay-policy" && mode!="--movie-modal"){assets(installed);scene_selections(installed);}
    }else std::cout<<"Installed menu asset checks skipped (set DC4_TEST_DATA).\n";
    if(scratch.parent_path()==parent && scratch.filename().string().rfind("arisa32-replay-",0)==0)
        std::filesystem::remove_all(scratch);
    return failures?1:0;
}
