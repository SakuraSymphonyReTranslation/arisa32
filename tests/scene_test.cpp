#include "arisa32/scene.h"
#include <algorithm>
#include <chrono>
#include <cmath>
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
int main(int argc, char** argv) {
    try {
        const fs::path root = fs::current_path() / "scene-test-data" /
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        fixture_round_trip(root); graphics_and_text_regressions(root); chip_regressions(root); animation_regressions(root); memory_snapshot_regressions(root); font_regression();
        dc2pc_dialogue_regressions(root);native_save_regressions(root);native_sprite_placement_regressions(root);
        if (argc > 1) game_round_trip(argv[1], root);
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; ++failures; }
    std::cout << (failures ? "Scene regressions FAILED\n" : "Scene regressions passed\n");
    return failures ? 1 : 0;
}
