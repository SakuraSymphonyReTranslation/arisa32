#include "arisa32/scene.h"
#include "arisa32/resource.h"
#include "arisa32/dc2pc_menu_transition.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace fs = std::filesystem;
namespace {
int failures = 0;
void check(bool value, const char* expression, int line) {
    if (!value) { std::cerr << line << ": " << expression << '\n'; ++failures; }
}
#define CHECK(value) check(bool(value), #value, __LINE__)
using Color = std::array<uint8_t, 3>;
constexpr Color red{200, 20, 30}, blue{30, 40, 210}, white{255, 255, 255}, black{0, 0, 0};

void write_file(const fs::path& path, const std::vector<uint8_t>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    if (!out) throw std::runtime_error("Cannot write transition fixture");
}

void image(const fs::path& path, int width, int height, Color color, bool atlas = false) {
    std::vector<uint8_t> bytes{'C','R','X','G',0,0,0,0};
    const auto word = [&](uint16_t value) { bytes.push_back(uint8_t(value)); bytes.push_back(uint8_t(value >> 8)); };
    word(uint16_t(width)); word(uint16_t(height)); word(1); word(0); word(0); word(0);
    std::vector<uint8_t> pixels(size_t(width) * height * 3);
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        const Color pixel = atlas && ((x / 32 == 1 && x % 32 < 16) || x / 32 >= 20) ? Color{255,0,255} : color;
        for (int c = 0; c < 3; ++c) pixels[(size_t(y) * width + x) * 3 + c] = pixel[2 - c];
    }
    for (size_t i = 0; i < pixels.size(); i += 8) {
        bytes.push_back(255);
        bytes.insert(bytes.end(), pixels.begin() + i, pixels.begin() + std::min(i + 8, pixels.size()));
    }
    write_file(path, bytes);
}

struct Script {
    std::vector<uint8_t> bytes{1,0,0,0,2,0,0,0,0x69,0x5F};
    void pair(uint8_t mode, uint8_t parameter = 0) { bytes.insert(bytes.end(), {0x0D,mode,parameter}); }
    void str(uint8_t op, const std::string& text) {
        bytes.push_back(op);
        for (unsigned char ch : text) bytes.push_back(op == 0x4C ? uint8_t(ch - 0x20) : ch);
        bytes.push_back(0);
    }
    void background(const char* name) { str(0x30, std::string("BG\\") + name); }
    void text(const char* text) { str(0x4C, text); }
    void begin() { background("red"); pair(5); text("BEFORE"); }
    void save(const fs::path& path) {
        auto saved = bytes;
        saved.push_back(0);
        write_file(path, saved);
    }
};

Color pixel(dc4scene::SceneRunner& scene, int native_x, int native_y) {
    const int x = 160 + native_x * 6 / 5, y = native_y * 6 / 5;
    const auto* data = scene.frame().data() + (size_t(y) * 1280 + x) * 4;
    return {data[0], data[1], data[2]};
}

void borders(dc4scene::SceneRunner& scene) {
    for (int y : {0,360,719}) for (int x : {0,159,1120,1279}) {
        const auto* p = scene.frame().data() + (size_t(y) * 1280 + x) * 4;
        CHECK(p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3] == 255);
    }
}

std::unique_ptr<dc4scene::SceneRunner> load(const fs::path& root, Script script, int setting = 0) {
    script.save(root / "MES/test.mes");
    auto scene = std::make_unique<dc4scene::SceneRunner>(root.string());
    scene->set_animation_enabled(true);
    CHECK(scene->load_script((root / "MES/test.mes").string()));
    scene->set_variable(99, 12, setting);
    scene->set_variable(99, 20, 1);
    CHECK(scene->variable(99, 20) == (script.bytes[9] == 0x5F ? 1 : 0));
    scene->set_message_visible(false);
    CHECK(scene->advance_to_next_text());
    CHECK(scene->current_line() == "BEFORE");
    scene->draw(-1);
    CHECK(pixel(*scene, 400, 300) == red);
    return scene;
}

void after(dc4scene::SceneRunner& scene) {
    CHECK(scene.advance_to_next_text());
    CHECK(scene.current_line() == "AFTER");
    CHECK(scene.last_error().empty());
}

Color blend(Color from, Color to, int alpha, int denominator) {
    Color result{};
    for (int c = 0; c < 3; ++c)
        result[c] = uint8_t((int(from[c]) * (denominator - alpha) + int(to[c]) * alpha) / denominator);
    return result;
}

void feather(const fs::path& root) {
    constexpr int duration[2][4]{{840,640,520,400},{1160,960,800,680}};
    for (int selector : {1,2}) for (int setting = 0; setting < 4; ++setting) {
        Script script; script.begin(); script.background("white"); script.pair(13, selector); script.text("AFTER");
        auto scene = load(root, script, setting);
        CHECK(!scene->advance_to_next_text() && scene->waiting_for_time());
        scene->draw(-1);
        CHECK(pixel(*scene, 0, 0) == blend(red, white, 8, 16));
        CHECK(pixel(*scene, 400, 0) == red);
        CHECK(pixel(*scene, 0, 400) == (selector == 1 ? blend(red, white, 8, 16) : red));
        const auto initial = scene->frame(); scene->draw(-1); CHECK(scene->frame() == initial);
        scene->update(39); scene->draw(-1); CHECK(scene->frame() == initial);
        scene->update(1); scene->draw(-1);
        const int alpha = std::min(16, 8 + 2 * (1 << setting));
        CHECK(pixel(*scene, 0, 0) == blend(blend(red, white, 8, 16), white, alpha, 16));
        borders(*scene);
        const int end = duration[selector - 1][setting];
        scene->update(end - 81); scene->draw(-1);
        CHECK(scene->waiting_for_time() && pixel(*scene, 799, 599) != white);
        scene->update(1); scene->draw(-1);
        CHECK(scene->waiting_for_time() && pixel(*scene, 799, 599) == white);
        scene->update(39); CHECK(!scene->script_ready());
        scene->update(1); scene->draw(-1);
        CHECK(scene->script_ready() && !scene->animating());
        CHECK(pixel(*scene, 0, 0) == white && pixel(*scene, 799, 599) == white);
        after(*scene);
    }
}

void dissolves(const fs::path& root) {
    for (int parameter : {0,1,2,3,5,7,8,9}) for (int setting : {0,3}) {
        Script script; script.begin(); script.background("blue"); script.pair(20, parameter); script.text("AFTER");
        auto scene = load(root, script, setting);
        CHECK(!scene->advance_to_next_text() && scene->waiting_for_time());
        const int step = parameter == 0 ? (setting == 0 ? 16 : 128) : parameter == 1 ? 128 :
            parameter == 2 ? 64 : parameter == 3 ? 32 : parameter == 5 ? 8 : 2;
        const int interval = parameter >= 8 ? (parameter - 7) * 40 : 40;
        const int duration = 256 / step * interval;
        scene->draw(-1);
        CHECK(pixel(*scene, 0, 0) == blend(red, blue, step, 256));
        CHECK(pixel(*scene, 799, 599) == blend(red, blue, step, 256));
        scene->update(interval - 1); scene->draw(-1);
        CHECK(pixel(*scene, 400, 300) == blend(red, blue, step, 256));
        scene->update(1); scene->draw(-1);
        CHECK(pixel(*scene, 400, 300) == blend(red, blue, step * 2, 256));
        scene->update(duration - interval * 2); scene->draw(-1);
        CHECK(pixel(*scene, 400, 300) == blue && scene->waiting_for_time());
        scene->update(interval - 1); CHECK(!scene->script_ready());
        scene->update(1); scene->draw(-1);
        CHECK(scene->script_ready() && !scene->animating()); borders(*scene);
        after(*scene);
    }
}

void circles(const fs::path& root) {
    for (int selector : {4,5}) {
        Script script; script.begin(); script.pair(14, selector);
        script.background("blue"); script.pair(13, 2); script.text("AFTER");
        auto scene = load(root, script);
        CHECK(!scene->advance_to_next_text() && scene->waiting_for_time()); scene->draw(-1);
        CHECK(pixel(*scene, 8, 8) == black);
        CHECK(pixel(*scene, 24, 8)[0] > 180);
        for (int row = 0; row < 19; ++row) for (int col = 0; col < 25; ++col) {
            const auto key = pixel(*scene, col * 32 + 8, row * 32 + 8);
            CHECK(selector == 4 || (row == 0 && col == 0) ? key == black : key == red);
            CHECK(pixel(*scene, col * 32 + 24, row * 32 + 8)[0] > 180);
        }
        borders(*scene);
        scene->update(40); scene->draw(-1);
        CHECK(pixel(*scene, 8, 8) == black);
        scene->update(360); scene->draw(-1);
        const auto advanced = scene->frame(); scene->draw(-1); CHECK(advanced == scene->frame());
        auto batched = load(root, script);
        CHECK(!batched->advance_to_next_text());
        for (int i = 0; i < 10; ++i) batched->update(40);
        batched->draw(-1); CHECK(advanced == batched->frame());
        const int duration = selector == 4 ? 1880 : 4240;
        scene->update(duration - 441); CHECK(!scene->script_ready());
        scene->update(1); scene->draw(-1);
        CHECK(scene->waiting_for_time() && pixel(*scene, 400, 300) == black);
        scene->update(39); CHECK(!scene->script_ready());
        scene->update(1); scene->draw(-1);
        CHECK(scene->script_ready() && !scene->animating());
        CHECK(pixel(*scene, 400, 300) == black); borders(*scene);
        CHECK(!scene->advance_to_next_text()); scene->draw(-1);
        CHECK(pixel(*scene, 0, 0) == blend(black, blue, 8, 16));
        scene->finish_script_wait(); after(*scene); scene->draw(-1);
        CHECK(pixel(*scene, 400, 300) == blue);
    }
}

void white_pair(const fs::path& root) {
    Script script; script.begin();
    script.background("white"); script.pair(13, 2);
    script.background("blue"); script.pair(13, 2); script.text("AFTER");
    auto scene = load(root, script);
    CHECK(!scene->advance_to_next_text()); scene->update(1160); scene->draw(-1);
    CHECK(scene->script_ready() && pixel(*scene, 400, 300) == white);
    CHECK(!scene->advance_to_next_text()); scene->draw(-1);
    CHECK(pixel(*scene, 0, 0) == blend(white, blue, 8, 16));
    CHECK(pixel(*scene, 799, 599) == white);
    scene->update(1160); scene->draw(-1);
    CHECK(scene->script_ready() && pixel(*scene, 799, 599) == blue);
    after(*scene);
}

void title_exit(const fs::path& root) {
    Script script; script.begin(); script.background("blue"); script.text("AFTER");
    auto scene = load(root, script);
    const auto before = scene->frame();
    scene->set_sidebar_visible(true);
    CHECK(scene->start_dc2ty_title_exit(before));
    dc4ui::Dc2pcMenuTransition presentation;
    int calls = 0;
    presentation.start_custom_outgoing(before,0,1000,
        [&](uint32_t elapsed) { ++calls; return scene->update_dc2ty_title_exit(elapsed); },
        [&](std::vector<uint8_t>* pixels) { scene->draw_dc2ty_title_exit(pixels); });
    std::vector<uint8_t> pixels;
    CHECK(!presentation.update(999) && calls == 0);
    CHECK(presentation.draw(&pixels) && pixels == before);
    CHECK(!presentation.update(1) && calls == 1);
    CHECK(presentation.draw(&pixels) && pixels != before);
    CHECK(pixel(*scene, 8, 8) == black && pixel(*scene, 24, 8)[0] > 180);
    const auto paused = pixels;
    CHECK(presentation.draw(&pixels) && pixels == paused && calls == 1);
    CHECK(!presentation.update(1879));
    CHECK(presentation.update(1));
    CHECK(!presentation.update(10000));
    CHECK(presentation.draw(&pixels));
    for (size_t i = 0; i < pixels.size(); i += 4)
        if (pixels[i] || pixels[i + 1] || pixels[i + 2] || pixels[i + 3] != 255) { CHECK(false); break; }
    CHECK(scene->current_line() == "BEFORE");
    presentation.finish();
    CHECK(!presentation.active() && !presentation.draw(&pixels));

    Script pc; pc.bytes[9] = 0x99; pc.begin();
    auto other = load(root, pc);
    CHECK(!other->start_dc2ty_title_exit(other->frame()));
}

void lifecycle(const fs::path& root) {
    for (int mode : {13,14,20}) {
        const int parameter = mode == 14 ? 5 : 2;
        Script script; script.begin(); script.background("blue"); script.pair(mode, parameter); script.text("AFTER");
        for (int action = 0; action < 4; ++action) {
            auto scene = load(root, script);
            std::vector<uint8_t> saved; CHECK(scene->capture_state(&saved));
            const auto before = scene->frame();
            CHECK(!scene->advance_to_next_text()); scene->update(80);
            if (action == 0) scene->finish_script_wait();
            if (action == 1) scene->set_effects_enabled(false);
            if (action == 2) scene->set_animation_enabled(false);
            if (action == 3) {
                CHECK(scene->restore_state(saved)); scene->draw(-1);
                CHECK(!scene->animating() && scene->frame() == before);
                continue;
            }
            scene->draw(-1);
            CHECK(!scene->animating());
            CHECK(pixel(*scene, 400, 300) == (mode == 14 ? black : blue));
            after(*scene);
        }
        auto disabled = load(root, script); disabled->set_effects_enabled(false);
        after(*disabled); disabled->draw(-1);
        CHECK(!disabled->animating() && pixel(*disabled, 400, 300) == blue);
    }
    Script fallback; fallback.begin(); fallback.pair(14, 4); fallback.text("AFTER");
    auto disabled = load(root, fallback); disabled->set_variable(99, 20, 0);
    CHECK(!disabled->advance_to_next_text()); disabled->draw(-1);
    CHECK(pixel(*disabled, 8, 8) == red);
    disabled->update(320); disabled->draw(-1);
    CHECK(pixel(*disabled, 8, 8) == blend(black, red, 8, 16));
    disabled->update(320); disabled->draw(-1);
    CHECK(disabled->script_ready() && pixel(*disabled, 400, 300) == black);
    for (int mode : {13,20}) {
        Script pc; pc.bytes[9] = 0x99; pc.begin(); pc.background("blue"); pc.pair(mode, 2); pc.text("AFTER");
        auto unchanged = load(root, pc); after(*unchanged);
        CHECK(!unchanged->animating() && !unchanged->waiting_for_time());
    }
}

void capture(const std::vector<uint8_t>& pixels, const fs::path& path) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << "P6\n" << dc4scene::kScreenW << ' ' << dc4scene::kScreenH << "\n255\n";
    for (size_t i = 0; i < pixels.size(); i += 4)
        out.write(reinterpret_cast<const char*>(pixels.data() + i), 3);
    if (!out) throw std::runtime_error("Cannot capture title-exit frame");
}

void real_title_exit(const fs::path& data, const fs::path& output) {
    dc4::configure_resources(data.string());
    dc4scene::SceneRunner scene(data.string());
    scene.set_message_visible(false);
    CHECK(scene.load_script((data / "MES/hyd_0611_a2_com.mes").string()));
    scene.set_animation_enabled(false);
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
    CHECK(dialogue_marker == 85 && scene.last_error().empty());
    scene.set_variable(99, 12, 0);
    scene.set_variable(99, 20, 1);
    CHECK(scene.variable(99, 20) == 1);
    scene.set_animation_enabled(true);
    scene.draw(-1);
    const auto before = scene.frame();
    const auto line = scene.current_line();
    if (!output.empty()) capture(before, output / "title-exit-before.ppm");
    CHECK(scene.start_dc2ty_title_exit(before));
    uint32_t previous = 0;
    for (uint32_t elapsed : {0u, 200u, 400u, 800u, 1880u}) {
        CHECK(scene.update_dc2ty_title_exit(elapsed - previous) == (elapsed == 1880));
        std::vector<uint8_t> pixels;
        scene.draw_dc2ty_title_exit(&pixels);
        if (!output.empty()) capture(pixels, output / ("title-exit-" + std::to_string(elapsed) + ".ppm"));
        size_t visible = 0;
        for (size_t i = 0; i < pixels.size(); i += 4)
            visible += pixels[i] != 0 || pixels[i + 1] != 0 || pixels[i + 2] != 0;
        CHECK(elapsed >= 800 ? visible == 0 : visible > 0);
        if (elapsed == 0) CHECK(pixels != before);
        CHECK(scene.current_line() == line);
        std::cout << "Original TY title exit " << elapsed << " ms: " << visible << " visible pixels\n";
        previous = elapsed;
    }
}
}

int main(int argc, char** argv) {
    const auto root = fs::temp_directory_path() / ("arisa32-dc2ty-transitions-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        image(root / "GRP/BG/red.CRX", 800, 600, red);
        image(root / "GRP/BG/blue.CRX", 800, 600, blue);
        image(root / "GRP/BG/white.CRX", 800, 600, white);
        image(root / "GRP/system/eff.CRX", 800, 32, black, true);
        dc4::configure_resources(root.string(), "", "utf-8");
        feather(root); dissolves(root); circles(root); white_pair(root); title_exit(root); lifecycle(root);
        fs::remove_all(root);
        const char* data = argc > 1 ? argv[1] : std::getenv("DC2TY_DATA");
        if (data && *data) real_title_exit(data, argc > 2 ? fs::path(argv[2]) : fs::path{});
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; ++failures;
    }
    std::cout << "DC2TY transition failures: " << failures << '\n';
    return failures ? 1 : 0;
}
