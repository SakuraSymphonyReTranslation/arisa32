#include "arisa32/system_menu.h"
#include "arisa32/resource.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <stdexcept>
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

struct FlowScript {
    std::vector<uint8_t> bytes{1,0,0,0,2,0,0,0,0x69,0x99};
    void param(uint8_t op, uint8_t a, uint8_t b) { bytes.insert(bytes.end(), {op,a,b}); }
    void string(uint8_t op, const std::string& value) {
        bytes.push_back(op);
        if (op == 0x2E) bytes.push_back(0);
        bytes.insert(bytes.end(), value.begin(), value.end()); bytes.push_back(0);
    }
    void words(uint8_t op, uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
        bytes.push_back(op);
        for (uint16_t value : {a,b,c,d}) {
            bytes.push_back(uint8_t(value)); bytes.push_back(uint8_t(value >> 8));
        }
    }
    void text(const std::string& value, uint8_t op = 0x4C) {
        bytes.push_back(op);
        for (unsigned char c : value) bytes.push_back(uint8_t(c - 0x20));
        bytes.push_back(0);
    }
};
void write_fixture(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    if (!out) throw std::runtime_error("cannot write DC2PC flow fixture");
}
void solid_image(const std::filesystem::path& path, uint8_t r, uint8_t g, uint8_t b) {
    // Literal-only CRX with a uniform colour, independent of game assets.
    std::vector<uint8_t> bytes{'C','R','X','G',0,0,0,0,64,0,36,0,1,0,0,0,0,0,0,0};
    std::vector<uint8_t> pixels;
    for (int i = 0; i < 64*36; ++i) pixels.insert(pixels.end(), {b,g,r});
    for (size_t i = 0; i < pixels.size(); i += 8) {
        bytes.push_back(255);
        bytes.insert(bytes.end(), pixels.begin()+i, pixels.begin()+std::min(i+8,pixels.size()));
    }
    write_fixture(path, bytes);
}
void colour(dc4scene::SceneRunner& scene, int r, int g, int b) {
    scene.draw(-1);
    const size_t at = (10*dc4scene::kScreenW+180)*4;
    CHECK(scene.frame()[at] == r); CHECK(scene.frame()[at+1] == g); CHECK(scene.frame()[at+2] == b);
    CHECK(scene.frame()[at+3] == 255);
    CHECK(scene.frame()[0] == 0); // DC2PC letterbox remains black.
}
void synthetic_transitions(const std::filesystem::path& root) {
    solid_image(root/"GRP/bg/red.CRX",160,80,32);
    solid_image(root/"GRP/bg/blue.CRX",32,160,224);
    FlowScript ui; ui.bytes.push_back(0); write_fixture(root/"MES/start.mes",ui.bytes);
    FlowScript script;
    script.string(0x30,"bg\\red"); script.param(0x0D,5,0); script.text("OLD");
    // Loading a different pending background must not change fade-out's source.
    script.string(0x30,"bg\\blue"); script.param(0x0D,2,0);
    script.param(0x12,1,0); script.param(0x0D,1,0); script.text("NEW");
    script.param(0x0D,14,7); script.string(0x2E,"main");
    write_fixture(root/"MES/transition.mes",script.bytes);
    dc4scene::SceneRunner scene(root.string()); scene.set_animation_enabled(true);
    scene.set_message_visible(false); scene.set_effect_speed(11);
    CHECK(scene.load_script((root/"MES/transition.mes").string()));
    scene.set_variable(99,12,1); scene.set_variable(99,16,0);
    CHECK(scene.advance_to_next_text()); CHECK(scene.current_line()=="OLD"); colour(scene,160,80,32);
    std::vector<uint8_t> saved; std::string error;
    CHECK(scene.capture_state(&saved,&error));
    CHECK(!scene.advance_to_next_text()); CHECK(scene.waiting_for_time()); CHECK(scene.animating());
    const auto fade_pc=scene.script_position();
    CHECK(!scene.advance_to_next_text()); CHECK(scene.script_position()==fade_pc);
    colour(scene,160,80,32); scene.update(160); colour(scene,80,40,16);
    scene.update(159); CHECK(scene.waiting_for_time());
    scene.update(1); CHECK(scene.script_ready()); CHECK(!scene.animating()); colour(scene,0,0,0);
    scene.finish_animations(); colour(scene,0,0,0);
    CHECK(!scene.advance_to_next_text()); CHECK(scene.waiting_for_time()); colour(scene,0,0,0);
    scene.finish_script_wait(); CHECK(scene.waiting_for_time()); // flags0 cannot be interrupted.
    scene.update(200); CHECK(scene.script_ready());
    CHECK(!scene.advance_to_next_text()); CHECK(scene.waiting_for_time()); colour(scene,0,0,0);
    scene.update(160); colour(scene,16,80,112); CHECK(!scene.advance_to_next_text());
    scene.update(160); CHECK(scene.script_ready()); colour(scene,32,160,224);
    CHECK(scene.advance_to_next_text()); CHECK(scene.current_line()=="NEW"); colour(scene,32,160,224);
    CHECK(!scene.advance_to_next_text()); CHECK(!scene.scene_ended());
    scene.update(319); CHECK(!scene.advance_to_next_text()); CHECK(!scene.scene_ended());
    scene.update(1); CHECK(!scene.advance_to_next_text());
    CHECK(scene.scene_ended()); CHECK(scene.next_script()=="main"); colour(scene,0,0,0);
    CHECK(scene.restore_state(saved,&error)); CHECK(!scene.waiting_for_time()); CHECK(!scene.animating());
    CHECK(scene.current_line()=="OLD"); colour(scene,160,80,32);

    FlowScript blend;
    blend.string(0x30,"bg\\red"); blend.param(0x0D,10,0); blend.text("SOURCE");
    blend.string(0x30,"bg\\blue"); blend.param(0x0D,0,9); blend.text("DESTINATION");
    write_fixture(root/"MES/blend.mes",blend.bytes);
    CHECK(scene.load_script((root/"MES/blend.mes").string()));
    CHECK(scene.advance_to_next_text()); colour(scene,160,80,32);
    CHECK(!scene.advance_to_next_text()); scene.update(40); colour(scene,112,110,104);
    scene.update(280); colour(scene,32,160,224);
    CHECK(scene.advance_to_next_text()); CHECK(scene.current_line()=="DESTINATION");

    FlowScript speed;
    speed.string(0x30,"bg\\red"); speed.param(0x0D,1,200); speed.text("DONE");
    write_fixture(root/"MES/speed.mes",speed.bytes);
    for (int rate=0;rate<4;++rate) {
        dc4scene::SceneRunner timed(root.string()); timed.set_animation_enabled(true);
        timed.set_message_visible(false); timed.set_effect_speed(11-rate);
        CHECK(timed.load_script((root/"MES/speed.mes").string())); timed.set_variable(99,12,rate);
        CHECK(!timed.advance_to_next_text()); colour(timed,0,0,0);
        const int duration=640/(1<<rate);
        timed.update(duration/2); colour(timed,80,40,16);
        timed.update(duration/2-1); CHECK(timed.waiting_for_time());
        timed.update(1); CHECK(timed.script_ready()); colour(timed,160,80,32);
        CHECK(timed.advance_to_next_text()); CHECK(timed.current_line()=="DONE");
    }
    // Disabled effects and headless execution settle at the
    // exact native endpoint and cannot leave a timed-wait deadlock behind.
    CHECK(scene.load_script((root/"MES/speed.mes").string()));
    CHECK(!scene.advance_to_next_text()); scene.set_animation_enabled(false);
    CHECK(scene.script_ready()); colour(scene,160,80,32);
    CHECK(scene.advance_to_next_text());
    CHECK(scene.load_script((root/"MES/speed.mes").string())); CHECK(scene.advance_to_next_text());
    scene.set_animation_enabled(true); scene.set_effects_enabled(false);
    CHECK(scene.load_script((root/"MES/speed.mes").string())); CHECK(scene.advance_to_next_text());
}
void synthetic_displayed_frame(const std::filesystem::path& root) {
    solid_image(root/"GRP/SYSTEM/MESWIN.CRX",240,80,200);
    solid_image(root/"GRP/bg/black.CRX",0,0,0);
    FlowScript ui;
    ui.words(0x4F,10,470,30,490); ui.string(0x3A,":qsave");
    ui.words(0x50,10,470,10,470); ui.words(0x52,20,20,0,99); ui.param(0x17,0,0);
    write_fixture(root/"MES/displayed_ui.mes",ui.bytes);
    FlowScript script;
    script.param(0x12,1,0); script.string(0x30,"bg\\red");
    script.param(0x0D,0,0); script.text("VISIBLE");
    script.param(0x0D,2,0); script.param(0x12,1,0);
    script.string(0x30,"bg\\blue"); script.param(0x0D,1,0); script.text("REVEALED");
    script.param(0x0D,2,0); script.string(0x30,"bg\\black"); script.text("BLACKDIALOGUE");
    write_fixture(root/"MES/displayed.mes",script.bytes);
    dc4scene::SceneRunner scene(root.string()); scene.set_animation_enabled(true);
    CHECK(scene.load_ui_script((root/"MES/displayed_ui.mes").string()));
    CHECK(scene.show_message_window("SYSTEM/MESWIN.CRX")); scene.set_window_opacity(100);
    CHECK(scene.load_script((root/"MES/displayed.mes").string())); scene.set_variable(99,12,1);
    const auto black = [&] {
        scene.draw(-1); bool opaque_black = true;
        for (size_t i=0;i<scene.frame().size();i+=4) {
            const auto& f=scene.frame();
            if (f[i] || f[i+1] || f[i+2] || f[i+3]!=255) { opaque_black=false; break; }
        }
        CHECK(opaque_black);
        CHECK(scene.hit_test(180,570)==nullptr);
    };
    const auto textbox = [&](int r,int g,int b) {
        scene.draw(-1); const size_t at=(470*dc4scene::kScreenW+180)*4;
        CHECK(scene.frame()[at]==r); CHECK(scene.frame()[at+1]==g); CHECK(scene.frame()[at+2]==b);
    };
    // Loaded message UI must not contaminate the first WAIT or blend source.
    black(); CHECK(!scene.advance_to_next_text()); black();
    scene.update(200); CHECK(!scene.advance_to_next_text()); black();
    scene.update(40); colour(scene,60,30,12); textbox(0,0,0);
    scene.update(280); colour(scene,160,80,32); textbox(0,0,0);
    CHECK(scene.advance_to_next_text()); textbox(240,80,200);
    const auto action=scene.hit_test(180,570); CHECK(action && *action==":qsave");
    std::vector<uint8_t> saved; std::string error; CHECK(scene.capture_state(&saved,&error));
    CHECK(!scene.advance_to_next_text()); scene.update(160); textbox(120,40,100);
    CHECK(scene.hit_test(180,570)==nullptr);
    scene.update(160); black(); scene.finish_animations(); black();
    CHECK(!scene.advance_to_next_text()); black(); scene.update(200);
    CHECK(!scene.advance_to_next_text()); black();
    // Mode1 reveals scene layers; the textbox reappears with the next line.
    scene.update(160); colour(scene,16,80,112); textbox(0,0,0);
    scene.update(160); colour(scene,32,160,224); textbox(0,0,0);
    CHECK(scene.advance_to_next_text()); textbox(240,80,200);
    CHECK(!scene.advance_to_next_text()); scene.update(320); black();
    // Real scripts also fade out, load bg/black, then speak without mode1.
    CHECK(scene.advance_to_next_text()); CHECK(scene.current_line()=="BLACKDIALOGUE");
    colour(scene,0,0,0); textbox(240,80,200);
    CHECK(scene.restore_state(saved,&error)); colour(scene,160,80,32); textbox(240,80,200);
    // Advancing an unrendered restore must fade from its restored dialogue.
    CHECK(scene.restore_state(saved,&error)); CHECK(!scene.advance_to_next_text());
    scene.update(160); colour(scene,80,40,16); textbox(120,40,100);
}
void synthetic_waits(const std::filesystem::path& root) {
    const struct { uint8_t count,flags; int duration; } waits[] = {
        {1,0,200},{255,0,51000},{254,0,50800},{3,2,600},{3,8,30},{3,10,60}};
    for (const auto& wait:waits) {
        FlowScript script; script.param(0x12,wait.count,wait.flags); script.text("DONE");
        write_fixture(root/"MES/wait.mes",script.bytes);
        dc4scene::SceneRunner scene(root.string()); scene.set_animation_enabled(true);
        CHECK(scene.load_script((root/"MES/wait.mes").string()));
        CHECK(!scene.advance_to_next_text()); CHECK(scene.waiting_for_time());
        scene.finish_script_wait(); CHECK(scene.waiting_for_time());
        scene.update(wait.duration-1); CHECK(!scene.advance_to_next_text());
        scene.update(1); CHECK(scene.script_ready());
        CHECK(scene.advance_to_next_text()); CHECK(scene.current_line()=="DONE");
    }
    for (uint8_t count:{uint8_t(0),uint8_t(5)}) {
        FlowScript script; script.param(0x12,count,1); script.text("INTERRUPTED");
        write_fixture(root/"MES/interrupt.mes",script.bytes);
        dc4scene::SceneRunner scene(root.string()); scene.set_animation_enabled(true);
        CHECK(scene.load_script((root/"MES/interrupt.mes").string()));
        CHECK(!scene.advance_to_next_text()); CHECK(scene.waiting_for_time());
        if (!count) {scene.update(60000);CHECK(scene.waiting_for_time());}
        scene.finish_script_wait(); CHECK(scene.advance_to_next_text()); CHECK(scene.current_line()=="INTERRUPTED");
    }
    FlowScript indefinite; indefinite.param(0x12,0,0); indefinite.text("UNREACHED");
    write_fixture(root/"MES/indefinite.mes",indefinite.bytes);
    dc4scene::SceneRunner scene(root.string()); scene.set_animation_enabled(true);
    CHECK(scene.load_script((root/"MES/indefinite.mes").string()));
    CHECK(!scene.advance_to_next_text()); scene.update(60000); scene.finish_script_wait();
    CHECK(scene.waiting_for_time()); CHECK(!scene.advance_to_next_text());

    FlowScript reference; reference.param(0x12,0,4); reference.text("ARMED");
    reference.param(0x12,1,4); reference.text("ELAPSED");
    reference.param(0x12,1,5); reference.text("SECOND");
    write_fixture(root/"MES/reference.mes",reference.bytes);
    CHECK(scene.load_script((root/"MES/reference.mes").string()));
    CHECK(scene.advance_to_next_text()); CHECK(scene.current_line()=="ARMED"); scene.update(75);
    CHECK(!scene.advance_to_next_text()); scene.update(124); CHECK(scene.waiting_for_time());
    scene.update(1); CHECK(scene.script_ready()); CHECK(scene.advance_to_next_text());
    CHECK(scene.current_line()=="ELAPSED");
    CHECK(!scene.advance_to_next_text()); scene.finish_script_wait(); CHECK(scene.waiting_for_time());
    scene.update(199); CHECK(scene.waiting_for_time()); scene.update(1);
    CHECK(scene.advance_to_next_text()); CHECK(scene.current_line()=="SECOND");

    // Profile isolation: native raw0F remains unknown; modern 0F still selects
    // transitions and modern WAIT255 retains its indefinite sentinel.
    const auto& native=dc4::mes_opcode_profile(dc4::MesDialect::ClassicDC2,0x9969);
    CHECK(native.opcodes[0x0F].operation==dc4::Opcode::UNKNOWN);
    const auto& modern=dc4::mes_opcode_profile(dc4::MesDialect::Modern,0xAAB6);
    CHECK(modern.opcodes[0x0F].operation==dc4::Opcode::TRANSITION);
    FlowScript legacy; legacy.bytes={1,0,0,0,3,0,0,0,0,0,0xB6,0xAA,0};
    legacy.param(0x15,255,0); legacy.text("MODERN",0x6B);
    write_fixture(root/"MES/modern.mes",legacy.bytes);
    CHECK(scene.load_script((root/"MES/modern.mes").string()));
    CHECK(!scene.advance_to_next_text()); scene.update(51000); CHECK(scene.waiting_for_time());
    scene.finish_script_wait(); CHECK(scene.advance_to_next_text()); CHECK(scene.current_line()=="MODERN");
}
}

int main(int argc, char** argv) {
    const auto synthetic=std::filesystem::temp_directory_path()/
        ("arisa32-dc2pc-transitions-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    synthetic_transitions(synthetic); synthetic_displayed_frame(synthetic); synthetic_waits(synthetic);
    std::filesystem::remove_all(synthetic);
    if (argc < 2) { std::cout << "Installed DC2PC flow checks skipped.\n"; return failures?1:0; }
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
    // The host reopens the catalogue after synchronizing newly seen scenes.
    menu.resume_scenes();
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
