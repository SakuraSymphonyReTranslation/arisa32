#include "arisa32/scene.h"
#include "arisa32/save_paths.h"
#include "arisa32/resource.h"
#include "arisa32/log_screen.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
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
    if (!out) throw std::runtime_error("cannot create test fixture");
}
struct Script {
    std::vector<uint8_t> bytes{1,0,0,0,3,0,0,0,0,0,0xB6,0xAA,0};
    void word(uint16_t value) { bytes.push_back(uint8_t(value)); bytes.push_back(uint8_t(value >> 8)); }
    void camera(uint16_t scale) { bytes.push_back(0x9D); word(0); word(scale); word(uint16_t(-100)); word(300); }
    void text(const std::string& text) { bytes.push_back(0x6B); for (unsigned char ch : text) bytes.push_back(uint8_t(ch - 0x20)); bytes.push_back(0); }
    void words(uint8_t op, uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
        bytes.push_back(op); word(a); word(b); word(c); word(d);
    }
    void string(uint8_t op, const std::string& value) {
        bytes.push_back(op); bytes.insert(bytes.end(), value.begin(), value.end()); bytes.push_back(0);
    }
    void slot(uint8_t index, const std::string& value) {
        bytes.push_back(0x3C); bytes.push_back(index); bytes.insert(bytes.end(), value.begin(), value.end()); bytes.push_back(0);
    }
};
// Tiny literal-only CRX fixture: RGB quadrants expose framing and stale layers
// without depending on proprietary assets or another image library.
void write_image(const fs::path& path, int width, int height) {
    std::vector<uint8_t> bytes{'C','R','X','G',0,0,0,0};
    auto word = [&](uint16_t value) { bytes.push_back(uint8_t(value)); bytes.push_back(uint8_t(value >> 8)); };
    word(uint16_t(width)); word(uint16_t(height)); word(1); word(0); word(0); word(0);
    const int stride = (width * 3 + 3) & ~3;
    std::vector<uint8_t> pixels(size_t(stride) * height, 0);
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        const size_t at = size_t(y) * stride + x * 3;
        pixels[at] = uint8_t(y < height / 2 ? 40 : 240);
        pixels[at + 1] = 50;
        pixels[at + 2] = uint8_t(x < width / 2 ? 240 : 40);
    }
    for (size_t i = 0; i < pixels.size(); i += 8) {
        bytes.push_back(255);
        bytes.insert(bytes.end(), pixels.begin() + i, pixels.begin() + std::min(i + 8, pixels.size()));
    }
    write_file(path, bytes);
}
void fixture_round_trip(const fs::path& root) {
    Script script;
    write_image(root / "GRP/BG/test.CRX", 64, 36);
    script.slot(1, "BG\\test"); script.string(0x49, "BG\\test");
    script.camera(100); script.text("FIRST\nLINE"); script.camera(160); script.text("SECOND"); script.camera(100); script.text("THIRD"); script.bytes.push_back(0);
    write_file(root / "MES/fixture.mes", script.bytes);
    std::vector<uint8_t> names(236, 0); names[0] = 7;
    write_file(root / "GRP/NAMES.DAT", names);
    dc4scene::SceneRunner scene(root.string());
    CHECK(scene.load_script((root / "MES/fixture.mes").string()));
    CHECK(scene.advance_to_next_text()); CHECK(scene.current_line() == "FIRST\nLINE");
    CHECK(scene.visible_text_length() == 9); CHECK(scene.camera_scale() == 1);
    scene.draw(-1); const auto normal_frame = scene.frame();
    CHECK(scene.advance_to_next_text()); CHECK(scene.camera_scale() == 1.6f);
    scene.draw(-1); const auto frame = scene.frame();
    CHECK(frame != normal_frame);
    scene.draw(-1); CHECK(scene.frame() == frame); // cached scene is identical
    const auto save = root / "saves/roundtrip.dc4sav";
    CHECK(scene.save_state(save.string()));
    CHECK(scene.advance_to_next_text()); CHECK(scene.camera_scale() == 1);
    scene.draw(-1); CHECK(scene.frame() == normal_frame);
    CHECK(scene.load_state(save.string())); CHECK(scene.current_line() == "SECOND"); CHECK(scene.camera_scale() == 1.6f);
    scene.draw(-1); CHECK(scene.frame() == frame);
    CHECK(scene.advance_to_next_text()); CHECK(scene.current_line() == "THIRD");
    // Truncation/corruption must not erase or advance the live scene.
    std::ifstream input(save, std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
    auto damaged = bytes; damaged.back() ^= 1;
    write_file(root / "saves/corrupt.dc4sav", damaged);
    CHECK(!scene.load_state((root / "saves/corrupt.dc4sav").string()));
    CHECK(scene.current_line() == "THIRD"); CHECK(scene.camera_scale() == 1);
    for (size_t length : {size_t(0), size_t(8), size_t(23), bytes.size() - 1}) {
        write_file(root / "saves/truncated.dc4sav", {bytes.begin(), bytes.begin() + length});
        CHECK(!scene.load_state((root / "saves/truncated.dc4sav").string())); CHECK(scene.current_line() == "THIRD");
    }
}
void dc4_sprite_priority_regressions(const fs::path& root) {
    const auto data=root/"sprite-priority";
    write_image(data/"GRP/BST/first.CRX",64,36);
    write_image(data/"GRP/BST/second.CRX",64,36);
    for (uint8_t version : {0xAA,0xAB}) {
        Script script; script.bytes[11]=version;
        script.words(0x8B,20,0,1,0);
        script.slot(4,"BST\\first"); script.slot(5,"BST\\first"); script.text("DEFAULT");
        script.words(0x8B,49,0,0,0x21); script.text("BEHIND");
        script.slot(5,"BST\\second"); script.text("REPLACED");
        script.words(0x8B,0,0,0,0x81); script.text("HIDDEN");
        script.slot(5,"BST\\second"); script.text("RESHOWN");
        script.words(0x8B,50,0,0,0x21); script.text("DEFAULT AGAIN");
        script.words(0x8B,65535,0,0,0x21); script.text("CLAMPED");
        script.bytes.push_back(0);
        write_file(data/"MES/priority.mes",script.bytes);
        dc4scene::SceneRunner scene(data.string());
        CHECK(scene.load_script((data/"MES/priority.mes").string()));
        CHECK(scene.advance_to_next_text()); scene.draw(-1);
        const auto front=scene.frame();
        CHECK(front[(10*1280+40)*4]==240);
        CHECK(scene.advance_to_next_text()); scene.draw(-1);
        const auto behind=scene.frame();
        CHECK(behind[(10*1280+40)*4]==40 && behind!=front);
        CHECK(scene.sprite(1).priority==49 && scene.sprite(0).priority==50);
        CHECK(scene.advance_to_next_text()); scene.draw(-1);
        CHECK(scene.frame()==behind && scene.sprite(1).priority==49);
        std::vector<uint8_t> saved;
        CHECK(scene.capture_state(&saved));
        CHECK(scene.advance_to_next_text() && !scene.sprite(1).visible);
        std::vector<uint8_t> hidden; CHECK(scene.capture_state(&hidden));
        CHECK(scene.advance_to_next_text()); scene.draw(-1); CHECK(scene.frame()==behind);
        CHECK(scene.advance_to_next_text()); scene.draw(-1); CHECK(scene.frame()==front);
        CHECK(scene.advance_to_next_text() && scene.sprite(1).priority==100);
        CHECK(scene.restore_state(saved)); scene.draw(-1);
        CHECK(scene.sprite(1).priority==49 && scene.frame()==behind);
        CHECK(scene.restore_state(hidden) && !scene.sprite(1).visible);
        CHECK(scene.advance_to_next_text()); scene.draw(-1); CHECK(scene.frame()==behind);

        Script native_script;
        native_script.bytes={2,0,0,0,3,0,0,0,3,0,0,0,99,0,100,0,0xB6,version,0};
        native_script.words(0x8B,20,0,1,0);
        native_script.slot(4,"BST\\first");native_script.slot(5,"BST\\second");
        native_script.words(0x8B,49,0,0,0x21);
        native_script.bytes.insert(native_script.bytes.end(),{3,0,0});native_script.text("VISIBLE");
        native_script.words(0x8B,0,0,0,0x81);
        native_script.bytes.insert(native_script.bytes.end(),{3,1,0});native_script.text("HIDDEN");
        native_script.slot(5,"BST\\second");native_script.text("RESHOWN");
        native_script.bytes.push_back(0);
        write_file(data/"MES/priority-native.mes",native_script.bytes);
        CHECK(scene.load_script((data/"MES/priority-native.mes").string()));
        CHECK(scene.advance_to_next_text());
        std::vector<uint8_t> native;std::string error;
        CHECK(scene.capture_pc_save(&native,&error));
        CHECK(native.size()==0x56FE4 && native[0x18E20+20*960+0x380]==49);
        CHECK(scene.restore_pc_save(native,&error));scene.draw(-1);
        CHECK(scene.sprite(1).priority==49 && scene.frame()==behind);
        CHECK(scene.advance_to_next_text() && !scene.sprite(1).visible);
        CHECK(scene.capture_pc_save(&native,&error));
        CHECK(scene.restore_pc_save(native,&error));
        CHECK(!scene.sprite(1).visible && scene.sprite(1).priority==49);
        CHECK(scene.advance_to_next_text());scene.draw(-1);CHECK(scene.frame()==behind);
    }
}
void dc4_implicit_sprite_pivot_regressions(const fs::path& root) {
    const auto data = root / "implicit-pivot";
    write_image(data / "GRP/BST/loose.CRX", 64, 36);
    for (const auto version : {uint16_t(0xAAB6), uint16_t(0xABB6)}) {
        Script main; main.bytes[11] = uint8_t(version >> 8); main.text("MAIN");
        write_file(data / "MES/main.mes", main.bytes);
        write_file(data / "MES/dc4_start.mes", main.bytes);
        for (int axis = 0; axis < 4; ++axis) {
            const int x = axis & 1 ? 10 : 0, y = axis & 2 ? 12 : 0;
            Script script; script.bytes[11] = uint8_t(version >> 8);
            script.words(0x8B, 300, 200, 0, 0);
            script.words(0x8B, 50, uint16_t(x), uint16_t(y), 16);
            script.slot(4, "BST\\loose"); script.text("FIRST");
            script.words(0x9D, 0, 200, 0, 0); script.text("ZOOM");
            script.words(0x8B, 100, uint16_t(x), uint16_t(y), 16);
            script.words(0x9D, 0, 100, 0, 0); script.text("UNIT");
            write_file(data / "MES/pivot.mes", script.bytes);
            dc4scene::SceneRunner scene(data.string());
            CHECK(scene.load_script((data / "MES/pivot.mes").string()));
            CHECK(scene.advance_to_next_text()); scene.set_message_visible(false); scene.draw(-1);
            const int left = x ? 305 : 316, top = y ? 206 : 209;
            auto has_pixel = [&](int px, int py) { return scene.frame()[(size_t(py) * 1280 + px) * 4] != 0; };
            CHECK(has_pixel(left, top)); CHECK(!has_pixel(left - 1, top)); CHECK(!has_pixel(left, top - 1));
            CHECK(scene.sprite(0).x == x && scene.sprite(0).y == y);
            CHECK(scene.advance_to_next_text()); scene.draw(-1);
            CHECK(has_pixel(left * 2 - 512, top * 2 - 288));
            CHECK(!has_pixel(left * 2 - 513, top * 2 - 288));
            const auto zoom = scene.frame();
            fs::create_directories(data / "saves");
            CHECK(scene.save_state((data / "saves/pivot.dc4sav").string()));
            CHECK(scene.advance_to_next_text()); scene.draw(-1);
            CHECK(has_pixel(300, 200)); CHECK(!has_pixel(299, 200));
            CHECK(scene.load_state((data / "saves/pivot.dc4sav").string())); scene.draw(-1);
            CHECK(scene.frame() == zoom);
            CHECK(scene.sprite(0).x == x && scene.sprite(0).y == y);
        }
    }
}
void dc4_cutin_visibility_regressions(const fs::path& root) {
    const auto data = root / "cutin-visibility";
    write_image(data / "GRP/BST/cut.CRX", 64, 36);
    for (const auto version : {uint16_t(0xAAB6), uint16_t(0xABB6)}) {
        Script script; script.bytes[11] = uint8_t(version >> 8);
        auto effect = [&](uint8_t value, const std::string& parameter) {
            script.bytes.push_back(0x40); script.bytes.push_back(value);
            script.bytes.insert(script.bytes.end(), parameter.begin(), parameter.end());
            script.bytes.push_back(0);
        };
        script.slot(25, "BST\\cut"); effect(0, "."); effect(1, "-n"); script.text("WATCH");
        effect(0, "-n"); script.text("HIDDEN");
        effect(1, "-n"); script.text("RESTORED");
        script.words(0x8C, 0, 3, 0, 128); script.text("DISMISS");
        script.slot(25, "BST\\cut"); effect(1, "-n"); script.text("REUSED");
        script.slot(26, "BST\\cut"); effect(0, "-n"); script.text("OTHER_SLOT");
        effect(1, "-n"); script.text("SHOW_PRIMARY");
        effect(0, "-"); script.text("REMOVE_PRIMARY");
        write_file(data / "MES/main.mes", script.bytes);
        write_file(data / "MES/dc4_start.mes", script.bytes);
        dc4scene::SceneRunner scene(data.string()); scene.set_animation_enabled(true);
        CHECK(scene.load_script((data / "MES/main.mes").string()));
        CHECK(scene.advance_to_next_text()); scene.set_message_visible(false); scene.draw(-1);
        CHECK(scene.sprite(12).visible);
        CHECK(scene.frame()[0] > 0); const auto visible = scene.frame();
        CHECK(scene.advance_to_next_text()); scene.draw(-1);
        CHECK(!scene.sprite(12).visible); CHECK(scene.frame()[0] == 0);
        CHECK(scene.advance_to_next_text()); scene.draw(-1); CHECK(scene.frame() == visible);
        std::vector<uint8_t> snapshot; CHECK(scene.capture_state(&snapshot));
        CHECK(scene.advance_to_next_text()); CHECK(scene.sprite(12).fade.active());
        scene.update(150); CHECK(scene.sprite(12).alpha > 0 && scene.sprite(12).alpha < 255);
        scene.update(150); CHECK(!scene.sprite(12).visible);
        scene.finish_animations(); CHECK(!scene.sprite(12).visible);
        CHECK(scene.restore_state(snapshot)); scene.draw(-1); CHECK(scene.frame() == visible);
        CHECK(scene.advance_to_next_text()); scene.finish_animations();
        CHECK(scene.advance_to_next_text()); scene.draw(-1);
        CHECK(scene.sprite(12).visible && scene.sprite(12).alpha == 255); CHECK(scene.frame() == visible);
        CHECK(scene.advance_to_next_text()); CHECK(!scene.sprite(12).visible && scene.sprite(13).visible);
        CHECK(scene.advance_to_next_text()); CHECK(scene.sprite(12).visible && scene.sprite(13).visible);
        CHECK(scene.advance_to_next_text()); CHECK(!scene.sprite(12).visible && scene.sprite(13).visible);
    }
}
void graphics_and_text_regressions(const fs::path& root) {
    // A small off-centre character exposes the native placement model: the
    // 0x8B place stores the zoom anchor (tgt) and the mode-0 op the current
    // position (cur); the canvas centre lands at cur + tgt*(1-s) + frame*s.
    write_image(root / "GRP/BST/sprite.CRX", 64, 36);
    Script script;
    script.slot(4, "BST\\sprite"); script.words(0x8B, 100, 640, 360, 16);
    script.words(0x8B, 640, 360, 0, 0);
    script.text("SPRITE"); script.camera(160); script.text("ZOOM");
    script.slot(4, "-"); script.text("HIDDEN");
    script.slot(4, "--"); script.text("REUSED");
    script.string(0x49, ":n"); script.slot(1, "BG\\test");
    script.string(0x49, "BG\\test"); script.text("COMMIT");
    script.string(0x49, ":r"); script.slot(1, "BG\\test");
    script.slot(4, "--"); script.string(0x49, "BG\\test"); script.text("QUEUED");
    // 0x80 explicitly hides the retained sprite.
    script.words(0x8B, 0, 0, 0, 128); script.text("OPCODE_HIDE");
    script.slot(4, "--"); script.words(0x8B, 100, uint16_t(-10), 360, 16); script.text("SIGNED");
    script.bytes.push_back(0);
    write_file(root / "MES/graphics.mes", script.bytes);
    dc4scene::SceneRunner scene(root.string()); scene.set_message_visible(false);
    CHECK(scene.load_script((root / "MES/graphics.mes").string()));
    CHECK(scene.advance_to_next_text()); CHECK(scene.sprite(0).visible);
    scene.draw(-1);
    // cur(640,360) + tgt(640,360)*(1-1) + frame(32,18)*1 -> TL (640,360).
    CHECK(scene.frame()[(size_t(360) * 1280 + 640) * 4] > 200);
    CHECK(scene.frame()[(size_t(360) * 1280 + 639) * 4] == 0);
    CHECK(scene.advance_to_next_text()); CHECK(scene.camera_scale() == 1.6f);
    scene.draw(-1);
    // Background pivot412,588 -> character pivot432,528. The native affine
    // composition moves the sprite's top-left 640,360 to 765,259 at scale1.6.
    CHECK(scene.frame()[(size_t(259) * 1280 + 765) * 4] > 200);
    CHECK(scene.frame()[(size_t(259) * 1280 + 764) * 4] == 0);
    const auto zoom = scene.frame();
    CHECK(scene.save_state((root / "saves/sprite.dc4sav").string()));
    CHECK(scene.advance_to_next_text()); CHECK(!scene.sprite(0).visible);
    CHECK(scene.load_state((root / "saves/sprite.dc4sav").string())); scene.draw(-1);
    CHECK(scene.frame() == zoom);
    CHECK(scene.advance_to_next_text()); CHECK(!scene.sprite(0).visible);
    CHECK(scene.advance_to_next_text()); CHECK(scene.sprite(0).visible);
    CHECK(scene.advance_to_next_text()); CHECK(scene.current_line() == "COMMIT");
    CHECK(scene.sprite(0).visible); CHECK(scene.camera_scale() == 1.6f); // :n preserves the retained sprite
    CHECK(scene.advance_to_next_text()); CHECK(scene.sprite(0).visible); CHECK(scene.camera_scale() == 1);
    CHECK(scene.advance_to_next_text()); CHECK(!scene.sprite(0).visible);
    CHECK(scene.advance_to_next_text()); CHECK(scene.sprite(0).x == -10);

#ifdef _WIN32
    dc4scene::FontRenderer font;
    CHECK(font.load("C:/Windows/Fonts/msgothic.ttc", 0, 22));
    Script text;
    text.text("@b1@t20@w1@bt1@d1HELLO");
    text.text("@crA@cgB@cbC@c2D@cwE");
    text.text("@lAAAA@zB@mC");
    text.text("@wA@mB"); text.bytes.push_back(0);
    write_file(root / "MES/richtext.mes", text.bytes);
    dc4scene::SceneRunner rich(root.string()); rich.set_font(&font);
    CHECK(rich.load_script((root / "MES/richtext.mes").string()));
    CHECK(rich.advance_to_next_text()); CHECK(rich.visible_text() == u"HELLO"); CHECK(rich.visible_text_length() == 5);
    CHECK(rich.advance_to_next_text()); CHECK(rich.visible_text() == u"ABCDE"); rich.draw(-1);
    const auto colours = rich.frame();
    bool red = false, green = false, blue = false, palette_red = false, white = false;
    for (int y = 560; y < 600; ++y) for (int x = 283; x < 338; ++x) {
        const size_t at = (size_t(y) * 1280 + x) * 4;
        const auto r = colours[at], g = colours[at + 1], b = colours[at + 2];
        if (x < 294 && r > 180 && g < 20 && b < 20) red = true;
        if (x >= 294 && x < 305 && g > 180 && r < 20 && b < 20) green = true;
        if (x >= 305 && x < 316 && b > 180 && r < 20 && g < 20) blue = true;
        if (x >= 316 && x < 327 && r > 180 && g < 20 && b < 20) palette_red = true;
        if (x >= 327 && r > 180 && g > 180 && b > 180) white = true;
    }
    CHECK(red && green && blue && palette_red && white);
    // Reveal counts printed characters only; hidden commands take no time.
    rich.draw(1);
    CHECK(rich.frame()[(size_t(582) * 1280 + 310) * 4 + 2] == 0);
    CHECK(rich.advance_to_next_text()); rich.draw(-1);
    bool large = false, beyond_default_width = false;
    for (int y = 560; y < 596; ++y) for (int x = 283; x < 365; ++x) {
        const auto r = rich.frame()[(size_t(y) * 1280 + x) * 4];
        if (y < 576 && r > 180) large = true;
        if (x > 339 && r > 180) beyond_default_width = true;
    }
    CHECK(large && beyond_default_width);
    CHECK(rich.advance_to_next_text()); CHECK(rich.visible_text() == u"AB"); rich.draw(-1);
    bool wide = false;
    for (int y = 570; y < 596; ++y) for (int x = 305; x < 327; ++x)
        if (rich.frame()[(size_t(y) * 1280 + x) * 4] > 180) wide = true;
    CHECK(wide);
#endif
}
void animation_regressions(const fs::path& root) {
    Script script;
    script.slot(1, "BG\\test"); script.string(0x49, "BG\\test");
    script.text("INITIAL");
    script.words(0x99, 5, 0, 0, 0); script.words(0x9D, 1, 160, uint16_t(-100), 300); script.text("ANIMATED");
    script.words(0x9D, 0, 130, 0, 0); script.text("IMMEDIATE");
    script.words(0x99, 0, 0, 0, 0); script.words(0x9D, 1, 180, 0, 0); script.text("ZERO_DURATION");
    script.words(0x99, 1, 0, 0, 0);
    script.words(0x9D, 1, 140, 40, 20); script.words(0x9D, 1, 120, 80, 40); script.text("QUEUED");
    script.words(0x99, 5, 0, 0, 0); script.words(0x9D, 1, 150, 0, 0); script.text("RETURN_TEST");
    script.words(0x9D, 1, 100, 200, 300); script.text("RETURN"); script.bytes.push_back(0);
    write_file(root / "MES/animation.mes", script.bytes);
    dc4scene::SceneRunner scene(root.string()); scene.set_message_visible(false);
    scene.set_animation_enabled(true);
    CHECK(scene.load_script((root / "MES/animation.mes").string()));
    CHECK(scene.advance_to_next_text()); scene.draw(-1); const auto initial = scene.frame();
    CHECK(scene.advance_to_next_text()); CHECK(scene.animating()); CHECK(scene.camera_scale() == 1);
    scene.update(500); CHECK(std::abs(scene.camera_scale() - 1.3f) < 0.0001f);
    scene.draw(-1); const auto midway = scene.frame(); CHECK(midway != initial);
    CHECK(scene.save_state((root / "saves/animation.dc4sav").string()));
    scene.update(500); CHECK(!scene.animating()); CHECK(scene.camera_scale() == 1.6f);
    scene.draw(-1); const auto target = scene.frame(); CHECK(target != midway);
    CHECK(scene.load_state((root / "saves/animation.dc4sav").string()));
    CHECK(scene.animating()); CHECK(std::abs(scene.camera_scale() - 1.3f) < 0.0001f);
    scene.draw(-1); CHECK(scene.frame() == midway);
    scene.update(500); scene.draw(-1); CHECK(scene.frame() == target); CHECK(!scene.animating());
    CHECK(scene.advance_to_next_text()); CHECK(!scene.animating()); CHECK(scene.camera_scale() == 1.3f);
    CHECK(scene.advance_to_next_text()); CHECK(!scene.animating()); CHECK(scene.camera_scale() == 1.8f);
    CHECK(scene.advance_to_next_text()); CHECK(scene.animating());
    scene.update(300); CHECK(std::abs(scene.camera_scale() - 1.3f) < 0.0001f); CHECK(scene.animating());
    CHECK(scene.save_state((root / "saves/queued.dc4sav").string()));
    scene.update(100); CHECK(scene.camera_scale() == 1.2f); CHECK(!scene.animating());
    CHECK(scene.load_state((root / "saves/queued.dc4sav").string()));
    scene.update(100); CHECK(scene.camera_scale() == 1.2f); CHECK(!scene.animating());
    CHECK(scene.advance_to_next_text()); scene.finish_animations(); CHECK(scene.camera_scale() == 1.5f);
    CHECK(scene.advance_to_next_text()); scene.update(1000); CHECK(scene.camera_scale() == 1); CHECK(!scene.animating());
    scene.draw(-1); CHECK(scene.frame() == initial);
    // Headless callers settle animated effects, including loaded in-flight saves.
    dc4scene::SceneRunner headless(root.string()); headless.set_message_visible(false);
    CHECK(headless.load_state((root / "saves/animation.dc4sav").string()));
    CHECK(!headless.animating()); CHECK(headless.camera_scale() == 1.6f);
    headless.draw(-1); CHECK(headless.frame() == target);
    // Opting out settles immediately; a later advance cannot retain stale work.
    CHECK(scene.load_state((root / "saves/animation.dc4sav").string()));
    scene.set_animation_enabled(false); CHECK(!scene.animating()); CHECK(scene.camera_scale() == 1.6f);
}
void memory_snapshot_regressions(const fs::path& root) {
    Script script;
    script.slot(1, "BG\\test"); script.string(0x49, "BG\\test");
    script.camera(140); script.text("CAPTURED");
    script.camera(180); script.text("LIVE"); script.bytes.push_back(0);
    write_file(root / "MES/memory.mes", script.bytes);
    dc4scene::SceneRunner scene(root.string()); scene.set_message_visible(false);
    CHECK(scene.load_script((root / "MES/memory.mes").string())); CHECK(scene.advance_to_next_text());
    scene.draw(-1); const auto captured_frame = scene.frame();
    std::vector<uint8_t> snapshot; std::string error = "old error";
    CHECK(scene.capture_state(&snapshot, &error)); CHECK(error.empty()); CHECK(snapshot.size() > 24);
    CHECK(scene.advance_to_next_text()); scene.draw(-1); const auto live_frame = scene.frame();
    CHECK(live_frame != captured_frame); CHECK(scene.current_line() == "LIVE");
    std::vector<uint8_t> live; CHECK(scene.capture_state(&live, &error));
    const auto unchanged_after_failure = [&](const std::vector<uint8_t>& broken) {
        CHECK(!scene.restore_state(broken, &error)); CHECK(!error.empty());
        CHECK(scene.current_line() == "LIVE"); CHECK(scene.camera_scale() == 1.8f);
        scene.draw(-1); CHECK(scene.frame() == live_frame);
        std::vector<uint8_t> after; CHECK(scene.capture_state(&after)); CHECK(after == live);
    };
    unchanged_after_failure({});
    auto corrupt = snapshot; corrupt.back() ^= 1; unchanged_after_failure(corrupt);
    // Recompute the envelope so these corruptions exercise late parser checks,
    // after the candidate runner has already reloaded scripts and images.
    const auto reseal = [](std::vector<uint8_t>& bytes) {
        const uint32_t length = uint32_t(bytes.size() - 24);
        for (int i = 0; i < 4; ++i) bytes[12 + i] = uint8_t(length >> (i * 8));
        uint64_t hash = 14695981039346656037ull;
        for (size_t i = 24; i < bytes.size(); ++i) { hash ^= bytes[i]; hash *= 1099511628211ull; }
        for (int i = 0; i < 8; ++i) bytes[16 + i] = uint8_t(hash >> (i * 8));
    };
    auto invalid_flag = snapshot; invalid_flag[invalid_flag.size() - 5] = 2; reseal(invalid_flag);
    unchanged_after_failure(invalid_flag);
    auto oversized_chat = snapshot; oversized_chat[oversized_chat.size() - 4] = 201; reseal(oversized_chat);
    unchanged_after_failure(oversized_chat);
    auto trailing_data = snapshot; trailing_data.push_back(1); reseal(trailing_data);
    unchanged_after_failure(trailing_data);
    // A save from another revision of the same-named script must also fail
    // without affecting the currently running instance and its resources.
    auto revised = script.bytes; revised[revised.size() - 3] ^= 1;
    write_file(root / "MES/memory.mes", revised);
    unchanged_after_failure(snapshot);
    write_file(root / "MES/memory.mes", script.bytes);
    // v4 predates the sixteen expression strings added in v5. Its empty
    // COMU footer remains intact, and old saves must reproduce the same frame.
    auto legacy = snapshot;
    CHECK(legacy[8] == 5);
    legacy.erase(legacy.end() - 69, legacy.end() - 5);
    legacy[8] = 4; reseal(legacy);
    CHECK(scene.restore_state(legacy, &error)); CHECK(error.empty());
    CHECK(scene.current_line() == "CAPTURED");
    scene.draw(-1); CHECK(scene.frame() == captured_frame);
    CHECK(scene.restore_state(snapshot, &error)); CHECK(error.empty()); CHECK(scene.current_line() == "CAPTURED");
    CHECK(scene.camera_scale() == 1.4f); scene.draw(-1); CHECK(scene.frame() == captured_frame);
    CHECK(scene.advance_to_next_text()); CHECK(scene.current_line() == "LIVE");
    CHECK(!scene.capture_state(nullptr, &error)); CHECK(!error.empty()); CHECK(scene.current_line() == "LIVE");
    // An uninitialized runner cannot overwrite an existing caller snapshot.
    dc4scene::SceneRunner empty(root.string()); std::vector<uint8_t> sentinel{1,2,3};
    CHECK(!empty.capture_state(&sentinel, &error)); CHECK(sentinel == std::vector<uint8_t>({1,2,3}));
}
void font_regression() {
#ifdef _WIN32
    dc4scene::FontRenderer font;
    if (!font.load("C:/Windows/Fonts/msgothic.ttc", 0, 22)) { std::cout << "MS Gothic unavailable; GDI test skipped\n"; return; }
    std::vector<uint8_t> blank(256 * 64 * 4, 0);
    font.rasterize_line(u" \u3000 ", 2, 30, &blank, 256, 64, 22);
    CHECK(std::all_of(blank.begin(), blank.end(), [](uint8_t b) { return b == 0; }));
    font.rasterize_line(u"\u3000A\u732b", 2, 30, &blank, 256, 64, 22);
    size_t whites = 0;
    for (size_t i = 0; i < blank.size(); i += 4) if (blank[i] > 220 && blank[i + 3] > 200) ++whites;
    CHECK(whites > 20);
#endif
}
void dialogue_size_regressions(const fs::path& root) {
    const auto font_path = fs::current_path() / "android/app/src/main/assets/fonts/MSGOTHIC.TTC";
    if (!fs::exists(font_path)) return;
    dc4scene::FontRenderer font;
    CHECK(font.load(font_path.string(), 0, 22));
    for (const auto name : {"dc4", "dc4ph"}) {
        const auto data = root / name;
        Script script; script.bytes[11] = std::string(name) == "dc4ph" ? 0xAB : 0xAA;
        script.text("Dialogue size and wrapping sample. Dialogue size and wrapping sample.");
        script.bytes.push_back(0);
        write_file(data / "MES/main.mes", script.bytes);
        dc4scene::SceneRunner scene(data.string()); scene.set_font(&font);
        CHECK(scene.load_script((data / "MES/main.mes").string()));
        CHECK(scene.advance_to_next_text());
        for (bool latin : {false, true}) {
            scene.set_latin_mode(latin);
            scene.set_dialogue_font_size(22); scene.draw(-1); const auto large = scene.frame();
            auto previous = large;
            for (int size = 21; size >= 18; --size) {
                scene.set_dialogue_font_size(size); scene.draw(-1);
                CHECK(scene.frame() != previous); previous = scene.frame();
            }
            std::vector<uint8_t> saved;
            CHECK(scene.capture_state(&saved));
            scene.set_dialogue_font_size(1); scene.draw(-1); CHECK(scene.frame() == previous);
            scene.set_dialogue_font_size(90); scene.draw(-1); CHECK(scene.frame() == large);
            CHECK(scene.restore_state(saved)); scene.draw(-1); CHECK(scene.frame() == large);
            scene.set_dialogue_font_size(18); scene.draw(-1); CHECK(scene.frame() == previous);
            CHECK(scene.load_script((data / "MES/main.mes").string()));
            CHECK(scene.advance_to_next_text()); scene.draw(-1); CHECK(scene.frame() == previous);
        }
    }
}
void game_round_trip(const fs::path& data, const fs::path& root) {
    dc4scene::FontRenderer font;
    CHECK(font.load("C:/Windows/Fonts/msgothic.ttc", 0, 22));
    dc4scene::SceneRunner scene(data.string()); scene.set_font(&font);
    CHECK(scene.load_sheets((data / "MES/start.mes").string()));
    CHECK(scene.load_ui_script((data / "MES/win_size.mes").string()));
    CHECK(scene.load_script((data / "MES/dc4_kyo20190204a1.mes").string()));
    for (int i = 0; i < 24; ++i) CHECK(scene.advance_to_next_text());
    CHECK(scene.camera_scale() == 1.4f);
    CHECK(scene.advance_to_next_text()); CHECK(scene.camera_scale() == 1.6f);
    CHECK(scene.visible_text().find(u"\u732b") != std::u16string::npos);
    scene.set_wait_marker(true); scene.draw(-1); const auto frame = scene.frame();
    CHECK(scene.save_state((root / "reference.dc4sav").string()));
    CHECK(scene.advance_to_next_text()); CHECK(scene.camera_scale() == 1.0f);
    const auto next_line = scene.current_line();
    CHECK(scene.load_state((root / "reference.dc4sav").string())); scene.draw(-1);
    CHECK(scene.frame() == frame); CHECK(scene.camera_scale() == 1.6f);
    CHECK(scene.advance_to_next_text()); CHECK(scene.current_line() == next_line);
    int lines = 0;
    while (lines++ < 40 && scene.advance_to_next_text()) { if (scene.speaker().size() && lines > 20) break; }
    CHECK(scene.last_error().empty()); CHECK(!scene.scene_ended());

    dc4scene::SceneRunner choice(data.string()); choice.set_font(&font);
    CHECK(choice.load_sheets((data / "MES/start.mes").string()));
    CHECK(choice.load_script((data / "MES/dc4_dc99990101a.mes").string()));
    lines = 0; while (lines++ < 200 && choice.advance_to_next_text()) {}
    CHECK(choice.waiting_for_choice()); CHECK(choice.choices().size() == 2);
    choice.draw(-1); const auto choice_frame = choice.frame();
    CHECK(choice.save_state((root / "choice.dc4sav").string()));
    CHECK(choice.choose(0)); CHECK(choice.advance_to_next_text()); const auto branch1 = choice.current_line();
    CHECK(choice.load_state((root / "choice.dc4sav").string())); CHECK(choice.waiting_for_choice());
    choice.draw(-1); CHECK(choice.frame() == choice_frame);
    CHECK(choice.choose(1)); CHECK(choice.advance_to_next_text()); CHECK(choice.current_line() != branch1);
}
}
void chip_regressions(const fs::path& root) {
    // 0x3C slot 19 registers a chip sheet under the raw slot id (it must not
    // render as a full-screen layer); 0x7A + 0x77 blits one chip top-left
    // anchored at 1:1; a background commit clears the retained chips.
    write_image(root / "GRP/SYSTEM/chip.CRX", 40, 30);
    write_image(root / "GRP/BG/after.CRX", 64, 36);
    Script script;
    script.slot(19, "SYSTEM\\chip");
    script.text("NOSHEET");
    script.words(0x7A, 100, 400, 0, 0);
    script.words(0x77, 20, 10, 19, 1);
    script.text("CHIP");
    script.slot(1, "BG\\after"); script.string(0x49, "BG\\after");
    script.text("CLEARED");
    script.bytes.push_back(0);
    write_file(root / "MES/chips.mes", script.bytes);
    dc4scene::SceneRunner scene(root.string()); scene.set_message_visible(false);
    CHECK(scene.load_script((root / "MES/chips.mes").string()));
    CHECK(scene.advance_to_next_text()); CHECK(scene.current_line() == "NOSHEET");
    scene.draw(-1);
    const uint8_t* bare = &scene.frame()[(size_t(600) * 1280 + 1100) * 4];
    CHECK(bare[0] == 0 && bare[1] == 0 && bare[2] == 0);
    CHECK(scene.advance_to_next_text()); CHECK(scene.current_line() == "CHIP");
    scene.draw(-1);
    // The 24bpp fixture decodes BGR, so the sheet's top-left quadrant reads
    // (240,50,40) here.
    const uint8_t* chip = &scene.frame()[(size_t(405) * 1280 + 105) * 4];
    CHECK(chip[0] == 240 && chip[1] == 50 && chip[2] == 40);
    CHECK(scene.advance_to_next_text()); CHECK(scene.current_line() == "CLEARED");
    scene.draw(-1);
    const uint8_t* cleared = &scene.frame()[(size_t(405) * 1280 + 105) * 4];
    CHECK(cleared[0] == 240 && cleared[1] == 50 && cleared[2] == 240);
}
void dc2pc_dialogue_regressions(const fs::path& root) {
    struct ClassicScript {
        std::vector<uint8_t> bytes{1,0,0,0,2,0,0,0,0x69,0x99};
        void text(uint8_t op, const std::string& value) {
            bytes.push_back(op);
            for (unsigned char ch : value) bytes.push_back(uint8_t(ch - 0x20));
            bytes.push_back(0);
        }
        void string(uint8_t op, const std::string& value) {
            bytes.push_back(op);
            if (op == 0x2E) bytes.push_back(0);
            bytes.insert(bytes.end(), value.begin(), value.end()); bytes.push_back(0);
        }
        void sprite(uint8_t op, uint8_t index, const std::string& value) {
            bytes.push_back(op); bytes.push_back(index);
            bytes.insert(bytes.end(), value.begin(), value.end()); bytes.push_back(0);
        }
        void words(uint8_t op, uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
            bytes.push_back(op);
            for (uint16_t value : {a,b,c,d}) {
                bytes.push_back(uint8_t(value)); bytes.push_back(uint8_t(value >> 8));
            }
        }
    };
    const fs::path data = root / "dc2pc";
    ClassicScript ui;
    write_file(data / "MES/start.mes", ui.bytes);
    ClassicScript first;
    first.text(0x4B, "Otome"); first.text(0x4C, "FIRST");
    first.text(0x4C, "NARRATION");
    first.text(0x4B, "Yume"); first.text(0x4C, "THIRD");
    first.string(0x2E, "second");
    write_file(data / "MES/first.mes", first.bytes);
    ClassicScript second;
    second.text(0x4B, "OtomeYume"); second.text(0x4C, "FOURTH");
    write_file(data / "MES/second.mes", second.bytes);
    dc4scene::SceneRunner scene(data.string());
    CHECK(scene.load_ui_script((data / "MES/start.mes").string()));
    CHECK(scene.load_script((data / "MES/first.mes").string()));
    CHECK(scene.advance_to_next_text());
    CHECK(scene.speaker() == "Otome" && scene.current_line() == "FIRST");
    CHECK(!scene.browse_dialogue(-1)); CHECK(!scene.browse_dialogue(1));
    CHECK(scene.advance_to_next_text());
    CHECK(scene.speaker().empty() && scene.current_line() == "NARRATION");
    CHECK(scene.advance_to_next_text());
    CHECK(scene.speaker() == "Yume" && scene.current_line() == "THIRD");
    std::vector<uint8_t> live;
    CHECK(scene.capture_state(&live));
    auto snapshot = [&]() { std::vector<uint8_t> bytes; CHECK(scene.capture_state(&bytes)); return bytes; };
    CHECK(scene.browse_dialogue(-1));
    CHECK(scene.displayed_line() == "NARRATION" && scene.displayed_speaker().empty());
    CHECK(scene.browse_dialogue(-1));
    CHECK(scene.displayed_line() == "FIRST" && scene.displayed_speaker() == "Otome");
    CHECK(!scene.browse_dialogue(-1)); CHECK(!scene.advance_to_next_text());
    scene.update(1000);
    CHECK(snapshot() == live);
    CHECK(scene.current_line() == "THIRD" && scene.speaker() == "Yume");
    CHECK(scene.browse_dialogue(1)); CHECK(scene.browse_dialogue(1));
    CHECK(!scene.reviewing_dialogue()); CHECK(!scene.browse_dialogue(1));
    CHECK(scene.browse_dialogue(-1)); scene.return_to_dialogue();
    CHECK(!scene.reviewing_dialogue()); CHECK(snapshot() == live);
    CHECK(scene.advance_to_next_text());
    CHECK(scene.script_name() == "second.mes" && scene.current_line() == "FOURTH");
    CHECK(scene.browse_dialogue(-1)); CHECK(scene.displayed_line() == "THIRD");
    std::string error;
    CHECK(!scene.restore_state({1,2,3}, &error));
    CHECK(scene.reviewing_dialogue() && scene.displayed_line() == "THIRD");
    CHECK(scene.restore_state(live, &error));
    CHECK(scene.current_line() == "THIRD" && scene.speaker() == "Yume");
    CHECK(!scene.reviewing_dialogue()); CHECK(!scene.browse_dialogue(-1));
    CHECK(scene.advance_to_next_text()); CHECK(scene.browse_dialogue(-1));
    CHECK(scene.displayed_line() == "THIRD");
    CHECK(scene.load_script((data / "MES/first.mes").string()));
    CHECK(!scene.reviewing_dialogue()); CHECK(!scene.browse_dialogue(-1));
    CHECK(scene.advance_to_next_text()); CHECK(!scene.browse_dialogue(-1));
    ClassicScript choice;
    choice.text(0x4C, "QUESTION");
    choice.bytes.insert(choice.bytes.end(), {0x08,1,0});
    for (const auto& entry : {std::pair<const char*,const char*>{"01","FIRSTOPTION"}, {"02","SECONDOPTION"}, {"03","THIRDOPTION"}}) {
        choice.string(0x44, entry.first); choice.text(0x4D, entry.second);
    }
    choice.bytes.insert(choice.bytes.end(), {0x09,0,0});
    for (const auto& entry : {std::pair<const char*,const char*>{"01","FIRSTBRANCH"}, {"02","SECONDBRANCH"}, {"03","THIRDBRANCH"}}) {
        choice.string(0x35, entry.first); choice.text(0x4C, entry.second);
    }
    write_file(data / "MES/choice.mes", choice.bytes);
    CHECK(scene.load_script((data / "MES/choice.mes").string()));
    CHECK(scene.advance_to_next_text()); CHECK(scene.current_line() == "QUESTION");
    CHECK(!scene.advance_to_next_text()); CHECK(scene.waiting_for_choice());
    CHECK(scene.current_line() == "QUESTION" && scene.choices().size() == 3);
    CHECK(scene.choices()[2].text == "THIRDOPTION");
    CHECK(scene.choice_at(450,260) == 0); CHECK(scene.choice_at(450,325) == 1);
    CHECK(scene.choice_at(450,390) == 2); CHECK(scene.choice_at(450,550) == -1);
    CHECK(scene.choice_at(450,295) == -1); CHECK(!scene.choose(3));
    const auto pending = snapshot();
    CHECK(scene.choose(2)); CHECK(scene.advance_to_next_text());
    CHECK(scene.current_line() == "THIRDBRANCH");
    CHECK(scene.restore_state(pending, &error)); CHECK(scene.waiting_for_choice());
    CHECK(scene.current_line() == "QUESTION" && scene.choices()[0].text == "FIRSTOPTION");
    CHECK(scene.choose(0)); CHECK(scene.advance_to_next_text()); CHECK(scene.current_line() == "FIRSTBRANCH");

    write_image(data / "GRP/BST/body.CRX", 32, 24);
    write_image(data / "GRP/BST/face.CRX", 8, 8);
    // A partial face is registered at (4,4), inside the full body canvas.
    std::ifstream face_file(data / "GRP/BST/face.CRX", std::ios::binary);
    std::vector<uint8_t> face_bytes((std::istreambuf_iterator<char>(face_file)), {});
    face_file.close(); face_bytes[4] = face_bytes[6] = 4;
    write_file(data / "GRP/BST/face.CRX", face_bytes);
    write_image(data / "GRP/BG/room.CRX", 64, 36);
    ClassicScript sprites;
    sprites.string(0x30, "bg\\room");
    for (uint8_t i = 0; i < 3; ++i) {
        sprites.words(0x65, 150, 10, 20, 16+i);
        sprites.words(0x64, uint16_t(-40+i*80), 12, i, 0);
        sprites.sprite(0x2B, i, i == 2 ? "bst\\body@" : "bst\\body");
    }
    sprites.text(0x4C, "BODIES");
    sprites.sprite(0x2C, 0, "bst\\face"); sprites.text(0x4C, "FACE");
    sprites.words(0x65, uint16_t(-125), 27, 0, 1);
    sprites.words(0x65, 175, 10, 20, 17);
    sprites.sprite(0x2C, 0, "-"); sprites.text(0x4C, "RESTOREDBODY");
    sprites.sprite(0x2B, 0, "-"); sprites.text(0x4C, "HIDEBODY");
    sprites.words(0x64, 0, 0, 255, 0); sprites.text(0x4C, "SWAPPED");
    sprites.sprite(0x2B, 4, "bst\\body");
    sprites.string(0x30, "bg\\room"); sprites.text(0x4C, "NEWROOM");
    write_file(data / "MES/sprites.mes", sprites.bytes);
    CHECK(scene.load_script((data / "MES/sprites.mes").string()));
    CHECK(scene.advance_to_next_text());
    for (size_t i=0;i<3;++i) {
        CHECK(scene.sprite(i).visible && scene.sprite(i).img.valid());
        CHECK(scene.sprite(i).scale == 1); // raw64 resets a previous zoom
        CHECK(scene.sprite(i).cur_x == -40+int(i)*80);
    }
    const auto clean = scene.sprite(0).img.rgba;
    CHECK(scene.sprite(2).img.rgba[0] == clean[0]*3/4);
    CHECK(scene.sprite(2).img.rgba[2] == clean[2]*3/4+16);
    CHECK(scene.advance_to_next_text());
    CHECK(scene.sprite(0).image == "bst\\body");
    CHECK(scene.sprite(0).expression == "bst\\face");
    CHECK(scene.sprite(0).img.w == 32 && scene.sprite(0).img.h == 24);
    const auto composited = scene.sprite(0).img.rgba;
    CHECK(composited[0] == clean[0]);
    const size_t inside = (10*32+10)*4;
    CHECK(composited[inside] != clean[inside]);
    const auto expression_save = snapshot();
    CHECK(scene.advance_to_next_text());
    CHECK(scene.sprite(0).visible && scene.sprite(0).expression.empty());
    CHECK(scene.sprite(0).img.rgba == clean);
    CHECK(scene.sprite(1).cur_x == -125 && scene.sprite(1).cur_y == 27);
    CHECK(scene.sprite(1).scale == 1.75f);
    CHECK(scene.restore_state(expression_save, &error));
    CHECK(scene.sprite(0).img.rgba == composited);
    CHECK(scene.sprite(0).image == "bst\\body" && scene.sprite(0).expression == "bst\\face");
    CHECK(scene.advance_to_next_text()); CHECK(scene.sprite(0).img.rgba == clean);
    CHECK(scene.advance_to_next_text()); CHECK(!scene.sprite(0).visible);
    CHECK(scene.sprite(1).visible && scene.sprite(2).visible);
    CHECK(scene.advance_to_next_text());
    CHECK(scene.sprite(0).visible && !scene.sprite(1).visible);
    CHECK(scene.sprite(0).cur_x == -40 && scene.sprite(0).scale == 1);
    CHECK(scene.sprite(1).cur_x == -125 && scene.sprite(1).scale == 1.75f);
    CHECK(scene.advance_to_next_text());
    for (size_t i=0;i<4;++i) CHECK(!scene.sprite(i).visible);
    CHECK(scene.sprite(4).visible); // raw30 only dismisses native layers 1..4

    ClassicScript hidden;
    hidden.words(0x64,uint16_t(-240),0,0,0);hidden.words(0x64,160,0,1,0);
    hidden.words(0x5C,9,40,99,0);hidden.text(0x4C,"HIDDEN");
    for(uint8_t i=0;i<2;++i) {
        hidden.words(0x65,100,441,277,16+i);hidden.sprite(0x2B,i,"bst\\body");
    }
    hidden.text(0x4C,"VISIBLE");
    write_file(data / "MES/hidden.mes",hidden.bytes);
    CHECK(scene.load_script((data / "MES/hidden.mes").string()));CHECK(scene.advance_to_next_text());
    auto old_hidden=snapshot();
    // Build an early v4 fixture whose absent body layers have default coords.
    size_t cursor=24;
    auto read32=[&]() { uint32_t n=0;for(int i=0;i<4;++i)n|=uint32_t(old_hidden[cursor++])<<(8*i);return n; };
    auto skip_string=[&]() {const auto length=read32();cursor+=length;};
    skip_string();cursor+=16;const auto variables=read32();cursor+=variables*12;skip_string();
    const auto choices=read32();for(uint32_t i=0;i<choices;++i){skip_string();skip_string();}
    cursor+=3;skip_string();skip_string();cursor+=31;
    const auto layers=read32();for(uint32_t i=0;i<layers;++i){cursor+=4;skip_string();}
    CHECK(read32()==16);
    for(int i=0;i<16;++i) {
        skip_string();skip_string();cursor+=25;
        std::fill(old_hidden.begin()+cursor,old_hidden.begin()+cursor+8,0);cursor+=8;
    }
    old_hidden.erase(old_hidden.end()-69,old_hidden.end()-5);old_hidden[8]=4;
    const auto size=uint32_t(old_hidden.size()-24);
    for(int i=0;i<4;++i)old_hidden[12+i]=uint8_t(size>>(8*i));
    uint64_t hash=14695981039346656037ull;
    for(size_t i=24;i<old_hidden.size();++i){hash^=old_hidden[i];hash*=1099511628211ull;}
    for(int i=0;i<8;++i)old_hidden[16+i]=uint8_t(hash>>(8*i));
    CHECK(scene.restore_state(old_hidden,&error));
    CHECK(scene.current_line()=="HIDDEN" && !scene.sprite(0).visible && !scene.sprite(1).visible);
    CHECK(scene.sprite(0).cur_x==-240 && scene.sprite(1).cur_x==160);
    scene.set_variable(9,40,12);
    auto repaired=snapshot();CHECK(scene.restore_state(repaired,&error));
    CHECK(scene.variable(9,40)==12); // recovery must not execute earlier assignments
    CHECK(scene.advance_to_next_text());
    CHECK(scene.sprite(0).visible && scene.sprite(1).visible);
    CHECK(scene.sprite(0).cur_x==-240 && scene.sprite(1).cur_x==160);

    // Character names below are fixture dialogue, not game identifiers or
    // a whitelist: the importer reads arbitrary speakers from the scenario.
    ClassicScript pc;
    pc.string(0x30, "bg\\room");
    pc.words(0x64, uint16_t(-77), 15, 0, 0);
    pc.words(0x5C, 9, 40, 99, 0); // must not run when importing a save
    pc.sprite(0x2B, 0, "bst\\body");
    pc.bytes.insert(pc.bytes.end(), {3,77,0});
    pc.text(0x4B, "Maya"); pc.text(0x4C, "SAVEDINANOTHERTRANSLATION");
    pc.bytes.insert(pc.bytes.end(), {7,0,0,3,78,0}); pc.text(0x4B,"Sakura"); pc.text(0x4C, "NEXT");
    pc.bytes.insert(pc.bytes.end(), {7,0,0,3,79,0}); pc.text(0x4B,"Yume"); pc.text(0x4C, "QUESTION");
    pc.bytes.insert(pc.bytes.end(), {7,0,0,4,80,0,8,1,0});
    pc.string(0x44, "01"); pc.text(0x4D, "FIRSTOPTION");
    pc.string(0x44, "02"); pc.text(0x4D, "SECONDOPTION");
    pc.bytes.insert(pc.bytes.end(), {9,0,0});
    pc.string(0x35, "01"); pc.text(0x4C, "FIRSTBRANCH");
    pc.string(0x35, "02"); pc.text(0x4C, "SECONDBRANCH");
    pc.bytes.push_back(0);
    write_file(data / "MES/pc.mes", pc.bytes);
    std::vector<uint8_t> native(0x2B20, 0);
    const auto u32 = [&](size_t at, uint32_t value) {
        for (int i = 0; i < 4; ++i) native[at+i] = uint8_t(value>>(8*i));
    };
    const auto str = [&](size_t at, const std::string& value) {
        std::copy(value.begin(), value.end(), native.begin()+at);
    };
    u32(0,1); str(4,"pc"); str(0x84,"bg\\room"); str(0x104,"bst\\body");
    str(0x184,"bst\\body"); native[0x184]=0; // hidden stale record
    u32(0x804,uint32_t(-1)); u32(0x810,0xFFFFFF); // other language's PC cannot be used
    u32(0x814,77); u32(0x818,78); u32(0x81C,7);
    native[0xFF0+9*100+40]=12;
    scene.set_variable(100,7,1);
    CHECK(scene.restore_pc_save(native,&error)); CHECK(error.empty());
    CHECK(scene.current_line()=="SAVEDINANOTHERTRANSLATION" && scene.speaker()=="Maya");
    CHECK(scene.variable(9,40)==12 && scene.variable(100,7)==1);
    CHECK(scene.sprite(0).visible && scene.sprite(0).cur_x==-77 && scene.sprite(0).cur_y==15);
    CHECK(!scene.sprite(1).visible); CHECK(scene.advance_to_next_text()); CHECK(scene.current_line()=="NEXT" && scene.speaker()=="Sakura");
    const auto before_import_error=snapshot();
    auto damaged_pc=native; damaged_pc[0]=2;
    CHECK(!scene.restore_pc_save(damaged_pc,&error)); CHECK(snapshot()==before_import_error);
    damaged_pc=native; damaged_pc.resize(0x2B1F);
    CHECK(!scene.restore_pc_save(damaged_pc,&error)); CHECK(snapshot()==before_import_error);
    damaged_pc=native; damaged_pc[0x814]=200;
    CHECK(!scene.restore_pc_save(damaged_pc,&error)); CHECK(snapshot()==before_import_error);
    damaged_pc=native; std::fill(damaged_pc.begin()+4,damaged_pc.begin()+132,'a');
    CHECK(!scene.restore_pc_save(damaged_pc,&error)); CHECK(snapshot()==before_import_error);
    // Native transforms and facial expressions override script placement.
    str(0x384,"bst\\face");
    u32(0x14D0,uint32_t(-91));u32(0x14D4,28);u32(0x14D8,175);
    u32(0x14E8,10);u32(0x14EC,20);
    CHECK(scene.restore_pc_save(native,&error));
    CHECK(scene.sprite(0).expression=="bst\\face");
    CHECK(scene.sprite(0).cur_x==-91 && scene.sprite(0).cur_y==28);
    CHECK(scene.sprite(0).scale==1.75f && scene.sprite(0).x==10 && scene.sprite(0).y==20);
    u32(0x814,80); u32(0x818,80); u32(0x81C,9);
    CHECK(scene.restore_pc_save(native,&error)); CHECK(error.empty());
    CHECK(scene.waiting_for_choice() && scene.choices().size()==2);
    CHECK(scene.current_line()=="QUESTION" && scene.choices()[1].text=="SECONDOPTION");
    const auto exported=data/"saves/DC2PC01.dat";
    CHECK(scene.save_state(exported.string()));
    std::ifstream exported_file(exported,std::ios::binary);
    std::vector<uint8_t> exported_bytes((std::istreambuf_iterator<char>(exported_file)),{});exported_file.close();
    CHECK(exported_bytes.size()==0x2B20 && exported_bytes[0]==1);
    CHECK(exported_bytes[0x818]>0 && exported_bytes[0x81C]==9);
    CHECK(exported_bytes[0x1389]>=1 && exported_bytes[0x1389]<=12);
    CHECK(exported_bytes[0x138A]>=1 && exported_bytes[0x138A]<=31);
    CHECK(scene.restore_pc_save(exported_bytes,&error));CHECK(scene.waiting_for_choice());
    u32(0x814,79); u32(0x818,80); u32(0x81C,7);
    CHECK(scene.restore_pc_save(native,&error)); CHECK(scene.current_line()=="QUESTION" && scene.speaker()=="Yume");
    u32(0x814,80); u32(0x81C,9); CHECK(scene.restore_pc_save(native,&error));
    CHECK(scene.choose(1)); CHECK(scene.advance_to_next_text()); CHECK(scene.current_line()=="SECONDBRANCH");
}
void native_sprite_placement_regressions(const fs::path& root) {
    const auto data=root/"native-sprites";
    const auto image=data/"GRP/BST/cropped.CRX";
    write_image(image,64,36);
    std::ifstream input(image,std::ios::binary);
    std::vector<uint8_t> cropped((std::istreambuf_iterator<char>(input)),{});input.close();
    cropped[4]=44;cropped[5]=1; // CRX crop starts at (300,114) in the canvas.
    cropped[6]=114;write_file(image,cropped);
    for(const auto version : {uint16_t(0xAAB6),uint16_t(0xABB6)}) {
        for(const float zoom : {1.f,1.4f}) for(const int percent : {65,78}) {
            Script script;
            script.bytes={2,0,0,0,3,0,0,0,3,0,0,0,99,0,100,0,0xB6,uint8_t(version>>8),0};
            script.slot(4,"BST\\cropped");
            script.bytes.insert(script.bytes.end(),{3,0,0});script.text("SAVED");
            script.words(0x8B,248,uint16_t(-40),0,0);
            script.bytes.insert(script.bytes.end(),{3,1,0});script.text("MOVED");script.bytes.push_back(0);
            write_file(data/"MES/sprite.mes",script.bytes);
            dc4scene::SceneRunner scene(data.string());std::string error;
            CHECK(scene.load_script((data/"MES/sprite.mes").string()));CHECK(scene.advance_to_next_text());
            std::vector<uint8_t> native;CHECK(scene.capture_pc_save(&native,&error));
            auto number=[&](size_t at,float value){std::memcpy(native.data()+at,&value,4);};
            auto integer=[&](size_t at,uint32_t value){for(int i=0;i<4;++i)native[at+i]=uint8_t(value>>(i*8));};
            const size_t record=0x18E20+10*960;
            // Independent native PC fields, matching the reported sprite scales
            // with a nonzero origin and pivot; no emulator companion is used.
            number(record,228);number(record+4,-40);
            number(record+120,float(percent));number(record+124,float(percent));
            number(record+144,512);number(record+148,318);
            for(const size_t at : {size_t(0x18E20),record}) {
                number(at+28,zoom);number(at+32,zoom);integer(at+44,1);
            }
            number(0x18E20+36,412);number(0x18E20+40,588);
            number(record+36,432);number(record+40,528);
            CHECK(scene.restore_pc_save(native,&error));
            const float expected_scale=percent/100.f;
            CHECK(std::abs(scene.sprite(0).scale-expected_scale)<0.000001f);
            CHECK(scene.sprite(0).cur_x==228 && scene.sprite(0).cur_y==-40);
            CHECK(scene.sprite(0).x==512 && scene.sprite(0).y==318);
            scene.draw(-1);const auto frame=scene.frame();
            // Check a pixel near the expected top-left of the cropped sprite.
            const int crop_left=percent==65?603:576;
            const int crop_top=percent==65?146:120;
            const int x=int(std::lround(432+(crop_left-432)*zoom));
            const int y=int(std::lround(528+(crop_top-528)*zoom));
            if(x>=0 && x<dc4scene::kScreenW && y>=0 && y<dc4scene::kScreenH)
                CHECK(frame[(size_t(y)*dc4scene::kScreenW+x)*4]==240);
            CHECK(scene.capture_pc_save(&native,&error));
            CHECK(scene.restore_pc_save(native,&error));scene.draw(-1);CHECK(scene.frame()==frame);
            CHECK(scene.camera_scale()==zoom);
            CHECK(scene.advance_to_next_text());CHECK(scene.current_line()=="MOVED");
            CHECK(scene.sprite(0).cur_x==248 && scene.sprite(0).cur_y==-40);
            // Camera multiplication and native export can round the float scale.
            CHECK(std::abs(scene.sprite(0).scale-expected_scale)<0.000001f && scene.sprite(0).x==512);
        }
    }
}
void native_save_regressions(const fs::path& root) {
    const auto data=root/"native";
    Script script;
    script.bytes={2,0,0,0,3,0,0,0,3,0,0,0,99,0,100,0,0xB6,0xAA,0};
    write_image(data/"GRP/BG/native.CRX",160,100);
    script.slot(1,"BG\\native");
    script.bytes.insert(script.bytes.end(),{3,0,0});script.text("FIRST");
    script.bytes.insert(script.bytes.end(),{3,1,0});script.text("SECOND");script.bytes.push_back(0);
    write_file(data/"MES/native.mes",script.bytes);
    dc4scene::SceneRunner scene(data.string());std::string error;
    CHECK(scene.load_script((data/"MES/native.mes").string()));CHECK(scene.advance_to_next_text());
    scene.set_variable(9,40,12);scene.set_variable(100,101,0);
    const auto save=data/"saves/DC403.dat";
    CHECK(scene.save_state(save.string()));
    std::ifstream file(save,std::ios::binary);std::vector<uint8_t> native((std::istreambuf_iterator<char>(file)),{});file.close();
    CHECK(native.size()==0x56FE4);CHECK(native[8]=='n');CHECK(native[0x184EC+940]==12);
    auto scalar=[&](size_t offset){float value;std::memcpy(&value,native.data()+offset,4);return value;};
    const size_t renderer=0x18E20;
    const float scale=scalar(renderer+120)/100.f;
    // Apply the PC renderer's pivot formula: the image covers the screen
    // with its excess height centred, rather than shifting to the right.
    CHECK(std::abs(scale-8.f)<0.001f);
    CHECK(std::abs(scalar(renderer)+scalar(renderer+144)*(1-scale))<0.01f);
    CHECK(std::abs(scalar(renderer+4)+scalar(renderer+148)*(1-scale)+40)<0.01f);
    CHECK(native[0x1873A]>=1 && native[0x1873A]<=12);
    CHECK(native[0x1873B]>=1 && native[0x1873B]<=31);
    CHECK(fs::is_regular_file(save.string()+".arisa"));
    CHECK(scene.advance_to_next_text());CHECK(scene.current_line()=="SECOND");
    CHECK(scene.load_state(save.string()));CHECK(scene.current_line()=="FIRST");
    // The native record loads independently, with no emulator companion.
    CHECK(scene.advance_to_next_text());CHECK(scene.restore_pc_save(native,&error));CHECK(scene.current_line()=="FIRST");
    CHECK(scene.variable(100,101)==0);CHECK(scene.variable(9,40)==12);
    CHECK(scene.advance_to_next_text());CHECK(scene.current_line()=="SECOND");
    auto snapshot=[&](){std::vector<uint8_t> bytes;CHECK(scene.capture_state(&bytes,&error));return bytes;};
    const auto before=snapshot();auto invalid=native;invalid.resize(0x1000);
    CHECK(!scene.restore_pc_save(invalid,&error));CHECK(snapshot()==before);
    invalid=native;invalid[0xAFC]=77;CHECK(!scene.restore_pc_save(invalid,&error));CHECK(snapshot()==before);
    invalid=native;
    const std::string missing="SE\\missing";
    std::copy(missing.begin(),missing.end(),invalid.begin()+0xA60);invalid[0xAE0]=1;
    CHECK(!scene.restore_pc_save(invalid,&error));CHECK(snapshot()==before);
    // Simulate a PC overwrite. A stale companion must never win over native.
    native[0xAFC]=1;write_file(save,native);CHECK(scene.load_state(save.string()));CHECK(scene.current_line()=="SECOND");
    // Translation byte lengths differ, but the native checkpoint stays stable.
    script.bytes={2,0,0,0,3,0,0,0,3,0,0,0,99,0,100,0,0xB6,0xAA,0};
    script.bytes.insert(script.bytes.end(),{3,0,0});script.text("MUCHLONGERTRANSLATEDFIRSTLINE");
    script.bytes.insert(script.bytes.end(),{3,1,0});script.text("TRANSLATEDSECOND");script.bytes.push_back(0);
    write_file(data/"MES/native.mes",script.bytes);
    CHECK(scene.restore_pc_save(native,&error));CHECK(scene.current_line()=="TRANSLATEDSECOND");
    Script choice;
    choice.bytes={2,0,0,0,3,0,0,0,3,0,0,0,99,0,100,0,0xB6,0xAA,0};
    choice.bytes.insert(choice.bytes.end(),{3,0,0});choice.text("QUESTION");
    choice.bytes.insert(choice.bytes.end(),{4,1,0,9,1,0});
    choice.string(0x5C,"left");choice.string(0x6C,"LEFT");
    choice.string(0x5C,"right");choice.string(0x6C,"RIGHT");
    choice.bytes.insert(choice.bytes.end(),{10,0,0});
    choice.string(0x4B,"left");choice.text("LEFTBRANCH");choice.bytes.push_back(0);
    choice.string(0x4B,"right");choice.text("RIGHTBRANCH");choice.bytes.push_back(0);
    write_file(data/"MES/choice.mes",choice.bytes);
    CHECK(scene.load_script((data/"MES/choice.mes").string()));CHECK(scene.advance_to_next_text());
    CHECK(!scene.advance_to_next_text());CHECK(scene.waiting_for_choice());
    CHECK(scene.capture_pc_save(&native,&error));
    CHECK(scene.choose(0));CHECK(scene.advance_to_next_text());CHECK(scene.current_line()=="LEFTBRANCH");
    CHECK(scene.restore_pc_save(native,&error));
    CHECK(scene.waiting_for_choice() && scene.choices().size()==2 && scene.current_line()=="QUESTION");
    CHECK(scene.choose(1));CHECK(scene.advance_to_next_text());CHECK(scene.current_line()=="RIGHTBRANCH");
    // PC changes its scale divisor at raw 1000. Large zooms must use
    // ten-thousandths so 10x does not silently reload as 0.1x.
    for(const auto dimensions : {std::pair<int,int>{128,72},{64,36}}) {
        write_image(data/"GRP/BG/native.CRX",dimensions.first,dimensions.second);
        Script zoom;zoom.bytes={2,0,0,0,3,0,0,0,3,0,0,0,99,0,100,0,0xB6,0xAA,0};
        zoom.slot(1,"BG\\native");zoom.bytes.insert(zoom.bytes.end(),{3,0,0});zoom.text("ZOOM");zoom.bytes.push_back(0);
        write_file(data/"MES/zoom.mes",zoom.bytes);
        CHECK(scene.load_script((data/"MES/zoom.mes").string()));CHECK(scene.advance_to_next_text());
        CHECK(scene.capture_pc_save(&native,&error));
        const float raw=scalar(renderer+120), decoded=raw/(raw>=1000?10000.f:100.f);
        CHECK(std::abs(decoded-1280.f/dimensions.first)<0.001f);
        CHECK(std::abs(scalar(renderer)+scalar(renderer+144)*(1-decoded))<0.01f);
        CHECK(std::abs(scalar(renderer+4)+scalar(renderer+148)*(1-decoded))<0.01f);
    }
}
void dc2dm_scene_regressions(const fs::path& root) {
    struct DmScript : Script {
        DmScript() { bytes[10] = 0x72; bytes[11] = 0x9D; }
        void labels() {
            dc4::MesFile mes; CHECK(mes.load(bytes.data(),bytes.size()));
            dc4::MesStream stream(mes); dc4::MesCommand c;
            std::vector<uint32_t> offsets(1,3);
            while(stream.next(&c)) if(c.op==3 || c.op==4) {
                const size_t id=c.p1 | size_t(c.p2)<<8;
                if(id>=offsets.size()) offsets.resize(id+1,3);
                offsets[id]=c.pos;
            }
            std::vector<uint8_t> indexed(4+offsets.size()*6,0);
            indexed[0]=uint8_t(offsets.size());indexed[1]=uint8_t(offsets.size()>>8);
            for(size_t i=0;i<offsets.size();++i) {
                for(int n=0;n<4;++n)indexed[4+i*4+n]=uint8_t(offsets[i]>>(8*n));
                indexed[4+offsets.size()*4+i*2]=uint8_t(i);
                indexed[5+offsets.size()*4+i*2]=uint8_t(i>>8);
            }
            indexed.insert(indexed.end(),bytes.begin()+10,bytes.end());bytes.swap(indexed);
        }
        void param(uint8_t op, uint8_t value, const std::string& name) {
            bytes.push_back(op); bytes.push_back(value);
            bytes.insert(bytes.end(), name.begin(), name.end()); bytes.push_back(0);
        }
        void text(uint8_t op, const std::string& value) {
            bytes.push_back(op);
            for (unsigned char ch : value) bytes.push_back(uint8_t(ch - 0x20));
            bytes.push_back(0);
        }
        void end_group() { bytes.insert(bytes.end(), {0x17, 1, 0}); }
        void button(int x, int y, int w, int h, const char* action, int sheet = 21) {
            words(0x54, x, y, x + w, y + h); string(0x3D, action);
            words(0x55, x, y, 0, 0); words(0x57, w, h, sheet, 99);
            words(0x55, x, y, 32, 0); words(0x57, w, h, sheet, 1);
            if (std::strcmp(action, ":qs") == 0 || std::strcmp(action, ":ql") == 0 ||
                std::strcmp(action, ":torisetu") == 0) {
                words(0x55, x, y, 0, 32); words(0x57, w, h, sheet, 2);
                words(0x55, 616, 348, 32, 32); words(0x57, 28, 28, sheet, 2);
            }
        }
    };
    const auto data = root / "dc2dm";
    write_image(data / "GRP/SYSTEM/MESWIN.CRX", 64, 64);
    write_image(data / "GRP/SYSTEM/ADVMENU_CHIP.CRX", 64, 64);
    write_image(data / "GRP/BST/sprite.CRX", 64, 36);
    DmScript ui;
    ui.param(0x2C, 0, "SYSTEM\\MESWIN");
    ui.button(907, 416, 38, 37, ":n"); ui.end_group();
    ui.button(640, 420, 28, 28, ":torisetu");
    ui.button(676, 420, 28, 28, ":auto");
    ui.button(856, 420, 28, 28, ":option");
    const std::pair<int, const char*> persistence_controls[] = {
        {300, ":ds"}, {340, ":dl"}, {380, ":qsave"}, {420, ":qload"}, {748, ":qs"}, {784, ":ql"}};
    for (const auto& control : persistence_controls) ui.button(control.first, 420, 28, 28, control.second);
    ui.button(620, 424, 16, 22, ":n"); ui.end_group();
    ui.button(100, 100, 28, 28, "confirmation"); ui.end_group();
    for (int i = 0; i < 5; ++i) {
        const std::string tag = ":" + std::to_string(i + 1);
        ui.button(300, 64 + i * 60, 28, 28, tag.c_str());
        ui.words(0x55, 300, 64 + i * 60, 0, 32); ui.words(0x57, 28, 28, 21, 2);
        ui.words(0x56, 312, 80 + i * 60, 135, 99); ui.string(0x44, "");
    }
    ui.end_group();
    ui.bytes.push_back(0);
    write_file(data / "MES/start.mes", ui.bytes);
    DmScript sidebar;
    sidebar.words(0x5A, 860, 16, 0, 0); sidebar.words(0x57, 32, 32, 22, 1);
    sidebar.button(872, 72, 28, 28, "save", 22);
    sidebar.button(872, 144, 28, 28, "config", 22);
    sidebar.button(872, 252, 28, 28, "return", 22);
    sidebar.bytes.insert(sidebar.bytes.end(), {0x09, 0, 0});
    sidebar.button(10, 10, 28, 28, "title", 22);
    sidebar.bytes.push_back(0);
    write_file(data / "MES/advmenu.mes", sidebar.bytes);
    DmScript story;
    story.string(0x34, "autoload");
    story.bytes.insert(story.bytes.end(), {4, 1, 0});
    story.bytes.insert(story.bytes.end(), {0x19, 0, 0});
    story.string(0x3D, "autoload");
    story.words(0x6B, 20, 40, 0, 0);
    story.words(0x6B, 100, 512, 322, 16);
    story.param(0x2B, 2, "BST\\sprite");
    story.param(0x2E, 0, ".");
    story.bytes.insert(story.bytes.end(), {3, 7, 0});
    story.text(0x4D, "CAPTION"); story.text(0x4E, "Sakura");
    story.text(0x4F, "FIRST_LINE");
    story.bytes.insert(story.bytes.end(), {6, 0, 0, 3, 8, 0});
    story.text(0x50, "CAPTION"); story.text(0x4F, "SECOND");
    story.text(0x4F, "\x82\xa0_");
    const std::string mixed_line = "\x81\x75" "Bukan,_bukan,_hurufnya_beda._Karena_dia_\n"
        "menenun_(\x90\x44/Ori),_makanya_Orihime" "\x81\x76";
    story.text(0x4F, mixed_line);
    story.text(0x4F, "\x82\xa0\x82\xa2\x82\xa4_PC_\x82\xa6\x82\xa8");
    story.bytes.push_back(0);
    story.labels();
    write_file(data / "MES/story.mes", story.bytes);
    dc4scene::SceneRunner scene(data.string());
    CHECK(scene.load_ui_script((data / "MES/start.mes").string()));
    CHECK(scene.buttons().size() == 10);
    CHECK(scene.load_script((data / "MES/story.mes").string()));
    const auto automatic_path=data/"SaveData/DC2DM02.dat";
    scene.set_auto_save_path(automatic_path.string());
    CHECK(scene.advance_to_next_text());
    CHECK(scene.current_line() == "FIRST_LINE" && scene.speaker() == "Sakura");
    CHECK(fs::is_regular_file(automatic_path));
    CHECK(fs::is_regular_file(automatic_path.string()+".arisa"));
    dc4scene::SceneRunner automatic(data.string());
    CHECK(automatic.load_script((data/"MES/story.mes").string()));
    CHECK(automatic.load_state(automatic_path.string()));
    CHECK(automatic.current_line().empty());
    CHECK(automatic.advance_to_next_text() && automatic.current_line()=="FIRST_LINE");
    fs::remove(automatic_path.string()+".arisa");
    CHECK(automatic.load_state(automatic_path.string()));
    CHECK(automatic.current_line().empty());
    CHECK(automatic.advance_to_next_text() && automatic.current_line()=="FIRST_LINE");
    dc4scene::SceneRunner replay_auto(data.string());
    CHECK(replay_auto.load_script((data/"MES/story.mes").string()));
    replay_auto.set_variable(9,7,1);
    const auto replay_auto_path=data/"SaveData/replay.dat";
    replay_auto.set_auto_save_path(replay_auto_path.string());
    CHECK(replay_auto.advance_to_next_text());
    CHECK(!fs::exists(replay_auto_path));
    DmScript choice;
    choice.bytes.insert(choice.bytes.end(),{3,1,0});
    choice.text(0x4E,"Sakura");choice.text(0x4F,"QUESTION");
    choice.bytes.insert(choice.bytes.end(),{6,0,0,4,2,0,7,0,0});
    choice.string(0x43,"chosen");choice.text(0x50,"Choose");
    choice.bytes.insert(choice.bytes.end(),{9,0,0});
    choice.string(0x34,"chosen");choice.text(0x4F,"CHOSEN");choice.bytes.push_back(0);
    choice.labels();write_file(data/"MES/choice.mes",choice.bytes);
    dc4scene::SceneRunner choice_scene(data.string());
    CHECK(choice_scene.load_script((data/"MES/choice.mes").string()));
    CHECK(choice_scene.advance_to_next_text());
    CHECK(!choice_scene.advance_to_next_text() && choice_scene.waiting_for_choice());
    std::vector<uint8_t> choice_save;std::string choice_error;
    CHECK(choice_scene.capture_pc_save(&choice_save,&choice_error));
    CHECK(choice_scene.restore_pc_save(choice_save,&choice_error));
    CHECK(choice_scene.waiting_for_choice() && choice_scene.current_line()=="QUESTION");
    CHECK(choice_scene.choices().size()==1 && choice_scene.choices()[0].text=="Choose");
    CHECK(choice_scene.choose(0));
    CHECK(choice_scene.advance_to_next_text() && choice_scene.current_line()=="CHOSEN");
    scene.set_variable(100,1,1);
    CHECK(scene.variable(10,1)==1);
    scene.set_variable(10,2,1);
    CHECK(scene.variable(100,2)==1 && scene.variable(100,100)==0);
    std::vector<uint8_t> native;
    std::string native_error;
    CHECK(scene.capture_pc_save(&native,&native_error));
    CHECK(native.size()==0x3FE0 && native[0]==0xE1 && native[1]==0x3F);
    dc4scene::SceneRunner imported(data.string());
    CHECK(imported.load_script((data / "MES/story.mes").string()));
    CHECK(imported.restore_pc_save(native,&native_error));
    CHECK(imported.current_line()=="FIRST_LINE" && imported.speaker()=="Sakura");
    CHECK(imported.sprite(0).cur_x==20 && imported.sprite(0).cur_y==40);
    CHECK(imported.sprite(0).x==512 && imported.sprite(0).y==322);
    CHECK(imported.variable(100,1)==1 && imported.variable(100,2)==1);
    CHECK(imported.advance_to_next_text() && imported.current_line()=="SECOND");
    auto broken=native;broken[0]=0;
    CHECK(!imported.restore_pc_save(broken,&native_error));
    CHECK(imported.current_line()=="SECOND");
    const auto native_path=data/"SaveData/DC2DM04.dat";
    CHECK(scene.save_state(native_path.string()));
    CHECK(fs::is_regular_file(native_path.string()+".arisa"));
    CHECK(imported.load_state(native_path.string()));
    CHECK(imported.current_line()=="FIRST_LINE");
    native[4]^=1;write_file(native_path,native);
    CHECK(imported.load_state(native_path.string()));
    CHECK(imported.current_line()=="FIRST_LINE");
    CHECK(scene.visible_text() == u"FIRST LINE");
    CHECK(scene.sprite(0).visible && scene.sprite(0).img.valid());
    CHECK(scene.sprite(0).cur_x == 20 && scene.sprite(0).cur_y == 40);
    CHECK(scene.sprite(0).x == 512 && scene.sprite(0).y == 322);
    const auto* action = scene.hit_test(850, 530);
    CHECK(action && *action == ":auto");
    action = scene.hit_test(1075, 530); CHECK(action && *action == ":option");
    CHECK(scene.set_sidebar_visible(true));
    action = scene.hit_test(1095, 95); CHECK(action && *action == ":ds");
    action = scene.hit_test(1095, 185); CHECK(action && *action == ":aaconfig");
    action = scene.hit_test(1095, 320); CHECK(action && *action == ":sidebarclose");
    CHECK(scene.hit_test(15, 15) == nullptr);
    CHECK(scene.hit_test(850, 530) == nullptr);
    scene.set_pointer(-1, -1, false); scene.draw(-1); const auto sidebar_normal = scene.frame();
    scene.set_pointer(1095, 95, false); scene.draw(-1); CHECK(scene.frame() != sidebar_normal);
    CHECK(scene.set_sidebar_visible(false));
    scene.set_taskbar_visible(false);
    action = scene.hit_test(1140, 525); CHECK(action && *action == ":n");
    CHECK(scene.hit_test(1270, 710) == nullptr);
    scene.set_taskbar_visible(true); scene.set_message_visible(false);
    scene.draw(-1);
    CHECK(scene.frame()[(size_t(50) * 1280 + 25) * 4] > 200);
    CHECK(scene.frame()[(size_t(50) * 1280 + 24) * 4] == 0);
    std::vector<uint8_t> snapshot;
    CHECK(scene.capture_state(&snapshot));
    dc4scene::SceneRunner restored(data.string());
    CHECK(restored.restore_state(snapshot)); restored.set_message_visible(true);
    CHECK(restored.buttons().size() == 10);
    action = restored.hit_test(850, 530); CHECK(action && *action == ":auto");
    restored.set_message_visible(false); restored.draw(-1);
    CHECK(restored.frame() == scene.frame());
    CHECK(scene.advance_to_next_text()); CHECK(scene.current_line() == "SECOND");
    CHECK(scene.advance_to_next_text());
    CHECK(scene.visible_text().size() == 2 && scene.visible_text().back() == u'_');
    CHECK(scene.advance_to_next_text());
    CHECK(scene.current_line() == mixed_line);
    CHECK(scene.visible_text() == u"\u300cBukan, bukan, hurufnya beda. Karena dia \n"
        u"menenun (\u7e54/Ori), makanya Orihime\u300d");
    CHECK(scene.advance_to_next_text());
    CHECK(scene.visible_text() == u"\u3042\u3044\u3046_PC_\u3048\u304a");

    scene.set_message_visible(true);
    const auto normal_save = data / "saves/normal.dc4sav";
    const auto replay_save = data / "saves/replay.dc4sav";
    CHECK(scene.save_load_allowed());
    CHECK(scene.save_state(normal_save.string()));
    for (const auto& control : persistence_controls) {
        action = scene.hit_test(int(control.first * 1.25) + 5, 530);
        CHECK(action && *action == control.second);
    }
    scene.set_pointer(-1, -1, false); scene.draw(-1);
    const auto normal_toolbar_frame = scene.frame();
    CHECK(scene.set_sidebar_visible(true));
    scene.set_variable(9, 7, 1);
    CHECK(!scene.save_load_allowed() && !scene.sidebar_visible());
    CHECK(!scene.set_sidebar_visible(true) && !scene.sidebar_visible());
    CHECK(!scene.save_state(replay_save.string()));
    CHECK(!fs::exists(replay_save));
    const auto replay_line = scene.current_line();
    CHECK(!scene.load_state(normal_save.string()));
    CHECK(scene.current_line() == replay_line && scene.variable(9, 7) == 1);
    scene.set_pointer(-1, -1, false); scene.draw(-1);
    const auto disabled_frame = scene.frame();
    for (const int x : {940, 985}) {
        const size_t pixel = (size_t(530) * 1280 + x) * 4;
        CHECK(normal_toolbar_frame[pixel + 2] == 40 && disabled_frame[pixel + 2] == 240);
    }
    bool only_quick_icons_changed = true;
    for (int y = 0; y < 720; ++y) for (int x = 0; x < 1280; ++x) {
        const bool quick_icon = y >= 525 && y < 560 && ((x >= 935 && x < 970) || (x >= 980 && x < 1015));
        const size_t pixel = (size_t(y) * 1280 + x) * 4;
        if (!quick_icon && std::memcmp(normal_toolbar_frame.data() + pixel, disabled_frame.data() + pixel, 4) != 0)
            only_quick_icons_changed = false;
    }
    CHECK(only_quick_icons_changed);
    for (const auto& control : persistence_controls) {
        const int x = int(control.first * 1.25) + 5;
        action = scene.hit_test(x, 530); CHECK(action && *action == ":disabled");
        scene.set_pointer(x, 530, false); scene.draw(-1); CHECK(scene.frame() == disabled_frame);
        scene.set_pointer(x, 530, true); scene.draw(-1); CHECK(scene.frame() == disabled_frame);
    }
    action = scene.hit_test(1075, 530); CHECK(action && *action == ":disabled");
    scene.set_pointer(1075, 530, false); scene.draw(-1); CHECK(scene.frame() == disabled_frame);
    action = scene.hit_test(850, 530); CHECK(action && *action == ":auto");
    std::vector<uint8_t> replay_snapshot;
    CHECK(scene.capture_state(&replay_snapshot));
    dc4scene::SceneRunner restored_replay(data.string());
    CHECK(restored_replay.restore_state(replay_snapshot));
    CHECK(!restored_replay.save_load_allowed());
    CHECK(!restored_replay.set_sidebar_visible(true) && !restored_replay.sidebar_visible());
    CHECK(!restored_replay.save_state(replay_save.string()));
    CHECK(!restored_replay.load_state(normal_save.string()));
    for (const auto& control : persistence_controls) {
        action = restored_replay.hit_test(int(control.first * 1.25) + 5, 530);
        CHECK(action && *action == ":disabled");
    }
    action = restored_replay.hit_test(1075, 530); CHECK(action && *action == ":disabled");
    restored_replay.finish_replay();
    CHECK(restored_replay.save_load_allowed() && restored_replay.variable(9, 7) == 0);
    CHECK(restored_replay.set_sidebar_visible(true));
    scene.finish_replay();
    CHECK(scene.save_load_allowed());
    CHECK(scene.load_state(normal_save.string()));
    action = scene.hit_test(1075, 530); CHECK(action && *action == ":option");
    for (const auto& control : persistence_controls) {
        action = scene.hit_test(int(control.first * 1.25) + 5, 530);
        CHECK(action && *action == control.second);
    }

    DmScript selection;
    selection.bytes.insert(selection.bytes.end(), {0x21, 0, 0});
    selection.button(370, 264, 28, 28, "chapter"); selection.end_group();
    selection.bytes.insert(selection.bytes.end(), {0x09, 0, 0});
    selection.string(0x34, "chapter"); selection.text(0x4F, "CHAPTER"); selection.bytes.push_back(0);
    write_file(data / "MES/selection.mes", selection.bytes);
    CHECK(scene.load_script((data / "MES/selection.mes").string()));
    scene.set_message_visible(true);
    CHECK(!scene.advance_to_next_text()); CHECK(scene.waiting_for_time());
    CHECK(scene.buttons().size() == 11);
    action = scene.hit_test(465, 332); CHECK(action && *action == "chapter");
    CHECK(scene.hit_test(850, 530) == nullptr);
    scene.draw(-1); const auto normal = scene.frame();
    scene.set_pointer(465, 332, false); scene.draw(-1);
    CHECK(scene.frame() != normal);
    CHECK(scene.jump_to_label("chapter")); CHECK(scene.advance_to_next_text());
    CHECK(scene.current_line() == "CHAPTER");

    const std::vector<uint8_t> voice_sample{'X','P','C','M',8,0,0,0,0,0,0,0,
        1,0,1,0,0x44,0xAC,0,0,0x44,0xAC,0,0,1,0,8,0,129,130,131,132,133,134,135,136};
    write_file(data / "current.pcm", voice_sample);
    write_file(data / "next.pcm", voice_sample);
    DmScript voice_lines;
    voice_lines.string(0x36, "current"); voice_lines.text(0x4E, "Anzu"); voice_lines.text(0x4F, "VOICED");
    voice_lines.bytes.insert(voice_lines.bytes.end(), {6, 0, 0});
    voice_lines.text(0x4E, "Yoshiyuki"); voice_lines.text(0x4F, "UNVOICED");
    voice_lines.bytes.insert(voice_lines.bytes.end(), {6, 0, 0});
    voice_lines.string(0x37, "next"); voice_lines.text(0x4F, "NARRATION");
    voice_lines.bytes.insert(voice_lines.bytes.end(), {6, 0, 0});
    voice_lines.string(0x36, "next"); voice_lines.text(0x4E, "Yoshiyuki_/_Otome");
    voice_lines.text(0x4F, "JOINT"); voice_lines.bytes.push_back(0);
    write_file(data / "MES/voice-lines.mes", voice_lines.bytes);
    CHECK(scene.load_script((data / "MES/voice-lines.mes").string()));
    CHECK(scene.advance_to_next_text() && scene.last_voice() == "current");
    action = scene.hit_test(805, 530); CHECK(action && *action == ":torisetu");
    scene.set_pointer(-1, -1, false); scene.draw(-1); const auto voiced = scene.frame();
    CHECK(scene.advance_to_next_text() && scene.last_voice().empty());
    action = scene.hit_test(805, 530); CHECK(action && *action == ":disabled");
    std::vector<uint8_t> unvoiced;
    CHECK(scene.capture_state(&unvoiced));
    scene.set_pointer(-1, -1, false); scene.draw(-1); const auto silent = scene.frame();
    const size_t replay_pixel = (size_t(530) * 1280 + 805) * 4;
    CHECK(voiced[replay_pixel + 2] == 40 && silent[replay_pixel + 2] == 240);
    scene.set_pointer(805, 530, false); scene.draw(-1); CHECK(scene.frame() == silent);
    scene.set_pointer(805, 530, true); scene.draw(-1); CHECK(scene.frame() == silent);
    CHECK(scene.advance_to_next_text() && scene.last_voice().empty());
    action = scene.hit_test(805, 530); CHECK(action && *action == ":disabled");
    CHECK(scene.advance_to_next_text() && scene.last_voice() == "next");
    action = scene.hit_test(805, 530); CHECK(action && *action == ":torisetu");
    CHECK(scene.restore_state(unvoiced) && scene.last_voice().empty());
    action = scene.hit_test(805, 530); CHECK(action && *action == ":disabled");

    for (int count = 1; count <= 5; ++count) {
        DmScript choices;
        choices.text(0x4F, "QUESTION");
        choices.bytes.insert(choices.bytes.end(), {0x07, 1, 0});
        for (int i = 0; i < count; ++i) {
            choices.string(0x43, "branch" + std::to_string(i));
            choices.text(0x50, "OPTION_" + std::to_string(i));
        }
        choices.bytes.insert(choices.bytes.end(), {0x09, 0, 0});
        for (int i = 0; i < count; ++i) {
            choices.string(0x34, "branch" + std::to_string(i));
            choices.text(0x4F, "BRANCH" + std::to_string(i));
        }
        choices.bytes.push_back(0);
        write_file(data / "MES/choices.mes", choices.bytes);
        CHECK(scene.load_script((data / "MES/choices.mes").string()));
        CHECK(scene.advance_to_next_text()); CHECK(!scene.advance_to_next_text());
        CHECK(scene.waiting_for_choice() && scene.choices().size() == size_t(count));
        std::vector<uint8_t> saved;
        CHECK(scene.capture_state(&saved));
        const int top = int(std::lround((240 - count * 30) * 1.25));
        CHECK(scene.choice_at(350, top + 5) == -1);
        CHECK(scene.choice_at(380, top - 1) == -1);
        CHECK(!scene.choose(-1) && !scene.choose(count));
        for (int i = 0; i < count; ++i) {
            CHECK(scene.restore_state(saved));
            CHECK(scene.choice_at(380, top + 5 + i * 75) == i);
            scene.set_pointer(-1, -1, false); scene.draw(-1); const auto normal = scene.frame();
            scene.set_pointer(380, top + 5 + i * 75, false); scene.draw(-1);
            CHECK(scene.frame() != normal);
            const auto hovered = scene.frame();
            scene.set_pointer(380, top + 5 + i * 75, true); scene.draw(-1);
            CHECK(scene.frame() != hovered);
            CHECK(scene.choose(i)); CHECK(scene.advance_to_next_text());
            CHECK(scene.current_line() == "BRANCH" + std::to_string(i));
        }
    }
}

void dc2dm_game_regressions(const fs::path& data, const fs::path& root) {
    dc4scene::FontRenderer font;
    CHECK(font.load("C:/Windows/Fonts/msgothic.ttc", 0, 15, false));
    auto next_line = [](dc4scene::SceneRunner& scene) {
        for (int i = 0; i < 256; ++i) {
            if (scene.advance_to_next_text()) return true;
            if (!scene.last_error().empty() || scene.scene_ended() || scene.waiting_for_choice()) return false;
            if (!scene.pending_movie().empty()) scene.finish_movie();
            scene.finish_script_wait();
            scene.update(60000);
        }
        return false;
    };
    if(const char* saves=std::getenv("DC2DM_SAVE_DATA")) for(const auto& entry:fs::directory_iterator(saves)) {
        if(!entry.is_regular_file() || entry.path().filename().string().find("DC2DM")!=0 || entry.path().extension()!=".dat")continue;
        dc4scene::SceneRunner imported(data.string());
        imported.set_font(&font);
        if(!imported.load_state(entry.path().string())) {
            std::cerr<<entry.path()<<": "<<imported.last_error()<<'\n';CHECK(false);continue;
        }
        const bool automatic=imported.current_line().empty();
        const auto exported=root/entry.path().filename();
        if(!imported.save_state(exported.string())) {
            std::cerr<<entry.path()<<": "<<imported.last_error()<<'\n';CHECK(false);continue;
        }
        fs::remove(exported.string()+".arisa");
        dc4scene::SceneRunner restored(data.string());restored.set_font(&font);
        CHECK(restored.load_state(exported.string()));
        CHECK(restored.current_line()==imported.current_line());
        CHECK(restored.speaker()==imported.speaker());
        if(automatic) {CHECK(next_line(imported));CHECK(next_line(restored));}
        imported.draw(-1);restored.draw(-1);CHECK(restored.frame()==imported.frame());
        CHECK(next_line(imported));CHECK(next_line(restored));
        CHECK(restored.current_line()==imported.current_line());
        std::cout<<"DC2DM native round trip: "<<entry.path().filename()<<'\n';
    }
    {
        dc4scene::SceneRunner scene(data.string());
        scene.set_font(&font);
        CHECK(scene.load_ui_script((data / "MES/start.mes").string()));
        CHECK(scene.load_script((data / "MES/mar_0129_e1.mes").string()));
        bool found = false;
        for (int line = 0; line < 300 && next_line(scene); ++line) {
            if (scene.current_line().find("Apa_yang_sebaiknya_aku_katakan_pada_Anzu") != std::string::npos) {
                found = true;
                break;
            }
        }
        CHECK(found && scene.last_voice().empty() && !scene.replay_voice());
        CHECK(!next_line(scene) && scene.waiting_for_choice());
        CHECK(scene.choices().size() == 2);
        CHECK(scene.choice_at(400, 230) == 0 && scene.choice_at(400, 305) == 1);
        std::vector<uint8_t> saved;
        CHECK(scene.capture_state(&saved));
        std::vector<uint8_t> native_choice;
        CHECK(scene.capture_pc_save(&native_choice));
        if (const char* captures = std::getenv("DC2DM_CHOICE_CAPTURES"))
            write_file(fs::path(captures) / "choice.dat", native_choice);
        dc4scene::SceneRunner native_restored(data.string());
        CHECK(native_restored.load_script((data / "MES/mar_0129_e1.mes").string()));
        CHECK(native_restored.restore_pc_save(native_choice));
        CHECK(native_restored.waiting_for_choice() && native_restored.choices().size() == 2);
        CHECK(native_restored.current_line() == scene.current_line() && native_restored.last_voice().empty());
        for (int branch = 0; branch < 2; ++branch) {
            CHECK(scene.restore_state(saved));
            scene.set_pointer(400, branch == 0 ? 230 : 305, false);
            scene.draw(-1);
            if (const char* captures = std::getenv("DC2DM_CHOICE_CAPTURES")) {
                fs::create_directories(captures);
                write_file(fs::path(captures) / ("choice-" + std::to_string(branch) + ".rgba"), scene.frame());
            }
            CHECK(scene.choose(branch) && next_line(scene));
            CHECK(scene.speaker() == "Yoshiyuki" && scene.last_voice().empty());
            auto first_line = scene.current_line();
            first_line.erase(std::remove(first_line.begin(), first_line.end(), '\n'), first_line.end());
            CHECK(first_line.find(branch == 0 ? "selamat_ulang_tahun_ya" : "tambah_tua_setahun_lagi") != std::string::npos);
            const std::string expected_voice = branch == 0 ? "ANZ022" : "ANZ028";
            bool first_voice = false, joined = false;
            for (int line = 0; line < 50 && next_line(scene); ++line) {
                if (!first_voice && !scene.last_voice().empty()) {
                    CHECK(scene.last_voice().find(expected_voice) != std::string::npos);
                    first_voice = true;
                }
                if (scene.current_line().find("Semuanya,_sekali_lagi_terima_kasih_buat_") != std::string::npos) {
                    CHECK(scene.last_voice().find("ANZ031") != std::string::npos);
                    joined = true;
                    break;
                }
            }
            CHECK(first_voice && joined);
        }
    }
    for (const auto& route : {std::pair<const char*, const char*>{"sce01", "MAR_0101_F1"},
                              {"sce02", "SIN_0326_F1"}, {"sce03", "SUC60_0811_A1_OTM"}}) {
        const int failures_before = failures;
        dc4scene::SceneRunner scene(data.string());
        scene.set_font(&font);
        CHECK(scene.load_ui_script((data / "MES/start.mes").string()));
        CHECK(scene.load_script((data / "MES/sceselect.mes").string()));
        scene.set_variable(100, 1, 1); scene.set_variable(100, 2, 1);
        CHECK(!scene.advance_to_next_text());
        CHECK(scene.waiting_for_time() && scene.last_error().empty());
        const auto button = std::find_if(scene.buttons().begin(), scene.buttons().end(),
            [&](const auto& value) { return value.action == route.first; });
        CHECK(button != scene.buttons().end());
        if (button == scene.buttons().end()) continue;
        const auto* action = scene.hit_test(button->x + button->w / 2, button->y + button->h / 2);
        CHECK(action && *action == route.first);
        CHECK(scene.jump_to_label(route.first));
        CHECK(next_line(scene));
        std::string script = scene.script_name();
        std::transform(script.begin(), script.end(), script.begin(), [](unsigned char c) { return char(std::toupper(c)); });
        CHECK(script == std::string(route.second) + ".MES");
        CHECK(!scene.visible_text().empty());
        scene.draw(-1);
        CHECK(std::count_if(scene.frame().begin(), scene.frame().end(), [](uint8_t byte) { return byte != 0; }) > 1280 * 720 * 2);
        for (int line = 1; line < 40; ++line) CHECK(next_line(scene));
        CHECK(scene.last_error().empty());
        scene.draw(-1);
        const auto frame = scene.frame();
        const auto text = scene.current_line();
        const auto path = root / (std::string(route.first) + ".dc4sav");
        CHECK(scene.save_state(path.string()));
        CHECK(next_line(scene));
        const auto following_text = scene.current_line();
        dc4scene::SceneRunner restored(data.string());
        restored.set_font(&font);
        CHECK(restored.load_state(path.string()));
        CHECK(restored.current_line() == text);
        restored.draw(-1); CHECK(restored.frame() == frame);
        CHECK(restored.set_sidebar_visible(true));
        const auto* save_action = restored.hit_test(1095, 95);
        CHECK(save_action && *save_action == ":ds");
        CHECK(restored.set_sidebar_visible(false));
        CHECK(next_line(restored)); CHECK(restored.current_line() == following_text);
        std::cout << "DC2DM route " << (failures == failures_before ? "passed: " : "FAILED: ") << route.first << '\n';
    }
}

void sakuramusubi_text_regressions(const fs::path& root) {
    const auto language = dc4::get_language();
    const auto decoder = dc4scene::g_sjis_decoder;
    dc4scene::g_sjis_decoder = [](const std::string& bytes) {
        std::u16string text(bytes.begin(), bytes.end());
        std::replace(text.begin(), text.end(), u'\u007f', u'\u001a');
        return text;
    };
    for (const auto mode : {dc4::Language::Japanese, dc4::Language::English, dc4::Language::Indonesian}) {
        dc4::set_language(mode);
        CHECK(dc4scene::sjis_to_utf16("Latin\x7f" "words") == u"Latin words");
        CHECK(dc4scene::sjis_to_utf16("literal\x1a" "control") == u"literal\u001acontrol");
    }
    dc4scene::g_sjis_decoder = decoder;
#ifdef _WIN32
    // dc4_sen20181220d.mes:0x1990 uses DEL between words and one authored LF.
    const std::string sentence = "Apparently, even people who barely knew each other turn into lovebirds\n"
        "  who act like soulmates once Shirakawa's through with them.";
    std::string patched = "\x81\x75" + sentence + "\x81\x76";
    std::replace(patched.begin(), patched.end(), ' ', '\x7f');
    const std::u16string expected = u"\u300cApparently, even people who barely knew each other turn into lovebirds\n"
        u"  who act like soulmates once Shirakawa's through with them.\u300d";
    const std::string japanese = "\x81\x75\x82\xa0\x82\xa2\x82\xa4\x81\x76";
    const auto data = root / "sakuramusubi";
    const auto script_path = data / "MES/text.mes";
    auto load_text = [&](dc4scene::SceneRunner& scene, const std::string& text) {
        Script script; script.bytes[11] = 0xAB;
        script.text(text); script.bytes.push_back(0);
        write_file(script_path, script.bytes);
        CHECK(scene.load_script(script_path.string()));
        CHECK(scene.advance_to_next_text());
    };
    for (const auto mode : {dc4::Language::Japanese, dc4::Language::English, dc4::Language::Indonesian}) {
        dc4::set_language(mode);
        dc4scene::SceneRunner scene(data.string());
        load_text(scene, patched);
        CHECK(scene.current_line() == patched && scene.visible_text() == expected);
        CHECK(scene.visible_text_length() == expected.size() - 1);
        load_text(scene, japanese);
        CHECK(scene.visible_text() == u"\u300c\u3042\u3044\u3046\u300d");
        load_text(scene, "Selamat\xa5pagi.");
        CHECK(scene.visible_text() == u"Selamat pagi.");
    }
    dc4scene::FontRenderer font;
    const auto font_path = fs::path(__FILE__).parent_path().parent_path() /
        "android/app/src/main/assets/fonts/MSGOTHIC.TTC";
    CHECK(font.load(font_path.string(), 2, 20, false));
    auto render = [&](const std::string& text) {
        dc4scene::SceneRunner scene(data.string()); scene.set_font(&font);
        load_text(scene, text); scene.draw(-1);
        return scene.frame();
    };
    dc4::set_language(dc4::Language::English);
    std::string supported_spaces = patched;
    std::replace(supported_spaces.begin(), supported_spaces.end(), '\x7f', '\xa5');
    CHECK(render(patched) == render(supported_spaces));
    size_t fit = 0;
    while (font.text_width(u"\u300c" + std::u16string(fit + 1, u'W'), 20) <= 750) ++fit;
    const std::string unbroken(fit + 10, 'W');
    CHECK(render("\x81\x75" + unbroken + "\x81\x76") ==
        render("\x81\x75" + unbroken.substr(0, fit) + "\n" + unbroken.substr(fit) + "\x81\x76"));
    std::string latin_prefix;
    while (font.text_width(std::u16string(latin_prefix.size() + 1, u'W') + u"(", 20) <= 750)
        latin_prefix += 'W';
    CHECK(render(latin_prefix + "(WWW)") == render(latin_prefix + "\n(WWW)"));
    dc4::set_language(dc4::Language::Japanese);
    std::string japanese_prefix;
    for (int i = 0; i < 29; ++i) japanese_prefix += "\x82\xa0";
    CHECK(render(japanese_prefix + japanese) == render(japanese_prefix + "\n" + japanese));
#endif
    dc4::set_language(language);
}

void dalsegno_game_regressions(const fs::path& data) {
    dc4scene::FontRenderer font;
    font.load("C:/Windows/Fonts/msgothic.ttc", 0, 22);
    dc4scene::SceneRunner scene(data.string());
    scene.set_font(&font);
    scene.set_use_system_menu(false);
    CHECK(scene.load_sheets((data / "MES/start.mes").string()));
    CHECK(scene.load_ui_script((data / "MES/start.mes").string()));
    const std::vector<std::string> toolbar{":n", ":t", ":rapid", ":auto", ":qs", ":ql",
        ":backlog", ":ds", ":dl", ":option", ":clear"};
    CHECK(scene.buttons().size() == toolbar.size());
    for (const auto& action : toolbar) {
        CHECK(std::any_of(scene.buttons().begin(), scene.buttons().end(), [&](const auto& b) {
            const auto* hit = scene.hit_test(b.x + b.w / 2, b.y + b.h / 2);
            return b.action == action && !hit;
        }));
    }
    CHECK(scene.load_script((data / "MES/main.mes").string()));
    CHECK(scene.variable(100, 105) == 0);
    CHECK(scene.variable(99, 225) == 100);
    CHECK(scene.variable(99, 7) == 80);
    scene.advance_to_next_text();
    CHECK(scene.last_error().empty());
    const std::vector<std::string> title{"sce_start", "qload", "load", "extra", "config", "end"};
    CHECK(scene.buttons().size() == title.size());
    for (const auto& action : title)
        CHECK(std::any_of(scene.buttons().begin(), scene.buttons().end(), [&](const auto& b) { return b.action == action; }));
    scene.set_variable(100, 105, 1);
    CHECK(scene.load_script((data / "MES/main.mes").string()));
    scene.advance_to_next_text();
    CHECK(scene.buttons().size() == title.size() + 1);
    CHECK(std::any_of(scene.buttons().begin(), scene.buttons().end(), [](const auto& b) { return b.action == "ds"; }));

    size_t sliders_checked = 0;
    bool master_checked = false;
    for (int tab = 0; tab < 5; ++tab) {
        scene.set_variable(100, 54, tab);
        CHECK(scene.load_script((data / "MES/config.mes").string()));
        scene.advance_to_next_text();
        CHECK(scene.last_error().empty());
        for (const auto& b : scene.buttons()) {
            if (!b.is_slider) continue;
            CHECK(b.has_knob && b.knob[0].w > 0);
            CHECK(b.slider_count > 1 && b.slider_right > b.slider_left);
            if (b.slider_count <= 1 || b.slider_right <= b.slider_left) continue;
            const int y = b.y + b.h / 2;
            std::string action;
            CHECK(scene.handle_slider_touch(0, b.x + b.w / 2, y, &action));
            CHECK(action.empty());
            CHECK(scene.handle_slider_touch(2, b.slider_left - 40, y, &action));
            CHECK(scene.variable(b.slider_bank, b.slider_index) == 0);
            scene.draw(-1);
            const auto minimum_frame = scene.frame();
            CHECK(scene.handle_slider_touch(2, b.slider_right + 40, y, &action));
            CHECK(scene.variable(b.slider_bank, b.slider_index) == b.slider_count - 1);
            scene.draw(-1);
            CHECK(scene.frame() != minimum_frame);
            CHECK(scene.handle_slider_touch(1, b.slider_right, y, &action));
            CHECK(action == b.action);
            CHECK(!scene.handle_slider_touch(2, b.slider_left, y, &action));
            CHECK(scene.variable(b.slider_bank, b.slider_index) == b.slider_count - 1);
            master_checked = master_checked || (b.slider_bank == 99 && b.slider_index == 225);
            ++sliders_checked;
        }
    }
    CHECK(master_checked && sliders_checked > 0);
    const auto capture = [&](const std::string& name) {
        const char* directory = std::getenv("DS_UI_CAPTURE");
        if (!directory) return;
        scene.draw(-1);
        std::ofstream out(fs::path(directory) / (name + ".ppm"), std::ios::binary);
        out << "P6\n1280 720\n255\n";
        const auto& pixels = scene.frame();
        for (size_t i = 0; i < pixels.size(); i += 4)
            out.write(reinterpret_cast<const char*>(pixels.data() + i), 3);
        CHECK(bool(out));
    };
    const auto open = [&](const std::string& script, int tab = 0) {
        scene.set_variable(100, 54, tab);
        CHECK(scene.load_script((data / "MES" / (script + ".mes")).string()));
        scene.advance_to_next_text();
        if (!scene.last_error().empty()) std::cout << "Open " << script << ": " << scene.last_error() << '\n';
        CHECK(scene.last_error().empty());
    };
    const auto settle = [&] {
        for (int i = 0; scene.waiting_for_time() && i < 20; ++i) {
            scene.update(2000);
            if (scene.script_ready()) scene.advance_to_next_text();
        }
        CHECK(scene.last_error().empty());
    };
    const auto click = [&](const std::string& target) {
        const auto button = std::find_if(scene.buttons().begin(), scene.buttons().end(),
            [&](const auto& b) { return b.action == target; });
        CHECK(button != scene.buttons().end());
        if (button == scene.buttons().end()) return;
        const auto* hit = scene.hit_test(button->x + button->w / 2, button->y + button->h / 2);
        CHECK(hit && *hit == target);
        CHECK(scene.jump_to_label(target));
        scene.advance_to_next_text();
        if (!scene.last_error().empty()) std::cout << "Click " << target << ": " << scene.last_error() << '\n';
        CHECK(scene.last_error().empty());
    };
    open("main");
    scene.set_pointer(-1, -1, false);
    capture("title-idle");
    scene.draw(-1); const auto idle = scene.frame();
    const auto config = std::find_if(scene.buttons().begin(), scene.buttons().end(),
        [](const auto& b) { return b.action == "config"; });
    CHECK(config != scene.buttons().end());
    if (config != scene.buttons().end()) {
        scene.set_pointer(config->x + 10, config->y + 10, false);
        scene.draw(-1); CHECK(scene.frame() != idle);
        capture("title-hover");
    }
    click("config");
    CHECK(fs::path(scene.script_name()).stem().string() == "CONFIG");
    CHECK(scene.current_line().empty());
    capture("config-system");
    for (int tab = 0; tab < 5; ++tab) {
        open("config", tab);
        CHECK(scene.current_line().empty());
        capture("config-tab-" + std::to_string(tab));
        std::cout << "CONFIG tab " << tab << ':';
        for (const auto& b : scene.buttons()) if (!b.is_slider) std::cout << ' ' << b.action;
        std::cout << '\n';
    }
    size_t config_actions_checked = 0;
    for (int tab = 0; tab < 5; ++tab) {
        open("config", tab);
        std::vector<std::string> actions;
        for (const auto& b : scene.buttons())
            if (!b.is_slider && !b.action.empty() && b.action != "init") actions.push_back(b.action);
        for (const auto& action : actions) {
            open("config", tab);
            click(action); settle();
            std::cout << "CONFIG click " << tab << ' ' << action << " -> " << scene.script_name() << '\n';
            ++config_actions_checked;
        }
    }
    open("main"); click("extra"); settle();
    capture("extra");
    std::vector<std::string> extra_actions;
    for (const auto& b : scene.buttons()) extra_actions.push_back(b.action);
    for (const auto& action : extra_actions) {
        open("main"); click("extra"); settle();
        click(action); settle();
        capture("extra-" + action);
        std::cout << "EXTRA click " << action << " -> " << scene.script_name() << '\n';
    }
    open("main"); click("load"); settle();
    capture("load");
    std::cout << "LOAD actions:";
    for (const auto& b : scene.buttons()) std::cout << ' ' << b.action;
    std::cout << '\n';
    open("config", 1);
    scene.set_animation_enabled(true);
    scene.set_variable(99, 26, 0);
    scene.draw(-1); const auto first_preview = scene.frame();
    scene.update(900); scene.draw(-1);
    CHECK(scene.frame() != first_preview);
    capture("config-preview");
    scene.set_variable(99, 26, 11);
    scene.draw(-1); const auto fastest_preview = scene.frame();
    scene.update(500); scene.draw(-1);
    CHECK(scene.frame() == fastest_preview);
    scene.set_variable(100, 49, 0);
    scene.draw(-1); const auto transparent = scene.frame();
    scene.set_variable(100, 49, 100);
    scene.draw(-1); CHECK(scene.frame() != transparent);
    capture("config-opacity");
    scene.set_animation_enabled(false);
    scene.set_variable(99, 7, 23);
    click("init");
    CHECK(scene.variable(99, 7) == 23);
    CHECK(scene.waiting_for_time());
    CHECK(!scene.hit_test(440, 12));
    CHECK(!scene.jump_to_label("menu1"));
    CHECK(!scene.handle_slider_touch(0, 120, 200));
    CHECK(!scene.advance_to_next_text());
    capture("default-confirmation");
    CHECK(scene.dalsegno_back_action() == "not_def");
    CHECK(scene.jump_to_label(scene.dalsegno_back_action()));
    scene.advance_to_next_text();
    CHECK(scene.variable(99, 7) == 23);
    click("init");
    const auto* yes = scene.hit_test(500, 270);
    CHECK(yes && *yes == "do_def");
    CHECK(scene.jump_to_label("do_def"));
    scene.advance_to_next_text();
    CHECK(scene.variable(99, 7) == 80);
    CHECK(scene.variable(100, 49) == 80);
    CHECK(scene.variable(94, 0) == 100);
    CHECK(scene.variable(100, 105) == 1);
    CHECK(scene.last_error().empty());
    using UiKey = dc4scene::SceneRunner::UiKey;
    std::string selected;
    scene.set_pointer(-1, -1, false);
    CHECK(scene.handle_dalsegno_key(UiKey::Next, &selected));
    CHECK(scene.handle_dalsegno_key(UiKey::Activate, &selected));
    CHECK(!selected.empty());
    const auto back = scene.dalsegno_back_action();
    CHECK(back == "retchk");
    CHECK(scene.jump_to_label(back)); scene.advance_to_next_text();
    CHECK(scene.script_name() == "main.mes" || scene.script_name() == "MAIN.mes");
    open("DS_1129B_CM");
    scene.set_animation_enabled(false);
    for (int i = 0; i < 100 && !scene.dalsegno_name_pending(); ++i) scene.advance_to_next_text();
    CHECK(scene.dalsegno_name_pending());
    CHECK(!scene.advance_to_next_text());
    CHECK(!scene.set_sidebar_visible(true));
    CHECK(!scene.set_dalsegno_name("", "Adam"));
    CHECK(!scene.set_dalsegno_name("Budi", ""));
    CHECK(!scene.set_dalsegno_name("Budi", "A$dam"));
    CHECK(!scene.set_dalsegno_name("Budi", std::string(33, 'a')));
    CHECK(scene.set_dalsegno_name("Budi", "Adam"));
    capture("name-custom");
    CHECK(scene.handle_dalsegno_key(UiKey::Next, &selected));
    CHECK(scene.handle_dalsegno_key(UiKey::Activate, &selected));
    CHECK(selected == ":namefamily");
    CHECK(scene.handle_dalsegno_action(":nameconfirm"));
    CHECK(scene.dalsegno_name_confirming());
    CHECK(!scene.set_dalsegno_name("Other", "Other"));
    CHECK(scene.dalsegno_back_action() == ":nameno");
    CHECK(scene.handle_dalsegno_action(":nameno"));
    CHECK(scene.dalsegno_name_pending() && !scene.dalsegno_name_confirming());
    CHECK(scene.dalsegno_given_name() == "Adam");
    CHECK(scene.handle_dalsegno_action(":namedefault"));
    CHECK(scene.dalsegno_family_name() == "Takamura" && scene.dalsegno_given_name() == "Atsuya");
    CHECK(scene.set_dalsegno_name("Budi", "Adam"));
    CHECK(scene.handle_dalsegno_action(":nameconfirm"));
    capture("name-confirmation");
    CHECK(scene.handle_dalsegno_action(":nameyes"));
    CHECK(!scene.dalsegno_name_pending());
    CHECK(scene.speaker() == "Adam");
    CHECK(scene.buttons().size() == toolbar.size());
    for (const auto& button : scene.buttons()) {
        const auto* hit = scene.hit_test(button.x + button.w / 2, button.y + button.h / 2);
        CHECK(hit && *hit == button.action);
    }
    scene.set_taskbar_visible(false);
    CHECK(!scene.hit_test(1074, 588));
    CHECK(scene.hit_test(1260, 588) && *scene.hit_test(1260, 588) == ":n");
    scene.set_taskbar_visible(true);
    scene.set_message_visible(false);
    CHECK(!scene.hit_test(1224, 588));
    scene.set_message_visible(true);
    const auto story_line = scene.current_line();
    const auto story_speaker = scene.speaker();
    const auto story_script = scene.script_name();
    CHECK(scene.set_sidebar_visible(true));
    capture("upper-menu");
    for (const auto& hit : std::vector<std::pair<int, std::string>>{
            {100, ":ds"}, {300, ":dl"}, {500, ":option"}, {700, ":aawin_size"}, {900, ":aatitle"}, {1100, ":sidebarclose"}}) {
        const auto* target = scene.hit_test(hit.first, 20);
        CHECK(target && *target == hit.second);
    }
    CHECK(!scene.advance_to_next_text());
    CHECK(scene.dalsegno_back_action() == ":sidebarclose");
    CHECK(scene.handle_dalsegno_action(":sidebarclose"));
    CHECK(scene.current_line() == story_line);
    CHECK(scene.open_dalsegno_config());
    click("init");
    CHECK(!scene.resume_dalsegno_config());
    CHECK(scene.jump_to_label(scene.dalsegno_back_action())); scene.advance_to_next_text();
    CHECK(scene.current_line().empty());
    scene.set_variable(99, 7, 31); scene.set_variable(100, 49, 47);
    CHECK(scene.resume_dalsegno_config());
    CHECK(scene.current_line() == story_line && scene.speaker() == story_speaker && scene.script_name() == story_script);
    CHECK(scene.variable(99, 7) == 31 && scene.variable(100, 49) == 47);
    std::vector<uint8_t> personal_save;
    std::string personal_error;
    CHECK(scene.capture_state(&personal_save, &personal_error));
    dc4scene::SceneRunner restored(data.string());
    restored.set_font(&font); restored.set_use_system_menu(false);
    CHECK(restored.restore_state(personal_save, &personal_error));
    if (!personal_error.empty()) std::cerr << "Personal restore: " << personal_error << '\n';
    CHECK(restored.current_line() == story_line && restored.speaker() == "Adam");
    scene.draw(-1);
    restored.draw(-1);
    CHECK(scene.frame() == restored.frame());
    int name_ink = 0;
    for (int y = 576; y < 597; ++y) for (int x = 300; x < 350; ++x) {
        const size_t pixel = (size_t(y) * 1280 + x) * 4;
        const auto& frame = scene.frame();
        if (frame[pixel] > 220 && frame[pixel + 1] > 220 && frame[pixel + 2] > 220) ++name_ink;
    }
    CHECK(name_ink > 20);
    capture("name-story-return");
    dc4ui::LogScreen log;
    CHECK(log.load(data.string()));
    CHECK(log.visible_rows() == 5 && log.row_pitch() == 125);
    std::vector<dc4ui::LogRow> rows;
    for (int i = 0; i < 12; ++i) {
        rows.push_back(dc4ui::capture_log_row(scene,i));
        scene.advance_to_next_text();
    }
    CHECK(rows.front().name == u"Adam" && !rows.front().snapshot.empty());
    CHECK(log.last_scroll(rows.size()) == 7);
    CHECK(log.thumb_scroll(-100,rows.size()) == 0);
    CHECK(log.thumb_scroll(93,rows.size()) == 0);
    CHECK(log.thumb_scroll(387,rows.size()) == 4);
    CHECK(log.thumb_scroll(681,rows.size()) == 7);
    CHECK(log.thumb_scroll(1000,rows.size()) == 7);
    CHECK(!log.thumb_hit(1127,200,0,5));
    CHECK(log.thumb_hit(1127,200,0,rows.size()));
    CHECK(!log.checkbox_hit(340,50));
    CHECK(log.back_hit(1235,670));
    for (int slot = 0; slot < 5; ++slot) {
        CHECK(log.jump_hit(70,130 + slot*125,3,rows.size()) == 3 + slot);
        CHECK(log.voice_hit(230,130 + slot*125,3,rows.size()) == 3 + slot);
    }
    CHECK(log.jump_hit(70,50,3,rows.size()) == -1);
    std::vector<uint8_t> log_frame(size_t(1280)*720*4,0);
    log.draw(rows,0,false,-1,-1,font,&log_frame);
    const auto first_log = log_frame;
    log.draw(rows,7,false,1127,681,font,&log_frame);
    CHECK(first_log != log_frame);
    if (const char* directory = std::getenv("DS_UI_CAPTURE")) {
        std::ofstream out(fs::path(directory)/"backlog-five-rows.ppm",std::ios::binary);
        out << "P6\n1280 720\n255\n";
        for (size_t i = 0; i < log_frame.size(); i += 4) out.write(reinterpret_cast<const char*>(log_frame.data()+i),3);
    }
    CHECK(restored.restore_state(rows[3].snapshot));
    CHECK(dc4ui::capture_log_row(restored,3).text == rows[3].text);

    bool voiced = false, multiple_bodies = false;
    for (int line = 0; line < 250 && !(voiced && multiple_bodies); ++line) {
        if (!scene.advance_to_next_text()) break;
        CHECK(scene.last_error().empty());
        if (!scene.last_voice().empty() && !voiced) {
            CHECK(dc4ui::capture_log_row(scene,line).voice == scene.last_voice());
            capture("story-first-voice"); voiced = true;
        }
        int bodies = 0;
        for (int body = 0; body < 4; ++body) if (scene.sprite(body).visible && scene.sprite(body).img.valid()) ++bodies;
        if (bodies >= 2 && !multiple_bodies) { capture("story-multiple-bodies"); multiple_bodies = true; }
    }
    CHECK(voiced && multiple_bodies);

    open("main"); click("end");
    CHECK(!scene.quit_requested());
    capture("end-confirmation");
    CHECK(scene.dalsegno_back_action() == "main");
    CHECK(scene.jump_to_label("main")); scene.advance_to_next_text();
    CHECK(!scene.quit_requested());
    click("end");
    CHECK(scene.jump_to_label("do_end")); scene.advance_to_next_text();
    CHECK(scene.quit_requested());
    CHECK(scene.last_error().empty());
    open("main"); scene.set_animation_enabled(true);
    click("end"); CHECK(scene.jump_to_label("do_end")); scene.advance_to_next_text();
    CHECK(scene.waiting_for_time() && !scene.quit_requested());
    scene.update(999); CHECK(!scene.script_ready());
    scene.update(1); CHECK(scene.script_ready());
    scene.advance_to_next_text(); CHECK(scene.quit_requested());
    std::cout << config_actions_checked << " CONFIG button actions checked\n";
    std::cout << "Dal Segno title gates, toolbar hit targets and " << sliders_checked << " slider drags checked\n";
}

void dalsegno_transition_regressions(const fs::path& data) {
    dc4scene::FontRenderer font;
    CHECK(font.load((data / "GRP/mplus-1c-heavy.ttf").string(), 0, 28));
    for (const bool automatic : {false, true}) {
    dc4scene::SceneRunner scene(data.string());
    scene.set_font(&font); scene.set_use_system_menu(false);
    scene.set_animation_enabled(true);
    CHECK(scene.load_ui_script((data / "MES/start.mes").string()));
    CHECK(scene.load_script((data / "MES/DS_1201A_CM.mes").string()));
    int last_wait = -1;
    scene.set_observer([&](const dc4::MesCommand& c, const std::string&) {
        if (c.op == 0x13 && c.kind == dc4::CmdKind::Param2) {
            last_wait = c.p1;
            std::cout << "Date WAIT " << int(c.p1) << ',' << int(c.p2) << '\n';
        }
    });
    if (automatic) {
        std::ifstream input(data / "savedata/DSAO01.dat", std::ios::binary);
        std::vector<uint8_t> save((std::istreambuf_iterator<char>(input)), {});
        CHECK(save.size() == 0x11AF8);
        if (save.size() != 0x11AF8) continue;
        CHECK(save[0x83C] == 0x5C && save[0x83D] == 1 && save[0x83E] == 0 && save[0x83F] == 0);
        std::fill(save.begin() + 8, save.begin() + 168, 0);
        const std::string script = "DS_1201A_CM";
        std::copy(script.begin(), script.end(), save.begin() + 8);
        CHECK(scene.restore_pc_save(save));
    } else scene.advance_to_next_text();
    for (int i = 0; i < 40 && last_wait != 0; ++i) {
        CHECK(scene.current_line().empty() && scene.waiting_for_time());
        scene.update(5000);
        if (scene.script_ready()) scene.advance_to_next_text();
    }
    CHECK(last_wait == 0 && scene.waiting_for_time() && scene.current_line().empty());
    CHECK(!scene.dalsegno_menu_active());
    CHECK(scene.last_error().empty());
    const auto toolbar = scene.buttons();
    CHECK(toolbar.size() == 11);
    for (const auto& b : toolbar) CHECK(!scene.hit_test(b.x + b.w / 2, b.y + b.h / 2));
    std::string action;
    CHECK(!scene.handle_dalsegno_key(dc4scene::SceneRunner::UiKey::Next, &action));
    scene.draw(-1); const auto date_frame = scene.frame();
    int cyan = 0, white = 0;
    for (int y = 544; y < 600; ++y) for (int x = 40; x < 384; ++x) {
        const size_t p = (size_t(y) * 1280 + x) * 4;
        cyan += date_frame[p] < 50 && date_frame[p+1] > 150 && date_frame[p+2] > 150;
        white += date_frame[p] > 240 && date_frame[p+1] > 240 && date_frame[p+2] > 240;
    }
    CHECK(cyan > 100 && white == 0);
    if (const char* directory = std::getenv("DS_UI_CAPTURE")) {
        std::ofstream out(fs::path(directory) / "date-1201.ppm", std::ios::binary);
        out << "P6\n1280 720\n255\n";
        for (size_t i = 0; i < date_frame.size(); i += 4)
            out.write(reinterpret_cast<const char*>(date_frame.data() + i), 3);
    }
    scene.update(60000); scene.draw(-1);
    CHECK(scene.waiting_for_time() && !scene.script_ready() && scene.frame() == date_frame);
    scene.finish_script_wait();
    CHECK(!scene.waiting_for_time());
    scene.advance_to_next_text();
    for (int i = 0; i < 40 && scene.current_line().empty(); ++i) {
        scene.update(5000);
        if (scene.script_ready()) scene.advance_to_next_text();
    }
    CHECK(scene.current_line() == " The sun's piercing rays were full of summer energy. ");
    CHECK(scene.last_error().empty() && !scene.waiting_for_time());
    scene.draw(-1);
    for (const auto& b : toolbar) {
        const auto* hit = scene.hit_test(b.x + b.w / 2, b.y + b.h / 2);
        CHECK(hit && *hit == b.action);
    }
    std::cout << "Dal Segno " << (automatic ? "automatic load" : "script entry")
              << " date origin, cyan alpha, hidden toolbar and click-to-dialogue checked\n";
    }
}

void dalsegno_music_regressions(const fs::path& data) {
    dc4::AudioEngine audio;
    dc4scene::FontRenderer font;
    CHECK(font.load((data / "GRP/mplus-1c-heavy.ttf").string(), 0, 28));
    dc4scene::SceneRunner scene(data.string());
    scene.set_audio(&audio,nullptr); scene.set_font(&font); scene.set_use_system_menu(false);
    scene.set_animation_enabled(false);
    CHECK(scene.load_script(dc4::resolve_asset((data / "MES/MUSIC.mes").string())));
    scene.advance_to_next_text();
    CHECK(scene.last_error().empty() && scene.dalsegno_music_active());
    CHECK(scene.dalsegno_music_titles().size() == 42);
    CHECK(scene.dalsegno_music_titles().front() == "Main Theme - A Beginning");
    CHECK(scene.dalsegno_music_titles().back() == "The Future Goes On");
    const auto click = [&](const std::string& action) {
        auto b = std::find_if(scene.buttons().begin(),scene.buttons().end(),[&](const auto& b) { return b.action == action; });
        CHECK(b != scene.buttons().end()); if (b == scene.buttons().end()) return;
        const auto* hit = scene.hit_test(b->x+b->w/2,b->y+b->h/2);
        CHECK(hit && *hit == action);
        if (!scene.handle_dalsegno_action(action)) { CHECK(scene.jump_to_label(action)); scene.advance_to_next_text(); }
        if (!scene.last_error().empty()) std::cerr << action << ": " << scene.last_error() << '\n';
        CHECK(scene.last_error().empty());
    };
    const auto scroll = [&](int offset) {
        CHECK(scene.handle_slider_touch(0,1135,255+offset*10));
        CHECK(scene.handle_slider_touch(1,1135,255+offset*10));
        CHECK(scene.dalsegno_music_scroll() == offset);
    };
    const auto capture = [&](const std::string& name) {
        scene.set_pointer(-1,-1,false); scene.draw(-1);
        if (const char* directory = std::getenv("DS_UI_CAPTURE")) {
            std::ofstream out(fs::path(directory)/(name+".ppm"),std::ios::binary);
            out << "P6\n1280 720\n255\n";
            for (size_t i=0;i<scene.frame().size();i+=4) out.write(reinterpret_cast<const char*>(scene.frame().data()+i),3);
        }
    };
    capture("music-idle");
    std::vector<int16_t> pcm(44100*2);
    for (int track=0;track<42;++track) {
        const int offset = std::min(track,36); scroll(offset);
        click(":dsmusic"+std::to_string(track-offset));
        CHECK(scene.dalsegno_music_track() == track);
        CHECK(audio.channel_playing(0) && audio.channel_duration_seconds(0) > 1);
        audio.seek_channel_fraction(0,.25); audio.render_pcm(pcm.data(),44100);
        CHECK(std::any_of(pcm.begin(),pcm.end(),[](int16_t sample) { return sample != 0; }));
        click("stop"); CHECK(!audio.channel_playing(0));
        click("play"); CHECK(audio.channel_playing(0));
        std::cout << "Music track " << track << " play/stop/restart passed\n";
    }
    capture("music-last-page");
    click("alloff");
    for (int i=0;i<42;++i) CHECK(!scene.dalsegno_music_checked(i));
    click("listplay"); CHECK(scene.variable(3,29) == 0);
    scroll(29); click("flag01"); click("flag03");
    CHECK(scene.dalsegno_music_checked(29) && scene.dalsegno_music_checked(31));
    click("listplay"); CHECK(scene.variable(3,29) == 1);
    click("allloop"); click(":dsmusic0");
    CHECK(scene.variable(9,30) == 1 && scene.dalsegno_music_track() == 29);
    click("next"); CHECK(scene.dalsegno_music_track() == 31);
    click("next"); CHECK(scene.dalsegno_music_track() == 29);
    click("back"); CHECK(scene.dalsegno_music_track() == 31);
    audio.seek_channel_fraction(0,.999999); audio.render_pcm(pcm.data(),1024);
    CHECK(!audio.channel_playing(0)); scene.update(16);
    CHECK(audio.channel_playing(0) && scene.dalsegno_music_track() == 29);
    click("oneloop");
    audio.seek_channel_fraction(0,.999999); audio.render_pcm(pcm.data(),1024); scene.update(16);
    CHECK(audio.channel_playing(0) && scene.dalsegno_music_track() == 29);
    click("ramloop");
    for (int n=0;n<8;++n) { click("next"); CHECK(scene.dalsegno_music_track() == (n%2==0?31:29)); }
    click("allon"); for (int i=0;i<42;++i) CHECK(scene.dalsegno_music_checked(i));
    click("alloff"); CHECK(scene.variable(3,29) == 0);
    scene.set_variable(99,7,40); click("musicvol++"); CHECK(scene.variable(99,7) == 41 && audio.user_volume(dc4::AudioGroup::Bgm) == 41);
    click("musicvol--"); CHECK(audio.user_volume(dc4::AudioGroup::Bgm) == 40);
    scene.set_variable(99,207,40); click("movievol++"); CHECK(scene.variable(99,207) == 41);
    click("movievol--"); CHECK(scene.variable(99,207) == 40);
    for (int x : {398,794}) {
        CHECK(scene.handle_slider_touch(0,x,680)); CHECK(scene.handle_slider_touch(1,x+236,680));
        CHECK(scene.variable(99,x==398?7:207) == 99);
        CHECK(scene.handle_slider_touch(0,x+235,680)); CHECK(scene.handle_slider_touch(1,x-200,680));
        CHECK(scene.variable(99,x==398?7:207) == 0);
    }
    capture("music-muted");
    scene.set_variable(99,7,80); scene.set_variable(99,207,80); click("allon");
    capture("music-playlist");
    CHECK(scene.dalsegno_back_action() == "return"); click("return");
    CHECK(!scene.dalsegno_music_active());
}

void dalsegno_native_save_regressions(const fs::path& data) {
    auto read_file=[](const fs::path& path) {
        std::ifstream in(path,std::ios::binary);
        if (!in) throw std::runtime_error("missing native save fixture");
        return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),{});
    };
    const auto root=fs::temp_directory_path()/"arisa32-dalsegno-video-20261121"/"native-save-tests";
    fs::create_directories(root);
    dc4scene::FontRenderer font;
    CHECK(font.load("C:/Windows/Fonts/arial.ttf",0,24));
    for (const auto& entry:fs::directory_iterator(data/"savedata")) {
        const auto filename=dc4::save_lower(entry.path().filename().string());
        if (filename.rfind("dsao",0)!=0 || entry.path().extension()!=".dat") continue;
        dc4scene::SceneRunner scene(data.string()); scene.set_font(&font);
        if (!scene.load_state(entry.path().string())) { std::cerr<<filename<<": "<<scene.last_error()<<'\n'; CHECK(false); continue; }
        for (int n=0;n<30 && scene.current_line().empty() && scene.waiting_for_time();++n) {
            scene.finish_script_wait(); scene.advance_to_next_text();
        }
        scene.draw(-1); const auto frame=scene.frame(); const auto line=scene.current_line();
        std::vector<uint8_t> native; std::string error;
        if (!scene.capture_pc_save(&native,&error)) { std::cerr<<filename<<" export: "<<error<<'\n'; CHECK(false); continue; }
        CHECK(native.size()==0x11AF8);
        CHECK(std::all_of(native.begin(),native.begin()+8,[](uint8_t b){return b==0;}));
        dc4scene::SceneRunner imported(data.string()); imported.set_font(&font);
        if (!imported.restore_pc_save(native,&error)) { std::cerr<<filename<<" reimport: "<<error<<'\n'; CHECK(false); continue; }
        imported.draw(-1); CHECK(imported.current_line()==line); CHECK(imported.frame()==frame);
        const auto target=root/"DSAO03.dat";
        CHECK(scene.save_state(target.string()));
        CHECK(fs::exists(root/"DSAO03.arisa.dat")); CHECK(fs::exists(root/"DSAO00.dat"));
        dc4scene::SceneRunner companion(data.string()); companion.set_font(&font);
        CHECK(companion.load_state(target.string())); companion.draw(-1); CHECK(companion.frame()==frame);
        const auto unchanged=companion.current_line(); auto damaged=native; damaged[0x844]=255; damaged[0x845]=255;
        CHECK(!companion.restore_pc_save(damaged,&error)); CHECK(companion.current_line()==unchanged);
        CHECK(!companion.restore_pc_save(std::vector<uint8_t>(native.begin(),native.end()-1),&error));
        companion.set_use_system_menu(false);
        CHECK(companion.open_dalsegno_config());
        for (int i = 0; i < 16; ++i) CHECK(!companion.sprite(i).visible);
        CHECK(companion.resume_dalsegno_config());
        companion.draw(-1); CHECK(companion.frame() == frame);
        CHECK(companion.load_script(dc4::resolve_asset((data/"MES/main.mes").string())));
        for (int i = 0; i < 16; ++i) CHECK(!companion.sprite(i).visible);
        companion.advance_to_next_text(); companion.draw(-1);
        std::cout<<filename<<" PC/Arisa round trip checked\n";
    }
    dc4scene::SceneRunner profile(data.string()); CHECK(profile.load_script((data/"MES/main.mes").string()));
    std::string error;
    CHECK(profile.load_dalsegno_system((data/"savedata/system.dat").string(),&error));
    profile.set_variable(99,7,23); profile.set_variable(99,207,37);
    profile.set_variable(94,2,0); profile.set_variable(95,2,0); profile.set_variable(100,105,1);
    CHECK(profile.save_dalsegno_profile(root.string(),&error));
    dc4scene::SceneRunner restored(data.string()); CHECK(restored.load_script((data/"MES/main.mes").string()));
    CHECK(restored.load_dalsegno_profile((root/"system-profile.dat").string(),&error));
    CHECK(restored.variable(99,7)==23 && restored.variable(99,207)==37);
    CHECK(restored.variable(94,2)==0 && restored.variable(95,2)==0 && restored.variable(100,105)==1);
    CHECK(restored.load_dalsegno_system((root/"system.dat").string(),&error));
    CHECK(restored.variable(99,7)==23 && restored.variable(99,207)==37);
    const auto system=read_file(root/"system.dat"), original=read_file(data/"savedata/system.dat");
    CHECK(system.size()==0x4D18);
    for (int i=0;i<=50;++i) CHECK(std::equal(system.begin()+0x4528+i*40,system.begin()+0x4548+i*40,original.begin()+0x4528+i*40));
    auto damaged=read_file(root/"system-profile.dat"); damaged.resize(20); write_file(root/"profile-bad.dat",damaged);
    CHECK(!restored.load_dalsegno_profile((root/"profile-bad.dat").string(),&error)); CHECK(restored.variable(99,7)==23);
    std::cout<<"Dal Segno settings and native system overlay checked\n";
}

void dalsegno_identity_regressions(const fs::path& root) {
    const auto data = root / "renamed/MES";
    const std::vector<uint8_t> script{0,0,0,0,0x9A,0x9F,0,0};
    write_file(data / "main.mes", script);
    dc4scene::SceneRunner unverified(data.parent_path().string());
    CHECK(!unverified.load_script((data / "main.mes").string()));
    CHECK(unverified.last_error().find("verified") != std::string::npos);
    write_file(data / "DS_1129B_CM.mes", script);
    dc4scene::SceneRunner verified(data.parent_path().string());
    CHECK(verified.load_script((data / "main.mes").string()));
    write_file(data / "DSIF_0101.mes", script);
    dc4scene::SceneRunner future(data.parent_path().string());
    CHECK(!future.load_script((data / "main.mes").string()));
    CHECK(!future.load_ui_script((data / "main.mes").string()));
    CHECK(future.last_error().find("separate game profile") != std::string::npos);
}

int main(int argc, char** argv) {
    try {
        const fs::path root = fs::current_path() / "scene-test-data" /
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        if (argc > 2 && std::string(argv[1]) == "--dalsegno-native-save") {
            dalsegno_native_save_regressions(argv[2]);
            std::cout<<(failures?"Dal Segno native saves FAILED\n":"Dal Segno native saves passed\n");
            return failures?1:0;
        }
        if (argc > 2 && std::string(argv[1]) == "--dalsegno-music") {
            dalsegno_music_regressions(argv[2]);
            std::cout << (failures ? "Dal Segno music FAILED\n" : "Dal Segno music passed\n");
            return failures ? 1 : 0;
        }
        if (argc > 2 && std::string(argv[1]) == "--dalsegno-transitions") {
            dalsegno_transition_regressions(argv[2]);
            return failures ? 1 : 0;
        }
        if (argc > 2 && std::string(argv[1]) == "--dalsegno") {
            dalsegno_transition_regressions(argv[2]);
            dalsegno_game_regressions(argv[2]);
            std::cout << (failures ? "Dal Segno regressions FAILED\n" : "Dal Segno regressions passed\n");
            return failures ? 1 : 0;
        }
        if (argc > 1 && std::string(argv[1]) == "--dc4-sprites") {
            dc4_sprite_priority_regressions(root);
            dc4_implicit_sprite_pivot_regressions(root);
            dc4_cutin_visibility_regressions(root);
            std::cout << (failures ? "DC4 sprite regressions FAILED\n" : "DC4 sprite regressions passed\n");
            return failures ? 1 : 0;
        }
        fixture_round_trip(root); graphics_and_text_regressions(root); chip_regressions(root); animation_regressions(root); memory_snapshot_regressions(root); font_regression();
        dalsegno_identity_regressions(root);
        dialogue_size_regressions(root);
        dc2pc_dialogue_regressions(root);native_save_regressions(root);native_sprite_placement_regressions(root);
        dc4_sprite_priority_regressions(root);
        dc4_implicit_sprite_pivot_regressions(root);
        dc4_cutin_visibility_regressions(root);
        dc2dm_scene_regressions(root);
        sakuramusubi_text_regressions(root);
        if (const char* data = std::getenv("DC2DM_DATA")) dc2dm_game_regressions(data, root);
        if (argc > 1) game_round_trip(argv[1], root);
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; ++failures; }
    std::cout << (failures ? "Scene regressions FAILED\n" : "Scene regressions passed\n");
    return failures ? 1 : 0;
}
