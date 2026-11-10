#include "arisa32/scene.h"
#include "arisa32/resource.h"

#include <algorithm>
#include <array>
#include <chrono>
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
constexpr Color black{0, 0, 0}, red{200, 20, 30}, blue{30, 40, 210};

void write_file(const fs::path& path, const std::vector<uint8_t>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    if (!out) throw std::runtime_error("Cannot write eyecatch fixture");
}

void capture(const std::vector<uint8_t>& pixels, const fs::path& path) {
    if (path.empty()) return;
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << "P6\n1280 720\n255\n";
    for (size_t i = 0; i < pixels.size(); i += 4)
        out.write(reinterpret_cast<const char*>(pixels.data() + i), 3);
    if (!out) throw std::runtime_error("Cannot capture eyecatch frame");
}

Color pixel(const dc4scene::SceneRunner& scene, int native_x, int native_y) {
    const int x = 160 + native_x * 6 / 5, y = native_y * 6 / 5;
    const auto* p = scene.frame().data() + (size_t(y) * 1280 + x) * 4;
    return {p[0], p[1], p[2]};
}

struct Script {
    std::vector<uint8_t> bytes{1,0,0,0,2,0,0,0,0x69,0x5F};
    void param(uint8_t op, uint8_t a, uint8_t b = 0) { bytes.insert(bytes.end(), {op,a,b}); }
    void words(uint8_t op, uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
        bytes.push_back(op);
        for (uint16_t n : {a,b,c,d}) { bytes.push_back(uint8_t(n)); bytes.push_back(uint8_t(n >> 8)); }
    }
    void str(uint8_t op, const std::string& value) {
        bytes.push_back(op);
        for (unsigned char ch : value) bytes.push_back(op == 0x4C ? uint8_t(ch - 0x20) : ch);
        bytes.push_back(0);
    }
    void bg(const char* name) { str(0x30, std::string("bg\\") + name); }
    void text(const char* value) { str(0x4C, value); }
    void begin() { bg("red"); param(0x0D,5); text("BEFORE"); }
    void save(const fs::path& path) const { auto data = bytes; data.push_back(0); write_file(path, data); }
};

constexpr Color green{20,180,50}, yellow{240,180,20};
void make_image(const fs::path& path, int width, int height, Color color, bool atlas = false) {
    std::vector<uint8_t> bytes{'C','R','X','G',0,0,0,0};
    for (uint16_t n : {uint16_t(width),uint16_t(height),uint16_t(1),uint16_t(0),uint16_t(0),uint16_t(0)}) {
        bytes.push_back(uint8_t(n)); bytes.push_back(uint8_t(n >> 8));
    }
    std::vector<uint8_t> pixels;
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        const auto value = !atlas ? color : x >= 160 && x < 192 && y < 24 ? red :
            x >= 48 && x < 72 && y >= 72 ? yellow : green;
        pixels.insert(pixels.end(), {value[2],value[1],value[0]});
    }
    for (size_t i = 0; i < pixels.size(); i += 8) {
        bytes.push_back(255);
        bytes.insert(bytes.end(), pixels.begin() + i, pixels.begin() + std::min(i + 8, pixels.size()));
    }
    write_file(path, bytes);
}

std::unique_ptr<dc4scene::SceneRunner> load(const fs::path& root, const Script& script, bool begin = true) {
    script.save(root / "MES/test.mes");
    auto scene = std::make_unique<dc4scene::SceneRunner>(root.string());
    CHECK(scene->load_ui_script((root / "MES/start.mes").string()));
    CHECK(scene->load_script((root / "MES/test.mes").string()));
    scene->set_message_visible(false);
    scene->set_animation_enabled(true);
    scene->set_variable(99,12,0);
    scene->set_variable(99,20,1);
    if (begin) { CHECK(scene->advance_to_next_text()); CHECK(scene->current_line() == "BEFORE"); }
    scene->draw(-1);
    return scene;
}

void composition(const fs::path& root) {
    Script script; script.begin();
    script.bg("blue"); script.str(0x31,"system\\chips");
    script.words(0x53,100,300,160,0); script.words(0x52,192,24,6,1);
    script.words(0x53,228,300,48,72); script.words(0x52,72,96,6,1);
    script.param(0x12,0,1);
    script.bg("red"); script.param(0x0D,5); script.text("AFTER");
    for (bool effects : {false,true}) {
        auto scene = load(root,script); scene->set_effects_enabled(effects);
        std::vector<uint8_t> saved; CHECK(scene->capture_state(&saved));
        CHECK(!scene->advance_to_next_text() && scene->waiting_for_time()); scene->draw(-1);
        CHECK(pixel(*scene,400,300) == blue);
        CHECK(pixel(*scene,110,310) == red && pixel(*scene,140,310) == blue);
        CHECK(pixel(*scene,240,310) == yellow && pixel(*scene,240,330) == blue);
        const auto frame = scene->frame(); const auto position = scene->script_position();
        scene->update(60000); scene->draw(-1);
        CHECK(scene->waiting_for_time() && scene->frame() == frame && scene->script_position() == position);
        CHECK(!scene->advance_to_next_text() && scene->script_position() == position);
        scene->finish_script_wait();
        CHECK(scene->advance_to_next_text() && scene->current_line() == "AFTER"); scene->draw(-1);
        CHECK(pixel(*scene,240,310) == red);
        CHECK(scene->restore_state(saved)); scene->draw(-1);
        CHECK(pixel(*scene,400,300) == red && !scene->waiting_for_time());
    }
}

void black_and_unchanged_waits(const fs::path& root) {
    Script fresh; fresh.param(0x12,0,1); fresh.text("AFTER");
    auto first = load(root,fresh,false);
    CHECK(!first->advance_to_next_text()); first->draw(-1);
    CHECK(pixel(*first,400,300) == black);
    for (int mode : {2,14}) {
        Script script; script.begin(); script.bg("blue"); script.param(0x0D,uint8_t(mode),4);
        script.param(0x12,1,0); script.bg("blue"); script.param(0x12,0,1); script.text("AFTER");
        auto scene = load(root,script);
        CHECK(!scene->advance_to_next_text()); scene->update(60000); scene->draw(-1);
        CHECK(pixel(*scene,400,300) == black);
        CHECK(!scene->advance_to_next_text()); scene->draw(-1);
        CHECK(pixel(*scene,400,300) == black && scene->waiting_for_time());
        scene->finish_script_wait(); CHECK(scene->waiting_for_time());
        scene->update(199); CHECK(scene->waiting_for_time()); scene->update(1);
        CHECK(!scene->advance_to_next_text()); scene->draw(-1);
        CHECK(pixel(*scene,400,300) == blue && scene->waiting_for_time());
    }
    Script unchanged; unchanged.begin(); unchanged.param(0x12,0,1); unchanged.text("AFTER");
    auto scene = load(root,unchanged);
    CHECK(scene->show_message_window("system/chips.CRX")); scene->set_message_visible(true); scene->draw(-1);
    const auto dialogue = scene->frame();
    CHECK(!scene->advance_to_next_text()); scene->draw(-1);
    CHECK(scene->frame() == dialogue);
}

void timer_publication(const fs::path& root) {
    Script arm; arm.begin(); arm.bg("blue"); arm.param(0x12,0,4); arm.param(0x0D,2); arm.text("AFTER");
    auto scene = load(root,arm);
    CHECK(!scene->advance_to_next_text()); scene->draw(-1);
    CHECK(scene->animating() && pixel(*scene,400,300) == red);

    for (int elapsed : {75,300}) {
        Script reference; reference.begin(); reference.param(0x12,0,4); reference.text("ARMED");
        reference.bg("blue"); reference.param(0x12,1,5);
        reference.bg("red"); reference.param(0x0D,2); reference.text("AFTER");
        auto timed = load(root,reference);
        CHECK(timed->advance_to_next_text() && timed->current_line() == "ARMED"); timed->update(elapsed);
        CHECK(!timed->advance_to_next_text()); timed->draw(-1);
        CHECK(pixel(*timed,400,300) == blue);
        if (elapsed == 75) {
            CHECK(!timed->animating()); timed->finish_script_wait(); CHECK(timed->waiting_for_time());
            timed->update(124); CHECK(timed->waiting_for_time()); timed->update(1);
            CHECK(!timed->advance_to_next_text()); timed->draw(-1);
            CHECK(pixel(*timed,400,300) == blue);
        }
        CHECK(timed->animating());
        timed->update(320); timed->draw(-1);
        const Color half{15,20,105}; CHECK(pixel(*timed,400,300) == half);
    }
}

void profile_isolation(const fs::path& root) {
    Script pc; pc.bytes[9] = 0x99; pc.begin(); pc.bg("blue"); pc.param(0x12,0,1); pc.text("AFTER");
    auto scene = load(root,pc);
    CHECK(!scene->advance_to_next_text()); scene->draw(-1);
    CHECK(pixel(*scene,400,300) == red);
}

void synthetic() {
    const auto root = fs::temp_directory_path() / ("arisa32-dc2ty-eyecatch-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Script{}.save(root / "MES/start.mes");
    make_image(root / "GRP/bg/red.CRX",800,600,red);
    make_image(root / "GRP/bg/blue.CRX",800,600,blue);
    make_image(root / "GRP/system/chips.CRX",512,96,black,true);
    make_image(root / "GRP/system/eff.CRX",800,32,black);
    dc4::configure_resources(root.string(), "", "utf-8");
    composition(root); black_and_unchanged_waits(root); timer_publication(root); profile_isolation(root);
    fs::remove_all(root);
}

void original_card(const fs::path& data, const fs::path& output, const std::string& name) {
    dc4::configure_resources(data.string());
    dc4scene::SceneRunner scene(data.string());
    CHECK(scene.load_sheets((data / "MES/start.mes").string()));
    CHECK(scene.load_ui_script((data / "MES/start.mes").string()));
    CHECK(scene.load_script((data / "MES" / name).string()));
    scene.set_animation_enabled(true);
    scene.set_message_visible(false);
    std::string background;
    bool card_wait = false;
    int chips = 0;
    scene.set_observer([&](const dc4::MesCommand& c, const std::string&) {
        if (c.op == 0x30) { background = c.str; chips = 0; }
        if (c.op == 0x52 && background.find("catchy_") != std::string::npos) ++chips;
        card_wait = c.op == 0x12 && c.p1 == 0 && c.p2 == 1 &&
                    background.find("catchy_") != std::string::npos;
    });
    for (int i = 0; i < 500 && !scene.scene_ended() && !card_wait; ++i) {
        if (scene.waiting_for_time()) scene.update(60000);
        if (scene.waiting_for_time()) scene.finish_script_wait();
        if (!scene.pending_movie().empty()) scene.finish_movie();
        scene.advance_to_next_text();
    }
    CHECK(card_wait && scene.waiting_for_time() && scene.last_error().empty());
    scene.draw(-1);
    const auto card = scene.frame();
    const auto position = scene.script_position();
    if (!output.empty()) capture(card, output / (fs::path(name).stem().string() + ".ppm"));
    size_t visible = 0;
    for (size_t i = 0; i < card.size(); i += 4)
        visible += card[i] != 0 || card[i + 1] != 0 || card[i + 2] != 0;
    std::cout << name << ": " << background << ", " << chips << " chips, "
              << visible << " visible pixels, date " << scene.variable(8, 13) << '/'
              << scene.variable(8, 12) << ", weekday " << scene.variable(8, 14) << '\n';
    CHECK(visible > 600000);
    CHECK(chips == (scene.variable(8, 13) ? 5 : 1));
    scene.update(60000); scene.draw(-1);
    CHECK(scene.waiting_for_time() && scene.script_position() == position && scene.frame() == card);
    scene.finish_script_wait();
    CHECK(!scene.waiting_for_time());
    scene.advance_to_next_text();
    CHECK(scene.script_position() > position);
    scene.update(60000); scene.draw(-1);
    CHECK(scene.frame() != card);
}
}

int main(int argc, char** argv) {
    try {
        synthetic();
        if (argc > 1) {
            const fs::path output = argc > 2 ? argv[2] : "";
            if (argc > 3) original_card(argv[1], output, argv[3]);
            else for (const char* name : {"ty_mhr02_b1_mhr.mes", "ty_myu01_d1_myu.mes",
                     "hyd_0611_a2_com.mes", "set_0616_a1_com.mes",
                     "ty_mhr01_d1_mhr.mes", "ty_myu09_d1_myu.mes"})
                original_card(argv[1], output, name);
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; ++failures;
    }
    std::cout << "DC2TY eyecatch failures: " << failures << '\n';
    return failures ? 1 : 0;
}
