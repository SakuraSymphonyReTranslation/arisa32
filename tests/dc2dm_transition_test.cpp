#include "arisa32/scene.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    if (!output) throw std::runtime_error("cannot write transition fixture");
}

struct Script {
    std::vector<uint8_t> bytes{1, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0x72, 0x9D, 0};
    void param(uint8_t op, uint8_t a, uint8_t b = 0) {
        bytes.insert(bytes.end(), {op, a, b});
    }
    void str(uint8_t op, const std::string& value) {
        bytes.push_back(op);
        bytes.insert(bytes.end(), value.begin(), value.end());
        bytes.push_back(0);
    }
    void slot(uint8_t id, const std::string& value) {
        bytes.push_back(0x2B);
        str(id, value);
    }
    void background(const std::string& value) {
        slot(1, value);
        str(0x33, value);
    }
    void text(const std::string& value, uint8_t opcode = 0x4F) {
        bytes.push_back(opcode);
        for (unsigned char c : value) bytes.push_back(uint8_t(c - 0x20));
        bytes.push_back(0);
    }
    void save(const fs::path& path) const {
        auto output = bytes;
        output.push_back(0);
        write_file(path, output);
    }
};

enum class Pattern { Solid, Horizontal, Vertical };

void image(const fs::path& path, int width, int height, Color color, Pattern pattern = Pattern::Solid) {
    std::vector<uint8_t> bytes{'C', 'R', 'X', 'G', 0, 0, 0, 0};
    auto word = [&](uint16_t v) { bytes.push_back(uint8_t(v)); bytes.push_back(uint8_t(v >> 8)); };
    word(uint16_t(width)); word(uint16_t(height)); word(1); word(0); word(0); word(0);
    const int stride = (width * 3 + 3) & ~3;
    std::vector<uint8_t> pixels(size_t(stride) * height);
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        for (int c = 0; c < 3; ++c)
            pixels[size_t(y) * stride + x * 3 + c] = pattern == Pattern::Horizontal ? uint8_t(x * 255 / (width - 1)) :
                pattern == Pattern::Vertical ? uint8_t(y * 255 / (height - 1)) : color[2 - c];
    }
    for (size_t i = 0; i < pixels.size(); i += 8) {
        bytes.push_back(255);
        bytes.insert(bytes.end(), pixels.begin() + i, pixels.begin() + std::min(i + 8, pixels.size()));
    }
    write_file(path, bytes);
}

Color pixel(const dc4scene::SceneRunner& scene, int x, int y) {
    const auto* p = scene.frame().data() + (size_t(y) * 1280 + x) * 4;
    return {p[0], p[1], p[2]};
}

void next(dc4scene::SceneRunner& scene, const char* expected) {
    CHECK(scene.advance_to_next_text());
    CHECK(scene.current_line() == expected);
    CHECK(scene.last_error().empty());
    scene.draw(-1);
}

void capture(const dc4scene::SceneRunner& scene, const std::string& name) {
    const char* directory = std::getenv("DC2DM_TRANSITION_CAPTURES");
    if (!directory || !*directory) return;
    fs::create_directories(directory);
    std::ofstream output(fs::path(directory) / (name + ".ppm"), std::ios::binary);
    output << "P6\n1280 720\n255\n";
    for (size_t i = 0; i < scene.frame().size(); i += 4)
        output.write(reinterpret_cast<const char*>(scene.frame().data() + i), 3);
    if (!output) throw std::runtime_error("cannot write transition capture");
}

bool near(Color value, Color expected, int tolerance = 2) {
    for (int c = 0; c < 3; ++c)
        if (std::abs(int(value[c]) - int(expected[c])) > tolerance) return false;
    return true;
}

void assets(const fs::path& root) {
    image(root / "GRP/BG/red.CRX", 1024, 576, red);
    image(root / "GRP/BG/blue.CRX", 1024, 576, blue);
    image(root / "GRP/BG/WHITE.CRX", 1024, 576, white);
    image(root / "GRP/MSK/MASK02B.CRX", 1024, 576, black, Pattern::Horizontal);
    image(root / "GRP/MSK/TALL.CRX", 1024, 600, black, Pattern::Vertical);
}

void mask_change(Script& script, const char* background, int step) {
    script.param(0x0D, 32);
    script.background(background);
    script.slot(12, "MSK\\MASK02B");
    script.param(0x0D, 21, uint8_t(step));
}

void recorded_wipe_script(const fs::path& root) {
    Script script;
    script.background("BG\\red");
    script.text("BEFORE");
    mask_change(script, "BG\\WHITE", 3);
    mask_change(script, "BG\\blue", 5);
    script.text("AFTER");
    script.save(root / "MES/recorded-wipe.mes");
}

Color native_blend(Color from, Color to, int mask, int progress) {
    const int q = mask - progress + 255;
    if (q >= 255) return from;
    if (q <= 0) return to;
    Color value{};
    for (int c = 0; c < 3; ++c)
        value[c] = uint8_t((int(to[c]) * (255 - q) + int(from[c]) * q) >> 8);
    return value;
}

int check_wipe(dc4scene::SceneRunner& scene, Color from, Color to, int step, const char* name) {
    CHECK(scene.waiting_for_time() && scene.animating());
    scene.draw(-1);
    CHECK(near(pixel(scene, 100, 200), from));
    CHECK(near(pixel(scene, 1180, 200), from));
    bool directional = false, soft = false, scaled = false;
    int elapsed = 0;
    const int render_ticks = (510 + step - 1) / step;
    for (int tick : {1, render_ticks / 4, render_ticks / 2, render_ticks * 3 / 4,
                     render_ticks, render_ticks + 1}) {
        scene.update(tick * 34 - elapsed);
        elapsed = tick * 34;
        scene.draw(-1);
        const int progress = elapsed / 34 * step;
        CHECK(pixel(scene, 0, 200) == native_blend(from, to, 0, progress));
        CHECK(pixel(scene, 640, 200) == native_blend(from, to, 127, progress));
        CHECK(pixel(scene, 1279, 200) == native_blend(from, to, 255, progress));
        const Color left = pixel(scene, 0, 200), middle = pixel(scene, 640, 200);
        const Color right = pixel(scene, 1279, 200);
        if (near(left, to, 10) && near(right, from, 10)) {
            if (!directional) capture(scene, name);
            directional = true;
        }
        if (!near(middle, from, 10) && !near(middle, to, 10)) soft = true;
        if (!near(pixel(scene, 1120, 200), pixel(scene, 1240, 200), 10)) scaled = true;
        CHECK(pixel(scene, 640, 100) == pixel(scene, 640, 500));
    }
    CHECK(scene.script_ready() && !scene.animating());
    CHECK(elapsed == ((510 + step - 1) / step + 1) * 34);
    CHECK(directional && soft && scaled);
    CHECK(near(pixel(scene, 80, 200), to));
    CHECK(near(pixel(scene, 1200, 200), to));
    scene.draw(-1);
    scene.update(16);
    scene.draw(-1);
    CHECK(near(pixel(scene, 640, 200), to));
    return elapsed;
}

void recorded_wipe(const fs::path& root) {
    dc4scene::SceneRunner scene(root.string());
    scene.set_animation_enabled(true);
    CHECK(scene.load_script((root / "MES/recorded-wipe.mes").string()));
    next(scene, "BEFORE");
    CHECK(!scene.advance_to_next_text());
    const int cover_ms = check_wipe(scene, red, white, 12, "synthetic-white-cover");
    CHECK(!scene.advance_to_next_text());
    const int reveal_ms = check_wipe(scene, white, blue, 24, "synthetic-white-reveal");
    CHECK(cover_ms > reveal_ms);
    next(scene, "AFTER");
    CHECK(near(pixel(scene, 640, 200), blue));
}

void explicit_mask_steps(const fs::path& root) {
    for (const auto& [parameter, step] : std::array<std::pair<int, int>, 6>{
             {{1, 4}, {2, 8}, {3, 12}, {4, 16}, {5, 24}, {255, 24}}}) {
        Script script;
        script.background("BG\\red");
        script.text("BEFORE");
        mask_change(script, "BG\\blue", parameter);
        script.text("AFTER");
        script.save(root / "MES/steps.mes");
        for (int speed : {0, 5, 11}) {
            dc4scene::SceneRunner scene(root.string());
            scene.set_animation_enabled(true);
            scene.set_effect_speed(speed);
            CHECK(scene.load_script((root / "MES/steps.mes").string()));
            next(scene, "BEFORE");
            CHECK(!scene.advance_to_next_text() && scene.waiting_for_time());
            const int duration = ((510 + step - 1) / step + 1) * 34;
            scene.update(duration - 35);
            scene.draw(-1);
            CHECK(!scene.script_ready());
            const int progress = (duration - 35) / 34 * step;
            CHECK(pixel(scene, 640, 200) == native_blend(red, blue, 127, progress));
            scene.update(1);
            scene.draw(-1);
            CHECK(!scene.script_ready());
            CHECK(pixel(scene, 640, 200) == blue);
            scene.update(33);
            CHECK(!scene.script_ready());
            scene.update(1);
            CHECK(scene.script_ready() && !scene.animating());
            next(scene, "AFTER");
        }
    }
}

void configured_mask_steps(const fs::path& root) {
    Script script;
    script.background("BG\\red");
    script.text("BEFORE");
    mask_change(script, "BG\\blue", 0);
    script.text("AFTER");
    script.save(root / "MES/configured-step.mes");
    for (const auto& [setting, step] : std::array<std::pair<int, int>, 5>{
             {{0, 2}, {1, 4}, {2, 8}, {3, 16}, {4, 16}}}) {
        dc4scene::SceneRunner scene(root.string());
        scene.set_animation_enabled(true);
        scene.set_effect_speed(11);
        CHECK(scene.load_script((root / "MES/configured-step.mes").string()));
        scene.set_variable(99, 12, setting);
        next(scene, "BEFORE");
        CHECK(!scene.advance_to_next_text() && scene.waiting_for_time());
        const int duration = ((510 + step - 1) / step + 1) * 34;
        scene.update(duration - 1);
        CHECK(!scene.script_ready());
        scene.update(1);
        CHECK(scene.script_ready() && !scene.animating());
        next(scene, "AFTER");
        CHECK(pixel(scene, 640, 200) == blue);
    }
}

void native_mask_crop(const fs::path& root) {
    Script script;
    script.background("BG\\red");
    script.text("BEFORE");
    script.param(0x0D, 32);
    script.background("BG\\blue");
    script.slot(12, "MSK\\TALL");
    script.param(0x0D, 21, 3);
    script.text("AFTER");
    script.save(root / "MES/tall-mask.mes");
    dc4scene::SceneRunner scene(root.string());
    scene.set_animation_enabled(true);
    CHECK(scene.load_script((root / "MES/tall-mask.mes").string()));
    next(scene, "BEFORE");
    CHECK(!scene.advance_to_next_text() && scene.waiting_for_time());
    scene.update(21 * 34);
    scene.draw(-1);
    for (int y : {0, 1, 37, 360, 575, 690, 719}) {
        const int native_y = y * 576 / 720;
        const int mask = native_y * 255 / 599;
        const Color expected = native_blend(red, blue, mask, 252);
        CHECK(pixel(scene, 0, y) == expected);
        CHECK(pixel(scene, 640, y) == expected);
        CHECK(pixel(scene, 1279, y) == expected);
    }
    CHECK(pixel(scene, 640, 719) != native_blend(red, blue, 255, 252));
    scene.finish_script_wait();
    next(scene, "AFTER");
}

Color global_blend(Color from, Color to, int progress) {
    if (progress >= 127) return to;
    Color value{};
    for (int c = 0; c < 3; ++c)
        value[c] = uint8_t((int(from[c]) * (127 - progress) + int(to[c]) * progress) / 127);
    return value;
}

void global_case(const fs::path& root, int mode, int parameter, int setting, int step,
                 bool missing_mask = false) {
    Script script;
    script.background("BG\\red");
    script.text("BEFORE");
    script.param(0x0D, 32);
    script.background("BG\\blue");
    if (missing_mask) script.slot(12, "MSK\\missing");
    script.param(0x0D, uint8_t(mode), uint8_t(parameter));
    script.text("AFTER");
    script.param(0x0D, 1, 1);
    script.text("REVEALED");
    script.save(root / "MES/global.mes");

    const int effective_mode = mode == 20 ? 2 : mode == 21 ? 0 : mode;
    const Color from = effective_mode == 1 ? black : red;
    const Color to = effective_mode == 2 ? black : blue;
    dc4scene::SceneRunner scene(root.string());
    scene.set_animation_enabled(true);
    scene.set_effect_speed(parameter % 2 ? 0 : 11);
    CHECK(scene.load_script((root / "MES/global.mes").string()));
    scene.set_variable(99, 12, setting);
    next(scene, "BEFORE");
    CHECK(!scene.advance_to_next_text() && scene.waiting_for_time() && scene.animating());
    scene.draw(-1);
    CHECK(pixel(scene, 0, 200) == global_blend(from, to, step));
    CHECK(pixel(scene, 1279, 200) == global_blend(from, to, step));
    const int tick = parameter >= 8 ? (parameter - 7) * 34 : 34;
    scene.update(tick);
    scene.draw(-1);
    CHECK(pixel(scene, 0, 200) == global_blend(from, to, step * 2));
    CHECK(pixel(scene, 1279, 200) == global_blend(from, to, step * 2));
    const int ticks = (127 + step - 1) / step;
    const int duration = ticks * tick + (effective_mode == 0 ? 34 : effective_mode == 2 ? tick : 0);
    scene.update(duration - tick - 1);
    CHECK(!scene.script_ready());
    scene.update(1);
    CHECK(scene.script_ready() && !scene.animating());
    next(scene, "AFTER");
    CHECK(pixel(scene, 640, 200) == to);
    if (effective_mode == 2) {
        scene.update(340);
        scene.draw(-1);
        CHECK(pixel(scene, 640, 200) == black);
        CHECK(!scene.advance_to_next_text() && scene.waiting_for_time());
        scene.draw(-1);
        CHECK(pixel(scene, 640, 200) == global_blend(black, blue, 32));
        scene.finish_script_wait();
        next(scene, "REVEALED");
        CHECK(pixel(scene, 640, 200) == blue);
    }
}

void global_fades(const fs::path& root) {
    constexpr int explicit_steps[] = {32, 16, 8, 4, 2, 1, 1};
    constexpr int configured_steps[] = {4, 8, 16, 32};
    for (int mode : {0, 1, 2}) {
        for (int parameter = 1; parameter <= 7; ++parameter)
            global_case(root, mode, parameter, 0, explicit_steps[parameter - 1]);
        for (int setting = 0; setting < 4; ++setting)
            global_case(root, mode, 0, setting, configured_steps[setting]);
        global_case(root, mode, 8, 0, 1);
        global_case(root, mode, 9, 0, 1);
    }
    for (int mode : {20, 21}) {
        global_case(root, mode, 3, 0, 8, true);
        global_case(root, mode, 0, 2, 16, true);
    }
}

void consumed_masks(const fs::path& root) {
    for (int first_mode : {20, 21}) for (bool skipped : {false, true}) {
        Script script;
        script.background("BG\\red");
        script.text("BEFORE");
        script.param(0x0D, 32);
        script.background("BG\\blue");
        script.slot(12, "MSK\\MASK02B");
        script.param(0x0D, uint8_t(first_mode), 3);
        script.param(0x0D, 32);
        script.background("BG\\red");
        script.param(0x0D, 21, 3);
        script.text("AFTER");
        script.save(root / "MES/consumed-mask.mes");
        dc4scene::SceneRunner scene(root.string());
        scene.set_animation_enabled(true);
        CHECK(scene.load_script((root / "MES/consumed-mask.mes").string()));
        next(scene, "BEFORE");
        CHECK(!scene.advance_to_next_text());
        if (skipped) scene.finish_script_wait();
        else scene.update(1496);
        CHECK(!scene.advance_to_next_text() && scene.waiting_for_time());
        scene.draw(-1);
        const Color expected = global_blend(first_mode == 20 ? black : blue, red, 8);
        CHECK(pixel(scene, 0, 200) == expected && pixel(scene, 1279, 200) == expected);
        scene.update(577);
        CHECK(!scene.script_ready());
        scene.update(1);
        CHECK(scene.script_ready() && !scene.animating());
        next(scene, "AFTER");
        CHECK(pixel(scene, 640, 200) == red);
    }
}

void masked_black(const fs::path& root) {
    Script script;
    script.background("BG\\red");
    script.text("BEFORE");
    script.param(0x0D, 32);
    script.background("BG\\blue");
    script.slot(12, "MSK\\MASK02B");
    script.param(0x0D, 20, 3);
    script.text("BLACK");
    script.save(root / "MES/masked-black.mes");
    dc4scene::SceneRunner scene(root.string());
    scene.set_animation_enabled(true);
    CHECK(scene.load_script((root / "MES/masked-black.mes").string()));
    next(scene, "BEFORE");
    CHECK(!scene.advance_to_next_text());
    check_wipe(scene, red, black, 12, "synthetic-mask-to-black");
    next(scene, "BLACK");
    CHECK(pixel(scene, 640, 200) == black);
}

void sweep(const fs::path& root) {
    Script script;
    script.background("BG\\red");
    script.text("BEFORE");
    script.param(0x0D, 32);
    script.background("BG\\blue");
    script.param(0x0D, 17, 1);
    script.text("AFTER");
    script.save(root / "MES/sweep.mes");
    for (int setting = 0; setting < 4; ++setting) {
        dc4scene::SceneRunner scene(root.string());
        scene.set_animation_enabled(true);
        CHECK(scene.load_script((root / "MES/sweep.mes").string()));
        scene.set_variable(99, 12, setting);
        next(scene, "BEFORE");
        CHECK(!scene.advance_to_next_text() && scene.waiting_for_time());
        int ticks = 1;
        while (2 * (setting + 1) * ticks * (ticks + 1) < 1024) ++ticks;
        int previous = 1;
        for (int n : {1, 2, ticks / 2, ticks - 1, ticks}) {
            scene.update((n - previous) * 34);
            previous = n;
            CHECK(!scene.script_ready());
            scene.draw(-1);
            const int boundary = std::min(1024, 2 * (setting + 1) * n * (n + 1));
            for (int x : {0, 40, 320, 640, 1000, 1279}) {
                const int native_x = x * 1024 / 1280;
                const int alpha = native_x < boundary ? 255 :
                    std::clamp(252 - 4 * (native_x - boundary), 0, 255);
                Color expected{};
                for (int c = 0; c < 3; ++c)
                    expected[c] = uint8_t((int(blue[c]) * alpha + int(red[c]) * (255 - alpha)) / 255);
                CHECK(pixel(scene, x, 200) == expected);
            }
        }
        scene.update(33);
        CHECK(!scene.script_ready());
        scene.update(1);
        CHECK(scene.script_ready() && !scene.animating());
        next(scene, "AFTER");
        CHECK(pixel(scene, 1279, 200) == blue);
    }
    dc4scene::SceneRunner skipped(root.string());
    skipped.set_animation_enabled(true);
    CHECK(skipped.load_script((root / "MES/sweep.mes").string()));
    next(skipped, "BEFORE");
    CHECK(!skipped.advance_to_next_text());
    skipped.update(68);
    skipped.finish_script_wait();
    next(skipped, "AFTER");
    CHECK(!skipped.animating() && pixel(skipped, 640, 200) == blue);
}

void profile_reset(const fs::path& root) {
    Script solid;
    solid.background("BG\\red");
    solid.param(0x0D, 2, 1);
    solid.text("BLACK");
    solid.save(root / "MES/held-black.mes");
    for (uint16_t version : {0xAAB6, 0xABB6, 0x9969}) {
        Script other;
        if (version == 0x9969) {
            other.bytes = {1, 0, 0, 0, 2, 0, 0, 0, 0x69, 0x99};
            other.str(0x30, "BG\\blue");
            other.text("OTHER", 0x4C);
        } else {
            other.bytes[10] = uint8_t(version);
            other.bytes[11] = uint8_t(version >> 8);
            other.bytes.push_back(0x3C);
            other.str(1, "BG\\blue");
            other.str(0x49, "BG\\blue");
            other.text("OTHER", 0x6B);
        }
        other.save(root / "MES/other.mes");
        dc4scene::SceneRunner scene(root.string());
        CHECK(scene.load_script((root / "MES/held-black.mes").string()));
        next(scene, "BLACK");
        CHECK(pixel(scene, 640, 200) == black);
        CHECK(scene.load_script((root / "MES/other.mes").string()));
        next(scene, "OTHER");
        CHECK(!scene.animating() && pixel(scene, 640, 200) == blue);
    }
}

void lifecycle(const fs::path& root) {
    const auto path = (root / "MES/recorded-wipe.mes").string();
    for (bool animated : {false, true}) {
        dc4scene::SceneRunner scene(root.string());
        scene.set_animation_enabled(animated);
        if (animated) scene.set_effects_enabled(false);
        CHECK(scene.load_script(path));
        next(scene, "BEFORE");
        next(scene, "AFTER");
        CHECK(!scene.animating() && !scene.waiting_for_time());
        CHECK(pixel(scene, 640, 200) == blue);
    }

    dc4scene::SceneRunner skipped(root.string());
    skipped.set_animation_enabled(true);
    CHECK(skipped.load_script(path));
    next(skipped, "BEFORE");
    CHECK(!skipped.advance_to_next_text());
    skipped.update(340);
    skipped.finish_script_wait();
    skipped.draw(-1);
    CHECK(!skipped.animating() && pixel(skipped, 640, 200) == white);
    CHECK(!skipped.advance_to_next_text());
    skipped.update(170);
    skipped.finish_animations();
    skipped.draw(-1);
    CHECK(!skipped.animating() && pixel(skipped, 640, 200) == blue);
    skipped.finish_script_wait();
    next(skipped, "AFTER");

    dc4scene::SceneRunner disabled(root.string());
    disabled.set_animation_enabled(true);
    CHECK(disabled.load_script(path));
    next(disabled, "BEFORE");
    CHECK(!disabled.advance_to_next_text());
    disabled.update(340);
    disabled.set_effects_enabled(false);
    disabled.draw(-1);
    CHECK(!disabled.animating() && pixel(disabled, 640, 200) == white);
    disabled.finish_script_wait();
    next(disabled, "AFTER");
    CHECK(pixel(disabled, 640, 200) == blue);

    Script clean;
    clean.background("BG\\blue");
    clean.text("RESET");
    clean.save(root / "MES/reset.mes");
    dc4scene::SceneRunner replaced(root.string());
    replaced.set_animation_enabled(true);
    CHECK(replaced.load_script(path));
    next(replaced, "BEFORE");
    CHECK(!replaced.advance_to_next_text());
    replaced.update(340);
    CHECK(replaced.load_script((root / "MES/reset.mes").string()));
    next(replaced, "RESET");
    CHECK(!replaced.animating() && pixel(replaced, 640, 200) == blue);

    for (bool replay : {false, true}) {
        dc4scene::SceneRunner interrupted(root.string());
        interrupted.set_animation_enabled(true);
        CHECK(interrupted.load_script(path));
        interrupted.set_variable(9, 7, replay ? 1 : 0);
        next(interrupted, "BEFORE");
        CHECK(!interrupted.advance_to_next_text() && interrupted.animating());
        interrupted.update(340);
        interrupted.finish_replay();
        CHECK(interrupted.save_load_allowed());
        CHECK(!interrupted.animating() && !interrupted.waiting_for_time() && !interrupted.script_ready());
        interrupted.update(5000);
        CHECK(!interrupted.animating() && !interrupted.waiting_for_time());
        CHECK(interrupted.load_script((root / "MES/reset.mes").string()));
        next(interrupted, "RESET");
        CHECK(pixel(interrupted, 640, 200) == blue);
    }
}

std::string readable_line(std::string line) {
    line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
    std::replace(line.begin(), line.end(), '_', ' ');
    return line;
}

void recorded_checkpoint(const fs::path& data) {
    const auto path = data / "MES/mar_0129_e1.mes";
    dc4scene::FontRenderer font;
    CHECK(font.load("C:/Windows/Fonts/msgothic.ttc", 0, 15, false));
    dc4scene::SceneRunner scene(data.string());
    scene.set_font(&font);
    CHECK(scene.load_ui_script((data / "MES/start.mes").string()));
    CHECK(scene.load_script(path.string()));
    bool found = false;
    for (int step = 0; step < 2048; ++step) {
        if (scene.advance_to_next_text()) {
            if (readable_line(scene.current_line()).find("Rencana ini baru saja disusun") != std::string::npos) {
                found = true;
                break;
            }
        } else {
            if (!scene.last_error().empty() || scene.scene_ended() || scene.waiting_for_choice()) break;
            if (!scene.pending_movie().empty()) scene.finish_movie();
            scene.finish_script_wait();
        }
    }
    CHECK(found);
    CHECK(scene.last_error().empty());
    if (!found) return;
    scene.draw(-1);
    capture(scene, "recorded-before");
    std::vector<uint8_t> native_save;
    CHECK(scene.capture_pc_save(&native_save));
    if (const char* directory = std::getenv("DC2DM_TRANSITION_CAPTURES"))
        if (*directory) write_file(fs::path(directory) / "transition.dat", native_save);

    dc4scene::SceneRunner restored(data.string());
    restored.set_font(&font);
    CHECK(restored.load_ui_script((data / "MES/start.mes").string()));
    CHECK(restored.load_script(path.string()));
    CHECK(restored.restore_pc_save(native_save));
    CHECK(restored.current_line() == scene.current_line());
    CHECK(restored.speaker() == scene.speaker());
    restored.draw(-1);
    CHECK(restored.frame() == scene.frame());
    std::cout << "DC2DM recorded transition checkpoint: " << path.filename() << '\n';

    dc4scene::SceneRunner selector(data.string());
    CHECK(selector.load_ui_script((data / "MES/start.mes").string()));
    CHECK(selector.load_script((data / "MES/sceselect.mes").string()));
    selector.set_variable(100, 1, 1);
    CHECK(!selector.advance_to_next_text() && selector.waiting_for_time());
    CHECK(std::any_of(selector.buttons().begin(), selector.buttons().end(),
                      [](const auto& button) { return button.action == "sce01"; }));
    CHECK(selector.save_load_allowed());
    selector.finish_replay();
    CHECK(!selector.waiting_for_time() && !selector.script_ready() && !selector.animating());
}

} // namespace

int main(int argc, char** argv) {
    const auto root = fs::temp_directory_path() / ("arisa32-dc2dm-transitions-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        if (argc < 2 || std::string(argv[1]) != "--capture-only") {
            assets(root);
            recorded_wipe_script(root);
            recorded_wipe(root);
            explicit_mask_steps(root);
            configured_mask_steps(root);
            native_mask_crop(root);
            global_fades(root);
            consumed_masks(root);
            masked_black(root);
            sweep(root);
            profile_reset(root);
            lifecycle(root);
        }
        if (const char* data = std::getenv("DC2DM_DATA")) {
            if (*data) recorded_checkpoint(data);
        } else {
            std::cout << "DC2DM_DATA unset; recorded-scene check skipped\n";
        }
        fs::remove_all(root);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        ++failures;
    }
    std::cout << "DC2DM transition failures: " << failures << '\n';
    return failures ? 1 : 0;
}
