#include "arisa32/system_menu.h"
#include "arisa32/mesvm.h"
#include "arisa32/save_screen.h"
#include "arisa32/names.h"
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
void bmp(const std::filesystem::path& path,const std::vector<uint8_t>& rgba,int width=1280,int height=720){
    std::ofstream o(path,std::ios::binary);if(!o)return;
    auto u16=[&](uint16_t v){o.put(char(v));o.put(char(v>>8));};
    auto u32=[&](uint32_t v){u16(uint16_t(v));u16(uint16_t(v>>16));};
    o.put('B');o.put('M');u32(54+width*height*4);u32(0);u32(54);u32(40);u32(width);u32(uint32_t(-height));u16(1);u16(32);u32(0);u32(width*height*4);u32(0);u32(0);u32(0);u32(0);
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
struct GalleryScript {
    std::vector<uint8_t> bytes;
    explicit GalleryScript(int version):bytes{1,0,0,0,3,0,0,0,0,0,uint8_t(version),uint8_t(version>>8),0}{}
    void word(int v){bytes.push_back(uint8_t(v));bytes.push_back(uint8_t(v>>8));}
    void param(int op,int a,int b){bytes.insert(bytes.end(),{uint8_t(op),uint8_t(a),uint8_t(b)});}
    void words(int op,int a,int b,int c,int d){bytes.push_back(uint8_t(op));word(a);word(b);word(c);word(d);}
    void str(int op,const std::string& value){bytes.push_back(uint8_t(op));bytes.insert(bytes.end(),value.begin(),value.end());bytes.push_back(0);}
    void save(const std::filesystem::path& root,const std::string& name){auto out=bytes;out.push_back(0);write_bytes(root/"MES"/(name+".mes"),out);}
};
void gallery_image(const std::filesystem::path& path,int width,int height,int kind){
    std::vector<uint8_t> bytes{'C','R','X','G',0,0,0,0};
    const auto word=[&](int n){bytes.push_back(uint8_t(n));bytes.push_back(uint8_t(n>>8));};
    word(width);word(height);word(1);word(0);word(0);word(0);
    const int stride=(width*3+3)&~3;
    std::vector<uint8_t> pixels(size_t(stride)*height);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        auto* p=&pixels[size_t(y)*stride+x*3];
        if(kind==0){p[2]=x<width/2?220:40;p[1]=y<height/2?30:210;p[0]=50;}
        else if(kind==1){p[0]=200;p[1]=50;p[2]=20;}
        else if(kind==2)p[0]=p[1]=p[2]=uint8_t(x*255/(width-1));
        else p[0]=p[1]=p[2]=100;
        if(kind==0 && x>=100 && x<200 && y>=100 && y<200)p[0]=p[1]=p[2]=(x+y)%2?255:0;
    }
    for(size_t i=0;i<pixels.size();i+=8){bytes.push_back(255);bytes.insert(bytes.end(),pixels.begin()+i,pixels.begin()+std::min(i+8,pixels.size()));}
    write_bytes(path,bytes);
}
void gallery_fixtures(const std::filesystem::path& scratch){
    for(int version:{0xAAB6,0xABB6}){
        const auto root=scratch/std::to_string(version);
        GalleryScript main(version);main.str(0x4b,"main");main.str(0x49,"system/titleback02");main.param(0x0b,0,0);main.save(root,"main");
        for(const char* name:{"config","music","sce_asa"}){
            GalleryScript script(version);script.str(0x4b,std::string(name)=="config"?"menu1":"menu");script.param(0x0b,0,0);script.save(root,name);
        }
        GalleryScript gallery(version);gallery.str(0x4b,"menu");gallery.str(0x49,"system/configback01");
        gallery.words(0x72,100,100,400,400);gallery.str(0x54,"cg0");
        gallery.words(0x72,500,100,800,400);gallery.str(0x54,"cg3");gallery.param(0x0b,0,0);
        for(int i=0;i<4;++i){
            gallery.str(0x4b,"cg"+std::to_string(i));gallery.str(0x49,"*"+std::to_string(i));gallery.str(0x54,"cg_draw2");
        }
        gallery.str(0x4b,"cg_draw2");gallery.bytes.push_back(0x3c);gallery.str(21,"MSK/MASK11A");
        gallery.param(0x0f,21,5);gallery.param(0x15,0,1);gallery.str(0x54,"cg_next");
        gallery.str(0x4b,"cg_next");gallery.str(0x54,"cg0");gallery.save(root,"cg_asa");
        std::vector<uint8_t> names(228);names[0]=7;
        const auto number=[&](int n){for(int i=0;i<4;++i)names.push_back(uint8_t(n>>(8*i)));};
        const auto name=[&](const std::string& text){const size_t at=names.size();names.resize(at+32);std::copy(text.begin(),text.end(),names.begin()+at);};
        number(0);number(0);number(5);for(int i=0;i<5;++i)name("BG/test"+std::to_string(i));
        number(1);name("DC4_SEN20181213B");write_bytes(root/"GRP/NAMES.DAT",names);
        for(const char* image:{"titleback02","titlechip01","configback01","configchip01","exchip"})
            gallery_image(root/"GRP/system"/(std::string(image)+".crx"),8,8,3);
        gallery_image(root/"GRP/BG/test0.crx",1920,1080,0);
        gallery_image(root/"GRP/BG/test1.crx",2560,1440,1);
        gallery_image(root/"GRP/BG/test3.crx",720,1080,0);
        gallery_image(root/"GRP/MSK/MASK11A.crx",1280,720,2);
        Menu menu;std::string error;CHECK(menu.load(root.string(),&error));
        for(int id:{0,1,3,4})menu.set_variable(97,id,1);
        menu.set_visual_effects_enabled(false);menu.open(Page::Gallery);
        dc4scene::FontRenderer font;
        const auto frame=[&](){std::vector<uint8_t> pixels;menu.draw(-1,-1,false,font,&pixels);return pixels;};
        const auto pixel=[](const std::vector<uint8_t>& f,int x,int y,int c){return f[(size_t(y)*1280+x)*4+c];};
        std::vector<uint8_t> hd;
        CHECK(!menu.draw_gallery(1920,1080,&hd));
        const auto hd_pixel=[&](int x,int y,int c){return hd[(size_t(y)*1920+x)*4+c];};
        const auto gallery_frame=frame();click(menu,"cg0");CHECK(menu.gallery_preview_active());
        CHECK(frame()==gallery_frame);menu.update(500);auto halfway=frame();
        CHECK(pixel(halfway,10,100,0)>200);CHECK(pixel(halfway,1270,100,0)>90);
        menu.update(500);const auto first=frame();
        CHECK(!menu.draw_gallery(0,1080,&hd));CHECK(!menu.draw_gallery(1920,720,&hd));
        CHECK(!menu.draw_gallery(7680,4320,&hd));CHECK(!menu.draw_gallery(1920,1080,nullptr));
        CHECK(menu.draw_gallery(1920,1080,&hd));CHECK(hd.size()==1920*1080*4);
        bool detail=true;
        for(int y=100;y<200;++y)for(int x=100;x<200;++x)
            detail &= hd_pixel(x,y,0)==((x+y)%2?255:0);
        CHECK(detail);
        const auto original_hd=hd;
        CHECK(menu.draw_gallery(1280,720,&hd));
        CHECK(hd[(size_t(100)*1280+100)*4]>=95 && hd[(size_t(100)*1280+100)*4]<=160);
        CHECK(menu.draw_gallery(1920,1080,&hd));CHECK(hd==original_hd);
        CHECK(pixel(first,10,10,0)==220);CHECK(pixel(first,1270,10,0)==40);
        CHECK(pixel(first,10,710,1)==210);CHECK(pixel(first,1270,710,1)==210);
        menu.pointer_down(640,360);menu.cancel_pointer();menu.pointer_up(640,360);CHECK(frame()==first);
        menu.scroll(1);CHECK(menu.variable(99,100)==1);const auto black=frame();CHECK(pixel(black,640,360,2)==0);
        menu.update(500);halfway=frame();CHECK(pixel(halfway,10,360,2)>190);CHECK(pixel(halfway,1270,360,2)<10);
        CHECK(menu.draw_gallery(1920,1080,&hd));CHECK(hd_pixel(15,540,2)>190);CHECK(hd_pixel(1905,540,2)<10);
        menu.scroll(1);CHECK(menu.variable(99,100)==1);CHECK(pixel(frame(),1270,360,2)==200);
        menu.scroll(1);CHECK(menu.variable(99,100)==3);menu.update(1000);const auto portrait=frame();
        CHECK(pixel(portrait,10,360,0)==0);CHECK(pixel(portrait,1270,360,0)==0);
        CHECK(pixel(portrait,410,10,0)==220);CHECK(pixel(portrait,870,710,1)==210);
        CHECK(menu.draw_gallery(1920,1080,&hd));CHECK(hd_pixel(599,540,0)==0);CHECK(hd_pixel(1320,540,0)==0);
        CHECK(hd_pixel(600,10,0)==220);CHECK(hd_pixel(1319,1070,1)==210);
        menu.scroll(1);CHECK(menu.variable(99,100)==0);menu.update(1000);CHECK(frame()==first);
        menu.scroll(-1);CHECK(menu.variable(99,100)==3);menu.back();CHECK(!menu.gallery_preview_active());CHECK(frame()==gallery_frame);
        CHECK(!menu.draw_gallery(1920,1080,&hd));
        click(menu,"cg0");menu.back();menu.update(1000);CHECK(frame()==gallery_frame);
        menu.set_variable(99,10,0);click(menu,"cg0");CHECK(frame()==first);
        menu.scroll(1);CHECK(pixel(frame(),1270,360,2)==200);menu.back();
        for(int speed:{0,5,11}){
            menu.set_variable(99,10,1);menu.set_variable(99,12,speed);frame();click(menu,"cg0");
            menu.update(999);CHECK(frame()!=first);menu.update(1);CHECK(frame()==first);menu.back();
        }
        gallery_image(root/"GRP/BG/test0.crx",1920,1080,1);
        CHECK(menu.load(root.string(),&error));menu.set_variable(97,0,1);menu.set_variable(99,10,0);
        menu.open(Page::Gallery);click(menu,"cg0");
        CHECK(menu.draw_gallery(1920,1080,&hd));CHECK(hd_pixel(120,120,2)==200);
        std::cout<<"Gallery fit, mask, unlock sequence, rapid taps, cancel, Back and settings passed: "<<std::hex<<version<<std::dec<<'\n';
    }
}
void gallery_assets(const std::string& data){
    dc4::GameNames names;CHECK(dc4::decode_game_names(read_bytes(std::filesystem::path(data)/"GRP/NAMES.DAT"),&names));
    Menu menu;std::string error;CHECK(menu.load(data,&error));
    for(int id:{0,1,3})menu.set_variable(97,id,1);
    menu.set_visual_effects_enabled(false);menu.open(Page::Gallery);
    dc4scene::FontRenderer font;std::vector<uint8_t> frame;
    menu.draw(-1,-1,false,font,&frame);click(menu,"cg0");
    for(int id:{0,1,3}){
        CHECK(menu.variable(99,100)==id);
        menu.update(500);menu.draw(-1,-1,false,font,&frame);
        if(const char* capture=std::getenv("DC4_MENU_CAPTURES")){
            std::filesystem::create_directories(capture);
            bmp(std::filesystem::path(capture)/("cg"+std::to_string(id)+"-half.bmp"),frame);
        }
        menu.update(500);menu.draw(-1,-1,false,font,&frame);
        const auto bytes=read_bytes(std::filesystem::path(data)/"GRP"/(names.cg[id]+".CRX"));
        dc4::CrxInfo info;std::vector<uint8_t> original;
        CHECK(dc4::crx_decode(bytes.data(),bytes.size(),&original,&info));
        CHECK(int(info.width)*720==int(info.height)*1280);
        std::vector<uint8_t> hd;CHECK(menu.draw_gallery(1920,1080,&hd));
        bool exact=info.width==1920 && info.height==1080 && hd.size()==original.size();
        if(exact)for(size_t i=0;i<original.size();i+=4)for(int c=0;c<3;++c)exact &= hd[i+c]==original[i+c];
        CHECK(exact);
        if(const char* capture=std::getenv("DC4_MENU_CAPTURES"))bmp(std::filesystem::path(capture)/("cg"+std::to_string(id)+"-hd.bmp"),hd,1920,1080);
        if(!original.empty())for(int y:{0,100,360,719})for(int x:{0,100,640,1279})for(int c=0;c<3;++c)
            CHECK(frame[(size_t(y)*1280+x)*4+c]==original[(size_t(y*info.height/720)*info.width+x*info.width/1280)*4+c]);
        if(const char* capture=std::getenv("DC4_MENU_CAPTURES"))bmp(std::filesystem::path(capture)/("cg"+std::to_string(id)+"-full.bmp"),frame);
        menu.scroll(1);
    }
    CHECK(menu.variable(99,100)==0);menu.back();CHECK(!menu.gallery_preview_active());
    std::cout<<"Original CG pixels match full-image scaling and route order: "<<data<<'\n';
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
void confirmations(const std::string& data, const std::filesystem::path& scratch) {
    using Kind = dc4ui::ConfirmKind;
    Menu menu; std::string error;
    CHECK(menu.load(data,&error));
    if (!menu.loaded()) { std::cerr<<error<<'\n'; return; }
    menu.open(Page::Config); menu.set_config_page(4);
    dc4ui::ConfirmDialog quick;
    dc4ui::SaveScreen saves;
    CHECK(saves.load(data,&error));
    menu.bind_confirmations(quick);
    menu.bind_confirmations(saves.confirm_dialog());
    const auto path = (scratch/"confirmations.profile").string();
    std::filesystem::create_directories(scratch);
    int writes = 0;
    menu.set_confirmation_changed_callback([&] { ++writes; CHECK(menu.save_profile(path,&error)); });
    const std::pair<Kind,int> kinds[] = {
        {Kind::Quit,0},{Kind::SceneJump,1},{Kind::Title,4},{Kind::ReplayExit,6},
        {Kind::Save,7},{Kind::Overwrite,7},{Kind::QuickSave,7},
        {Kind::Load,8},{Kind::QuickLoad,8},{Kind::ResetSettings,10},{Kind::SceneReplay,13}
    };
    CHECK(click(menu,"yesnoalloff").type==Type::SettingsChanged);
    for (const auto& entry:kinds) CHECK(!quick.open(entry.first));
    CHECK(click(menu,"yesnoallon").type==Type::SettingsChanged);
    for (const auto& entry:kinds) { CHECK(quick.open(entry.first)); quick.answer(false); }
    const std::pair<const char*,Kind> switches[] = {
        {"yesno01",Kind::Quit},{"yesno03",Kind::Title},{"yesno04",Kind::QuickSave},
        {"yesno05",Kind::QuickLoad},{"yesno06",Kind::SceneJump},
        {"yesno07",Kind::SceneReplay},{"yesno08",Kind::ReplayExit},{"yesno09",Kind::ResetSettings}
    };
    for (const auto& entry:switches) {
        CHECK(click(menu,entry.first).type==Type::SettingsChanged);
        CHECK(!quick.open(entry.second));
        CHECK(click(menu,entry.first).type==Type::SettingsChanged);
        CHECK(quick.open(entry.second)); quick.answer(false);
    }
    CHECK(quick.open(Kind::QuickSave)); quick.set_dont_confirm(true); quick.answer(false);
    CHECK(menu.variable(93,7)==0 && writes==0);
    CHECK(quick.open(Kind::QuickSave));
    quick.pointer_down(580,440); quick.pointer_up(580,440);
    CHECK(quick.dont_confirm());
    quick.pointer_down(520,380); CHECK(quick.pointer_up(520,380)==dc4ui::ConfirmResult::Yes);
    CHECK(menu.variable(93,7)==1 && writes==1);
    CHECK(!quick.open(Kind::QuickSave));
    CHECK(!saves.confirm_dialog().open(Kind::Overwrite));
    CHECK(saves.confirm_dialog().open(Kind::Load)); saves.answer_confirmation(false);
    CHECK(saves.activate({dc4ui::SaveActionType::Slot,0}).type==dc4ui::SaveActionType::Slot);
    CHECK(click(menu,"yesno04").type==Type::SettingsChanged);
    CHECK(saves.activate({dc4ui::SaveActionType::Slot,0}).type==dc4ui::SaveActionType::None);
    CHECK(saves.confirming()); saves.answer_confirmation(false);
    Menu restarted; CHECK(restarted.load(data,&error)); CHECK(restarted.load_profile(path,&error));
    dc4ui::ConfirmDialog restored; restarted.bind_confirmations(restored);
    CHECK(!restored.open(Kind::QuickSave));
    CHECK(restored.open(Kind::Load)); restored.answer(false);
    menu.set_variable(93,0,0); menu.open(Page::Title);
    CHECK(menu.activate("end").type==Type::None && menu.confirming());
    menu.pointer_down(580,440); menu.pointer_up(580,440);
    menu.pointer_down(520,380); CHECK(menu.pointer_up(520,380).type==Type::Quit);
    CHECK(writes==2 && menu.variable(93,0)==1);
    CHECK(!quick.open(Kind::Quit));
    menu.open(Page::Config); menu.set_config_page(4); click(menu,"yesnoallon");
    CHECK(quick.open(Kind::Quit)); quick.answer(false);
    menu.activate("yesnoalloff");
    CHECK(menu.activate("defaultparts").type==Type::SettingsChanged);
    CHECK(quick.open(Kind::QuickSave)); quick.answer(false);
    Menu unrelated;
    unrelated.set_variable(93,7,1); unrelated.bind_confirmations(restored);
    CHECK(restored.open(Kind::QuickSave)); restored.answer(false);
    std::cout<<"Confirmation switches, shared checkbox policy, cancel, persistence and reset passed: "<<data<<'\n';
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
    m.set_variable(100,109,0);
    m.open(Page::Extras);CHECK(has(m,"cgmode"));CHECK(has(m,"scemode"));CHECK(!has(m,"musmode"));
    m.set_variable(100,109,1);CHECK(has(m,"musmode"));shot("extras");
    click(m,"cgmode");CHECK(m.page()==Page::Gallery);CHECK(!has(m,"cg0"));shot("gallery-locked");
    m.mark_cg_seen("bg/dc4_ev_asa07b");m.mark_cg_seen("bg/dc4_ev_asa07a");m.open(Page::Gallery);CHECK(has(m,"cg0"));shot("gallery-unlocked");
    CHECK(!m.gallery_preview_active());
    click(m,"cg0");CHECK(m.gallery_preview_active());shot("gallery-view");
    m.scroll(1);CHECK(m.gallery_preview_active());shot("gallery-variant");
    m.back();CHECK(!m.gallery_preview_active());CHECK(m.page()==Page::Gallery);CHECK(has(m,"cg0"));
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
void progression(const std::string& data,const std::filesystem::path& scratch){
    std::filesystem::create_directories(scratch);
    Menu menu;std::string error;CHECK(menu.load(data,&error));
    CHECK(!has(menu,"dc"));CHECK(menu.activate("dc").type==Type::None);CHECK(!menu.confirming());
    for(int i=100;i<150;++i)CHECK(menu.variable(100,i)==0);
    dc4::MesFile ending;CHECK(ending.load_file(data+"/MES/ed.mes",&error));
    for(unsigned mask=0;mask<256;++mask){
        dc4::MesVm vm(ending);
        for(int i=0;i<8;++i)vm.write_variable(100,101+i,(mask>>i)&1);
        dc4::MesCommand command;int steps=0;
        while(vm.next(&command)&&++steps<1000){}
        CHECK(steps<1000 && vm.error().empty() && vm.transferring());
        CHECK(vm.read_variable(100,100)==int(mask==255));
    }
    // Nino and Hiyori set 101 and 103. Neither opening Extra nor restarting
    // with this profile may promote the other six route completions.
    menu.set_variable(100,101,1);menu.set_variable(100,103,1);
    menu.set_variable(100,110,1);menu.set_variable(100,112,1);
    menu.open(Page::Extras);menu.open(Page::Scenes);menu.open(Page::Title);
    CHECK(!has(menu,"dc") && menu.activate("dc").type==Type::None);
    const auto two_routes=(scratch/"two-routes.dat").string();
    CHECK(menu.save_profile(two_routes,&error));
    Menu two;CHECK(two.load(data,&error));CHECK(two.load_profile(two_routes,&error));
    CHECK(!has(two,"dc") && two.variable(100,101)==1 && two.variable(100,103)==1);
    for(int i:{102,104,105,106,107,108})CHECK(two.variable(100,i)==0);
    const auto stale_path=(scratch/"stale-dc.dat").string();
    {std::ofstream stale(stale_path,std::ios::binary);stale<<"DC4ENGINE_PROFILE 1\nv 100 100 1\nv 100 101 1\nv 100 103 1\n";}
    Menu stale;CHECK(stale.load(data,&error));CHECK(stale.load_profile(stale_path,&error));
    CHECK(stale.variable(100,100)==0 && !has(stale,"dc"));
    CHECK(stale.activate("dc").type==Type::None && !stale.confirming());
    dc4scene::SceneRunner polluted(data);CHECK(polluted.load_script(data+"/MES/main.mes"));
    for(int i=100;i<120;++i)polluted.set_variable(100,uint16_t(i),1);
    polluted.set_variable(5,2,7);polluted.set_variable(9,8,42);
    std::vector<uint8_t> polluted_slot;CHECK(polluted.capture_state(&polluted_slot));
    CHECK(polluted.restore_state(polluted_slot));stale.restore_scene_progress(polluted);
    for(int i=100;i<120;++i)CHECK(polluted.variable(100,uint16_t(i))==int(i==101 || i==103));
    CHECK(polluted.variable(5,2)==7 && polluted.variable(9,8)==42);
    CHECK(!stale.sync_progress(polluted));CHECK(!stale.sync_progress(polluted));
    CHECK(stale.save_profile(stale_path,&error));
    {std::ifstream saved(stale_path);std::string contents((std::istreambuf_iterator<char>(saved)),{});
        CHECK(contents.find("v 100 100 0\n")!=std::string::npos);}
    Menu repaired;CHECK(repaired.load(data,&error));CHECK(repaired.load_profile(stale_path,&error));
    CHECK(!has(repaired,"dc") && repaired.variable(100,101)==1 && repaired.variable(100,103)==1);
    // A stale native D.C. flag cannot replace any one of the eight endings.
    for(int missing=101;missing<=108;++missing){
        std::vector<uint8_t> native(0x4B80);
        for(int i=100;i<=108;++i)native[0xD8+i]=i==missing?0:1;
        write_bytes(scratch/"stale-system.dat",native);
        Menu imported;CHECK(imported.load(data,&error));
        CHECK(imported.load_system_dat((scratch/"stale-system.dat").string(),&error));
        CHECK(imported.variable(100,100)==0 && !has(imported,"dc"));
        CHECK(imported.activate("dc").type==Type::None && !imported.confirming());
    }
    for(int i=101;i<=108;++i)repaired.set_variable(100,i,1);
    CHECK(!has(repaired,"dc"));
    repaired.set_variable(100,100,1);CHECK(has(repaired,"dc"));
    repaired.activate("dc");CHECK(repaired.confirming());
    repaired.set_variable(100,108,0);
    CHECK(repaired.answer_confirmation(true).type==Type::None);
    for(int i=100;i<150;++i)menu.set_variable(100,i,0);
    const auto end_y=[&](){for(const auto& c:menu.controls())if(c.tag=="end")return c.y;return -1;};
    CHECK(end_y()==536);
    dc4scene::FontRenderer font;std::vector<uint8_t> locked,unlocked;
    menu.draw(-1,-1,false,font,&locked);
    const auto path=(scratch/"profile.dat").string();
    for(bool complete:{false,true}){
        for(int i=100;i<150;++i)menu.set_variable(100,i,complete?1:0);
        CHECK(menu.save_profile(path,&error));
        Menu restored;CHECK(restored.load(data,&error));CHECK(restored.load_profile(path,&error));
        CHECK(has(restored,"dc")==complete);
        dc4scene::SceneRunner scene(data);CHECK(scene.load_script(data+"/MES/main.mes"));
        restored.initialize_scene(scene,false);
        CHECK(scene.variable(100,100)==int(complete));CHECK(scene.variable(100,112)==int(complete));
        CHECK(!restored.sync_progress(scene));
        scene.set_variable(5,2,7);scene.set_variable(100,112,complete?0:1);
        std::vector<uint8_t> old_slot;CHECK(scene.capture_state(&old_slot));
        CHECK(scene.restore_state(old_slot));restored.restore_scene_progress(scene);
        CHECK(scene.variable(100,112)==int(complete));CHECK(scene.variable(5,2)==7);
        CHECK(!restored.sync_progress(scene));CHECK(restored.variable(100,112)==int(complete));
        restored.open(Page::Config);restored.set_variable(93,19,1);
        restored.activate("defaultparts");if(restored.confirming())restored.answer_confirmation(true);
        for(int i=100;i<150;++i)CHECK(restored.variable(100,i)==int(complete));
        restored.open(Page::Title);CHECK(has(restored,"dc")==complete);
    }
    menu.open(Page::Title);CHECK(end_y()==592);CHECK(has(menu,"dc"));
    menu.draw(-1,-1,false,font,&unlocked);CHECK(locked!=unlocked);
    CHECK(click(menu,"dc").asset=="DC4_DC99990101A");
    if(const char* capture=std::getenv("DC4_MENU_CAPTURES")){
        std::filesystem::create_directories(capture);
        bmp(std::filesystem::path(capture)/"title-locked.bmp",locked);
        bmp(std::filesystem::path(capture)/"title-complete.bmp",unlocked);
    }
    for(bool complete:{false,true}){
        std::vector<uint8_t> native(0x4B80);
        for(int i=100;i<150;++i)native[0xD8+i]=complete?1:0;
        write_bytes(scratch/"system.dat",native);
        Menu imported;CHECK(imported.load(data,&error));
        CHECK(imported.load_system_dat((scratch/"system.dat").string(),&error));
        CHECK(has(imported,"dc")==complete);CHECK(imported.variable(100,112)==int(complete));
        {std::ofstream old(path,std::ios::binary);old<<"DC4ENGINE_PROFILE 1\nv 100 100 0\nv 100 112 0\n";}
        CHECK(imported.load_profile(path,&error));
        CHECK(has(imported,"dc")==complete);CHECK(imported.variable(100,112)==int(complete));
        CHECK(imported.save_profile(path,&error));
        Menu restarted;CHECK(restarted.load(data,&error));CHECK(restarted.load_profile(path,&error));
        CHECK(has(restarted,"dc")==complete);
    }
    Menu partial;CHECK(partial.load(data,&error));
    dc4scene::SceneRunner story(data);CHECK(story.load_script(data+"/MES/main.mes"));
    partial.initialize_scene(story,false);
    story.set_variable(100,101,1);CHECK(partial.sync_progress(story));
    CHECK(partial.save_profile(path,&error));
    Menu resumed;CHECK(resumed.load(data,&error));CHECK(resumed.load_profile(path,&error));
    CHECK(resumed.variable(100,101)==1);CHECK(resumed.variable(100,102)==0);CHECK(!has(resumed,"dc"));
    story.set_variable(100,101,0);resumed.restore_scene_progress(story);
    CHECK(story.variable(100,101)==1);CHECK(story.variable(100,112)==0);
    std::cout<<"Fresh/completed progression, profile restart, settings reset and PC import checked: "<<data<<'\n';
}
void ambient_menu(const std::string& data,const std::filesystem::path& scratch){
    Menu menu; std::string error;
    CHECK(menu.load(data,&error));
    dc4scene::FontRenderer font;
    std::vector<uint8_t> first,moving,hidden,resumed;
    menu.draw(-1,-1,false,font,&first);
    menu.update(3200); menu.draw(-1,-1,false,font,&moving);
    CHECK(first != moving);
    std::filesystem::create_directories(scratch);
    bmp(scratch/"title-on.bmp",moving);
    menu.set_visual_effects_enabled(false); menu.draw(-1,-1,false,font,&hidden);
    CHECK(hidden != moving);
    menu.update(1000); menu.draw(-1,-1,false,font,&resumed); CHECK(hidden == resumed);
    bmp(scratch/"title-off.bmp",hidden);
    if (const char* capture = std::getenv("DC4_MENU_CAPTURES")) {
        std::filesystem::create_directories(capture);
        bmp(std::filesystem::path(capture)/"title-on.bmp",moving);
        bmp(std::filesystem::path(capture)/"title-off.bmp",hidden);
    }
    menu.set_visual_effects_enabled(true); menu.draw(-1,-1,false,font,&resumed); CHECK(moving == resumed);
    menu.update(400); menu.draw(-1,-1,false,font,&resumed); CHECK(moving != resumed);
    menu.set_variable(99,10,0); menu.draw(-1,-1,false,font,&resumed); CHECK(hidden == resumed);
    std::cout<<"Title sakura animation and On/Off checked: "<<data<<'\n';
}
void map_progression(const std::string& data){
    const char* maps[]={"dc4_kyo20190208c1_m0","dc4_kyo20190209b_m0",
        "dc4_kyo20190211b_m0","dc4_kyo20190212c_m0"};
    dc4scene::FontRenderer font;
#ifdef _WIN32
    font.load("C:/Windows/Fonts/msgothic.ttc",0,22);
#endif
    for(int map=0;map<4;++map)for(bool unlocked:{false,true}){
        dc4scene::SceneRunner scene(data);scene.set_font(&font);scene.set_animation_enabled(false);
        CHECK(scene.load_sheets(data+"/MES/start.mes"));
        CHECK(scene.load_script(data+"/MES/"+maps[map]+".mes"));
        scene.set_variable(100,112,unlocked?1:0);
        for(int steps=0;steps<1000 && !scene.waiting_for_choice() && !scene.scene_ended();++steps){
            scene.advance_to_next_text();
            if(scene.waiting_for_time())scene.finish_script_wait();
            if(!scene.pending_movie().empty())scene.finish_movie();
        }
        CHECK(scene.last_error().empty());CHECK(scene.waiting_for_choice());
        CHECK(scene.choices().size()==(unlocked?4u:3u));
        std::set<int> targets;
        for(int y=0;y<720;++y)for(int x:{0,640,1279}){
            const int choice=scene.choice_at(x,y);
            CHECK(choice<0 || size_t(choice)<scene.choices().size());
            if(choice>=0)targets.insert(choice);
        }
        CHECK(targets.size()==scene.choices().size());
        scene.draw(-1);
        if(const char* capture=std::getenv("DC4_MENU_CAPTURES")){
            std::filesystem::create_directories(capture);
            bmp(std::filesystem::path(capture)/("map"+std::to_string(map)+(unlocked?"-unlocked.bmp":"-locked.bmp")),scene.frame());
        }
    }
    std::cout<<"Four map screens: locked/unlocked choices and touch targets checked: "<<data<<'\n';
}
}
int main(int argc,char** argv){
    profile();
    const auto parent=std::filesystem::canonical(std::filesystem::temp_directory_path());
    const auto scratch=parent/("arisa32-replay-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    replay_policy_fixtures(scratch);
    if(argc==1)for(const char* variable:{"DC4_DATA","DC4PH_DATA"})
        if(const char* data=std::getenv(variable))progression(data,scratch/variable);
    const char* installed=argc>1?argv[1]:std::getenv("DC4_TEST_DATA");
    if(installed){
        const std::string mode=argc>2?argv[2]:"";
        if(mode=="--map-progression") map_progression(installed);
        else if(mode=="--progression") progression(installed,scratch/"progression");
        else if(mode=="--sakura") ambient_menu(installed,scratch/"sakura");
        else if(mode=="--confirmations") confirmations(installed,scratch/"confirmations");
        else if(mode=="--menus") assets(installed);
        else if(mode=="--gallery") {gallery_fixtures(scratch/"gallery");gallery_assets(installed);}
        else {
            if(mode!="--movie-modal")replay_policy_assets(installed,scratch/"installed");
            if(mode!="--replay-policy")movie_modal_assets(installed);
            if(mode!="--replay-policy" && mode!="--movie-modal"){assets(installed);scene_selections(installed);}
        }
    }else std::cout<<"Installed menu asset checks skipped (set DC4_TEST_DATA).\n";
    if(scratch.parent_path()==parent && scratch.filename().string().rfind("arisa32-replay-",0)==0)
        std::filesystem::remove_all(scratch);
    return failures?1:0;
}
