#include "arisa32/system_menu.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

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
void capture(Menu& menu,const std::string& name) {
    if(const char* directory=std::getenv("MENU_REGRESSION_CAPTURES")) {
        std::filesystem::create_directories(directory);
        dc4scene::FontRenderer font;std::vector<uint8_t> rgba;
        menu.draw(-1,-1,false,font,&rgba);
        std::ofstream out(std::filesystem::path(directory)/(name+".ppm"),std::ios::binary);
        out<<"P6\n1280 720\n255\n";
        for(size_t i=0;i<rgba.size();i+=4)out.write(reinterpret_cast<const char*>(rgba.data()+i),3);
    }
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
    // Modern replay persistence retains its prior behavior.
    dc4scene::SceneRunner scene(data);
    CHECK(scene.load_script(data+"/MES/main.mes"));
    m.initialize_scene(scene,true);CHECK(scene.save_load_allowed());
    std::cout<<"Checked "<<count<<" music rows, first/last playback, playlist, scrolling and page voices.\n";
}
}
int main(int argc,char** argv) {
    Menu empty;voice_events(empty);
    if(argc>2)catalogue(argv[1],std::stoi(argv[2]));
    else std::cout<<"Installed catalogue checks skipped (arguments: AdvData row-count).\n";
    return failures?1:0;
}
