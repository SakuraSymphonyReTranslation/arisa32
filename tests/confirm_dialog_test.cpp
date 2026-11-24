#include "arisa32/confirm_dialog.h"
#include "arisa32/save_screen.h"
#include <iostream>
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace {
int failures=0;
#define CHECK(x) do{if(!(x)){std::cerr<<__LINE__<<": " #x "\n";++failures;}}while(false)
using namespace dc4ui;
void dialog(){
    ConfirmDialog d;
    for(int i=0;i<int(ConfirmKind::Count);++i){
        auto kind=ConfirmKind(i);CHECK(d.open(kind));
        // Press outside, drag onto Yes: no action. A cancelled gesture must
        // not arm the next dialog, either.
        d.pointer_down(10,10);CHECK(d.pointer_up(540,380)==ConfirmResult::None);
        d.pointer_down(540,380);d.cancel_pointer();CHECK(d.pointer_up(540,380)==ConfirmResult::None);
        d.pointer_down(540,380);d.close();CHECK(d.open(kind));
        CHECK(d.pointer_up(540,380)==ConfirmResult::None);
        d.pointer_down(600,445);CHECK(d.pointer_up(600,445)==ConfirmResult::None);CHECK(d.dont_confirm());
        CHECK(d.answer(false)==ConfirmResult::No);
        CHECK(d.open(kind));CHECK(!d.dont_confirm());
        d.set_dont_confirm(true);d.pointer_down(540,380);CHECK(d.pointer_up(540,380)==ConfirmResult::Yes);
        CHECK(!d.active());CHECK(d.answer(true)==ConfirmResult::None);
        CHECK(!d.open(kind)); // Suppression applies only after accepting Yes.
    }
    ConfirmDialog fresh;CHECK(fresh.open(ConfirmKind::Load));
    CHECK(!fresh.hit_no(699,421)); // No invisible legacy hit rectangles.
    fresh.pointer_down(720,380);CHECK(fresh.pointer_up(720,380)==ConfirmResult::No);
}
void scene_replay_prompt(){
    const auto font_path=std::filesystem::path(__FILE__).parent_path().parent_path()/
        "android/app/src/main/assets/fonts/PretendardJP-Bold.ttf";
    dc4scene::FontRenderer font;
    if(!font.load(font_path.string(),0,24,false)){CHECK(false);return;}
    ConfirmDialog actual,expected;
    actual.set_language(dc4::Language::Indonesian);
    expected.set_language(dc4::Language::Indonesian);
    CHECK(actual.open(ConfirmKind::SceneReplay));
    expected.open(u"Putar Scenario ini?");
    const auto render=[&](const ConfirmDialog& dialog){
        std::vector<uint8_t> frame(1280*720*4,0);
        dialog.draw(-1,-1,false,font,&frame);
        return frame;
    };
    const auto frame=render(actual);
    CHECK(frame==render(expected));
    expected.open(u"Putar adegan ini?");
    CHECK(frame!=render(expected));
}
void saves(){
    SaveScreen s;s.set_confirm_enabled(true);s.set_navigation(true,true,true);
    s.set_slot_occupied(0,true);
    for(auto mode:{SaveMode::Save,SaveMode::Load}){
        s.set_mode(mode);
        CHECK(s.activate({SaveActionType::Slot,0}).type==SaveActionType::None);CHECK(s.confirming());
        CHECK(s.activate({SaveActionType::Page,2}).type==SaveActionType::None);CHECK(s.page()==0);
        CHECK(s.answer_confirmation(false).type==SaveActionType::None);CHECK(!s.confirming());
        CHECK(s.activate({SaveActionType::Slot,0}).type==SaveActionType::None);
        s.pointer_down(540,380);auto accepted=s.pointer_up(540,380);
        CHECK(accepted.type==SaveActionType::Slot&&accepted.index==0);CHECK(!s.confirming());
        CHECK(s.pointer_up(540,380).type==SaveActionType::None);
    }
    s.set_mode(SaveMode::Save);CHECK(s.activate({SaveActionType::Slot,1}).type==SaveActionType::None);
    CHECK(s.confirming());CHECK(s.answer_confirmation(true).index==1);
    s.set_mode(SaveMode::Load);CHECK(s.activate({SaveActionType::Slot,1}).type==SaveActionType::None);CHECK(!s.confirming());
    s.activate({SaveActionType::Slot,0});s.set_slot_occupied(0,false);
    CHECK(s.answer_confirmation(true).type==SaveActionType::None);
    s.set_shortcuts(true,true,true);
    for(auto type:{SaveActionType::LatestLoad,SaveActionType::AutoLoad,SaveActionType::QuickLoad,SaveActionType::Title,SaveActionType::Quit}){
        CHECK(s.activate({type}).type==SaveActionType::None);CHECK(s.confirming());
        CHECK(s.answer_confirmation(true).type==type);CHECK(!s.confirming());
    }
    s.activate({SaveActionType::QuickLoad});s.set_mode(SaveMode::Save);
    CHECK(!s.confirming());CHECK(s.answer_confirmation(true).type==SaveActionType::None);
}
void dc2dm(bool ds=false){
    namespace fs=std::filesystem;
    const auto parent=fs::canonical(fs::temp_directory_path());
    const auto root=parent/("dc2dm-confirm-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root/"GRP/SYSTEM");fs::create_directory(root/"MES");
    const std::vector<uint8_t> mes=ds?std::vector<uint8_t>{0,0,0,0,0x9A,0x9F,0,0}:
        std::vector<uint8_t>{1,0,0,0,3,0,0,0,0,0,0x72,0x9D,0,0};
    std::ofstream(root/"MES/main.mes",std::ios::binary).write(reinterpret_cast<const char*>(mes.data()),mes.size());
    if(ds)std::ofstream(root/"MES/DS_1129B_CM.mes",std::ios::binary).write(reinterpret_cast<const char*>(mes.data()),mes.size());
    const int width=ds?776:775,height=ds?369:640;
    std::vector<uint8_t> bytes{'C','R','X','G',0,0,0,0};
    auto word=[&](int n){bytes.push_back(uint8_t(n));bytes.push_back(uint8_t(n>>8));};
    word(width);word(height);word(1);word(0);word(0);word(0);
    const int stride=(width*3+3)&~3;
    std::vector<uint8_t> pixels(size_t(stride)*height,0);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        const size_t at=size_t(y)*stride+x*3;
        pixels[at]=uint8_t((x+y)%251);pixels[at+1]=uint8_t(y%251);pixels[at+2]=uint8_t(x%251);
    }
    for(size_t at=0;at<pixels.size();at+=8){
        bytes.push_back(255);bytes.insert(bytes.end(),pixels.begin()+at,pixels.begin()+std::min(at+8,pixels.size()));
    }
    std::ofstream(root/"GRP/SYSTEM"/(ds?"ADVMENUCHIP.CRX":"ADVMENU_CHIP.CRX"),std::ios::binary)
        .write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    ConfirmDialog d;std::string error;
    CHECK(d.load(root.string(),&error));CHECK(error.empty());
    if(ds){
        CHECK(d.hit_yes(480,348)&&!d.hit_yes(600,348)&&!d.hit_yes(480,412));
        CHECK(d.hit_no(666,348)&&!d.hit_no(786,348)&&!d.hit_no(666,347));
        CHECK(!d.hit_checkbox(600,445));
        const dc4scene::FontRenderer font;std::vector<uint8_t> frame(1280*720*4,0);
        auto matches=[&](int x,int y,int sx,int sy){const size_t at=(size_t(y)*1280+x)*4;
            return frame[at]==sx%251&&frame[at+1]==sy%251&&frame[at+2]==(sx+sy)%251;};
        const std::array<std::array<int,3>,10> prompts{{
            {int(ConfirmKind::Save),388,279},{int(ConfirmKind::Overwrite),388,279},
            {int(ConfirmKind::QuickSave),388,279},{int(ConfirmKind::Load),388,309},
            {int(ConfirmKind::QuickLoad),388,309},{int(ConfirmKind::Title),0,339},
            {int(ConfirmKind::Quit),0,249},{int(ConfirmKind::ReplayExit),388,249},
            {int(ConfirmKind::SceneJump),388,339},{int(ConfirmKind::SceneReplay),388,339}}};
        for(const auto& prompt:prompts){
            CHECK(d.open(ConfirmKind(prompt[0])));std::fill(frame.begin(),frame.end(),0);
            const int top=prompt[0]==int(ConfirmKind::SceneJump)?164:268;
            const int button_y=top+112;
            d.draw(-1,-1,false,font,&frame);
            CHECK(matches(290,top,0,0)&&matches(989,top+183,699,183));
            CHECK(matches(451,top+21,prompt[1]+1,prompt[2]+1));
            CHECK(matches(480,top+80,0,185)&&matches(666,top+80,120,185)&&matches(626,top+100,700,0));
            CHECK(frame[((top-1)*1280+290)*4+3]==0);
            CHECK(d.hit_yes(480,top+80)&&!d.hit_yes(600,top+80)&&!d.hit_yes(480,top+144));
            CHECK(d.hit_no(666,top+80)&&!d.hit_no(786,top+80)&&!d.hit_no(666,top+79));
            CHECK(!d.hit_yes(500,top==164?380:276));
            d.draw(500,button_y,true,font,&frame);CHECK(matches(480,top+80,240,185));
            d.draw(700,button_y,true,font,&frame);CHECK(matches(666,top+80,360,185));
            d.pointer_down(10,10);CHECK(d.pointer_up(500,button_y)==ConfirmResult::None);
            d.pointer_down(500,button_y);CHECK(d.pointer_up(700,button_y)==ConfirmResult::None);
            d.pointer_down(500,button_y);d.cancel_pointer();CHECK(d.pointer_up(500,button_y)==ConfirmResult::None);
            d.pointer_down(700,button_y);CHECK(d.pointer_up(700,button_y)==ConfirmResult::No);
        }
        CHECK(d.open(ConfirmKind::Save));d.pointer_down(500,380);CHECK(d.pointer_up(500,380)==ConfirmResult::Yes);
        std::error_code ec;if(fs::weakly_canonical(root,ec)==root&&!ec&&root.parent_path()==parent)fs::remove_all(root,ec);
        return;
    }
    CHECK(d.hit_yes(435,355) && !d.hit_yes(620,355));
    CHECK(d.hit_no(670,355) && !d.hit_no(855,355));
    CHECK(!d.hit_yes(435,354) && !d.hit_yes(435,435));
    CHECK(!d.hit_checkbox(600,445));
    CHECK(d.open(ConfirmKind::Save));
    d.pointer_down(500,400);d.cancel_pointer();CHECK(d.pointer_up(500,400)==ConfirmResult::None);
    d.pointer_down(500,400);CHECK(d.pointer_up(700,400)==ConfirmResult::None);
    const dc4scene::FontRenderer no_font;std::vector<uint8_t> frame(1280*720*4,0);
    auto matches=[&](int x,int y,int sx,int sy){const size_t at=(size_t(y)*1280+x)*4;
        return frame[at]==sx%251 && frame[at+1]==sy%251 && frame[at+2]==(sx+sy)%251;};
    d.draw(-1,-1,false,no_font,&frame);
    CHECK(matches(438,357,444,1));
    CHECK(matches(420,260,380,300));
    d.draw(500,400,true,no_font,&frame);CHECK(matches(438,357,444,129));
    d.pointer_down(700,400);CHECK(d.pointer_up(700,400)==ConfirmResult::No);
    CHECK(d.open(ConfirmKind::Save));d.pointer_down(500,400);CHECK(d.pointer_up(500,400)==ConfirmResult::Yes);
    struct Prompt { ConfirmKind kind; int x, y; };
    const Prompt prompts[] = {
        {ConfirmKind::Save,373,297}, {ConfirmKind::Overwrite,373,297},
        {ConfirmKind::QuickSave,373,297}, {ConfirmKind::Load,373,229},
        {ConfirmKind::QuickLoad,373,229}, {ConfirmKind::NewGame,373,365},
        {ConfirmKind::SceneReplay,373,365}, {ConfirmKind::SceneJump,373,365},
        {ConfirmKind::Movie,373,365}, {ConfirmKind::ResetSettings,373,433},
        {ConfirmKind::Title,1,433}, {ConfirmKind::Quit,1,229},
        {ConfirmKind::ReplayExit,1,569}
    };
    // Localized captions come from the installed atlas, even without a font.
    for(auto language:{dc4::Language::Japanese,dc4::Language::English,dc4::Language::Indonesian}) {
        d.set_language(language);
        for(const auto& prompt:prompts) {
            CHECK(d.open(prompt.kind));
            std::fill(frame.begin(),frame.end(),0);
            d.draw(-1,-1,false,no_font,&frame);
            CHECK(matches(420,260,prompt.x+7,prompt.y+3));
            CHECK(frame[(199*1280+365)*4+3]==0);
            CHECK(frame[(200*1280+365)*4+3]==255);
            d.close();
        }
    }
    std::error_code ec;
    if(fs::weakly_canonical(root,ec)==root && !ec && root.parent_path()==parent)fs::remove_all(root,ec);
}
}
int main(){dialog();scene_replay_prompt();saves();dc2dm();dc2dm(true);return failures?1:0;}
