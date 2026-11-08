#include "arisa32/dc2dm_minigame.h"
#include "arisa32/dc2dm_reversi.h"
#include "arisa32/scene.h"
#include "arisa32/crx.h"
#include "arisa32/crm.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>

using namespace dc4scene;
using Stage=Dc2dmMinigame::Stage;
using Key=Dc2dmMinigame::Key;
void check(bool ok,const char* message) { if(!ok)throw std::runtime_error(message); }
bool fixture(const std::string&,Image* out,std::string*) {
    out->w=768;out->h=1152;out->rgba.assign(size_t(out->w)*out->h*4,0);return true;
}
void click(Dc2dmMinigame& game,int x,int y) {
    game.pointer(0,x,y);game.pointer(1,x,y);
}
void choose(Dc2dmMinigame& game,int index) {
    for(int i=0;i<index;++i)game.key(Key::Next);
    game.key(Key::Accept);
}
void input_checks() {
    Dc2dmMinigame game;std::string error;
    check(!game.start(2,0,0,1,fixture,&error) && !game.active() && !error.empty(),"unknown service");
    check(!game.start(1,0,0,1,[](const std::string&,Image*,std::string* e){*e="missing";return false;},&error) && !game.active(),"missing assets");
    check(game.start(1,2,3,1,fixture,&error),"start reversi");
    game.pointer(1,300,350);check(game.stage()==Stage::Difficulty,"unpaired release");
    game.pointer(0,300,350);game.pointer(3,-1,-1);game.pointer(1,300,350);
    check(game.stage()==Stage::Difficulty,"cancelled tap");
    game.pointer(0,300,350);game.pointer(1,650,350);
    check(game.stage()==Stage::Difficulty,"mismatched release");
    click(game,300,350);check(game.stage()==Stage::Order,"difficulty");
    click(game,950,350);check(game.stage()==Stage::Thinking,"second player");
    const auto board=game.reversi().board();
    game.update(999);check(game.reversi().board()==board,"CPU delay");
    game.key(Key::Cancel);game.update(5000);
    check(game.reversi().board()==board && game.confirming_exit(),"confirmation freezes CPU");
    game.key(Key::Accept);check(!game.confirming_exit(),"default continue");
    game.update(1);check(game.stage()==Stage::Playing && game.reversi().count(Dc2dmReversi::White)==1,"CPU opens as black");
    const auto after=game.reversi().board();
    click(game,241,1);check(game.reversi().board()==after,"illegal corner ignored");
    game.key(Key::Cancel);game.key(Key::Previous);game.key(Key::Accept);
    check(!game.active() && game.wins()==2 && game.losses()==3,"abort must not award result");
}
void full_games() {
    bool outcomes[3]={};
    for(int sample=0;sample<72;++sample) {
        Dc2dmMinigame game;std::string error;
        check(game.start(1,254,254,unsigned(sample+1),fixture,&error),"start match");
        const int level=sample<6 ? sample/2 : 0;
        const bool second=sample%2!=0;
        choose(game,level);choose(game,second?1:0);
        uint32_t random=unsigned(sample+7);
        int loops=0;
        while(game.stage()!=Stage::Result && ++loops<130) {
            if(game.stage()==Stage::Thinking)game.update(1000);
            else {
                auto legal=game.reversi().legal_moves(game.reversi().turn());
                check(!legal.empty(),"automatic pass");
                random=random*1664525u+1013904223u;
                const int move=legal[random%legal.size()];
                click(game,240+(move%8)*90+45,(move/8)*90+45);
            }
        }
        check(loops<130 && game.reversi().finished(),"match terminates");
        const int result=game.reversi().result_for(second?Dc2dmReversi::White:Dc2dmReversi::Black);
        outcomes[result]=true;
        check(game.wins()==254+(result==1) && game.losses()==254+(result==2),"player perspective totals");
        click(game,600,350);check(game.stage()==Stage::Result,"result hold");
        game.update(2000);click(game,600,350);check(game.stage()==Stage::Replay,"result acknowledgement");
        if(sample==0) {
            click(game,400,350);check(game.stage()==Stage::Difficulty,"replay resets setup");
            check(game.reversi().count(Dc2dmReversi::Black)==2,"replay resets board");
        } else {
            click(game,900,350);check(!game.active(),"exit after result");
        }
        game.update(10000);
        check(game.wins()==254+(result==1) && game.losses()==254+(result==2),"result awarded once");
    }
    check(outcomes[0] && outcomes[1] && outcomes[2],"win loss draw exercised");
}
void suit_inputs() {
    Dc2dmMinigame game;std::string error;
    check(game.start(0,0,0,1,fixture,&error),"start suit");
    // MSVCRT seed1 first random hand is Paper: Rock loses.
    click(game,75,150);
    check(game.losses()==1 && game.wins()==0,"suit actual hand hit");
    game.key(Key::Next);game.key(Key::Accept);
    check(!game.active(),"suit no exits to script");
}
void animation_inputs() {
    Dc2dmMinigame game;std::string error;
    check(game.start(0,0,0,1,fixture,&error,true,false),"animated suit");
    check(game.stage()==Stage::Boot && !game.take_music_start(),"boot before music");
    click(game,75,150);game.key(Key::Accept);
    check(game.stage()==Stage::Boot,"first boot cannot skip");
    game.update(4499);check(game.stage()==Stage::Boot,"boot timing");
    game.update(1);
    check(game.stage()==Stage::Suit && game.take_music_start() && !game.take_music_start(),"music starts once after boot");
    click(game,75,150);check(game.losses()==0,"choices wait for console");
    game.update(266);click(game,75,150);check(game.losses()==0,"last choice row still pending");
    game.update(1);click(game,75,150);check(game.losses()==1,"visible suit choice");
    game.key(Key::Next);game.key(Key::Accept);check(game.active(),"replay waits for result text");
    game.update(300);game.key(Key::Accept);check(!game.active(),"visible replay choice");
    check(game.start(1,0,0,1,fixture,&error,true,true),"repeated boot");
    click(game,300,350);check(game.stage()==Stage::Difficulty && game.take_music_start(),"allowed boot skip");
    check(game.start(1,0,0,1,fixture,&error,true,true),"boot held pointer");
    game.pointer(0,300,350);game.update(4500);game.pointer(1,300,350);
    check(game.stage()==Stage::Difficulty,"boot gesture cannot activate difficulty");
}
void reversi_flip_animation() {
    auto colored=[](const std::string& name,Image* out,std::string* error) {
        fixture(name,out,error);
        if(name.find("REVERSI_CHIP")!=std::string::npos)
            for(int y=0;y<72;++y)for(int x=0;x<144;++x) {
                const size_t p=(size_t(y)*out->w+x)*4;
                out->rgba[p]=x<72?17:211;out->rgba[p+3]=255;
            }
        return true;
    };
    Dc2dmMinigame game;std::string error;
    check(game.start(1,5,6,1,colored,&error,true,true),"animated reversi");
    game.key(Key::Accept);choose(game,0);choose(game,0);
    click(game,555,225);
    check(game.stage()==Stage::Flipping && game.take_sound_effect()==398,"placement starts flip and sound");
    std::vector<uint8_t> frame;
    auto disk=[&](int square) {
        game.draw(nullptr,&frame);
        return frame[(size_t(square/8*90+45)*1280+240+square%8*90+45)*4];
    };
    check(disk(19)==17 && disk(27)==211,"placed disk appears before captured disk changes");
    const auto placed=game.reversi().board();
    game.key(Key::Next);game.key(Key::Accept);click(game,645,315);
    check(game.reversi().board()==placed && game.stage()==Stage::Flipping,"flip blocks move input");
    game.update(100);
    check(game.stage()==Stage::Flipping && disk(27)==211 && game.take_sound_effect()==-1,"strict 100ms flip boundary");
    game.key(Key::Cancel);game.update(5000);game.key(Key::Cancel);
    check(game.stage()==Stage::Flipping && disk(27)==211,"quit confirmation pauses flip");
    game.update(1);
    check(game.stage()==Stage::Thinking && disk(27)==17 && game.take_sound_effect()==398,"first wave recolors disk");
    game.key(Key::Next);game.key(Key::Accept);
    game.update(898);
    check(game.stage()==Stage::Thinking && game.reversi().board()==placed,"CPU wait overlaps flip and freezes during confirmation");
    game.update(1);
    check(game.stage()==Stage::Flipping && game.take_sound_effect()==398,"CPU move also animates");
    game.update(101);
    check(game.stage()==Stage::Playing && game.take_sound_effect()==398,"CPU first flip completes");

    bool multiple_waves=false,corner=false;
    int turns=0;
    while(game.stage()!=Stage::Result && ++turns<64) {
        const auto before=game.reversi().board();
        if(game.stage()==Stage::Thinking)game.update(1000);
        else {
            const auto legal=game.reversi().legal_moves(game.reversi().turn());
            check(!legal.empty(),"animated game has a legal move");
            const int move=legal.front();
            click(game,240+move%8*90+45,move/8*90+45);
        }
        check(game.stage()==Stage::Flipping,"every placed move enters flip stage");
        int move=-1,last=0;
        for(int i=0;i<64;++i)if(before[i]==Dc2dmReversi::Empty && game.reversi().board()[i]!=before[i])move=i;
        check(move>=0,"placed square identified");
        for(int i=0;i<64;++i)if(before[i]!=Dc2dmReversi::Empty && before[i]!=game.reversi().board()[i])
            last=std::max(last,std::max(std::abs(i%8-move%8),std::abs(i/8-move/8)));
        const bool is_corner=(move%8==0 || move%8==7) && (move/8==0 || move/8==7);
        corner|=is_corner;multiple_waves|=last>1;
        check(game.take_sound_effect()==(is_corner?399:398),"corner placement sound");
        for(int wave=1;wave<=last;++wave) {
            check(game.wins()==5 && game.losses()==6,"result waits for final flip");
            game.update(100);
            check(game.stage()==Stage::Flipping && game.take_sound_effect()==-1,"each wave waits more than 100ms");
            game.update(1);
            check(game.take_sound_effect()==398 && game.take_sound_effect()==-1,"one sound per distance wave");
            check((game.stage()==Stage::Flipping)==(wave<last),"distance waves finish in order");
        }
    }
    check(turns<64 && multiple_waves && corner,"animated match covers long captures and corners");
    const int result=game.reversi().result_for(Dc2dmReversi::Black);
    check(game.wins()==5+(result==1) && game.losses()==6+(result==2),"animated result awarded after final wave");
}
void score_text() {
    check(dc2dm_minigame_score_text("win \x81\x96 loss \x81\x97",12,255)==
          "win \x82\x50\x82\x51 loss \x82\x51\x82\x54\x82\x54","CP932 fullwidth scores");
    check(dc2dm_minigame_score_text(u8"勝ち＊、負け＠",0,17)==u8"勝ち０、負け１７","UTF8 fullwidth scores");
    check(dc2dm_minigame_score_text("literal * @",1,2)=="literal * @","ASCII symbols unchanged");
}
void actual_artwork() {
    const char* data=std::getenv("DC2DM_DATA");
    const char* captures=std::getenv("DC2DM_MINIGAME_CAPTURES");
    if(!data || !captures)return;
    auto loader=[&](const std::string& name,Image* image,std::string* error) {
        std::filesystem::path p=std::filesystem::path(data)/"GRP";
        for(auto& part:std::filesystem::path(name+".crx")) {
            const auto found=dc4::find_file_ci(p.string(),part.string());
            p/=found.empty()?part:std::filesystem::path(found);
        }
        std::ifstream f(p,std::ios::binary);
        const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(f),{}};
        dc4::CrxInfo info;
        if(!dc4::crx_decode(bytes.data(),bytes.size(),&image->rgba,&info,error))return false;
        image->w=info.width;image->h=info.height;return true;
    };
    FontRenderer font;
    const char* font_path=std::getenv("DC2DM_MENU_FONT");
    check(font_path && font.load(font_path,0,22,false),"font");
    std::filesystem::create_directories(captures);
    auto shot=[&](Dc2dmMinigame& game,const char* name) {
        std::vector<uint8_t> frame;game.draw(&font,&frame);
        check(frame.size()==size_t(1280)*720*4,"frame dimensions");
        std::ofstream f(std::filesystem::path(captures)/(std::string(name)+".ppm"),std::ios::binary);
        f<<"P6\n1280 720\n255\n";
        for(size_t i=0;i<frame.size();i+=4)f.write(reinterpret_cast<const char*>(frame.data()+i),3);
    };
    Dc2dmMinigame game;std::string error;
    check(game.start(1,0,0,1,loader,&error,true,false),error.c_str());shot(game,"boot");
    game.update(4500);shot(game,"difficulty");
    game.key(Key::Accept);shot(game,"order");
    game.key(Key::Accept);shot(game,"board");
    click(game,555,225);shot(game,"flipping");
    game.update(101);shot(game,"thinking");
    std::vector<uint8_t> before_quit,after_quit;
    game.draw(&font,&before_quit);
    game.key(Key::Cancel);shot(game,"quit");
    game.draw(&font,&after_quit);
    check(std::equal(before_quit.begin(),before_quit.begin()+1280*190*4,after_quit.begin()),
          "quit dialog preserves board cursor outside overlay");
    check(game.start(0,0,0,1,loader,&error),error.c_str());shot(game,"suit");
    click(game,75,150);shot(game,"suit-result");
}
int main() {
    try {input_checks();full_games();suit_inputs();animation_inputs();reversi_flip_animation();score_text();actual_artwork();}
    catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
    std::puts("DC2DM minigame controller: input, matches, scores, assets PASS");
}
