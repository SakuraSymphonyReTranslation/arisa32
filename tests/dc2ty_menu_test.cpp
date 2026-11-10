#include "arisa32/system_menu.h"
#include "arisa32/crx.h"
#include "arisa32/names.h"
#include "arisa32/resource.h"
#include "arisa32/save_screen.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>

namespace {
int failures=0;
void check(bool value,int line) {
    if(!value) { std::cerr<<"line "<<line<<'\n'; ++failures; }
}
#define CHECK(x) check(bool(x),__LINE__)
bool has(const dc4ui::SystemMenu& menu,const std::string& tag) {
    const auto controls=menu.controls();
    return std::any_of(controls.begin(),controls.end(),[&](const auto& c){return c.enabled && c.tag==tag;});
}
std::vector<uint8_t> bytes(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary);
    CHECK(bool(in));
    return {std::istreambuf_iterator<char>(in),{}};
}
void write_bytes(const std::filesystem::path& path,const std::vector<uint8_t>& data) {
    std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()),std::streamsize(data.size()));
    CHECK(bool(out));
}
std::vector<uint8_t> capture(dc4ui::SystemMenu& menu,const char* name,int mouse_x=-1,int mouse_y=-1,
                             const char* font_path=nullptr,bool down=false) {
    dc4scene::FontRenderer font;
#ifdef _WIN32
    font.load(font_path?font_path:"C:/Windows/Fonts/msgothic.ttc",font_path?0:dc4::is_latin()?2:0,22,false);
#else
    if(font_path)font.load(font_path,0,22,false);
#endif
    std::vector<uint8_t> frame;
    menu.draw(mouse_x,mouse_y,down,font,&frame);
    if(const char* output=name?std::getenv("DC2TY_MENU_CAPTURES"):nullptr) {
        std::filesystem::create_directories(output);
        std::ofstream out(std::filesystem::path(output)/(std::string(name)+".ppm"),std::ios::binary);
        out<<"P6\n1280 720\n255\n";
        for(size_t i=0;i<frame.size();i+=4)out.write(reinterpret_cast<const char*>(frame.data()+i),3);
    }
    return frame;
}
void save_pages(const char* data) {
    using A=dc4ui::SaveActionType;
    using Mode=dc4ui::SaveMode;
    dc4ui::SaveScreen screen;
    std::string error;
    CHECK(screen.load(data,&error));
    CHECK(error.empty() && screen.loaded() && screen.dc2ty() && screen.dc2pc());
    CHECK(screen.page_count()==10);
    if(!screen.loaded())return;
    auto px=[](int x){return 160+(x*6+2)/5;};
    auto py=[](int y){return (y*6+2)/5;};
    auto click=[&](int x,int y) {
        screen.pointer_down(px(x),py(y));
        return screen.pointer_up(px(x),py(y));
    };
    auto action_is=[](dc4ui::SaveAction action,A type,int index=-1) {
        return action.type==type && action.index==index;
    };
    CHECK(action_is(screen.visible_slot_action(0),A::QuickSave));
    CHECK(action_is(screen.visible_slot_action(1),A::None));
    CHECK(screen.slot_hit(px(588),py(133))==-1);
    CHECK(screen.slot_hit(px(588),py(218))==-1);
    CHECK(action_is(screen.hit_test(px(588),py(218)),A::None));
    // Native page zero has three manual rows; each remaining page has five.
    constexpr int first_slot[]={0,3,8,13,18,23,28,33,38,43};
    std::set<int> all_slots;
    for(int page=0;page<10;++page) {
        screen.set_page(page);
        const int first_row=page==0?2:0;
        for(int row=first_row;row<5;++row) {
            const int index=first_slot[page]+row-first_row;
            CHECK(screen.slot_hit(px(588),py(133+85*row))==index);
            CHECK(action_is(screen.hit_test(px(588),py(133+85*row)),A::Slot,index));
            CHECK(action_is(screen.visible_slot_action(row),A::Slot,index));
            all_slots.insert(index);
        }
        for(int row:{-1,5,9,10})CHECK(action_is(screen.visible_slot_action(row),A::None));
        CHECK(screen.slot_hit(px(588),py(175))==-1);
    }
    CHECK(all_slots.size()==48 && *all_slots.begin()==0 && *all_slots.rbegin()==47);
    CHECK(action_is(click(316,457),A::Page,0));
    CHECK(action_is(click(207,457),A::Page,9));
    CHECK(action_is(click(289,547),A::Back));

    screen.set_confirm_enabled(false);
    CHECK(action_is(click(588,133),A::None) && screen.choosing_icon());
    CHECK(action_is(click(108,219),A::Slot,43));
    CHECK(screen.selected_icon()==0 && !screen.choosing_icon());
    CHECK(action_is(screen.activate(screen.visible_slot_action(4)),A::None));
    CHECK(action_is(click(172,219),A::Slot,47) && screen.selected_icon()==1);

    screen.set_mode(Mode::Load);
    screen.set_shortcuts(true,false,true);
    for(int row=0;row<5;++row) {
        CHECK(action_is(screen.hit_test(px(588),py(133+85*row)),A::None));
        CHECK(action_is(screen.visible_slot_action(row),A::None));
        screen.set_slot_occupied(43+row,true);
        CHECK(action_is(click(588,133+85*row),A::Slot,43+row));
        CHECK(action_is(screen.visible_slot_action(row),A::Slot,43+row));
    }
    screen.set_page(0);
    CHECK(action_is(click(588,133),A::QuickLoad));
    CHECK(action_is(click(588,218),A::LatestLoad));
    CHECK(action_is(screen.visible_slot_action(0),A::QuickLoad));
    CHECK(action_is(screen.visible_slot_action(1),A::LatestLoad));
    screen.set_shortcuts(false,false,false);
    CHECK(action_is(click(588,133),A::None));
    CHECK(action_is(click(588,218),A::None));

    dc4scene::FontRenderer font;
#ifdef _WIN32
    CHECK(font.load("C:/Windows/Fonts/msgothic.ttc",0,22,false));
#endif
    std::vector<dc4ui::SaveSlot> slots(48);
    std::vector<uint8_t> chip;
    dc4::CrxInfo chip_info;
    const auto chip_source=bytes(std::filesystem::path(data)/"GRP/SYSTEM/saveandload_chip00.CRX");
    CHECK(dc4::crx_decode(chip_source.data(),chip_source.size(),&chip,&chip_info));
    CHECK(chip_info.width==576 && chip_info.height==288);
    auto render=[&](const char* name) {
        std::vector<uint8_t> frame;
        screen.draw(slots,-1,-1,false,font,&frame);
        CHECK(frame.size()==1280u*720u*4u);
        if(const char* output=std::getenv("DC2TY_MENU_CAPTURES")) {
            std::filesystem::create_directories(output);
            std::ofstream out(std::filesystem::path(output)/(std::string(name)+".ppm"),std::ios::binary);
            out<<"P6\n1280 720\n255\n";
            for(size_t i=0;i<frame.size();i+=4)out.write(reinterpret_cast<const char*>(frame.data()+i),3);
            CHECK(bool(out));
        }
        return frame;
    };
    for(Mode mode:{Mode::Save,Mode::Load}) {
        screen.set_mode(mode);
        const std::string prefix=mode==Mode::Save?"save":"load";
        for(int page:{0,1,9}) {
            screen.set_page(page);
            const auto frame=render((prefix+"-page"+std::to_string(page+1)).c_str());
            std::vector<uint8_t> background;
            dc4::CrxInfo info;
            const auto source=bytes(std::filesystem::path(data)/"GRP/SYSTEM"/
                (prefix+(page==0?"_BG00.CRX":"_BG01.CRX")));
            CHECK(dc4::crx_decode(source.data(),source.size(),&background,&info));
            CHECK(info.width==800 && info.height==600);
            if(background.size()!=800u*600u*4u || frame.size()!=1280u*720u*4u)continue;
            // This region contains the Quick/New captions only on native page one.
            for(int y=120;y<=230;y+=5)for(int x=300;x<=430;x+=5) {
                const size_t src=(size_t(y)*800+x)*4;
                const size_t dst=(size_t(y*6/5+1)*1280+160+x*6/5+1)*4;
                for(int c=0;c<3;++c)CHECK(frame[dst+c]==background[src+c]);
            }
            if(chip.size()==576u*288u*4u)for(int row=0;row<5;++row) {
                const int source_y=(page==0 && row<2?row*72:144)+60;
                const size_t src=(size_t(source_y)*576+280)*4;
                const size_t dst=(size_t(py(97+row*85)+73)*1280+px(444)+337)*4;
                CHECK(chip[src+3]==255);
                for(int c=0;c<3;++c)CHECK(frame[dst+c]==chip[src+c]);
            }
        }
    }
    const auto native_dir=std::filesystem::path(data)/"Savegame";
    if(std::filesystem::is_regular_file(native_dir/"dc2ty00.dat") &&
       std::filesystem::is_regular_file(native_dir/"dc2ty01.dat") &&
       std::filesystem::is_regular_file(native_dir/"dc2ty02.dat")) {
        auto native_slot=[&](const char* name) {
            dc4ui::SaveSlot slot;
            const auto raw=bytes(native_dir/name);
            if(raw.size()!=0x2944 || raw[0]!=1 || raw[1] || raw[2] || raw[3])return slot;
            slot.occupied=true;
            slot.character_icon=raw[0x1388];
            const auto title_begin=raw.begin()+0x28DC;
            const auto title_end=std::find(title_begin,title_begin+64,0);
            slot.excerpt=dc4scene::sjis_to_utf16(std::string(title_begin,title_end));
            char stamp[40];
            std::snprintf(stamp,sizeof(stamp),"%04d %02d/%02d %02d:%02d",
                2000+raw[0x138D],raw[0x1389],raw[0x138A],raw[0x138B],raw[0x138C]);
            const std::string text=stamp;
            slot.timestamp.assign(text.begin(),text.end());
            return slot;
        };
        screen.set_special_slots(native_slot("dc2ty00.dat"),native_slot("dc2ty01.dat"));
        slots[0]=native_slot("dc2ty02.dat");
        screen.set_slot_occupied(0,slots[0].occupied);
        screen.set_page(0);
        screen.set_mode(Mode::Save);
        render("save-native-data-page1");
    }
    std::cout<<"DC2TY Save/Load: 48 manual slots, first-page Quick/New, page wrapping, icon selection and original BG00/BG01 checked.\n";
}
dc4ui::SystemMenuAction click(dc4ui::SystemMenu& menu,const char* tag) {
    for(const auto& control:menu.controls())if(control.enabled && control.tag==tag) {
        const int x=control.x+control.w/2,y=control.y+control.h/2;
        menu.pointer_down(x,y);
        return menu.pointer_up(x,y);
    }
    std::cerr<<"Missing control: "<<tag<<'\n';
    CHECK(false);
    return {};
}
std::vector<uint8_t> music_header(const std::vector<uint8_t>& frame) {
    CHECK(frame.size()==1280u*720u*4u);
    if(frame.size()!=1280u*720u*4u)return {};
    std::vector<uint8_t> header;
    for(int y=98*6/5;y<116*6/5;++y) {
        const size_t begin=(size_t(y)*1280+160+56*6/5)*4;
        header.insert(header.end(),frame.begin()+begin,frame.begin()+begin+372*6/5*4);
    }
    return header;
}
void music_mode(const char* data) {
    using Page=dc4ui::SystemMenuPage;
    using Type=dc4ui::SystemMenuActionType;
    dc4ui::SystemMenu menu;
    std::string error;
    if(!menu.load(data,&error)) { CHECK(false); return; }
    menu.set_visual_effects_enabled(false);
    menu.open(Page::Music);
    CHECK(menu.variable(3,5)==0 && menu.variable(3,8)==0);
    CHECK(has(menu,"music0") && has(menu,"music15") && !has(menu,"music16"));
    const auto before=capture(menu,"music-before");
    // Independent music.mes all_play_num0..47 mapping; rows are not BGM catalogue indices.
    constexpr int bgm_ids[]={1,3,9,11,12,13,14,15,16,17,19,20,21,22,23,24,
        25,27,28,29,30,31,32,33,34,36,38,54,57,58,62,68,
        69,76,77,78,79,80,81,82,83,84,85,86,87,88,71,89};
    dc4::GameNames names;
    CHECK(dc4::decode_game_names(bytes(std::filesystem::path(data)/"DAT/NAMES.DAT"),&names));
    const auto check_track=[&](const dc4ui::SystemMenuAction& action,int row) {
        CHECK(action.type==Type::PlayMusic && action.index==bgm_ids[row]);
        CHECK(size_t(bgm_ids[row])<names.bgm.size());
        if(size_t(bgm_ids[row])<names.bgm.size())CHECK(action.asset==names.bgm[size_t(bgm_ids[row])]);
        CHECK(menu.variable(3,5)==row && menu.variable(3,7)==0);
    };
    check_track(click(menu,"music9"),9);
    const auto selected=capture(menu,"music-selected");
    CHECK(music_header(selected)!=music_header(before));
    for(const auto& control:menu.controls())if(control.enabled && control.tag=="music4") {
        const int x=control.x+control.w/2,y=control.y+control.h/2;
        CHECK(menu.pointer_move(x,y).type==Type::None);
        const auto hovered=capture(menu,"music-hover",x,y);
        CHECK(hovered!=selected && music_header(hovered)==music_header(selected));
        CHECK(menu.variable(3,5)==9 && menu.variable(3,6)==0 && menu.variable(3,7)==0);
    }
    CHECK(capture(menu,nullptr)==selected);
    const auto change_page=[&](const char* tag,int expected_page) {
        const int row=menu.variable(3,5),mode=menu.variable(3,6),stopped=menu.variable(3,7);
        const auto title=music_header(capture(menu,nullptr));
        CHECK(click(menu,tag).type==Type::None);
        CHECK(menu.page()==Page::Music && menu.variable(3,8)==expected_page);
        CHECK(menu.variable(3,5)==row && menu.variable(3,6)==mode && menu.variable(3,7)==stopped);
        CHECK(music_header(capture(menu,nullptr))==title);
        CHECK(has(menu,"music"+std::to_string(expected_page*16)));
        CHECK(has(menu,"music"+std::to_string(expected_page*16+15)));
    };
    change_page("next",1); capture(menu,"music-page2");
    change_page("next",2); change_page("next",0);
    change_page("back",2); change_page("back",1); change_page("back",0);
    CHECK(click(menu,"stop").type==Type::StopMusic);
    CHECK(menu.variable(3,5)==9 && menu.variable(3,7)==1);
    const auto stopped=capture(menu,"music-stop");
    CHECK(music_header(stopped)==music_header(selected));
    CHECK(menu.music_finished().type==Type::None && capture(menu,nullptr)==stopped);
    for(const auto mode:{std::pair<const char*,int>{"play",0},{"rplay",1},{"all_play_mode",3}}) {
        check_track(click(menu,mode.first),9);
        CHECK(menu.variable(3,6)==mode.second);
        CHECK(menu.variable(100,26)==(mode.second==1?0:1));
        CHECK(music_header(capture(menu,nullptr))==music_header(selected));
        if(mode.second!=3) {
            const auto state=capture(menu,nullptr);
            CHECK(menu.music_finished().type==Type::None);
            CHECK(menu.variable(3,5)==9 && menu.variable(3,6)==mode.second && menu.variable(3,7)==0);
            CHECK(capture(menu,nullptr)==state);
        }
    }
    change_page("next",1);
    check_track(menu.music_finished(),10);
    CHECK(menu.variable(3,6)==3 && menu.variable(3,8)==1 && menu.variable(99,250)==0);
    CHECK(music_header(capture(menu,nullptr))!=music_header(selected));
    CHECK(click(menu,"stop").type==Type::StopMusic);
    const auto all_stopped=capture(menu,nullptr);
    CHECK(menu.music_finished().type==Type::None && capture(menu,nullptr)==all_stopped);
    change_page("back",0);
    check_track(click(menu,"play"),10);
    for(int row=0;row<48;++row) {
        if(row && row%16==0)change_page("next",row/16);
        check_track(click(menu,("music"+std::to_string(row)).c_str()),row);
        CHECK(menu.variable(3,6)==0 && menu.variable(3,8)==row/16 && menu.variable(100,26)==1);
    }
    check_track(click(menu,"all_play_mode"),47);
    const auto last_title=music_header(capture(menu,nullptr));
    check_track(menu.music_finished(),0);
    CHECK(menu.variable(3,6)==3 && menu.variable(3,8)==2 && menu.variable(99,250)==0);
    CHECK(music_header(capture(menu,nullptr))==music_header(before));
    CHECK(music_header(before)!=last_title);
    check_track(menu.music_finished(),1);
    CHECK(menu.variable(3,8)==2 && menu.variable(99,250)==0);
#ifdef _WIN32
    const auto pretendard=std::filesystem::path(__FILE__).parent_path().parent_path()/
        "android/app/src/main/assets/fonts/PretendardJP-Bold.ttf";
    for(int face=0;face<2;++face) {
        if(face && !std::filesystem::is_regular_file(pretendard))continue;
        const std::string font=face?pretendard.string():"C:/Windows/Fonts/msgothic.ttc";
        menu.set_variable(3,5,-1);
        const auto blank=capture(menu,nullptr,-1,-1,font.c_str());
        for(const int row:{1,7}) {
            menu.set_variable(3,5,row);
            const std::string label=std::string("music-header-")+(face?"pretendard-":"msgothic-")+std::to_string(row);
            const auto frame=capture(menu,label.c_str(),-1,-1,font.c_str());
            int ink=0,top=720,bottom=0;
            // Compare against the same native background, excluding the rounded header border.
            for(int y=98*6/5;y<116*6/5;++y)for(int x=160+76*6/5;x<160+404*6/5;++x) {
                const size_t at=(size_t(y)*1280+x)*4;
                if(frame[at]<180 || frame[at+1]<180 || frame[at+2]<180)continue;
                if(std::abs(int(frame[at])-blank[at])<30 && std::abs(int(frame[at+1])-blank[at+1])<30 &&
                   std::abs(int(frame[at+2])-blank[at+2])<30)continue;
                ++ink; top=std::min(top,y); bottom=std::max(bottom,y);
            }
            std::cout<<label<<" white ink: "<<ink<<", y="<<top<<".."<<bottom<<'\n';
            CHECK(ink>80 && top>98*6/5 && bottom<116*6/5-1);
        }
    }
#endif
    menu.open(Page::Extras);
    const int row=menu.variable(3,5),mode=menu.variable(3,6),page=menu.variable(3,8);
    CHECK(menu.music_finished().type==Type::None);
    CHECK(menu.page()==Page::Extras && menu.variable(3,5)==row && menu.variable(3,6)==mode && menu.variable(3,8)==page);
    CHECK(menu.error().empty());
}
void replay_row_states(const char* data,const char* font_path=nullptr) {
    using Page=dc4ui::SystemMenuPage;
    using Type=dc4ui::SystemMenuActionType;
    dc4ui::SystemMenu menu;
    std::string error;
    if(!menu.load(data,&error)) { CHECK(false); return; }
    menu.set_visual_effects_enabled(false);
    for(int id=0;id<129;++id)menu.set_variable(98,id,1);
    menu.open(Page::Scenes);
    const auto render=[&](dc4ui::SystemMenu& value,const char* name,int x=-1,int y=-1,bool down=false) {
        const std::string label=name?std::string(name)+(font_path?"-pretendard":""):std::string{};
        return capture(value,name?label.c_str():nullptr,x,y,font_path,down);
    };
    std::vector<uint8_t> scene_sheet;
    dc4::CrxInfo scene_sheet_info;
    const auto sheet_data=bytes(std::filesystem::path(data)/"GRP/system/sce_chip.crx");
    CHECK(dc4::crx_decode(sheet_data.data(),sheet_data.size(),&scene_sheet,&scene_sheet_info));
    const auto check_rim=[&](const std::vector<uint8_t>& frame,const dc4ui::SystemMenu::Control& c,
                             bool pink,bool hovered) {
        const int source_y=pink?(hovered?384:328):(hovered?272:216);
        int checked=0;
        for(int y=0;y<c.h;++y)for(int x=72;x<c.w-12;++x) {
            const int sx=x*232/c.w,sy=y*56/c.h;
            if(sy>4 && sy<52)continue;
            const size_t src=(size_t(source_y+sy)*scene_sheet_info.width+sx)*4;
            if(src+3>=scene_sheet.size() || scene_sheet[src+3]!=255)continue;
            const size_t dst=(size_t(c.y+y)*1280+c.x+x)*4;
            for(int channel=0;channel<3;++channel)CHECK(frame[dst+channel]==scene_sheet[src+channel]);
            ++checked;
        }
        CHECK(checked>100);
    };
    dc4::GameNames names;
    CHECK(dc4::decode_game_names(bytes(std::filesystem::path(data)/"DAT/NAMES.DAT"),&names));
    const auto change_page=[&](const char* tag,int expected_page) {
        const auto action=click(menu,tag);
        // next/back jump through the menu's authored BGM request.
        CHECK(action.type==Type::PlayMusic && action.index==25);
        CHECK(names.bgm.size()>25 && action.asset==names.bgm[25]);
        CHECK(menu.page()==Page::Scenes && menu.variable(3,8)==expected_page);
    };
    const auto rows=[&] {
        std::vector<dc4ui::SystemMenu::Control> result;
        for(const auto& c:menu.controls())if(c.w==232*6/5 && c.h==56*6/5)result.push_back(c);
        return result;
    };
    const auto date_ink=[](const std::vector<uint8_t>& frame,const dc4ui::SystemMenu::Control& c) {
        std::vector<size_t> ink;
        // Opaque palette-14 cores exclude antialiasing that blends with each row background.
        for(int y=c.y+2;y<c.y+26*6/5;++y)for(int x=c.x+58*6/5;x<c.x+200*6/5;++x) {
            const size_t at=(size_t(y)*1280+x)*4;
            if(frame[at]==0 && frame[at+1]==77 && frame[at+2]==165)ink.push_back(size_t(y)*1280+x);
        }
        return ink;
    };
#ifdef _WIN32
    dc4scene::FontRenderer header_font;
    CHECK(header_font.load(font_path?font_path:"C:/Windows/Fonts/msgothic.ttc",font_path?0:dc4::is_latin()?2:0,22,false));
    const auto expected_header=[&](int header,int row,bool pink) {
        std::u16string text=header==1?u" prologue":header==2?u" epilogue":u"";
        if(header>=100) {
            const auto number=[](int value) {
                std::u16string result=value<10?u" ":u"";
                for(char c:std::to_string(value))result.push_back(char16_t(c));
                return result;
            };
            text=number(header/100)+u"\u6708"+number(header%100)+u"\u65e5";
        }
        // Native row text starts at x58/y6, within the 162px caption clip.
        constexpr int width=163,height=19,out_w=width*6/5,out_h=height*6/5;
        std::vector<uint8_t> glyph(size_t(out_w)*out_h*4,0);
        for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)if(dx || dy)
            header_font.rasterize_line(text,1+dx,15+dy,&glyph,out_w,out_h,16,0xFFFFFF,false,0,true,0,0,1.2f,false);
        header_font.rasterize_line(text,1,15,&glyph,out_w,out_h,16,0x004DA5,false,0,true,0,0,1.2f,false);
        const int left=160+(258+254*(row%2)+57)*6/5,top=(144+64*(row/2)+5)*6/5;
        const int row_left=160+(258+254*(row%2))*6/5,row_top=(144+64*(row/2))*6/5;
        std::vector<size_t> ink;
        for(int y=0;y<out_h;++y)for(int x=0;x<out_w;++x) {
            const size_t at=(size_t(y)*out_w+x)*4;
            if(!glyph[at+3])continue;
            const int sx=(left+x-row_left)*232/(232*6/5);
            const int sy=(top+y-row_top)*56/(56*6/5)+(pink?328:216);
            const size_t bg=(size_t(sy)*scene_sheet_info.width+sx)*4;
            CHECK(scene_sheet[bg+3]==255);
            const int alpha=glyph[at+3];
            const int red=(glyph[at]*alpha+scene_sheet[bg]*(255-alpha))/255;
            const int green=(glyph[at+1]*alpha+scene_sheet[bg+1]*(255-alpha))/255;
            const int blue=(glyph[at+2]*alpha+scene_sheet[bg+2]*(255-alpha))/255;
            if(red==0 && green==77 && blue==165)
                ink.push_back(size_t(top+y)*1280+left+x);
        }
        return ink;
    };
#endif
    const auto check_row=[&](const dc4ui::SystemMenu::Control& c,const std::vector<uint8_t>& before,
                             bool expected_date,bool pink,const std::string& prefix) {
        const int x=c.x+c.w/2,y=c.y+c.h/2;
        CHECK(menu.pointer_move(x,y).type==Type::None);
        const auto hover=render(menu,prefix.empty()?nullptr:(prefix+"-hover").c_str(),x,y);
        menu.pointer_down(x,y);
        const auto down=render(menu,prefix.empty()?nullptr:(prefix+"-down").c_str(),x,y,true);
        CHECK(down==hover);
        if(c.enabled)CHECK(hover!=before);
        else CHECK(hover==before);
        check_rim(before,c,pink,false);
        check_rim(hover,c,pink,c.enabled);
        check_rim(down,c,pink,c.enabled);
#ifdef _WIN32
        const auto ink=date_ink(before,c);
        if(expected_date)CHECK(ink.size()>12);
        CHECK(date_ink(hover,c)==ink && date_ink(down,c)==ink);
#endif
        CHECK(menu.pointer_up(-1,-1).type==Type::None);
        CHECK(render(menu,nullptr)==before);
    };
    // Native sce_*.mes catalogue lengths, including their partial final pages.
    const char* routes[]={"hyd","set","mhr","myu"};
    const int first_ids[]={0,42,75,100},counts[]={42,33,25,29};
    // DC2TY.exe's replay lookup, in NAMES.DAT order; 1/2 mean prologue/epilogue.
    constexpr int headers[]={
        1,611,611,611,611,612,612,612,612,613,613,613,1208,1208,1211,1213,1216,1216,
        1218,1219,1222,1223,107,108,108,108,108,109,109,117,117,118,123,125,216,216,217,303,307,312,318,2,
        1,613,613,214,616,616,616,214,617,617,617,214,618,618,618,618,214,619,619,619,
        619,619,619,214,619,619,620,620,620,620,620,620,2,
        1,907,914,917,922,924,925,1003,1007,1013,1015,1015,1024,1103,1202,1217,1221,1222,1224,1231,119,122,124,204,2,
        1,620,621,621,621,621,621,621,622,622,622,622,622,623,623,623,624,624,624,624,624,624,625,625,625,626,627,627,2};
    for(int route=0;route<4;++route) {
        if(route)CHECK(click(menu,routes[route]).type==Type::None);
        CHECK(menu.variable(3,3)==route && menu.variable(3,8)==0);
        const int pages=(counts[route]+9)/10;
        for(int page=0;page<pages;++page) {
            const std::string prefix=std::string("scene-")+routes[route]+"-"+std::to_string(page);
            std::cout<<prefix<<'\n';
            CHECK(menu.variable(3,8)==page);
            CHECK(!has(menu,"#catalogue:scroll"));
            const auto controls=rows();
            const int expected=std::min(10,counts[route]-page*10);
            if(int(controls.size())!=expected)
                std::cerr<<prefix<<" expected "<<expected<<" rows, got "<<controls.size()<<'\n';
            CHECK(int(controls.size())==expected);
            if(controls.empty())return;
            const auto before=render(menu,(prefix+"-before").c_str());
            // Native raw55 lies outside the 800px canvas; its thumb must not enter the letterbox.
            bool clean_margin=true;
            for(int y=120;y<600;++y)for(int x=1120;x<1152;++x) {
                const size_t at=(size_t(y)*1280+x)*4;
                clean_margin&=before[at]==0 && before[at+1]==0 && before[at+2]==0 && before[at+3]==255;
            }
            CHECK(clean_margin);
            for(size_t index=0;index<std::min(controls.size(),size_t(expected));++index)
                CHECK(controls[index].tag=="sce"+std::to_string(page*10+int(index)));
            for(size_t index=0;index<std::min(controls.size(),size_t(expected));++index) {
                const int id=first_ids[route]+page*10+int(index);
                check_rim(before,controls[index],id==42 || id==124,false);
            }
#ifdef _WIN32
            for(size_t index=0;index<std::min(controls.size(),size_t(expected));++index) {
                const auto shape=date_ink(before,controls[index]);
                CHECK(shape.size()>12);
                const int header=headers[first_ids[route]+page*10+int(index)];
                const int id=first_ids[route]+page*10+int(index);
                const auto expected_ink=expected_header(header,int(index),id==42 || id==124);
                if(shape!=expected_ink) {
                    std::cerr<<prefix<<" row "<<index<<" header "<<header<<" actual "<<shape.size()
                             <<" expected "<<expected_ink.size()<<'\n';
                }
                CHECK(shape==expected_ink);
            }
#endif
            for(const auto index:{size_t(0),std::min(controls.size(),size_t(expected))-1}) {
                CHECK(controls[index].enabled);
                const int id=first_ids[route]+page*10+int(index);
                check_row(controls[index],before,true,id==42 || id==124,
                          page==0 && index==0?prefix:std::string{});
            }
            if(route==3 && page==2 && controls.size()>4)
                check_row(controls[4],before,true,true,prefix+"-special");
            menu.update(30000);
            CHECK(render(menu,nullptr)==before);
            if(expected<10) {
                const int x=160+(258+254*(expected%2)+116)*6/5;
                const int y=(144+64*(expected/2)+28)*6/5;
                menu.pointer_down(x,y);
                CHECK(render(menu,nullptr,x,y,true)==before);
                CHECK(menu.pointer_up(x,y).type==Type::None);
            }
            change_page("next",(page+1)%pages);
        }
        CHECK(menu.variable(3,8)==0);
        change_page("back",pages-1);
        change_page("next",0);
        menu.set_variable(98,first_ids[route],0); menu.activate("menu");
        const auto locked=rows();
        CHECK(!locked.empty() && !locked.front().enabled);
        if(!locked.empty())check_row(locked.front(),render(menu,nullptr),false,false,{});
        menu.set_variable(98,first_ids[route],1); menu.activate("menu");
        const auto restored=rows();
        CHECK(!restored.empty() && restored.front().enabled);
        if(restored.empty())return;
        const auto before_replay=render(menu,nullptr);
        auto action=click(menu,restored.front().tag.c_str());
        if(menu.confirming())action=menu.answer_confirmation(true);
        CHECK(action.type==Type::StartScene);
        menu.resume_scenes(); menu.update(30000);
        CHECK(menu.page()==Page::Scenes && menu.variable(3,3)==route);
        CHECK(render(menu,nullptr)==before_replay);
        check_row(restored.front(),before_replay,true,first_ids[route]==42,{});
    }
    menu.set_variable(98,124,0); menu.activate("menu");
    change_page("next",1); change_page("next",2);
    const auto locked_special=rows();
    CHECK(locked_special.size()>4 && !locked_special[4].enabled);
    if(locked_special.size()>4)check_row(locked_special[4],render(menu,"scene-myu-special-locked"),false,false,{});
    CHECK(menu.error().empty());
}
std::vector<int> config_values(const dc4ui::SystemConfig& c) {
    std::vector<int> values={c.master_volume,c.music_volume,c.se_volume,c.voice_volume,c.movie_volume,
        c.master_enabled,c.music_enabled,c.se_enabled,c.voice_enabled,c.movie_enabled,c.duck_music_for_voice,
        c.text_speed,c.auto_speed,c.auto_wait,c.window_opacity,c.skip_unread,c.stop_skip_at_choice,
        c.stop_auto_at_choice,c.wait_for_voice,c.stop_voice_on_advance,c.fullscreen,c.effects,c.effect_speed};
    values.insert(values.end(),c.character_volume.begin(),c.character_volume.end());
    values.insert(values.end(),c.character_muted.begin(),c.character_muted.end());
    return values;
}
std::vector<int> settings(const dc4ui::SystemMenu& menu) {
    auto values=config_values(menu.config());
    for(int i=0;i<30;++i)values.push_back(menu.variable(99,i));
    values.push_back(menu.variable(100,56));
    for(int i=60;i<76;++i)values.push_back(menu.variable(100,i));
    return values;
}
bool same_controls(const std::vector<dc4ui::SystemMenu::Control>& a,
                   const std::vector<dc4ui::SystemMenu::Control>& b) {
    return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),[](const auto& x,const auto& y) {
        return x.x==y.x && x.y==y.y && x.w==y.w && x.h==y.h && x.tag==y.tag && x.enabled==y.enabled;
    });
}
bool same_outside_reset(const std::vector<uint8_t>& before,const std::vector<uint8_t>& popup) {
    if(before.size()!=1280u*720u*4u || popup.size()!=before.size())return false;
    // option.mes draws the native 248x128 reset panel at (276,236) on its 800x600 canvas.
    constexpr int left=160+276*6/5,top=236*6/5,width=248*6/5,height=128*6/5;
    for(int y=0;y<720;++y)for(int x=0;x<1280;++x) {
        if(x>=left && x<left+width && y>=top && y<top+height)continue;
        const size_t at=(size_t(y)*1280+x)*4;
        if(!std::equal(before.begin()+at,before.begin()+at+4,popup.begin()+at)) {
            std::cerr<<"Reset changed background pixel "<<x<<','<<y<<'\n';
            return false;
        }
    }
    return true;
}
void reset_config(const char* data) {
    using Page=dc4ui::SystemMenuPage;
    using Type=dc4ui::SystemMenuActionType;
    dc4ui::SystemMenu menu;
    std::string error;
    const bool loaded=menu.load(data,&error);
    CHECK(loaded);
    if(!loaded)return;
    menu.set_visual_effects_enabled(false);
    menu.open(Page::Config);
    std::vector<uint8_t> default_frame;
    for(int cycle=0;cycle<3;++cycle) {
        const std::string prefix="reset-system-"+std::to_string(cycle);
        const auto shot=[&](const char* suffix) { return capture(menu,(prefix+suffix).c_str()); };
        for(const auto value:{std::pair<int,int>{7,6},{8,13},{9,19},{10,0},{11,3},
                              {12,2},{13,2},{16,11},{17,1},{20,0}})
            menu.set_variable(99,value.first,value.second);
        menu.set_variable(100,56,3);
        click(menu,"voiceSet");
        CHECK(has(menu,"chara_01") && has(menu,"chara_16") && !has(menu,"initConfig"));
        click(menu,"chara_01"); click(menu,"chara_16");
        const auto voice_frame=shot("-voice-before");
        click(menu,"vreturn");
        CHECK(has(menu,"initConfig"));
        const auto before_config=menu.config();
        const auto before_settings=settings(menu);
        const auto before_controls=menu.controls();
        const auto before=shot("-before");
        const auto open_reset=[&]() {
            click(menu,"initConfig");
            CHECK(menu.page()==Page::Config && !menu.confirming());
            CHECK(has(menu,"init") && has(menu,"menu") && !has(menu,"initConfig"));
            int enabled=0;
            for(const auto& control:menu.controls())if(control.enabled) {
                ++enabled;
                CHECK(control.tag=="init" || control.tag=="menu");
            }
            CHECK(enabled==2);
            CHECK(settings(menu)==before_settings);
        };
        open_reset();
        const auto popup=shot("-popup");
        CHECK(popup!=before && same_outside_reset(before,popup));
        // The BGM slider at native y=107 lies entirely above the reset panel.
        const int slider_y=107*6/5;
        menu.pointer_down(160+535*6/5,slider_y);
        CHECK(menu.pointer_move(160+663*6/5,slider_y).type==Type::None);
        CHECK(menu.pointer_up(160+663*6/5,slider_y).type==Type::None);
        CHECK(settings(menu)==before_settings);
        CHECK(shot("-blocked-slider")==popup);
        CHECK(click(menu,"menu").type!=Type::Back);
        CHECK(!menu.confirming() && menu.page()==Page::Config);
        CHECK(settings(menu)==before_settings && same_controls(menu.controls(),before_controls));
        CHECK(shot("-no")==before);
        open_reset();
        CHECK(menu.back().type!=Type::Back);
        CHECK(!menu.confirming() && menu.page()==Page::Config);
        CHECK(settings(menu)==before_settings && same_controls(menu.controls(),before_controls));
        CHECK(shot("-back")==before);
        open_reset();
        CHECK(click(menu,"init").type==Type::SettingsChanged);
        CHECK(!menu.confirming() && menu.page()==Page::Config);
        CHECK(has(menu,"initConfig") && !has(menu,"init"));
        for(const auto value:{std::pair<int,int>{7,25},{8,25},{9,29},{10,1},{11,8},
                              {12,1},{13,0},{17,0},{20,1}})
            CHECK(menu.variable(99,value.first)==value.second);
        CHECK(menu.variable(99,16)==11 && menu.variable(100,56)==0);
        auto expected=before_config;
        expected.music_volume=25*100/29; expected.se_volume=25*100/29;
        expected.voice_volume=100; expected.effects=1;
        expected.window_opacity=8*100/15; expected.text_speed=11/3;
        CHECK(config_values(menu.config())==config_values(expected));
        const auto after=shot("-yes");
        if(cycle==0)default_frame=after;
        else CHECK(after==default_frame);
        click(menu,"voiceSet");
        CHECK(!has(menu,"initConfig"));
        CHECK(shot("-voice-after")==voice_frame);
        click(menu,"vreturn");
        CHECK(capture(menu,(prefix+"-returned").c_str())==after);
        CHECK(menu.error().empty());
    }
}
void translated_reset_config(const char* data) {
    using Page=dc4ui::SystemMenuPage;
    using Type=dc4ui::SystemMenuActionType;
    auto base=std::filesystem::absolute(data).lexically_normal();
    if(base.filename().empty())base=base.parent_path();
    const auto overlay=base.parent_path()/"id_Data";
    const auto translated_script=overlay/"MES/option.mes";
    if(!std::filesystem::is_regular_file(translated_script)) {
        std::cout<<"Sibling id_Data/MES/option.mes absent; translated reset test skipped.\n";
        return;
    }
    dc4::configure_resources(base.string(),overlay.string());
    CHECK(dc4::get_language()==dc4::Language::Indonesian);
    std::error_code path_error;
    CHECK(std::filesystem::equivalent(dc4::resolve_asset((base/"MES/option.mes").string()),
                                     translated_script,path_error));
    CHECK(!path_error);
    {
        dc4ui::SystemMenu menu;
        std::string error;
        const bool loaded=menu.load(data,&error);
        CHECK(loaded);
        if(loaded) {
            menu.set_visual_effects_enabled(false);
            menu.open(Page::Config);
            const auto before_settings=settings(menu);
            const auto before_controls=menu.controls();
            const auto before=capture(menu,"reset-translated-before");
            click(menu,"initConfig");
            CHECK(!menu.confirming() && has(menu,"init") && has(menu,"menu"));
            const auto popup=capture(menu,"reset-translated-popup");
            CHECK(popup!=before && same_outside_reset(before,popup));
            CHECK(settings(menu)==before_settings);
#ifdef _WIN32
            if(before.size()==1280u*720u*4u && popup.size()==before.size()) {
                int left=1280,top=720,right=0,bottom=0,pixels=0;
                // Exclude the cyan panel border and buttons; count newly drawn white title ink.
                for(int y=256*6/5;y<305*6/5;++y)for(int x=160+292*6/5;x<160+508*6/5;++x) {
                    const size_t at=(size_t(y)*1280+x)*4;
                    if(popup[at]<200 || popup[at+1]<200 || popup[at+2]<200)continue;
                    if(std::abs(int(popup[at])-before[at])<16 &&
                       std::abs(int(popup[at+1])-before[at+1])<16 &&
                       std::abs(int(popup[at+2])-before[at+2])<16)continue;
                    left=std::min(left,x); top=std::min(top,y);
                    right=std::max(right,x+1); bottom=std::max(bottom,y+1); ++pixels;
                }
                std::cout<<"Translated reset title ink: "<<left<<','<<top<<".."<<right<<','<<bottom
                         <<", "<<pixels<<" pixels\n";
                CHECK(pixels>250);
                CHECK(left>510 && right<769 && top>307 && bottom<366);
                CHECK(right-left>=180 && right-left<=260);
                // An 18px native font cannot reach this height after the 1.2x canvas scaling.
                CHECK(bottom-top>=25 && bottom-top<=34);
                CHECK(std::abs((left+right)-1280)<=6);
                CHECK(std::abs((top+bottom)-667)<=6);
            }
#endif
            CHECK(click(menu,"menu").type!=Type::Back);
            CHECK(!menu.confirming() && menu.page()==Page::Config);
            CHECK(settings(menu)==before_settings && same_controls(menu.controls(),before_controls));
            CHECK(capture(menu,"reset-translated-no")==before);
            CHECK(menu.error().empty());
        }
    }
    dc4::configure_resources(base.string());
    CHECK(dc4::get_language()==dc4::Language::Japanese);
    CHECK(!dc4::prefers_utf8_text());
}
void translated_replay_titles(const char* data) {
#ifdef _WIN32
    const auto base=std::filesystem::absolute(data).lexically_normal();
    const auto overlay=base.parent_path()/"id_Data";
    if(!std::filesystem::is_regular_file(overlay/"MES/sce_hyd.mes")) {
        std::cout<<"Sibling id_Data/MES/sce_hyd.mes absent; translated replay title test skipped.\n";
        return;
    }
    dc4::configure_resources(base.string(),overlay.string());
    CHECK(dc4::get_language()==dc4::Language::Indonesian);
    const auto fonts=std::filesystem::path(__FILE__).parent_path().parent_path()/
        "android/app/src/main/assets/fonts";
    for(const char* face:{"MSGOTHIC.TTC","PretendardJP-Bold.ttf"}) {
        dc4scene::FontRenderer font;
        const auto font_path=(fonts/face).string();
        CHECK(font.load(font_path,0,22,false));
        dc4ui::SystemMenu menu;
        std::string error;
        CHECK(menu.load(data,&error));
        menu.set_visual_effects_enabled(false);
        for(int id=0;id<129;++id)menu.set_variable(98,id,1);
        menu.open(dc4ui::SystemMenuPage::Scenes);
        const auto before=capture(menu,(std::string("scene-latin-")+face).c_str(),-1,-1,font_path.c_str());
        // These captions come from the PC translation's first and third catalogue entries.
        for(const auto& entry:{std::pair<int,std::u16string>{0,u"Awal dari Permulaan"},
                               {2,u"Keseharian di Akademi Kazami"}}) {
            const auto controls=menu.controls();
            const auto row=std::find_if(controls.begin(),controls.end(),[&](const auto& c) {
                return c.tag=="sce"+std::to_string(entry.first);
            });
            CHECK(row!=controls.end());
            if(row==controls.end())continue;
            const int left=160+(258+57)*6/5,top=(144+64*(entry.first/2)+31)*6/5;
            std::u16string displayed=entry.second;
            int size=16;
            while(size>14 && font.text_width(displayed,size)>160)--size;
            const bool shortened=font.text_width(displayed,size)>160;
            if(shortened) {
                while(!displayed.empty() && font.text_width(displayed+u"...",size)>160)displayed.pop_back();
                while(!displayed.empty() && displayed.back()==u' ')displayed.pop_back();
                displayed+=u"...";
            }
            if(entry.first==0)CHECK(size==16 && !shortened);
            CHECK(font.text_width(displayed,size)<=160);
            constexpr int width=163*6/5;
            const int height=(size+3)*6/5;
            std::vector<uint8_t> glyph(size_t(width)*height*4,0);
            font.rasterize_line(displayed,1,size-1,&glyph,width,height,size,0x004DA5,
                                false,0,false,0,0,1.2f,false);
            const int x=row->x+row->w/2,y=row->y+row->h/2;
            const auto hover=capture(menu,nullptr,x,y,font_path.c_str());
            menu.pointer_down(x,y);
            const auto down=capture(menu,nullptr,x,y,font_path.c_str(),true);
            CHECK(down==hover);
            int cores=0,top_ink=height,bottom_ink=0,right_ink=0;
            for(int yy=0;yy<height;++yy)for(int xx=0;xx<width;++xx) {
                const size_t at=(size_t(yy)*width+xx)*4;
                if(glyph[at+3]!=255)continue;
                const size_t target=(size_t(top+yy)*1280+left+xx)*4;
                for(int channel=0;channel<3;++channel) {
                    CHECK(before[target+channel]==glyph[at+channel]);
                    CHECK(hover[target+channel]==glyph[at+channel]);
                }
                ++cores;top_ink=std::min(top_ink,yy);bottom_ink=std::max(bottom_ink,yy+1);
                right_ink=std::max(right_ink,xx+1);
            }
            CHECK(cores>35 && bottom_ink-top_ink>=9);
            CHECK(right_ink<width-2);
            CHECK(menu.pointer_up(-1,-1).type==dc4ui::SystemMenuActionType::None);
            CHECK(capture(menu,nullptr,-1,-1,font_path.c_str())==before);
            std::cout<<"Translated replay "<<face<<" row "<<entry.first<<": "<<size
                     <<"px, "<<cores<<" opaque glyph pixels, ellipsis="<<shortened<<'\n';
        }
        CHECK(error.empty() && menu.error().empty());
    }
    dc4::configure_resources(base.string());
#else
    (void)data;
#endif
}
}

int main(int argc,char** argv) {
    using Page=dc4ui::SystemMenuPage;
    using Type=dc4ui::SystemMenuActionType;
    const char* data=argc>1?argv[1]:std::getenv("DC2TY_DATA");
    if(!data) { std::cout<<"DC2TY_DATA unset; installed-data test skipped.\n"; return 0; }
    save_pages(data);
    translated_reset_config(data);
    translated_replay_titles(data);
    reset_config(data);
    music_mode(data);
    replay_row_states(data);
#ifdef _WIN32
    const auto replay_font=std::filesystem::path(__FILE__).parent_path().parent_path()/
        "android/app/src/main/assets/fonts/PretendardJP-Bold.ttf";
    CHECK(std::filesystem::is_regular_file(replay_font));
    if(std::filesystem::is_regular_file(replay_font))replay_row_states(data,replay_font.string().c_str());
#endif
    dc4ui::SystemMenu menu;
    std::string error;
    CHECK(menu.load(data,&error));
    if(!error.empty())std::cerr<<error<<'\n';
    CHECK(menu.is_dc2ty()); CHECK(!menu.is_dc2pc()); CHECK(!menu.is_dc2dm());
    CHECK(menu.startup_movie()=="movie\\dc2ty_op.mpg");
    CHECK(!menu.title_music().empty());
    CHECK(has(menu,"start") && has(menu,"load") && has(menu,"extra") && has(menu,"config"));
    CHECK(menu.variable(100,0)==0);
    const auto rainy=capture(menu,"title-rainy");
    menu.set_variable(100,0,1);
    CHECK(capture(menu,"title-day")!=rainy);
    menu.set_variable(100,0,0);
    menu.open_title_intro(); CHECK(menu.title_intro_active()); CHECK(menu.controls().empty());
    const auto sky=capture(menu,"intro-sky");
    menu.update(2000); CHECK(menu.title_intro_active());
    CHECK(capture(menu,"intro-pan")!=sky);
    menu.update(30000); CHECK(!menu.title_intro_active()); CHECK(has(menu,"start"));
    menu.open_title_intro(); menu.back(); CHECK(!menu.title_intro_active()); CHECK(!menu.confirming());
    menu.open_title_intro(); menu.update(5844); CHECK(!menu.title_voice_ready());
    menu.update(1); CHECK(menu.title_voice_ready() && menu.title_intro_active());
    menu.update(599); CHECK(menu.title_intro_active());
    menu.update(1); CHECK(!menu.title_intro_active());
    static const char* stories[]={"hyd_0611_a1_avn","set_0214_f1_214","ty_mhr01_d1_mhr","ty_myu01_d1_myu"};
    for(int route=0;route<4;++route) {
        const auto start=menu.activate("start"); CHECK(start.type==Type::PlayVoice);
        CHECK(menu.story_picker_active());
        CHECK(menu.controls().empty());
        menu.update(255); CHECK(has(menu,"#ty:story:back"));
        if(!route)capture(menu,"story-picker");
        const std::string tag="#ty:story:"+std::to_string(route);
        CHECK(menu.activate(tag).type==Type::None);
        CHECK(menu.activate(tag).type==Type::None);
        menu.update(255);
        if(!route)capture(menu,"story-preview");
        const auto chosen=menu.activate(tag);
        CHECK(chosen.type==Type::NewGame && chosen.asset==stories[route] && chosen.voice_event==24+route);
        CHECK(menu.variable(3,32)==route);
        menu.open(Page::Title);
        CHECK(!menu.story_picker_active());
    }
    menu.activate("start"); menu.update(255);
    menu.activate("#ty:story:1"); menu.update(255);
    menu.back(); menu.update(255); CHECK(has(menu,"#ty:story:0"));
    menu.back(); CHECK(has(menu,"start"));
    menu.activate("start"); menu.update(255);
    menu.activate("#ty:story:back"); CHECK(has(menu,"start"));
    menu.activate("start"); menu.update(255); menu.scroll(-1);
    menu.activate("#ty:story:accept"); CHECK(!menu.story_picker_active());
    menu.activate("start"); menu.update(255); menu.scroll(1);
    CHECK(menu.activate("#ty:story:accept").type==Type::None); menu.update(255);
    CHECK(menu.activate("#ty:story:accept").asset==stories[0]); menu.open(Page::Title);
    static const char* actors[]={"otm","yme","kko","anz","akn","mhr","myk","yki","mik","myu","skr","sgn","wtr","kmk","mki","hrm"};
    for(int actor=0;actor<16;++actor) {
        menu.set_variable(100,56,actor);
        for(const auto event:{std::pair<int,int>{1,2},{2,2},{7,3},{8,4},{10,5},{14,11},{15,6},{16,7},{17,8},{22,9},{23,10}}) {
            char expected[64];std::snprintf(expected,sizeof(expected),"VOICE/sys/sys_%s_%02d",actors[actor],event.second);
            CHECK(menu.system_voice(event.first)==expected);
        }
        CHECK(menu.system_voice(12).empty()); CHECK(menu.system_voice(13).empty()); CHECK(menu.system_voice(20).empty());
    }
    const auto load=menu.activate("load"); CHECK(load.type==Type::Load && load.voice_event==8);
    menu.activate("extra"); CHECK(menu.page()==Page::Extras);
    CHECK(has(menu,"cg") && has(menu,"scene") && has(menu,"bgm"));
    capture(menu,"extras");
    for(const auto entry:{std::pair<const char*,Page>{"cg",Page::Gallery},{"scene",Page::Scenes},{"bgm",Page::Music}}) {
        menu.activate(entry.first); CHECK(menu.page()==entry.second); CHECK(has(menu,"return"));
        CHECK(menu.error().empty()); capture(menu,entry.first);
        menu.back(); CHECK(menu.page()==Page::Extras);
    }
    menu.open(Page::Config); CHECK(has(menu,"tr_off"));
    CHECK(menu.activate("tr_off").type==Type::SettingsChanged); CHECK(!menu.config().effects);
    CHECK(menu.activate("tr_on").type==Type::SettingsChanged); CHECK(menu.config().effects);
    for(const auto& slider:menu.controls())if(slider.tag=="voiceMenu") {
        const int x=slider.x+slider.w/2,y=slider.y+slider.h/2;
        menu.pointer_down(x,y);
        const auto preview=menu.pointer_up(x,y);
        CHECK(preview.type==Type::PlayVoice && preview.asset=="VOICE\\sys\\sys_hrm_12");
    }
    menu.activate("voiceSet"); CHECK(has(menu,"chara_01") && has(menu,"chara_16")); capture(menu,"voice-config");
    CHECK(!menu.config().character_muted[0]);
    const auto mute=menu.activate("chara_01");
    CHECK(mute.type==Type::PlayVoice && mute.asset=="VOICE\\sys\\sys_otm_14");
    CHECK(menu.config().character_muted[0] && menu.variable(100,60)==1);
    menu.activate("chara_01"); CHECK(!menu.config().character_muted[0]);
    menu.activate("chara_16"); CHECK(menu.config().character_muted[15]);
    menu.activate("chara_16"); CHECK(!menu.config().character_muted[15]);
    menu.activate("vreturn"); menu.activate("sysVoice"); CHECK(has(menu,"mnt") && has(menu,"hrm")); capture(menu,"system-voice");
    const auto sample=menu.activate("mnt");
    CHECK(sample.type==Type::PlayVoice && sample.asset=="VOICE\\sys\\sys_kko_13");
    CHECK(menu.variable(100,56)==2);
    menu.open(Page::Title); CHECK(menu.page_voice_event(Page::Extras)==0);
    const auto profile=std::filesystem::temp_directory_path()/("arisa32-dc2ty-menu-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".profile");
    menu.set_variable(100,1,1); menu.set_variable(100,56,2);
    CHECK(menu.save_profile(profile.string(),&error));
    dc4ui::SystemMenu restored; CHECK(restored.load(data,&error));
    CHECK(restored.load_profile(profile.string(),&error));
    CHECK(restored.variable(100,1)==1 && restored.variable(100,56)==2);
    CHECK(restored.system_voice(1)=="VOICE/sys/sys_kko_02");
    CHECK(!restored.load_system_dat(profile.string(),&error));
    std::vector<uint8_t> native(0x738,0);
    native[0x08]=8; native[0x10]=1; native[0x18]=1;
    native[0x2C]=13; native[0x34]=14; native[0x3C]=15;
    native[0x70+2]=1; native[0x70+56]=3; native[0x70+60]=1;
    native[0xD4]=1; native[0x2D4]=1; native[0x6D4+1]=1;
    const auto native_path=profile.string()+".system.dat";
    write_bytes(native_path,native);
    CHECK(restored.load_system_dat(native_path,&error));
    CHECK(restored.variable(100,1)==1 && restored.variable(100,2)==1);
    CHECK(restored.variable(100,56)==2 && restored.variable(100,60)==0);
    CHECK(restored.variable(99,7)==25);
    CHECK(restored.variable(97,0)==1 && restored.variable(98,0)==1 && restored.variable(96,1)==1);
    dc4ui::SystemMenu imported; CHECK(imported.load(data,&error));
    CHECK(imported.load_system_dat(native_path,&error));
    CHECK(imported.variable(100,56)==3 && imported.variable(100,60)==1);
    CHECK(imported.variable(99,11)==8);
    CHECK(imported.variable(99,7)==13 && imported.variable(99,8)==14 && imported.variable(99,9)==15);
    imported.set_variable(100,56,5);
    CHECK(imported.load_system_dat(native_path,&error)); CHECK(imported.variable(100,56)==5);
    native[0x70]=1; native.back()=2; write_bytes(native_path,native);
    CHECK(!imported.load_system_dat(native_path,&error)); CHECK(imported.variable(100,0)==0);
    native.back()=0; native[0x18]=4; write_bytes(native_path,native);
    CHECK(!imported.load_system_dat(native_path,&error)); CHECK(imported.variable(100,0)==0);
    CHECK(restored.load_profile(profile.string(),&error)); CHECK(restored.variable(100,2)==1);
    std::filesystem::remove(native_path);
    std::filesystem::remove(profile);

    dc4::GameNames names;
    CHECK(dc4::decode_game_names(bytes(std::filesystem::path(data)/"DAT/NAMES.DAT"),&names));
    CHECK(names.cg.size()>70);
    if(names.cg.size()>70) {
        std::string asset=names.cg[70]; std::replace(asset.begin(),asset.end(),'\\','/');
        std::vector<uint8_t> decoded;
        const auto compressed=bytes(std::filesystem::path(data)/"GRP"/(asset+".crx"));
        dc4::CrxInfo info;
        CHECK(dc4::crx_decode(compressed.data(),compressed.size(),&decoded,&info));
        CHECK(info.width==800 && info.height==947);
        menu.set_variable(97,70,1); menu.set_variable(99,12,1);
        menu.open(Page::Gallery); menu.activate("set"); menu.activate("cg70");
        CHECK(menu.gallery_preview_active() && menu.variable(99,100)==70);
        menu.update(640);
        const auto check_pixels=[&](int offset_y) {
            for(const auto size:{std::pair<int,int>{640,360},{1920,1080}}) {
                std::vector<uint8_t> frame;
                CHECK(menu.draw_gallery(size.first,size.second,&frame));
                const int multiplier=size.second/360,left=(size.first-size.second*4/3)/2;
                for(int y=2;y<580;y+=55)for(int x=2;x<780;x+=55) {
                    const int dx=left+(1+(x-2)/5*3)*multiplier+(multiplier-1)/2;
                    const int dy=(1+(y-2)/5*3)*multiplier+(multiplier-1)/2;
                    const size_t dst=(size_t(dy)*size.first+dx)*4;
                    const size_t src=(size_t(y+offset_y)*info.width+x)*4;
                    for(int c=0;c<3;++c)CHECK(frame[dst+c]==decoded[src+c]);
                }
            }
        };
        check_pixels(0);
        menu.scroll(1); menu.update(170); CHECK(menu.variable(99,100)==70); check_pixels(40);
        menu.update(3000); check_pixels(347);
        menu.back(); CHECK(!menu.gallery_preview_active());
    }
    menu.set_variable(98,0,1); menu.open(Page::Scenes);
    CHECK(has(menu,"sce0"));
    auto replay=menu.activate("sce0");
    if(menu.confirming())replay=menu.answer_confirmation(true);
    CHECK(replay.type==Type::StartScene);
    dc4scene::SceneRunner scene(data);
    CHECK(scene.load_script((std::filesystem::path(data)/"MES"/(replay.asset+".mes")).string(),replay.label));
    menu.initialize_scene(scene,true);
    CHECK(scene.variable(3,30)==1 && !scene.save_load_allowed());
    menu.resume_scenes(); CHECK(menu.page()==Page::Scenes && has(menu,"sce0"));
    std::cout<<"DC2TY title, selector, voices, Music, Config/reset popup, native/profile progress, source-resolution CG pan and replay checked.\n";
    return failures?1:0;
}
