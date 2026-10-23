#include "arisa32/system_menu.h"
#include "arisa32/resource.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include "arisa32/names.h"

namespace {
int failures=0;
void check(bool value,int line){if(!value){std::cerr<<"line "<<line<<'\n';++failures;}}
#define CHECK(x) check(bool(x),__LINE__)
bool has(dc4ui::SystemMenu& menu,const std::string& tag){
    for(const auto& control:menu.controls())if(control.enabled&&control.tag==tag)return true;
    return false;
}
void capture(dc4ui::SystemMenu& menu){
    if(const char* output=std::getenv("DC2PC_MENU_CAPTURES")){
        std::vector<uint8_t> rgba;dc4scene::FontRenderer font;
#ifdef _WIN32
        font.load("C:/Windows/Fonts/msgothic.ttc",2,16,false);
#endif
        menu.draw(-1,-1,false,font,&rgba);
        std::ofstream out(std::filesystem::path(output)/"replay-unlocked.ppm",std::ios::binary);
        out<<"P6\n1280 720\n255\n";
        for(size_t i=0;i<rgba.size();i+=4)out.write(reinterpret_cast<const char*>(rgba.data()+i),3);
    }
}
}

int main(int argc, char** argv) {
    if (argc < 2) { std::cout << "Installed DC2PC flow checks skipped.\n"; return 0; }
    if (argc > 2) dc4::configure_resources(argv[1], argv[2]);
    dc4ui::SystemMenu menu;
    std::string error;
    if (!menu.load(argv[1], &error)) { std::cerr << error; return 1; }
    dc4scene::SceneRunner scene(argv[1]);
    scene.set_use_system_menu(true);
    scene.set_observer([&](const dc4::MesCommand& raw, const std::string& script) {
        auto c = dc4::mes_engine_command(raw);
        if (c.opcode() == dc4::Opcode::TRANSFER || c.opcode()==dc4::Opcode::MOVIE)
            std::cout << script << " @" << raw.pos << " raw=" << int(raw.op) << " " << raw.str
                      << " replay=" << scene.variable(9,7) << '\n';
        menu.observe(raw, script);
    });
    auto start=menu.activate("start");
    if(menu.confirming())start=menu.answer_confirmation(true);
    CHECK(start.type==dc4ui::SystemMenuActionType::NewGame);
    if (!scene.load_script(std::string(argv[1])+"/MES/"+start.asset+".mes")) return 1;
    menu.initialize_scene(scene,start.type==dc4ui::SystemMenuActionType::StartScene);
    CHECK(scene.variable(9,7)==0);
    bool movie=false;
    for (int i=0; i<1000 && scene.script_name()==start.asset+".mes"; ++i) {
        scene.advance_to_next_text(); menu.sync_progress(scene);
        if (scene.waiting_for_time()) scene.finish_script_wait();
        if (!scene.pending_movie().empty()) {movie=true;scene.finish_movie();}
        if(scene.scene_ended())break;
    }
    CHECK(movie);
    CHECK(!scene.scene_ended());CHECK(scene.last_error().empty());
    CHECK(scene.script_name()=="fst_1216_b1_cmn.mes");
    CHECK(menu.scene_seen(start.asset));CHECK(menu.variable(98,1036)==1);
    menu.open(dc4ui::SystemMenuPage::Scenes);CHECK(has(menu,"other"));
    menu.activate("other");CHECK(menu.variable(3,3)==12);
    CHECK(has(menu,"sce0"));CHECK(!has(menu,"sce2"));capture(menu);
    auto replay=menu.activate("sce0");CHECK(menu.confirming());
    if(menu.confirming())replay=menu.answer_confirmation(true);
    CHECK(replay.type==dc4ui::SystemMenuActionType::StartScene);
    CHECK(replay.asset==start.asset);
    CHECK(scene.load_script(std::string(argv[1])+"/MES/"+replay.asset+".mes",replay.label));
    menu.initialize_scene(scene,true);CHECK(scene.variable(9,7)==1);
    movie=false;
    for(int i=0;i<1000 && !scene.scene_ended();++i){
        scene.advance_to_next_text();
        if(scene.waiting_for_time())scene.finish_script_wait();
        if(!scene.pending_movie().empty()){movie=true;scene.finish_movie();}
    }
    CHECK(scene.scene_ended());CHECK(scene.last_error().empty());
    CHECK(scene.next_script()=="sc_end");CHECK(!movie);
    menu.resume_scenes();std::cout<<"RESUME route="<<menu.variable(3,3)<<" top="<<menu.variable(99,246)<<" error="<<menu.error()<<'\n';CHECK(menu.variable(3,3)==12);CHECK(has(menu,"sce0"));
    menu.mark_scene_seen("fst_1216_b13_cmn");
    menu.scroll(1);std::cout<<"SCROLL top="<<menu.variable(99,246)<<" combined="<<menu.variable(206,80)<<" error="<<menu.error()<<'\n';CHECK(menu.variable(99,246)==2);CHECK(has(menu,"sce4"));
    auto scrolled=menu.activate("sce4");
    if(menu.confirming())scrolled=menu.answer_confirmation(true);
    CHECK(scrolled.asset=="fst_1216_b13_cmn");
    menu.resume_scenes();CHECK(menu.variable(99,246)==2);CHECK(has(menu,"sce4"));
    dc4::MesFile mes;CHECK(mes.load_file(std::string(argv[1])+"/MES/fst_1217_a1_cmn.mes"));
    dc4::MesVm vm(mes);
    vm.write_variable(0,0,-1);CHECK(vm.read_variable(0,0)==0);
    vm.write_variable(0,0,300);CHECK(vm.read_variable(0,0)==255);
    vm.write_variable(206,80,398);CHECK(vm.read_variable(206,80)==398);
    CHECK(vm.read_variable(6,82)==1 && vm.read_variable(6,83)==142);
    vm.write_variable(206,80,-2);CHECK(vm.read_variable(206,80)==-2);
    vm.write_variable(97,1335,1);vm.write_variable(98,1433,1);
    vm.write_variable(100,100,1);CHECK(vm.read_variable(100,100)==0);
    dc4::MesVmSnapshot snapshot;CHECK(vm.capture_snapshot(&snapshot));
    dc4::MesVm other(mes);CHECK(other.restore_snapshot(snapshot,&error));
    CHECK(other.read_variable(206,80)==-2);
    CHECK(other.read_variable(97,1335)==1 && other.read_variable(98,1433)==1);
    // Execute the actual branching tail using each route selector.
    dc4::MesStream stream(mes);dc4::MesCommand raw;uint32_t tail=0;
    while(stream.next(&raw))if(raw.op==0x35 && raw.str=="end")tail=raw.pos;
    CHECK(tail>0);
    for(int route=0;route<4;++route){
        dc4::MesVm branch(mes);branch.seek_to_pc(tail);branch.write_variable(9,7,0);
        if(route<3)branch.write_variable(7,route==2?40:route,1);
        std::string transfer;
        while(branch.next(&raw)){auto c=dc4::mes_engine_command(raw);if(c.opcode()==dc4::Opcode::TRANSFER)transfer=c.str;}
        std::cout<<"BRANCH route="<<route<<" -> "<<transfer<<" error="<<branch.error()<<'\n';
        CHECK(branch.error().empty());CHECK(transfer=="fst_1217_a2_cm"+std::to_string(route+1));
    }
    const auto profile=std::filesystem::temp_directory_path()/"arisa32-dc2pc-flow.profile";
    std::ifstream nf(std::string(argv[1])+"/DAT/names.dat",std::ios::binary);
    std::vector<uint8_t> nb((std::istreambuf_iterator<char>(nf)),{});dc4::GameNames names;
    CHECK(dc4::decode_game_names(nb,&names));
    if(names.cg.empty())return 1;
    raw={};raw.dc2pc=true;raw.op=0x30;raw.kind=dc4::CmdKind::Str;raw.str=names.cg.back();
    menu.observe(raw);CHECK(menu.cg_seen(names.cg.back()));CHECK(menu.variable(97,1335)==1);
    CHECK(menu.save_profile(profile.string(),&error));
    dc4ui::SystemMenu restored;CHECK(restored.load(argv[1],&error));
    CHECK(restored.load_profile(profile.string(),&error));
    CHECK(restored.scene_seen(start.asset));CHECK(restored.variable(98,1036)==1);
    CHECK(restored.cg_seen(names.cg.back()));CHECK(restored.variable(97,1335)==1);
    menu.mark_cg_seen(names.cg[1223]);menu.mark_cg_seen(names.cg[1224]);
    menu.mark_cg_seen(names.cg[1230]);
    menu.open(dc4ui::SystemMenuPage::Gallery);menu.activate("other");
    CHECK(has(menu,"cg1223"));
    menu.activate("cg1223");
    dc4scene::FontRenderer font;std::vector<uint8_t> a,b,c;
    menu.draw(-1,-1,false,font,&a);menu.scroll(1);menu.draw(-1,-1,false,font,&b);
    CHECK(a!=b);menu.scroll(1);menu.draw(-1,-1,false,font,&c);CHECK(a==c);
    menu.back();CHECK(has(menu,"cg1223"));
    menu.scroll(1);CHECK(menu.error().empty());CHECK(!has(menu,"cg1223"));
    menu.scroll(-1);CHECK(has(menu,"cg1223"));
    restored.open(dc4ui::SystemMenuPage::Scenes);restored.activate("other");CHECK(has(restored,"sce0"));
    std::filesystem::remove(profile);
    return failures?1:0;
}
