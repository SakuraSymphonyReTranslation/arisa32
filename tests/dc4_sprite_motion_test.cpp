#include "arisa32/scene.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
namespace {
int failures = 0;
#define CHECK(v) do { if (!(v)) { std::cerr << __LINE__ << ": " #v "\n"; ++failures; } } while (0)
bool near(double a, double b) { return std::abs(a - b) < .001; }
void write(const fs::path& path, const std::vector<uint8_t>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    if (!out) throw std::runtime_error("cannot write motion fixture");
}
struct Script {
    std::vector<uint8_t> bytes{1,0,0,0,3,0,0,0,0,0,0xB6,0xAA,0};
    explicit Script(uint16_t version) { bytes[11] = uint8_t(version >> 8); }
    void words(int op, int a, int b, int c, int d) {
        bytes.push_back(uint8_t(op));
        for (int v : {a,b,c,d}) { bytes.push_back(uint8_t(v)); bytes.push_back(uint8_t(v >> 8)); }
    }
    void slot(int id, const std::string& name) {
        bytes.push_back(0x3C); bytes.push_back(uint8_t(id));
        bytes.insert(bytes.end(), name.begin(), name.end()); bytes.push_back(0);
    }
    void text(const std::string& line) {
        bytes.push_back(0x6B);
        for (unsigned char c : line) bytes.push_back(uint8_t(c - 0x20));
        bytes.push_back(0);
    }
    void save(const fs::path& root) {
        auto data = bytes; data.push_back(0);
        write(root / "MES/main.mes", data); write(root / "MES/dc4_start.mes", data);
    }
};
void image(const fs::path& path) {
    std::vector<uint8_t> bytes{'C','R','X','G',0,0,0,0,8,0,8,0,1,0,0,0,0,0,0,0};
    for (int i = 0; i < 24; ++i) { bytes.push_back(255); bytes.insert(bytes.end(), 8, 200); }
    write(path, bytes);
}
void start(dc4scene::SceneRunner& scene, const fs::path& root) {
    scene.set_animation_enabled(true);
    CHECK(scene.load_script((root / "MES/main.mes").string()));
    CHECK(scene.advance_to_next_text());
}
void tick(dc4scene::SceneRunner& scene, int frame) {
    scene.update(uint32_t((frame * 1000 + 59) / 60 - ((frame - 1) * 1000 + 59) / 60));
}
void trajectories(const fs::path& root, uint16_t version) {
    Script s(version);
    for (int slot = 0; slot < 5; ++slot) {
        s.words(0x8B, slot * 100, 20, slot, 0);
        for (auto stage : {std::pair<int,int>{-10,1}, {0,2}, {-10,1}, {0,2}})
            s.words(0x8E, 0, stage.first, 64 + slot, stage.second);
    }
    s.text("BOUNCE"); s.text("NEXT"); s.save(root);
    dc4scene::SceneRunner scene(root.string()), chunks(root.string()), coarse(root.string());
    start(scene,root); start(chunks,root); start(coarse,root);
    CHECK(scene.animating());
    for (int frame = 1; frame <= 40; ++frame) {
        tick(scene, frame);
        const int local = frame <= 20 ? frame : frame - 20;
        const double dy = local <= 7 ? -10.0 * (local - 1) / 6 : -10.0 + 10.0 * (local - 8) / 12;
        for (int slot = 0; slot < 5; ++slot) {
            CHECK(near(scene.sprite(slot).position(0),slot*100));
            CHECK(near(scene.sprite(slot).position(1),20+dy));
        }
        if (frame % 3 == 0) {
            coarse.update(50);
            for (int slot = 0; slot < 5; ++slot)
                CHECK(near(coarse.sprite(slot).position(1), scene.sprite(slot).position(1)));
        }
        if (frame == 9) {
            std::vector<uint8_t> state; std::string error;
            CHECK(scene.capture_state(&state,&error));
            CHECK(chunks.restore_state(state,&error));
            CHECK(near(chunks.sprite(0).position(1),scene.sprite(0).position(1)));
            chunks.update(517);
        }
    }
    CHECK(!scene.animating()); CHECK(!chunks.animating());
    CHECK(near(chunks.sprite(0).position(1),20));
    CHECK(scene.advance_to_next_text()); CHECK(!scene.animating());
    start(scene,root); scene.update(100); scene.finish_animations();
    CHECK(!scene.animating()); CHECK(scene.sprite(0).cur_y == 20);
    start(scene,root); scene.set_animation_enabled(false);
    CHECK(!scene.animating()); CHECK(scene.sprite(4).cur_y == 20);
}
void origins_and_waves(const fs::path& root, uint16_t version) {
    Script s(version);
    s.words(0x8B,100,200,0,0);
    s.words(0x8E,10,-20,64,1); s.words(0x8E,20,-40,64,2);
    s.words(0x8E,300,400,0,1); s.words(0x8E,-10,5,64,1);
    s.words(0x8E,-10,5,64,1); s.text("ORIGIN");
    s.words(0x8E,60,0,0,32); s.words(0x8E,40,0,360,16);
    s.words(0x8E,60,1,0,32); s.words(0x8E,20,0,360,17);
    s.text("WAVES"); s.text("DONE"); s.save(root);
    dc4scene::SceneRunner scene(root.string()); start(scene,root);
    const auto& moves = scene.sprite(0).moves;
    CHECK(moves.size() == 4);
    CHECK(moves[1].to_x == 120 && moves[1].to_y == 160);
    CHECK(moves[3].to_x == 290 && moves[3].to_y == 405);
    scene.finish_animations(); CHECK(scene.sprite(0).cur_x == 290);
    CHECK(scene.advance_to_next_text());
    scene.update(267);
    CHECK(near(scene.sprite(0).position(0),330));
    CHECK(near(scene.sprite(1).position(1),-20));
    std::vector<uint8_t> state; CHECK(scene.capture_state(&state));
    scene.update(500); const double x = scene.sprite(0).position(0), y = scene.sprite(1).position(1);
    CHECK(scene.restore_state(state)); scene.update(500);
    CHECK(near(scene.sprite(0).position(0),x) && near(scene.sprite(1).position(1),y));
    scene.update(300); CHECK(!scene.animating());
    CHECK(near(scene.sprite(0).position(0),290) && near(scene.sprite(1).position(1),0));

    Script zero(version); zero.words(0x8B,0,0,0,0); zero.words(0x8E,60,0,64,0); zero.text("DEFAULT"); zero.save(root);
    start(scene,root); CHECK(scene.sprite(0).moves.front().frames == 60);
    scene.update(500); CHECK(near(scene.sprite(0).position(0),29));
    scene.update(517); CHECK(near(scene.sprite(0).position(0),60));
}
void other_character_operands(const fs::path& root, uint16_t version) {
    // DC4 corpus: HIY at dc4_asa20190430b:14672, ALI at dc4_kyo20190204a2:32897,
    // SII at dc4_kyo20190204b:4851. These operands address slots, not character IDs.
    for (const auto& sample : {std::array<int,3>{3,-10,2}, {0,-10,3}, {0,11,3}}) {
        const int slot = sample[0], displacement = sample[1], duration = sample[2];
        Script s(version);
        s.words(0x8B,0,100,slot,0);
        s.words(0x8E,0,displacement,64+slot,duration);
        s.text("CORPUS"); s.save(root);
        dc4scene::SceneRunner scene(root.string()); start(scene,root);
        scene.update(uint32_t((1+duration*3)*1000/60+1));
        CHECK(near(scene.sprite(slot).position(1),100+displacement/2.0));
        scene.update(1000);
        CHECK(near(scene.sprite(slot).position(1),100+displacement));
        CHECK(!scene.animating());
    }
}
void dialogue(const fs::path& root, uint16_t version) {
    image(root/"GRP/BST/first.CRX"); image(root/"GRP/BST/second.CRX");
    Script s(version);
    s.slot(5,"BST\\first");
    s.words(0xB0,0,0,0,0); s.words(0xB0,255,0,0,0);
    s.slot(5,"BST\\second"); s.words(0x8B,45,0,0,0x21);
    s.words(0xB0,0,0,0,1);
    s.words(0xB0,0,0,0,0); s.words(0xB0,255,1,0,0);
    s.words(0x8B,70,0,0,0x22); s.words(0xB0,0,0,0,0);
    s.text("@by2@t50@d1@t60@cmd2LINE"); s.text("NEXT"); s.save(root);
    dc4scene::SceneRunner scene(root.string()); start(scene,root);
    CHECK(scene.sprite(1).image == "BST\\first"); CHECK(scene.sprite(1).moves.size() == 4);
    scene.update(100); CHECK(near(scene.sprite(1).position(0),4));
    CHECK(scene.sprite(1).priority == 50);
    std::vector<uint8_t> saved; std::string error; CHECK(scene.capture_state(&saved,&error));
    scene.update(4899); CHECK(scene.sprite(1).image == "BST\\first");
    scene.update(1); CHECK(scene.sprite(1).image == "BST\\second"); CHECK(scene.sprite(1).priority == 45);
    CHECK(scene.sprite(2).priority == 50);
    scene.update(1000); CHECK(scene.sprite(2).priority == 70); CHECK(!scene.animating());
    CHECK(scene.restore_state(saved,&error));
    CHECK(scene.sprite(1).image == "BST\\first");
    scene.update(4900); CHECK(scene.sprite(1).image == "BST\\second");
    CHECK(scene.advance_to_next_text()); CHECK(scene.sprite(2).priority == 70); CHECK(!scene.animating());
    CHECK(scene.restore_state(saved,&error)); scene.set_animation_enabled(false);
    CHECK(scene.sprite(1).image == "BST\\second" && scene.sprite(2).priority == 70);

    Script gesture(version); gesture.text("@t2@bt1@by4TEXT"); gesture.save(root);
    start(scene,root); CHECK(scene.sprite(0).moves.empty()); CHECK(scene.sprite(3).moves.size() == 4);
    scene.update(199); CHECK(scene.sprite(0).moves.empty());
    scene.update(1); CHECK(scene.sprite(0).moves.size() == 4);
    scene.update(100); CHECK(near(scene.sprite(0).position(1),4));
    scene.finish_animations(); CHECK(near(scene.sprite(0).position(1),0));

    Script prefix(version); prefix.text("1234567890@bt3TEXT"); prefix.save(root);
    start(scene,root); scene.update(65); CHECK(scene.sprite(2).moves.empty());
    scene.update(1); CHECK(scene.sprite(2).moves.size() == 4);
}
void entrance(const fs::path& root, uint16_t version) {
    Script s(version);
    for(int slot=0;slot<3;++slot) {
        s.words(0x8B,100,200,slot,0); s.words(0x8B,slot,0,0,64+slot);
    }
    s.text("ENTER"); s.text("DONE"); s.save(root);
    dc4scene::SceneRunner scene(root.string()); start(scene,root);
    CHECK(near(scene.sprite(1).position(0),80) && near(scene.sprite(2).position(0),120));
    scene.update(100);
    CHECK(near(scene.sprite(1).position(0),80+20.0*5/12));
    CHECK(scene.sprite(1).alpha == 106);
    std::vector<uint8_t> state; CHECK(scene.capture_state(&state));
    scene.update(117); CHECK(scene.sprite(1).alpha == 255 && scene.sprite(1).visible);
    CHECK(near(scene.sprite(1).position(0),100)); CHECK(!scene.animating());
    CHECK(scene.restore_state(state)); CHECK(scene.sprite(1).alpha == 106);
    scene.finish_animations(); CHECK(scene.sprite(1).visible && scene.sprite(1).alpha == 255);
    Script cut(version); cut.words(0x8C,100,200,0,0); cut.words(0x8C,2,0,0,64);
    cut.text("CUT"); cut.save(root); start(scene,root);
    CHECK(near(scene.sprite(12).position(0),120)); scene.update(117);
    CHECK(near(scene.sprite(12).position(0),100) && scene.sprite(12).alpha == 255);
    Script interrupted(version);
    interrupted.words(0x8C,1,0,0,64);
    interrupted.words(0xB0,0,0,0,0); interrupted.words(0xB0,255,0,0,0);
    interrupted.words(0x8C,0,3,0,128); interrupted.words(0xB0,0,0,0,0);
    interrupted.text("@t1@d1FADE"); interrupted.save(root); start(scene,root);
    scene.update(100); CHECK(scene.sprite(12).fade.end_alpha == 0);
    scene.update(300); CHECK(!scene.sprite(12).visible && !scene.animating());
}
void installed(const fs::path& data, const fs::path& output) {
    dc4::MesFile script;
    CHECK(script.load_file((data/"MES/dc4_mia20190315c.mes").string()));
    const int expression_ms = script.version == 0xAAB6 ? 4500 : 5000;
    const std::string marker = "@by2@t"+std::to_string(expression_ms/100)+"@d1";
    dc4scene::SceneRunner scene(data.string());
    CHECK(scene.load_script((data/"MES/dc4_mia20190315c.mes").string()));
    scene.set_animation_enabled(true);
    int count = 0;
    while (++count < 400) {
        scene.finish_script_wait();
        if (!scene.advance_to_next_text()) {
            if (scene.waiting_for_time() || scene.script_ready()) { scene.update(10000); continue; }
            break;
        }
        if (scene.current_line().find(marker) != std::string::npos) break;
    }
    if (scene.current_line().find(marker) == std::string::npos)
        throw std::runtime_error("installed replay stopped at " + std::to_string(scene.script_position()) +
            " after " + std::to_string(count) + " advances: " + scene.last_error() + " movie=" + scene.pending_movie() +
            " next=" + scene.next_script() + " line=" + scene.current_line());
    CHECK(scene.current_line().find(marker) != std::string::npos);
    CHECK(scene.sprite(1).image.find("SOR_A1_A08") != std::string::npos);
    CHECK(scene.sprite(1).moves.size() == 4);
    fs::create_directories(output);
    const auto dump = [&](const std::string& name) {
        scene.draw(-1);
        std::ofstream image(output/(name+".ppm"), std::ios::binary);
        image << "P6\n1280 720\n255\n";
        const auto& pixels = scene.frame();
        for (size_t i = 0; i < pixels.size(); i += 4)
            image.write(reinterpret_cast<const char*>(pixels.data()+i), 3);
        CHECK(bool(image));
    };
    std::ofstream sorane(output/"engine-sorane.csv"); sorane << "frame,milliseconds,x,y\n";
    for (int frame = 0; frame <= 44; ++frame) {
        if (frame) tick(scene,frame);
        sorane << frame << ',' << (frame*1000+59)/60 << ',' << scene.sprite(1).position(0) << ',' << scene.sprite(1).position(1) << '\n';
        if (frame % 11 == 0) dump("sorane-"+std::to_string(frame));
    }
    scene.update(2766); CHECK(scene.sprite(1).image.find("SOR_A1_A08") != std::string::npos);
    std::vector<uint8_t> saved; CHECK(scene.capture_state(&saved));
    scene.update(expression_ms-3500); CHECK(scene.sprite(1).image.find("SOR_A2_A01") != std::string::npos);
    dump("sorane-"+std::to_string(expression_ms)+"ms");
    CHECK(scene.restore_state(saved));
    CHECK(scene.advance_to_next_text());
    CHECK(scene.sprite(0).image.find("NIN_A1_AB09") != std::string::npos);
    CHECK(scene.sprite(0).moves.size() == 4);
    std::ofstream csv(output/"engine-nino.csv"); csv << "frame,milliseconds,x,y\n";
    for (int frame = 0; frame <= 40; ++frame) {
        if (frame) tick(scene,frame);
        csv << frame << ',' << (frame*1000+59)/60 << ',' << scene.sprite(0).position(0) << ',' << scene.sprite(0).position(1) << '\n';
        if (frame == 0 || frame == 7 || frame == 20 || frame == 27 || frame == 40)
            dump("nino-"+std::to_string(frame));
    }
    CHECK(!scene.sprite(0).moving());
}
}
int main(int argc, char** argv) {
    const auto root = fs::current_path()/"build"/"sprite-motion-test"/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    try {
        for(uint16_t version : {0xAAB6,0xABB6}) {
            auto data=root/std::to_string(version);
            trajectories(data/"trajectory",version); origins_and_waves(data/"origins",version);
            other_character_operands(data/"other-characters",version);
            dialogue(data/"dialogue",version); entrance(data/"entrance",version);
        }
        if(argc>1) installed(argv[1],argc>2?fs::path(argv[2]):root);
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; ++failures; }
    fs::remove_all(root);
    if(failures) { std::cerr << failures << " motion checks failed\n"; return 1; }
    std::cout << "DC4/DC4PH sprite motion regressions passed\n";
}
