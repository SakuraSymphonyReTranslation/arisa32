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
#define CHECK(value) do { if (!(value)) { std::cerr << __LINE__ << ": " #value "\n"; ++failures; } } while (0)
void write(const fs::path& path, const std::vector<uint8_t>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    if (!file) throw std::runtime_error("cannot write effect fixture");
}
struct Script {
    std::vector<uint8_t> bytes{1,0,0,0,3,0,0,0,0,0,0xB6,0xAA,0};
    explicit Script(uint16_t version = 0xAAB6) { bytes[10] = uint8_t(version); bytes[11] = uint8_t(version >> 8); }
    void param(int op, int a, int b) { bytes.insert(bytes.end(), {uint8_t(op), uint8_t(a), uint8_t(b)}); }
    void str(int op, const std::string& value) {
        bytes.push_back(uint8_t(op)); bytes.insert(bytes.end(), value.begin(), value.end()); bytes.push_back(0);
    }
    void slot(int id, const std::string& value) { bytes.push_back(0x3C); str(id, value); }
    void text(const std::string& value) {
        bytes.push_back(0x6B); for (unsigned char ch : value) bytes.push_back(uint8_t(ch - 0x20)); bytes.push_back(0);
    }
    void save(const fs::path& root, const char* name) { auto code = bytes; code.push_back(0); write(root / "MES" / name, code); }
};
void fixture_image(const fs::path& path, int width, int height, int kind) {
    std::vector<uint8_t> bytes{'C','R','X','G',0,0,0,0};
    const auto word = [&](int v) { bytes.push_back(uint8_t(v)); bytes.push_back(uint8_t(v >> 8)); };
    word(width); word(height); word(1); word(0); word(0); word(0);
    const int stride = (width * 3 + 3) & ~3;
    std::vector<uint8_t> pixels(size_t(stride) * height);
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        auto* p = &pixels[size_t(y) * stride + x * 3];
        if (kind == 0) p[2] = 200;
        if (kind == 1) p[0] = 200;
        if (kind == 2) p[0] = p[1] = p[2] = uint8_t(255 - x * 255 / (width - 1));
        if (kind == 3) p[0] = p[1] = p[2] = 255;
        if (kind == 4) { p[(x / 28) % 3] = uint8_t(100 + y / 28 * 25); }
    }
    for (size_t i = 0; i < pixels.size(); i += 8) {
        bytes.push_back(255);
        bytes.insert(bytes.end(), pixels.begin() + i, pixels.begin() + std::min(i + 8, pixels.size()));
    }
    write(path, bytes);
}
void assets(const fs::path& root) {
    fixture_image(root / "GRP/BG/red.CRX", 64, 36, 0);
    fixture_image(root / "GRP/BG/blue.CRX", 64, 36, 1);
    fixture_image(root / "GRP/MSK/MASK15B.CRX", 1280, 750, 2);
    fixture_image(root / "GRP/SYSTEM/EFFCHANGESCREEN1.CRX", 1024, 720, 3);
    fixture_image(root / "GRP/SYSTEM/EFFSAKURA01.CRX", 256, 168, 4);
}
void change(Script& script, const char* background, bool flowers, int duration, bool reload = true) {
    script.param(0x0F, 32, 0);
    script.slot(1, background); script.str(0x49, background);
    if (reload) {
        script.slot(21, "MSK\\MASK15B"); script.slot(22, "SYSTEM\\EFFCHANGESCREEN1");
    }
    if (flowers) script.param(0x38, 1, duration);
    script.param(0x0F, 21, duration);
    script.param(0x15, duration, 0);
    script.param(0x15, 254, 0);
    script.text("AFTER");
}
uint8_t pixel(const dc4scene::SceneRunner& scene, int x, int y, int channel) {
    return scene.frame()[(size_t(y) * 1280 + x) * 4 + channel];
}
size_t white_pixels(const dc4scene::SceneRunner& scene) {
    size_t result = 0;
    for (size_t i = 0; i < scene.frame().size(); i += 4)
        if (scene.frame()[i] > 240 && scene.frame()[i + 1] > 240 && scene.frame()[i + 2] > 240) ++result;
    return result;
}
void ppm(const dc4scene::SceneRunner& scene, const fs::path& path) {
    std::ofstream output(path, std::ios::binary);
    output << "P6\n1280 720\n255\n";
    for (size_t i = 0; i < scene.frame().size(); i += 4)
        output.write(reinterpret_cast<const char*>(scene.frame().data() + i), 3);
}
void transitions(const fs::path& root, uint16_t version, bool original = false) {
    Script script(version);
    script.slot(1, "BG\\red"); script.str(0x49, "BG\\red"); script.text("BEFORE");
    change(script, "BG\\blue", true, 5);
    change(script, "BG\\red", true, 5, false);
    script.save(root, "transition.mes");
    for (int speed : {0, 5, 11}) {
        dc4scene::SceneRunner scene(root.string()); scene.set_animation_enabled(true); scene.set_effect_speed(speed);
        CHECK(scene.load_script((root / "MES/transition.mes").string()));
        CHECK(scene.advance_to_next_text()); scene.draw(-1);
        CHECK(!scene.advance_to_next_text() && scene.waiting_for_time()); scene.draw(-1);
        CHECK(pixel(scene, 640, 360, 0) == 200);
        if (original && speed == 5) ppm(scene, root / "transition-0000.ppm");
        scene.update(500); scene.draw(-1);
        CHECK(white_pixels(scene) > 10000);
        if (original && speed == 5) ppm(scene, root / "transition-0500.ppm");
        scene.update(499); CHECK(!scene.script_ready());
        scene.update(1); CHECK(scene.script_ready() && scene.animating());
        CHECK(!scene.advance_to_next_text() && scene.waiting_for_time());
        scene.update(399); CHECK(!scene.script_ready());
        scene.update(1); CHECK(scene.script_ready() && !scene.animating());
        CHECK(scene.advance_to_next_text() && scene.current_line() == "AFTER"); scene.draw(-1);
        CHECK(pixel(scene, 640, 360, 2) == 200 && white_pixels(scene) == 0);
        if (original && speed == 5) ppm(scene, root / "transition-1400.ppm");
        std::vector<uint8_t> snapshot;
        CHECK(scene.capture_state(&snapshot));
        CHECK(scene.restore_state(snapshot));
        CHECK(!scene.advance_to_next_text()); scene.update(500); scene.draw(-1);
        CHECK(white_pixels(scene) > 10000);
        scene.finish_script_wait(); CHECK(!scene.animating());
        scene.draw(-1); CHECK(white_pixels(scene) == 0);
    }
    Script mask(version);
    mask.slot(1,"BG\\red"); mask.str(0x49,"BG\\red"); mask.text("BEFORE");
    change(mask,"BG\\blue",false,5); mask.save(root,"mask.mes");
    dc4scene::SceneRunner wipe(root.string()); wipe.set_animation_enabled(true);
    CHECK(wipe.load_script((root / "MES/mask.mes").string()));
    CHECK(wipe.advance_to_next_text()); CHECK(!wipe.advance_to_next_text()); wipe.update(500); wipe.draw(-1);
    CHECK(pixel(wipe,0,360,0) >= 195 && pixel(wipe,1279,360,2) >= 195);
    for (int speed : {0,5,11}) {
        Script fade(version);
        fade.slot(1,"BG\\red"); fade.str(0x49,"BG\\red"); fade.text("BEFORE");
        fade.param(0x0F,32,0); fade.slot(1,"BG\\blue"); fade.str(0x49,"BG\\blue");
        fade.param(0x0F,0,10); fade.param(0x15,254,0); fade.text("AFTER"); fade.save(root,"fade.mes");
        dc4scene::SceneRunner scene(root.string()); scene.set_animation_enabled(true); scene.set_effect_speed(speed);
        CHECK(scene.load_script((root / "MES/fade.mes").string()));
        CHECK(scene.advance_to_next_text()); CHECK(!scene.advance_to_next_text());
        const int duration = speed == 0 ? 5500 : speed == 5 ? 3667 : 1834;
        scene.update(duration - 1); CHECK(!scene.script_ready());
        scene.update(1); CHECK(scene.script_ready());
    }
}
void ambient(const fs::path& root, uint16_t version, bool original = false) {
    Script clean(version); clean.text("CLEAN"); clean.save(root,"clean.mes");
    dc4scene::SceneRunner legacy(root.string());
    CHECK(legacy.load_script((root / "MES/clean.mes").string())); CHECK(legacy.advance_to_next_text());
    std::vector<uint8_t> old_save; CHECK(legacy.capture_state(&old_save)); CHECK(old_save[8] == 5);
    Script script(version);
    script.slot(28,"SYSTEM\\EFFSAKURA01"); script.param(0x28,9,116); script.text("PETALS");
    script.param(0x28,13,0); script.text("CONTINUE");
    script.param(0x28,1,0); script.text("STOP"); script.save(root,"ambient.mes");
    dc4scene::SceneRunner scene(root.string()), split(root.string());
    for (auto* s : {&scene, &split}) {
        s->set_animation_enabled(true); CHECK(s->load_script((root / "MES/ambient.mes").string()));
        CHECK(s->advance_to_next_text()); CHECK(!s->animating() && !s->waiting_for_time());
    }
    scene.update(3200);
    for (int i = 0; i < 200; ++i) split.update(16);
    scene.draw(-1); split.draw(-1); CHECK(scene.frame() == split.frame());
    const auto frame = scene.frame();
    CHECK(std::count_if(frame.begin(), frame.end(), [](uint8_t v) { return v != 0; }) > 1280 * 720);
    if (original) ppm(scene, root / "ambient-3200.ppm");
    std::vector<uint8_t> saved; CHECK(scene.capture_state(&saved)); CHECK(saved[8] == 7);
    scene.update(400); scene.draw(-1); CHECK(scene.frame() != frame);
    CHECK(scene.restore_state(saved)); scene.draw(-1); CHECK(scene.frame() == frame);
    scene.update(4800); split.update(4800); scene.draw(-1); split.draw(-1); CHECK(scene.frame() == split.frame());
    const auto later = scene.frame();
    scene.set_effects_enabled(false); scene.draw(-1); CHECK(scene.frame() != later);
    scene.update(4000); scene.set_effects_enabled(true); scene.draw(-1); CHECK(scene.frame() == later);
    CHECK(scene.advance_to_next_text()); scene.draw(-1); CHECK(scene.frame() == later);
    CHECK(scene.advance_to_next_text()); scene.draw(-1); CHECK(scene.frame() != later);
    std::vector<uint8_t> stopped; CHECK(scene.capture_state(&stopped));
    CHECK(scene.restore_state(saved)); CHECK(scene.restore_state(stopped)); scene.draw(-1);
    CHECK(std::count_if(scene.frame().begin(), scene.frame().end(), [](uint8_t v) { return v != 0; }) == 1280 * 720);
    auto damaged = saved; damaged.back() ^= 1;
    CHECK(!scene.restore_state(damaged));
    CHECK(scene.restore_state(saved)); CHECK(scene.restore_state(old_save)); scene.draw(-1);
    CHECK(std::count_if(scene.frame().begin(), scene.frame().end(), [](uint8_t v) { return v != 0; }) == 1280 * 720);
    CHECK(scene.restore_state(saved));
    Script classic(0x9D72);
    classic.bytes.insert(classic.bytes.end(), {0x28,9,116,0x4F,'C'-0x20,0});
    classic.save(root,"classic.mes");
    CHECK(scene.load_script((root / "MES/classic.mes").string()));
    CHECK(scene.advance_to_next_text()); scene.update(3200); scene.draw(-1);
    CHECK(!scene.animating());
    CHECK(std::count_if(scene.frame().begin(), scene.frame().end(), [](uint8_t v) { return v != 0; }) == 1280 * 720);
}

void capture_dark(const fs::path& root) {
    fixture_image(root / "GRP/BG/black.CRX",64,36,5);
    Script script;
    script.slot(1,"BG\\black"); script.str(0x49,"BG\\black"); script.text("BEFORE");
    change(script,"BG\\black",true,5); script.save(root,"dark.mes");
    dc4scene::SceneRunner scene(root.string()); scene.set_animation_enabled(true);
    CHECK(scene.load_script((root / "MES/dark.mes").string())); CHECK(scene.advance_to_next_text());
    scene.draw(-1);
    const auto baseline_start = std::chrono::steady_clock::now();
    for (int i = 0; i < 20; ++i) scene.draw(-1);
    const double baseline_ms = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-baseline_start).count()/20;
    CHECK(!scene.advance_to_next_text());
    fs::create_directories(root / "frames");
    double draw_ms = 0, wipe_ms = 0, tail_ms = 0;
    for (int frame = 0; frame <= 42; ++frame) {
        const auto start = std::chrono::steady_clock::now();
        scene.draw(-1);
        const double sample = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        draw_ms += sample;
        if (frame < 30) wipe_ms += sample;
        else tail_ms += sample;
        ppm(scene,root / "frames" / (std::to_string(frame)+".ppm"));
        scene.update(uint32_t((frame+1)*1000/30-frame*1000/30));
    }
    std::cout << "Sakura software draw average: " << draw_ms/43 << "ms, baseline: " << baseline_ms
              << "ms, wipe: " << wipe_ms/30 << "ms, tails: " << tail_ms/13 << "ms\n";
}
} // namespace

int main(int argc, char** argv) {
    const fs::path root = argc > 1 ? fs::path(argv[1]) : fs::temp_directory_path() /
        ("arisa-dc4-effects-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        assets(root);
        for (auto version : {uint16_t(0xAAB6), uint16_t(0xABB6)}) { transitions(root,version); ambient(root,version); }
        if (argc > 1) {
            for (const char* variable : {"DC4_DATA", "DC4PH_DATA"}) if (const char* data = std::getenv(variable)) {
                const auto original = root / variable; assets(original);
                for (const char* name : {"SYSTEM/EFFCHANGESCREEN1.CRX", "SYSTEM/EFFSAKURA01.CRX", "MSK/MASK15B.CRX"})
                    fs::copy_file(fs::path(data) / "GRP" / name, original / "GRP" / name, fs::copy_options::overwrite_existing);
                transitions(original, variable == std::string("DC4_DATA") ? 0xAAB6 : 0xABB6, true);
                ambient(original, variable == std::string("DC4_DATA") ? 0xAAB6 : 0xABB6, true);
                if (variable == std::string("DC4_DATA")) capture_dark(original);
            }
        }
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; ++failures; }
    if (argc == 1 && failures == 0) fs::remove_all(root);
    std::cout << "dc4_effect_test failures=" << failures << "\n";
    return failures ? 1 : 0;
}
