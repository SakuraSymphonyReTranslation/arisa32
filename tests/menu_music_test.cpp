#include "arisa32/system_menu.h"
#include "arisa32/resource.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>

namespace {
using Menu=dc4ui::SystemMenu;
using Page=dc4ui::SystemMenuPage;
using Type=dc4ui::SystemMenuActionType;
int failures=0;
#define CHECK(x) do { if (!(x)) { std::cerr<<__LINE__<<": "<<#x<<'\n'; ++failures; } } while(false)
bool has(const Menu& m,const std::string& tag) {
    for(const auto& c:m.controls())if(c.tag==tag&&c.enabled)return true;
    return false;
}
void capture_frame(const std::vector<uint8_t>& rgba,const std::string& name) {
    if(const char* directory=std::getenv("MENU_REGRESSION_CAPTURES")) {
        std::filesystem::create_directories(directory);
        std::ofstream out(std::filesystem::path(directory)/(name+".ppm"),std::ios::binary);
        out<<"P6\n1280 720\n255\n";
        for(size_t i=0;i<rgba.size();i+=4)out.write(reinterpret_cast<const char*>(rgba.data()+i),3);
    }
}
void capture(Menu& menu,const std::string& name) {
    dc4scene::FontRenderer font;std::vector<uint8_t> rgba;
    menu.draw(-1,-1,false,font,&rgba);
    capture_frame(rgba,name);
}
size_t row_difference(const std::vector<uint8_t>& a,const std::vector<uint8_t>& b,
                      const Menu::Control& row) {
    size_t changed=0;
    for(int y=row.y+4;y<row.y+40;++y)for(int x=row.x+12;x<row.x+530;++x) {
        const size_t p=(size_t(y)*1280+x)*4;
        if(a[p]!=b[p] || a[p+1]!=b[p+1] || a[p+2]!=b[p+2])++changed;
    }
    return changed;
}
void all_rows(const std::string& data,int count,const std::string& font_path,
              const std::string& profile) {
    Menu locked,unlocked,actual;std::string error;
    for(auto* menu:{&locked,&unlocked,&actual}) {
        CHECK(menu->load(data,&error));if(!menu->loaded()){std::cerr<<error;return;}
        menu->set_visual_effects_enabled(false);
    }
    if(!profile.empty())CHECK(actual.load_profile(profile,&error));
    for(int i=1;i<=count;++i) {
        char name[32];std::snprintf(name,sizeof(name),"music/%03d",i);
        unlocked.mark_music_seen(name);
    }
    for(auto* menu:{&locked,&unlocked,&actual})menu->open(Page::Music);
    dc4scene::FontRenderer font,no_font;
    CHECK(font.load(font_path,0,24));
    std::set<int> rows,open_rows;
    for(int top=0;top<count;top+=6) {
        for(auto* menu:{&locked,&unlocked,&actual}) {menu->scroll(-1000);menu->scroll(top);}
        std::vector<uint8_t> hidden,marked,titles,current;
        locked.draw(-1,-1,false,no_font,&hidden);
        locked.draw(-1,-1,false,font,&marked);
        unlocked.draw(-1,-1,false,font,&titles);
        actual.draw(-1,-1,false,font,&current);
        capture_frame(current,"profile-page-"+std::to_string(top/6+1));
        capture_frame(marked,"locked-page-"+std::to_string(top/6+1));
        capture_frame(titles,"unlocked-page-"+std::to_string(top/6+1));
        for(const auto& row:actual.controls()) {
            if(row.tag.rfind("#music:",0)!=0 || row.tag=="#music:scroll")continue;
            const int id=std::stoi(row.tag.substr(7));
            CHECK(id>=0 && id<count);rows.insert(id);
            char asset[32];std::snprintf(asset,sizeof(asset),"music/%03d",id+1);
            CHECK(row.enabled==actual.music_seen(asset));
            CHECK(row_difference(hidden,marked,row)>100);
            CHECK(row_difference(hidden,titles,row)>100);
            CHECK(row_difference(marked,titles,row)>100);
            CHECK(row_difference(current,row.enabled?titles:marked,row)==0);
            CHECK(locked.activate(row.tag).type==Type::None);
            const auto play=unlocked.activate(row.tag);
            std::string expected(asset);expected[5]='\\';
            CHECK(play.type==Type::PlayMusic && play.index==id && play.asset==expected);
            if(row.enabled)open_rows.insert(id);
            else {
                actual.pointer_down(row.x+50,row.y+20);
                CHECK(actual.pointer_up(row.x+50,row.y+20).type==Type::None);
                CHECK(!actual.music_seen(asset));
            }
        }
    }
    CHECK(int(rows.size())==count);
    std::cout<<"Rendered every row: "<<rows.size()<<" titles, locked markers and playback mappings; profile "
             <<open_rows.size()<<" unlocked, "<<rows.size()-open_rows.size()<<" locked.\n";
}
void voice_events(Menu& menu) {
    menu.set_variable(100,56,0);
    for(const auto& item:{std::pair<Page,const char*>{Page::Extras,"010"},
                         {Page::Config,"014"},{Page::Gallery,"015"},
                         {Page::Music,"016"},{Page::Scenes,"017"}}) {
        menu.open(item.first);
        CHECK(menu.system_voice(menu.page_voice_event())==std::string("VOICE\\SYSTEM\\SYSTEM_ASA")+item.second);
    }
}
void catalogue(const std::string& data,int count) {
    Menu m;std::string error;
    CHECK(m.load(data,&error));if(!m.loaded()){std::cerr<<error;return;}
    voice_events(m);
    m.open(Page::Music);CHECK(!has(m,"#music:0"));
    m.mark_music_seen("music/000");m.open(Page::Music);CHECK(!has(m,"#music:0"));
    dc4scene::FontRenderer font;std::vector<uint8_t> locked,unlocked;
    m.draw(-1,-1,false,font,&locked);
    m.mark_music_seen("music/001");m.open(Page::Music);
    CHECK(has(m,"#music:0"));CHECK(!has(m,"#music:1"));
    m.draw(-1,-1,false,font,&unlocked);
    // The formerly blank first title must be drawn, independently of focus.
    size_t changed=0;
    for(int y=216;y<272;++y)for(int x=260;x<780;++x) {
        const size_t p=(size_t(y)*1280+x)*4;
        if(locked[p]!=unlocked[p] || locked[p+1]!=unlocked[p+1] || locked[p+2]!=unlocked[p+2])++changed;
    }
    CHECK(changed>100);
    auto first=m.activate("#music:0");
    CHECK(first.type==Type::PlayMusic && first.index==0 && first.asset=="music\\001");
    m.mark_music_seen("music/002");
    auto second=m.activate("next");CHECK(second.index==1 && second.asset=="music\\002");
    CHECK(m.activate("back").asset=="music\\001");
    const std::string last_asset=count==43?"music/043":"music/042";
    const std::string last_tag="#music:"+std::to_string(count-1);
    m.mark_music_seen(last_asset);m.open(Page::Music);
    capture(m,"music-first");
    m.pointer_down(1080,217);m.pointer_move(1080,400);m.pointer_move(1080,600);m.pointer_up(1080,600);
    CHECK(has(m,last_tag));
    m.activate("allon");
    auto last=m.activate(last_tag);
    CHECK(last.type==Type::PlayMusic && last.index==count-1);
    CHECK(last.asset==std::string("music\\")+(count==43?"043":"042"));
    CHECK(m.activate("next").asset=="music\\001");
    CHECK(m.activate("back").index==count-1);
    // Selecting only the last row must also work through playlist/random mode.
    m.activate("alloff");m.activate("flag06");m.set_variable(100,21,1);
    m.set_variable(100,26,2);
    for(int i=0;i<16;++i)CHECK(m.activate("next").index==count-1);
    m.scroll(1000);CHECK(has(m,last_tag));capture(m,"music-last");
    m.scroll(-1000);CHECK(has(m,"#music:0"));
    dc4scene::SceneRunner scene(data);
    CHECK(scene.load_script(data+"/MES/main.mes"));
    m.initialize_scene(scene,true);CHECK(!scene.save_load_allowed());
    m.initialize_scene(scene,false);CHECK(scene.save_load_allowed());
    std::cout<<"Checked "<<count<<"-row catalogue bounds, first/last playback, playlist, scrolling and page voices.\n";
}
}
int main(int argc,char** argv) {
    Menu empty;voice_events(empty);
    if(argc>2) {
        dc4::configure_resources(argv[1],argc>5?argv[5]:"");
        catalogue(argv[1],std::stoi(argv[2]));
        if(argc>3)all_rows(argv[1],std::stoi(argv[2]),argv[3],argc>4?argv[4]:"");
    }
    else std::cout<<"Installed catalogue checks skipped (arguments: AdvData row-count).\n";
    return failures?1:0;
}
