#include "arisa32/scene.h"
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
void write_file(const fs::path& path, const std::vector<uint8_t>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    if (!out) throw std::runtime_error("cannot create sprite fixture");
}
struct Script {
    std::vector<uint8_t> bytes{1,0,0,0,3,0,0,0,0,0,0x72,0x9D,0};
    void words(uint8_t op, uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
        bytes.push_back(op);
        for (uint16_t v : {a,b,c,d}) { bytes.push_back(uint8_t(v)); bytes.push_back(uint8_t(v >> 8)); }
    }
    void param(uint8_t op, uint8_t index, const std::string& value) {
        bytes.push_back(op); bytes.push_back(index);
        bytes.insert(bytes.end(), value.begin(), value.end()); bytes.push_back(0);
    }
    void string(uint8_t op, const std::string& value) {
        bytes.push_back(op); bytes.insert(bytes.end(), value.begin(), value.end()); bytes.push_back(0);
    }
    void text(const std::string& value) {
        bytes.push_back(0x4F);
        for (unsigned char c : value) bytes.push_back(uint8_t(c - 0x20));
        bytes.push_back(0);
    }
};
void image(const fs::path& path, int x, int y, int w, int h, std::array<uint8_t,3> rgb) {
    std::vector<uint8_t> bytes{'C','R','X','G'};
    auto word = [&](uint16_t v) { bytes.push_back(uint8_t(v)); bytes.push_back(uint8_t(v >> 8)); };
    word(uint16_t(x)); word(uint16_t(y)); word(uint16_t(w)); word(uint16_t(h));
    word(1); word(0); word(0); word(0);
    const int stride = (w * 3 + 3) & ~3;
    std::vector<uint8_t> pixels(size_t(stride) * h, 0);
    for (int row = 0; row < h; ++row) for (int col = 0; col < w; ++col)
        for (int c = 0; c < 3; ++c) pixels[size_t(row)*stride+col*3+c] = rgb[2-c];
    for (size_t i = 0; i < pixels.size(); i += 8) {
        bytes.push_back(255);
        bytes.insert(bytes.end(), pixels.begin()+i, pixels.begin()+std::min(i+8,pixels.size()));
    }
    write_file(path, bytes);
}
std::array<uint8_t,3> pixel(const dc4scene::SceneRunner& scene, int x, int y) {
    const auto* p = scene.frame().data() + (size_t(y)*1280+x)*4;
    return {p[0],p[1],p[2]};
}
void capture(const dc4scene::SceneRunner& scene, const fs::path& path) {
    std::ofstream out(path, std::ios::binary);
    out << "P6\n1280 720\n255\n";
    for (size_t i = 0; i < scene.frame().size(); i += 4)
        out.write(reinterpret_cast<const char*>(scene.frame().data()+i), 3);
}
void next(dc4scene::SceneRunner& scene, const char* line) {
    CHECK(scene.advance_to_next_text()); CHECK(scene.current_line() == line); scene.draw(-1);
}
void lifecycle(const fs::path& root) {
    const std::array<uint8_t,3> body{200,40,30}, hat{30,210,50}, pose{30,50,220}, black{0,0,0};
    image(root/"GRP/BST/body.CRX", 16, 32, 64, 80, body);
    image(root/"GRP/BST/hat.CRX", 24, 16, 32, 24, hat);
    image(root/"GRP/BST/pose.CRX", 16, 32, 64, 80, pose);
    image(root/"GRP/BG/black.CRX", 0, 0, 16, 9, black);
    Script script;
    for (int i = 0; i < 4; ++i) {
        script.words(0x6B, uint16_t(40+i*200), 80, uint16_t(i), 0);
        script.words(0x6B, 100, 512, 300, uint16_t(16+i));
        script.param(0x2B, uint8_t(2+i), "BST\\body"); script.param(0x2E, uint8_t(i), ".");
        script.param(0x2B, uint8_t(6+i), "BST\\hat"); script.param(0x2F, uint8_t(i), ".");
    }
    script.text("FOUR");
    script.param(0x2F, 0, "-"); script.text("HAT_REMOVED");
    script.param(0x2F, 0, "--"); script.text("HAT_STAYS_REMOVED");
    script.param(0x2E, 0, "-"); script.text("BODY_REMOVED");
    script.param(0x2E, 0, "--"); script.text("BODY_STAYS_REMOVED");
    script.param(0x2B, 2, "--"); script.param(0x2E, 0, "."); script.text("BODY_RELOADED");
    script.param(0x2B, 3, "BST\\pose"); script.param(0x2E, 1, "."); script.text("POSE");
    script.words(0x6B, 150, 0, 0, 17); script.param(0x2E, 1, "."); script.text("ZOOM");
    script.words(0x6B, 320, 120, 1, 0); script.param(0x2E, 1, "."); script.text("MOVE_RESET");
    script.param(0x2E, 1, "-"); script.param(0x2F, 1, "."); script.text("PAIR_REMOVED");
    script.param(0x2B, 3, "--"); script.param(0x2E, 1, "."); script.text("POSE_RELOADED");
    script.param(0x2B, 7, "--"); script.param(0x2F, 1, "."); script.text("HAT_RELOADED");
    script.words(0x6B, 328, 96, 2, 0); script.param(0x2E, 2, "."); script.text("LAYER_ORDER");
    script.param(0x2B, 1, "BG\\black"); script.string(0x33, "BG\\black"); script.text("BACKGROUND");
    script.param(0x2E, 1, "--"); script.param(0x2F, 1, "--"); script.text("STAYS_CLEARED");
    script.bytes.push_back(0); write_file(root/"MES/sprites.mes", script.bytes);
    dc4scene::SceneRunner scene(root.string()); scene.set_message_visible(false);
    CHECK(scene.load_script((root/"MES/sprites.mes").string()));
    next(scene,"FOUR");
    for (int i = 0; i < 4; ++i) {
        CHECK(scene.sprite(i).visible && scene.sprite(i+4).visible);
        CHECK(pixel(scene,80+i*250,160) == body);
        CHECK(pixel(scene,85+i*250,125) == hat);
    }
    const auto before = scene.frame();
    std::vector<uint8_t> state; CHECK(scene.capture_state(&state));
    dc4scene::SceneRunner restored(root.string()); CHECK(restored.restore_state(state));
    restored.draw(-1); CHECK(restored.frame() == before);
    next(scene,"HAT_REMOVED"); CHECK(scene.sprite(0).visible && !scene.sprite(4).visible);
    CHECK(pixel(scene,80,160) == body); CHECK(pixel(scene,85,125) == black);
    next(restored,"HAT_REMOVED"); CHECK(restored.frame() == scene.frame());
    next(scene,"HAT_STAYS_REMOVED"); CHECK(!scene.sprite(4).visible);
    next(scene,"BODY_REMOVED"); CHECK(!scene.sprite(0).visible && !scene.sprite(4).visible);
    next(scene,"BODY_STAYS_REMOVED"); CHECK(!scene.sprite(0).visible);
    next(scene,"BODY_RELOADED"); CHECK(scene.sprite(0).visible && !scene.sprite(4).visible);
    next(scene,"POSE"); CHECK(pixel(scene,330,160) == pose && pixel(scene,335,125) == hat);
    next(scene,"ZOOM"); CHECK(scene.sprite(1).scale == 1.5f);
    CHECK(pixel(scene,350,180) == pose && pixel(scene,350,135) == hat);
    next(scene,"MOVE_RESET"); CHECK(scene.sprite(1).scale == 1 && scene.sprite(1).x == 0 && scene.sprite(1).y == 0);
    CHECK(pixel(scene,430,210) == pose && pixel(scene,435,175) == hat);
    next(scene,"PAIR_REMOVED"); CHECK(!scene.sprite(1).visible && !scene.sprite(5).visible);
    next(scene,"POSE_RELOADED"); CHECK(scene.sprite(1).visible && !scene.sprite(5).visible);
    next(scene,"HAT_RELOADED"); CHECK(scene.sprite(5).visible);
    next(scene,"LAYER_ORDER"); CHECK(pixel(scene,435,175) == body);
    next(scene,"BACKGROUND");
    for (int i = 0; i < 8; ++i) CHECK(!scene.sprite(i).visible);
    next(scene,"STAYS_CLEARED"); CHECK(!scene.sprite(1).visible && !scene.sprite(5).visible);
}
void real_scene(const fs::path& data, const fs::path& output) {
    dc4scene::SceneRunner scene(data.string()); scene.set_message_visible(false);
    CHECK(scene.load_script((data/"MES/mar_0201_a1.mes").string()));
    uint16_t marker = 0, dialogue_marker = 0;
    scene.set_observer([&](const dc4::MesCommand& c, const std::string&) {
        if (c.op == 0x03) marker = uint16_t(c.p1 | (uint16_t(c.p2) << 8));
        if (c.op == 0x4F) dialogue_marker = marker;
    });
    for (int i = 0; i < 300 && dialogue_marker < 65; ++i) {
        if (scene.waiting_for_time()) scene.finish_script_wait();
        if (!scene.pending_movie().empty()) scene.finish_movie();
        scene.advance_to_next_text();
    }
    CHECK(dialogue_marker == 65);
    scene.draw(-1); const auto four = scene.frame();
    if (!output.empty()) { fs::create_directories(output); capture(scene,output/"nanaka-four.ppm"); }
    CHECK(scene.sprite(0).image == "BST\\NNK_0C_000,BST\\NNK_C");
    CHECK(scene.sprite(4).image == "BST\\E_NNK_HAT0,BST\\E_NNK");
    for (int i = 0; i < 4; ++i) CHECK(scene.sprite(i).visible);
    std::vector<uint8_t> state; CHECK(scene.capture_state(&state));
    dc4scene::SceneRunner restored(data.string());
    std::string restore_error;
    CHECK(restored.restore_state(state, &restore_error));
    if (!restore_error.empty()) std::cerr << "Restore: " << restore_error << '\n';
    restored.draw(-1);
    CHECK(restored.frame() == four);
    CHECK(scene.advance_to_next_text()); scene.draw(-1);
    CHECK(restored.advance_to_next_text()); restored.draw(-1); CHECK(restored.frame() == scene.frame());
    if (!output.empty()) capture(scene,output/"nanaka-next.ppm");
    CHECK(scene.sprite(0).image != "BST\\NNK_0C_000,BST\\NNK_C");
    for (int i = 2; i < 8; ++i) CHECK(!scene.sprite(i).visible);
    std::cout << "Real DC2DM marker65 and following removal checked\n";
}
}
int main(int argc, char** argv) {
    const auto root = fs::temp_directory_path() / ("arisa32-dc2dm-sprites-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        if (argc < 2 || std::string(argv[1]) != "--capture-only") lifecycle(root);
        const char* data = std::getenv("DC2DM_DATA");
        if (data && *data) real_scene(data, argc > 2 ? fs::path(argv[2]) : fs::path{});
        else std::cout << "DC2DM_DATA unset; original-asset check skipped\n";
        fs::remove_all(root);
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; ++failures; }
    std::cout << "DC2DM sprite failures: " << failures << '\n';
    return failures ? 1 : 0;
}
