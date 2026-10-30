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
void dc2dm(){
    namespace fs=std::filesystem;
    const auto parent=fs::canonical(fs::temp_directory_path());
    const auto root=parent/("dc2dm-confirm-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root/"GRP/SYSTEM");fs::create_directory(root/"MES");
    const std::array<uint8_t,14> mes{1,0,0,0,3,0,0,0,0,0,0x72,0x9D,0,0};
    std::ofstream(root/"MES/main.mes",std::ios::binary).write(reinterpret_cast<const char*>(mes.data()),mes.size());
    std::vector<uint8_t> bytes{'C','R','X','G',0,0,0,0};
    auto word=[&](int n){bytes.push_back(uint8_t(n));bytes.push_back(uint8_t(n>>8));};
    word(775);word(640);word(1);word(0);word(0);word(0);
    const int stride=(775*3+3)&~3;
    std::vector<uint8_t> pixels(size_t(stride)*640,0);
    for(int y=0;y<640;++y)for(int x=0;x<775;++x){
        const size_t at=size_t(y)*stride+x*3;
        pixels[at]=uint8_t((x+y)%251);pixels[at+1]=uint8_t(y%251);pixels[at+2]=uint8_t(x%251);
    }
    for(size_t at=0;at<pixels.size();at+=8){
        bytes.push_back(255);bytes.insert(bytes.end(),pixels.begin()+at,pixels.begin()+std::min(at+8,pixels.size()));
    }
    std::ofstream(root/"GRP/SYSTEM/ADVMENU_CHIP.CRX",std::ios::binary)
        .write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    ConfirmDialog d;std::string error;
    CHECK(d.load(root.string(),&error));CHECK(error.empty());
    CHECK(d.hit_yes(435,375) && !d.hit_yes(620,375));
    CHECK(d.hit_no(670,375) && !d.hit_no(855,375));
    CHECK(!d.hit_checkbox(600,445));
    CHECK(d.open(ConfirmKind::Save));
    d.pointer_down(500,400);d.cancel_pointer();CHECK(d.pointer_up(500,400)==ConfirmResult::None);
    d.pointer_down(500,400);CHECK(d.pointer_up(700,400)==ConfirmResult::None);
    const dc4scene::FontRenderer no_font;std::vector<uint8_t> frame(1280*720*4,0);
    auto matches=[&](int x,int y,int sx,int sy){const size_t at=(size_t(y)*1280+x)*4;
        return frame[at]==sx%251 && frame[at+1]==sy%251 && frame[at+2]==(sx+sy)%251;};
    d.draw(-1,-1,false,no_font,&frame);
    CHECK(matches(438,377,444,1));
    CHECK(matches(420,280,380,300));
    d.draw(500,400,true,no_font,&frame);CHECK(matches(438,377,444,129));
    d.pointer_down(700,400);CHECK(d.pointer_up(700,400)==ConfirmResult::No);
    CHECK(d.open(ConfirmKind::Save));d.pointer_down(500,400);CHECK(d.pointer_up(500,400)==ConfirmResult::Yes);
    std::error_code ec;
    if(fs::weakly_canonical(root,ec)==root && !ec && root.parent_path()==parent)fs::remove_all(root,ec);
}
}
int main(){dialog();saves();dc2dm();return failures?1:0;}
