#include "arisa32/scene.h"
#include "arisa32/crx.h"

#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
namespace {
int failures = 0;
#define CHECK(v) do { if (!(v)) { std::cerr << __LINE__ << ": " #v "\n"; ++failures; } } while (0)
bool near(double a, double b) { return std::abs(a - b) < .001; }
float scalar(const std::vector<uint8_t>& bytes, size_t at) {
    float value; std::memcpy(&value, bytes.data()+at, 4); return value;
}
void seal(std::vector<uint8_t>& bytes) {
    const uint32_t size = uint32_t(bytes.size() - 24);
    for (int i = 0; i < 4; ++i) bytes[12+i] = uint8_t(size >> (i*8));
    uint64_t hash = 14695981039346656037ull;
    for (size_t i = 24; i < bytes.size(); ++i) { hash ^= bytes[i]; hash *= 1099511628211ull; }
    for (int i = 0; i < 8; ++i) bytes[16+i] = uint8_t(hash >> (i*8));
}
void write(const fs::path& path, const std::vector<uint8_t>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    CHECK(bool(out));
}
struct Script {
    std::vector<uint8_t> bytes{0,0,0,0,0x9A,0x9F,0};
    std::vector<uint32_t> labels;
    void words(int op, int a, int b, int c, int d) {
        bytes.push_back(uint8_t(op));
        for (int v : {a,b,c,d}) { bytes.push_back(uint8_t(v)); bytes.push_back(uint8_t(v >> 8)); }
    }
    void slot(int index, const std::string& name) {
        bytes.insert(bytes.end(), {0x3A,uint8_t(index)});
        bytes.insert(bytes.end(), name.begin(), name.end()); bytes.push_back(0);
    }
    void text(const std::string& line) {
        const auto index=labels.size();
        bytes.insert(bytes.end(), {3,uint8_t(index),uint8_t(index>>8)});
        labels.push_back(uint32_t(bytes.size()-4));
        bytes.push_back(0x61);
        for (unsigned char c : line) bytes.push_back(uint8_t(c - 0x20));
        bytes.insert(bytes.end(), {0,6,0,0});
    }
    void save(const fs::path& root) {
        auto data = bytes; data.push_back(0);
        std::vector<uint8_t> table;
        for (auto offset:labels) for (int i=0;i<4;++i) table.push_back(uint8_t(offset>>(i*8)));
        for (size_t index=0;index<labels.size();++index) {
            table.push_back(uint8_t(index)); table.push_back(uint8_t(index>>8));
        }
        for (int i=0;i<4;++i) data[i]=uint8_t(labels.size()>>(i*8));
        data.insert(data.begin()+4,table.begin(),table.end());
        write(root / "MES/main.mes", data);
        write(root / "MES/DS_1129B_CM.mes", {0,0,0,0,0x9A,0x9F,0,0});
    }
};
void image(const fs::path& path, int width = 8, int height = 8, int (*shade)(int) = nullptr) {
    std::vector<uint8_t> bytes{'C','R','X','G',0,0,0,0,uint8_t(width),uint8_t(width >> 8),
                               uint8_t(height),uint8_t(height >> 8),1,0,0,0,0,0,0,0};
    const int stride = (width * 3 + 3) & ~3;
    for (int i = 0; i < stride * height; ++i) {
        if (i % 8 == 0) bytes.push_back(255);
        bytes.push_back(shade ? uint8_t(shade(i % stride / 3)) : 200);
    }
    write(path, bytes);
}
void start(dc4scene::SceneRunner& scene, const fs::path& root, bool animate = true) {
    scene.set_animation_enabled(animate);
    CHECK(scene.load_script((root / "MES/main.mes").string()));
    CHECK(scene.advance_to_next_text());
}
void masked_transitions(const fs::path& root) {
    image(root / "GRP/BG/light.CRX",1280,8);
    image(root / "GRP/BG/black.CRX",1280,8,[](int) { return 0; });
    image(root / "GRP/MSK/forward.CRX",1280,4,[](int x) { return x * 255 / 1279; });
    image(root / "GRP/MSK/reverse.CRX",1280,4,[](int x) { return 255 - x * 255 / 1279; });
    Script script;
    script.slot(1,"BG\\light"); script.slot(16,"MSK\\forward"); script.text("AIRPORT");
    script.bytes.insert(script.bytes.end(), {0x0E,32,0});
    script.slot(1,"BG\\black");
    script.bytes.insert(script.bytes.end(), {0x0E,21,5,0x13,1,0,0x0E,32,0});
    script.slot(1,"BG\\light"); script.slot(16,"MSK\\reverse");
    script.bytes.insert(script.bytes.end(), {0x0E,21,3}); script.text("COAST");
    script.slot(16,"MSK\\missing"); script.bytes.insert(script.bytes.end(), {0x0E,32,0});
    script.slot(1,"BG\\black"); script.bytes.insert(script.bytes.end(), {0x0E,21,5}); script.text("DONE");
    script.save(root);
    dc4scene::SceneRunner scene(root.string()); start(scene,root);
    scene.set_variable(99,10,1);
    const auto pixel = [&](int x) { return scene.frame()[(360 * 1280 + x) * 4]; };
    std::vector<uint8_t> state, pc;
    CHECK(scene.capture_state(&state)); CHECK(scene.capture_pc_save(&pc));
    for (bool native : {false,true}) {
        CHECK(native ? scene.restore_pc_save(pc) : scene.restore_state(state));
        scene.set_effect_speed(native ? 0 : 11);
        CHECK(!scene.advance_to_next_text() && scene.waiting_for_time() && scene.current_line().empty());
        scene.draw(-1); CHECK(pixel(0)==200 && pixel(1279)==200);
        scene.update(500); scene.draw(-1);
        CHECK(pixel(0)<=1 && pixel(640)>=98 && pixel(640)<=101 && pixel(1279)>=199);
        scene.update(499); CHECK(scene.waiting_for_time());
        scene.update(1); scene.draw(-1); CHECK(scene.script_ready() && pixel(1279)==0);
        CHECK(!scene.advance_to_next_text()); scene.update(199); CHECK(scene.waiting_for_time());
        scene.update(1); CHECK(scene.script_ready());
        CHECK(!scene.advance_to_next_text()); scene.update(300); scene.draw(-1);
        CHECK(pixel(0)<=1 && pixel(640)>=98 && pixel(640)<=101 && pixel(1279)>=199);
        scene.update(299); CHECK(scene.waiting_for_time());
        scene.update(1); CHECK(scene.advance_to_next_text() && scene.current_line()=="COAST");
        CHECK(scene.advance_to_next_text() && scene.current_line()=="DONE");
    }
    CHECK(scene.restore_state(state)); CHECK(!scene.advance_to_next_text());
    scene.finish_script_wait(); scene.draw(-1); CHECK(pixel(0)==0 && pixel(1279)==0);
    CHECK(scene.restore_state(state)); scene.set_variable(99,10,0);
    CHECK(!scene.advance_to_next_text()); scene.update(200);
    CHECK(scene.advance_to_next_text() && scene.current_line()=="COAST");
}
void screen_petals(const fs::path& root) {
    image(root / "GRP/SYSTEM/EFFCHANGESCREEN.CRX", 998,720,[](int) { return 255; });
    for (int pattern : {0,1}) {
        const int duration = pattern ? 10 : 6;
        Script script;
        script.slot(1,"BG\\BLACK"); script.slot(17,"SYSTEM\\EFFCHANGESCREEN"); script.text("START");
        script.bytes.insert(script.bytes.end(), {0x37,uint8_t(duration),uint8_t(pattern),0x13,254,0});
        script.text("AFTER");
        script.slot(17,"-"); script.bytes.insert(script.bytes.end(), {0x37,6,0,0x13,254,0}); script.text("END");
        script.save(root);
        dc4scene::SceneRunner scene(root.string()); start(scene,root);
        scene.set_variable(99,10,1);
        std::vector<uint8_t> state,pc;
        CHECK(scene.capture_state(&state) && scene.capture_pc_save(&pc));
        for (bool native : {false,true}) {
            CHECK(native ? scene.restore_pc_save(pc) : scene.restore_state(state));
            scene.set_effect_speed(native ? 11 : 0);
            CHECK(!scene.advance_to_next_text() && scene.waiting_for_time());
            scene.update(duration * 50); scene.draw(-1);
            const auto bright = [&](int x) {return scene.frame()[(360 * 1280 + x) * 4] == 255;};
            CHECK(bright(130) && (bright(1150) == bool(pattern)));
            scene.update(duration * 150 + 40); scene.draw(-1);
            CHECK(scene.animating() && scene.waiting_for_time());
            int lit=0, solid=0;
            for (size_t i=0;i<scene.frame().size();i+=4) {
                lit += scene.frame()[i] > 0;
                solid += scene.frame()[i] == 255;
            }
            CHECK(lit > 100 && solid == 0);
            scene.update(359); CHECK(scene.waiting_for_time());
            scene.update(1); CHECK(!scene.animating() && scene.advance_to_next_text());
            CHECK(scene.current_line()=="AFTER");
            CHECK(scene.advance_to_next_text() && scene.current_line()=="END");
        }
        CHECK(scene.restore_state(state)); scene.set_visual_effects_enabled(false);
        CHECK(scene.advance_to_next_text() && scene.current_line()=="AFTER");
    }
}
void sprites(const fs::path& root) {
    image(root / "GRP/BST/body.CRX"); image(root / "GRP/BST/accessory.CRX");
    Script script;
    script.slot(2,"BST\\body"); script.slot(3,"BST\\body"); script.slot(6,"BST\\accessory");
    script.words(0x7E,100,100,0,0); script.words(0x7E,200,100,1,0);
    script.words(0x7E,0,0,0,0x40); script.text("ENTRANCE");
    script.words(0x80,160,120,0,32); script.text("SLIDE");
    script.words(0x80,999,999,16,1); script.text("NOOP");
    script.words(0x7E,0,2,0,0x80); script.text("EXIT"); script.text("DONE"); script.save(root);
    dc4scene::SceneRunner scene(root.string()); start(scene,root);
    CHECK(scene.sprite(0).cur_x == 80 && scene.sprite(0).alpha == 0 && scene.animating());
    scene.update(167); CHECK(scene.sprite(0).position(0) > 80 && scene.sprite(0).position(0) < 100);
    scene.finish_animations(); CHECK(scene.sprite(0).cur_x == 100 && scene.sprite(0).alpha == 255);
    CHECK(scene.advance_to_next_text());
    CHECK(scene.sprite(0).moves.size() == 1 && scene.sprite(0).moves.front().frames == 192);
    scene.draw(-1); const auto before = scene.frame();
    scene.update(1000); scene.draw(-1); CHECK(scene.frame() != before);
    std::vector<uint8_t> state; std::string error;
    CHECK(scene.capture_state(&state,&error)); scene.update(500); scene.draw(-1); const auto middle = scene.frame();
    CHECK(scene.restore_state(state,&error)); scene.update(500); scene.draw(-1); CHECK(scene.frame() == middle);
    scene.finish_animations(); CHECK(scene.sprite(0).cur_x == 160 && scene.sprite(0).cur_y == 120);
    CHECK(scene.advance_to_next_text()); CHECK(!scene.animating() && scene.sprite(0).cur_x == 160);
    CHECK(scene.advance_to_next_text()); CHECK(scene.sprite(0).fade.active() && scene.sprite(1).visible);
    scene.update(200); CHECK(scene.sprite(0).alpha < 255 && scene.sprite(0).alpha > 0 && scene.sprite(1).alpha == 255);
    scene.update(201); CHECK(!scene.sprite(0).visible && scene.sprite(1).visible);
    scene.draw(-1); CHECK(scene.frame()[(120*1280+160)*4] == 0);
    CHECK(scene.capture_state(&state,&error)); CHECK(scene.restore_state(state,&error));
    CHECK(!scene.sprite(0).visible && scene.sprite(1).visible);
    start(scene,root,false); CHECK(!scene.animating() && scene.sprite(0).cur_x == 100);
    CHECK(scene.advance_to_next_text()); CHECK(scene.sprite(0).cur_x == 160);
    CHECK(scene.advance_to_next_text()); CHECK(scene.advance_to_next_text()); CHECK(!scene.sprite(0).visible);
}
void lens(const fs::path& root) {
    image(root / "GRP/SYSTEM/EFFLENS.CRX", 208, 80);
    Script script;
    script.words(0x93,0,550,264,0); script.words(0x93,1,120,370,15);
    script.bytes.insert(script.bytes.end(), {0x27,15,5});
    script.text("SWEEP");
    script.bytes.insert(script.bytes.end(), {0x27,13,1});
    script.text("AFTER"); script.save(root);
    dc4scene::SceneRunner scene(root.string()); start(scene,root);
    scene.set_variable(99,10,1);
    scene.set_message_visible(false);
    scene.set_visual_effects_enabled(false); scene.draw(-1); const auto base=scene.frame();
    scene.set_visual_effects_enabled(true);
    const auto direction = [&](uint32_t elapsed, double expected) {
        scene.update(elapsed); scene.draw(-1);
        double mx=0, my=0;
        for (int y=0;y<720;y+=2) for (int x=0;x<1280;x+=2) {
            const int dx=x-560, dy=y-274;
            if (dx*dx+dy*dy < 150*150) continue;
            const size_t offset=(size_t(y)*1280+x)*4;
            const int light=int(scene.frame()[offset])-base[offset];
            mx+=double(light)*dx; my+=double(light)*dy;
        }
        CHECK(mx!=0 || my!=0);
        const double degrees=std::atan2(mx,my)*180.0/3.141592653589793;
        CHECK(std::abs(std::remainder(degrees-expected,360.0)) < 15);
    };
    direction(0,120); direction(1000,203.3); direction(1000,286.7); direction(1000,370);
    const auto held=scene.frame();
    scene.update(1200); scene.draw(-1); CHECK(scene.frame()==held);
    scene.set_variable(99,10,0); scene.draw(-1); CHECK(scene.frame()==base);
    scene.set_variable(99,10,1); scene.draw(-1); CHECK(scene.frame()==held);
    CHECK(scene.advance_to_next_text()); scene.set_message_visible(false); scene.draw(-1);
    CHECK(scene.frame()==base);
}
void camera(const fs::path& root) {
    image(root / "GRP/BST/body.CRX"); image(root / "GRP/BG/background.CRX");
    Script script; script.slot(1,"BG\\background"); script.slot(2,"BST\\body");
    script.words(0x7E,128,-40,0,0); script.words(0x7E,78,494,298,16);
    script.text("BASE"); script.words(0x8E,1,140,0,-120); script.text("ZOOM");
    script.words(0x8E,1,140,0,110); script.text("PAN");
    script.words(0x8E,1,100,0,0); script.text("RESET");
    script.words(0x7C,30,-20,99,2); script.text("BACKGROUND");
    script.words(0x8E,0,170,-360,-180); script.text("CLOSEUP");
    script.slot(1,"BG\\background"); script.slot(2,"BST\\body"); script.text("RELOADED");
    script.text("DONE"); script.save(root);
    dc4scene::SceneRunner scene(root.string()); start(scene,root);
    scene.draw(-1); const auto base = scene.frame();
    std::vector<uint8_t> legacy;
    CHECK(scene.capture_state(&legacy)); CHECK(legacy[8] == 15);
    legacy[8] = 14;
    CHECK(scene.restore_state(legacy)); scene.draw(-1); CHECK(scene.frame() == base);
    legacy.resize(legacy.size() - 8); legacy[8] = 13; seal(legacy);
    CHECK(scene.restore_state(legacy)); scene.draw(-1); CHECK(scene.frame() == base);
    CHECK(scene.advance_to_next_text()); CHECK(scene.animating());
    CHECK(near(scene.sprite(0).scale,.78)); scene.update(500);
    CHECK(near(scene.sprite(0).scale,1.015));
    std::vector<uint8_t> state; std::string error;
    CHECK(scene.capture_state(&state,&error)); scene.update(500); scene.draw(-1); const auto zoom = scene.frame();
    CHECK(near(scene.sprite(0).scale,1.25)); CHECK(scene.sprite(0).cur_x == -654 && scene.sprite(0).cur_y == 83);
    CHECK(!scene.animating()); CHECK(scene.restore_state(state,&error)); scene.update(500); scene.draw(-1);
    CHECK(scene.frame() == zoom && !scene.animating());
    std::vector<uint8_t> native;
    CHECK(scene.capture_pc_save(&native,&error));
    if (native.size()==0x11AF8) {
        const size_t bg=0x4C4C,body=bg+10*0x1B0;
        CHECK(near(scalar(native,bg),636) && near(scalar(native,bg+4),524));
        CHECK(near(scalar(native,bg+0x54),140) && near(scalar(native,bg+0x58),140));
        CHECK(near(scalar(native,bg+0xB8),636) && near(scalar(native,bg+0xBC),356));
        CHECK(near(scalar(native,bg+0xC0),100) && native[bg+0xC4]==1);
        CHECK(near(scalar(native,body),-654) && near(scalar(native,body+4),83));
        CHECK(near(scalar(native,body+0x54),125) && near(scalar(native,body+0x58),125));
        CHECK(near(scalar(native,body+0xB8),128) && near(scalar(native,body+0xBC),-40));
        CHECK(near(scalar(native,body+0xC0),78) && native[body+0xC4]==1);
        dc4scene::SceneRunner imported(root.string());
        CHECK(imported.restore_pc_save(native,&error)); imported.draw(-1); CHECK(imported.frame()==zoom);
        auto corrupt=native; std::fill_n(corrupt.begin()+body+0xC0,4,0);
        CHECK(!imported.restore_pc_save(corrupt,&error)); imported.draw(-1); CHECK(imported.frame()==zoom);
        CHECK(imported.advance_to_next_text()); imported.finish_animations(); CHECK(imported.sprite(0).cur_y==-271);
        CHECK(imported.advance_to_next_text()); imported.finish_animations(); imported.draw(-1); CHECK(imported.frame()==base);
        CHECK(near(imported.sprite(0).scale,.78) && imported.sprite(0).cur_x==128 && imported.sprite(0).cur_y==-40);
        CHECK(imported.capture_pc_save(&native,&error));
        CHECK(native[bg+0xC4]==0 && native[body+0xC4]==0);
    } else CHECK(false);
    auto damaged = state;
    for (int i = 1; i <= 8; ++i) damaged[damaged.size()-i] = 255;
    seal(damaged); CHECK(!scene.restore_state(damaged,&error)); scene.draw(-1); CHECK(scene.frame() == zoom);
    CHECK(scene.advance_to_next_text()); scene.finish_animations(); CHECK(scene.sprite(0).cur_y == -271);
    CHECK(scene.advance_to_next_text()); scene.finish_animations(); scene.draw(-1); CHECK(scene.frame() == base);
    CHECK(near(scene.sprite(0).scale,.78) && scene.sprite(0).cur_x == 128 && scene.sprite(0).cur_y == -40);
    CHECK(scene.advance_to_next_text()); scene.update(200); CHECK(scene.animating());
    CHECK(scene.capture_state(&state,&error)); scene.update(200); scene.draw(-1); const auto moved = scene.frame();
    CHECK(!scene.animating()); CHECK(scene.restore_state(state,&error)); scene.finish_animations(); scene.draw(-1);
    CHECK(scene.frame() == moved);
    CHECK(scene.advance_to_next_text()); CHECK(scene.sprite(0).cur_x != 128);
    CHECK(scene.advance_to_next_text());
    CHECK(near(scene.sprite(0).scale,.78) && scene.sprite(0).cur_x == 128 && scene.sprite(0).cur_y == -40);
}
void real_wipes(const fs::path& data, const fs::path& root) {
    std::vector<fs::path> masks;
    for (const auto& file : fs::directory_iterator(data / "GRP/MSK")) {
        if (file.path().extension()==".CRX" || file.path().extension()==".crx") masks.push_back(file.path());
    }
    std::sort(masks.begin(),masks.end());
    CHECK(!masks.empty());
    Script script;
    script.slot(1,"BG\\WHITE"); script.text("START");
    bool outgoing=true;
    for (const auto& file : masks) {
        script.bytes.insert(script.bytes.end(), {0x0E,32,0});
        script.slot(1,outgoing ? "BG\\BLACK" : "BG\\WHITE");
        script.slot(16,"MSK\\"+file.stem().string());
        script.bytes.insert(script.bytes.end(), {0x0E,21,3}); script.text("AFTER");
        outgoing=!outgoing;
    }
    script.save(root);
    dc4scene::SceneRunner scene(data.string()); start(scene,root); scene.set_variable(99,10,1);
    for (const auto& file : masks) {
        std::ifstream in(file,std::ios::binary);
        std::vector<uint8_t> bytes(std::istreambuf_iterator<char>(in),{}), rgba;
        dc4::CrxInfo info{};
        CHECK(dc4::crx_decode(bytes.data(),bytes.size(),&rgba,&info));
        if (info.width<1280 || info.height<720 || rgba.empty()) { CHECK(false); return; }
        scene.set_message_visible(false); scene.draw(-1);
        const auto source=scene.frame();
        CHECK(!scene.advance_to_next_text() && scene.waiting_for_time());
        scene.update(300); scene.draw(-1);
        const auto middle=scene.frame();
        scene.update(300); scene.draw(-1);
        const auto& target=scene.frame();
        int max_error=0;
        for (int y=4;y<720;y+=19) for (int x=3;x<1280;x+=17) {
            const int mask=rgba[(size_t(y)*info.width+x)*4+2];
            const size_t pixel=(y*1280+x)*4;
            const int expected=(source[pixel]*mask+target[pixel]*(255-mask))/255;
            max_error=std::max(max_error,std::abs(middle[pixel]-expected));
        }
        std::cout << file.filename().string() << " midpoint max error " << max_error << '\n';
        CHECK(max_error<=1);
        CHECK(scene.advance_to_next_text());
    }
}
void real_camera(const fs::path& data, const fs::path& root) {
    Script script;
    script.slot(1,"BG\\BG025N");
    script.words(0x7E,128,-40,0,0); script.words(0x7E,78,494,298,16);
    script.slot(2,"BST\\AM_B1_B12_B,BST\\AM_B");
    script.text("BASE"); script.words(0x8E,1,140,0,-120); script.text("ZOOM");
    script.words(0x8E,1,140,0,110); script.text("PAN");
    script.words(0x8E,1,100,0,0); script.text("RESET"); script.save(root);
    dc4scene::SceneRunner scene(data.string()); start(scene,root);
    for (const char* label : {"base", "zoom", "pan", "reset"}) {
        scene.finish_animations(); scene.draw(-1);
        const auto& body = scene.sprite(0);
        CHECK(body.img.w==812 && body.img.h==935 && body.canvas_w==1024 && body.canvas_h==1024);
        CHECK(near(body.frame_cx-body.img.w/2.f,75) && near(body.frame_cy-body.img.h/2.f,89));
        const bool close=std::string(label)=="zoom" || std::string(label)=="pan";
        CHECK(near(body.scale,close?1.25:.78) && body.cur_x==128);
        CHECK(body.cur_y==(std::string(label)=="zoom"?83:std::string(label)=="pan"?-271:-40));
        std::cout << label << ": " << body.cur_x << ',' << body.cur_y << " scale " << body.scale
                  << " crop " << body.frame_cx - body.img.w / 2.f << ',' << body.frame_cy - body.img.h / 2.f
                  << " size " << body.img.w << ',' << body.img.h << " canvas " << body.canvas_w << ',' << body.canvas_h << '\n';
        std::ofstream shot(root / (std::string(label) + ".ppm"), std::ios::binary);
        shot << "P6\n1280 720\n255\n";
        const auto& frame = scene.frame();
        for (size_t i = 0; i < frame.size(); i += 4) shot.write(reinterpret_cast<const char*>(frame.data()+i),3);
        if (std::string(label) != "reset") CHECK(scene.advance_to_next_text());
    }
}
}
int main(int argc, char** argv) {
    const auto root = fs::temp_directory_path() / ("dalsegno-motion-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if (argc == 3 && std::string(argv[1]) == "--wipes") {
        real_wipes(argv[2],root / "wipes");
    } else if (argc > 1) {
        const auto captures = argc > 2 ? fs::path(argv[2]) : root / "real";
        real_camera(argv[1], captures);
    } else { sprites(root / "sprites"); camera(root / "camera"); lens(root / "lens"); masked_transitions(root / "wipes"); screen_petals(root / "petals"); }
    fs::remove_all(root);
    return failures ? 1 : 0;
}
