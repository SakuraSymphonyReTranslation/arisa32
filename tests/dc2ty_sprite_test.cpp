#include "arisa32/scene.h"
#include "miniz.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
namespace {
int failures = 0;
void check(bool value, const char* expression, int line) {
    if (!value) { std::cerr << line << ": " << expression << '\n'; ++failures; }
}
#define CHECK(value) check(bool(value), #value, __LINE__)
using Color = std::array<uint8_t,3>;
const Color body{200,40,30}, face{30,210,50}, normal_face{30,50,220}, black{0,0,0};

void word(std::vector<uint8_t>& bytes, uint16_t value) {
    bytes.push_back(uint8_t(value)); bytes.push_back(uint8_t(value >> 8));
}
void write_file(const fs::path& path, const std::vector<uint8_t>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    if (!out) throw std::runtime_error("cannot write DC2TY sprite fixture");
}
struct Script {
    std::vector<uint8_t> bytes;
    explicit Script(uint16_t version = 0x5F69) : bytes{1,0,0,0,2,0,0,0} { word(bytes, version); }
    void words(uint8_t op, uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
        bytes.push_back(op);
        for (uint16_t value : {a,b,c,d}) word(bytes,value);
    }
    void param(uint8_t op, const std::string& value) {
        bytes.insert(bytes.end(), {op,0});
        bytes.insert(bytes.end(), value.begin(), value.end()); bytes.push_back(0);
    }
    void text(const std::string& value) {
        bytes.push_back(0x4C);
        for (unsigned char c : value) bytes.push_back(uint8_t(c - 0x20));
        bytes.push_back(0);
    }
};
void image(const fs::path& path, int x, int y, int w, int h, int patch_x, int patch_y,
           Color color, int canvas_w = 0, int canvas_h = 0) {
    std::vector<uint8_t> bytes{'C','R','X','G'};
    for (int value : {x,y,w,h,canvas_w ? 3 : 2,0,0,0}) word(bytes,uint16_t(value));
    if (canvas_w) {
        bytes.insert(bytes.end(), {1,0,0,0,0,0,0,0});
        word(bytes,uint16_t(canvas_w)); word(bytes,uint16_t(canvas_h));
        bytes.insert(bytes.end(), 8, 0);
    }
    std::vector<uint8_t> raw;
    raw.reserve(size_t(h) * (w * 3 + 1));
    for (int row = 0; row < h; ++row) {
        raw.push_back(0);
        Color previous{};
        for (int col = 0; col < w; ++col) {
            const Color pixel = col >= patch_x && col < patch_x + 40 && row >= patch_y && row < patch_y + 40
                              ? color : Color{255,0,255};
            for (int channel = 2; channel >= 0; --channel)
                raw.push_back(uint8_t(pixel[channel] - previous[channel]));
            previous = pixel;
        }
    }
    mz_ulong size = mz_compressBound(mz_ulong(raw.size()));
    std::vector<uint8_t> compressed(size);
    if (mz_compress2(compressed.data(), &size, raw.data(), mz_ulong(raw.size()), MZ_BEST_SPEED) != MZ_OK)
        throw std::runtime_error("cannot compress sprite fixture");
    bytes.insert(bytes.end(), compressed.begin(), compressed.begin()+size);
    write_file(path,bytes);
}
Color pixel(const dc4scene::SceneRunner& scene, int x, int y) {
    const auto* p = scene.frame().data() + (size_t(y)*dc4scene::kScreenW+x)*4;
    return {p[0],p[1],p[2]};
}
void capture(const dc4scene::SceneRunner& scene, const fs::path& path) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path,std::ios::binary);
    out << "P6\n" << dc4scene::kScreenW << ' ' << dc4scene::kScreenH << "\n255\n";
    for (size_t i = 0; i < scene.frame().size(); i += 4)
        out.write(reinterpret_cast<const char*>(scene.frame().data()+i),3);
    if (!out) throw std::runtime_error("cannot capture sprite scene");
}
void next(dc4scene::SceneRunner& scene, const char* line) {
    CHECK(scene.advance_to_next_text()); CHECK(scene.current_line() == line); scene.draw(-1);
}
void loose_sprites(const fs::path& root, uint16_t version) {
    image(root/"GRP/BST/body_b.CRX",0,0,1260,1080,400,200,body);
    image(root/"GRP/BST/body.CRX",0,0,882,756,300,200,body);
    image(root/"GRP/BST/face_b.CRX",400,200,40,40,0,0,face);
    image(root/"GRP/BST/face.CRX",300,200,40,40,0,0,normal_face);
    image(root/"GRP/BST/registered.CRX",400,200,80,80,0,0,body,1260,1080);
    Script script(version);
    script.words(0x64,210,0,0,0); script.words(0x65,100,441,322,16);
    script.param(0x2B,"bst\\body_b"); script.text("CLOSEUP");
    script.param(0x2C,"bst\\face"); script.text("EXPRESSION");
    script.param(0x2C,"-"); script.text("CLEAN");
    script.param(0x2B,"--"); script.text("CACHED");
    script.words(0x65,75,441,322,16); script.text("REDUCED_CLOSEUP");
    script.param(0x2B,"bst\\body"); script.text("REDUCED_STANDARD");
    script.words(0x65,100,441,322,16); script.text("STANDARD");
    script.param(0x2C,"bst\\face_b"); script.text("STANDARD_EXPRESSION");
    script.param(0x2B,"bst\\registered"); script.text("REGISTERED");
    script.bytes.push_back(0); write_file(root/"MES/sprites.mes",script.bytes);
    dc4scene::SceneRunner scene(root.string()); scene.set_message_visible(false);
    CHECK(scene.load_script((root/"MES/sprites.mes").string()));
    next(scene,"CLOSEUP");
    CHECK(scene.sprite(0).canvas_w == 1260 && scene.sprite(0).canvas_h == 1080);
    CHECK(scene.sprite(0).frame_cx == 630 && scene.sprite(0).frame_cy == 540);
    // Native marker85 anchor correction is (-185,-135), then the 1.2 viewport scale.
    CHECK(pixel(scene,680,90) == body); CHECK(pixel(scene,900,250) == black);
    const auto clean = scene.frame();
    next(scene,"EXPRESSION");
    CHECK(scene.sprite(0).expression == "bst\\face_b"); CHECK(pixel(scene,680,90) == face);
    std::vector<uint8_t> state; CHECK(scene.capture_state(&state));
    dc4scene::SceneRunner restored(root.string()); CHECK(restored.restore_state(state));
    restored.draw(-1); CHECK(restored.frame() == scene.frame());
    CHECK(restored.sprite(0).canvas_w == 1260 && restored.sprite(0).canvas_h == 1080);
    next(restored,"CLEAN"); CHECK(restored.frame() == clean);
    next(scene,"CLEAN"); CHECK(scene.frame() == clean);
    next(scene,"CACHED"); CHECK(scene.frame() == clean);
    next(scene,"REDUCED_CLOSEUP"); CHECK(scene.sprite(0).scale == .75f);
    CHECK(pixel(scene,910,285) == body); CHECK(pixel(scene,680,90) == black);
    next(scene,"REDUCED_STANDARD");
    CHECK(scene.sprite(0).canvas_w == 882 && scene.sprite(0).canvas_h == 756);
    CHECK(pixel(scene,820,285) == body);
    next(scene,"STANDARD"); CHECK(pixel(scene,780,250) == body);
    next(scene,"STANDARD_EXPRESSION");
    CHECK(scene.sprite(0).expression == "bst\\face"); CHECK(pixel(scene,780,250) == normal_face);
    next(scene,"REGISTERED");
    CHECK(scene.sprite(0).canvas_w == 1260 && scene.sprite(0).canvas_h == 1080);
    CHECK(scene.sprite(0).frame_cx == 322 && scene.sprite(0).frame_cy == 240);
    CHECK(pixel(scene,538,90) == body);
}
void real_scene(const fs::path& data, const fs::path& output, bool assert_fixed) {
    dc4scene::SceneRunner scene(data.string()); scene.set_message_visible(false);
    CHECK(scene.load_script((data/"MES/hyd_0611_a2_com.mes").string()));
    uint16_t marker = 0, dialogue_marker = 0;
    scene.set_observer([&](const dc4::MesCommand& command, const std::string&) {
        if (command.op == 0x03) marker = uint16_t(command.p1 | (uint16_t(command.p2) << 8));
        if (command.op == 0x4C) dialogue_marker = marker;
    });
    for (int i = 0; i < 300 && dialogue_marker < 85 && !scene.scene_ended(); ++i) {
        if (scene.waiting_for_time()) scene.finish_script_wait();
        if (!scene.pending_movie().empty()) scene.finish_movie();
        scene.advance_to_next_text(); scene.update(1000);
    }
    CHECK(dialogue_marker == 85); CHECK(scene.last_error().empty());
    const auto& sprite = scene.sprite(0);
    CHECK(sprite.image == "bst\\yme_1d_013_b");
    CHECK(sprite.img.w == 1260 && sprite.img.h == 1080);
    CHECK(sprite.cur_x == 210 && sprite.cur_y == 0 && sprite.x == 441 && sprite.y == 322 && sprite.scale == 1);
    if (assert_fixed) CHECK(sprite.canvas_w == 1260 && sprite.canvas_h == 1080);
    scene.draw(-1);
    if (!output.empty()) capture(scene,output/"hyd_0611_a2_com-marker85.ppm");
    const auto frame = scene.frame();
    std::vector<uint8_t> state; CHECK(scene.capture_state(&state));
    dc4scene::SceneRunner restored(data.string()); CHECK(restored.restore_state(state));
    restored.draw(-1); CHECK(restored.frame() == frame);
    std::cout << "DC2TY marker " << dialogue_marker << ": " << sprite.image << ' '
              << sprite.img.w << 'x' << sprite.img.h << ", canvas " << sprite.canvas_w << 'x' << sprite.canvas_h
              << ", origin " << sprite.cur_x << ',' << sprite.cur_y << ", anchor " << sprite.x << ',' << sprite.y << '\n';
}
}
int main(int argc, char** argv) {
    const auto root = fs::temp_directory_path()/
        ("arisa32-dc2ty-sprites-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        const bool capture_only = argc > 1 && std::string(argv[1]) == "--capture-only";
        if (!capture_only) { loose_sprites(root/"dc2ty",0x5F69); loose_sprites(root/"dc2pc",0x9969); }
        const char* data = capture_only ? (argc > 2 ? argv[2] : nullptr) : (argc > 1 ? argv[1] : std::getenv("DC2TY_DATA"));
        const int output_index = capture_only ? 3 : 2;
        if (data && *data) real_scene(data,argc > output_index ? fs::path(argv[output_index]) : fs::path{},!capture_only);
        else if (capture_only) throw std::runtime_error("--capture-only requires a DC2TY AdvData path");
        else std::cout << "DC2TY_DATA unset; original sprite check skipped\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; ++failures; }
    fs::remove_all(root);
    std::cout << "DC2TY sprite failures: " << failures << '\n';
    return failures ? 1 : 0;
}
