#include "arisa32/scene.h"
#include "arisa32/resource.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace fs = std::filesystem;
namespace {
int failures = 0;
void check(bool ok, const char* expression, int line) {
    if (!ok) { std::cerr << line << ": " << expression << '\n'; ++failures; }
}
#define CHECK(value) check(bool(value), #value, __LINE__)
int px(int x) { return 160 + int(std::lround(x * 1.2)); }
int py(int y) { return int(std::lround(y * 1.2)); }
void write_file(const fs::path& path, const std::vector<uint8_t>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    if (!out) throw std::runtime_error("Cannot write choice fixture");
}
struct Script {
    std::vector<uint8_t> bytes{1,0,0,0,2,0,0,0,0x69,0x5F};
    explicit Script(bool dc2ty = true) { if (!dc2ty) bytes[9] = 0x99; }
    void word(uint16_t value) { bytes.push_back(uint8_t(value)); bytes.push_back(uint8_t(value >> 8)); }
    void pair(uint8_t op, uint8_t a = 0, uint8_t b = 0) { bytes.insert(bytes.end(), {op,a,b}); }
    void words(uint8_t op, int a, int b, int c, int d) {
        bytes.push_back(op); word(uint16_t(a)); word(uint16_t(b)); word(uint16_t(c)); word(uint16_t(d));
    }
    void string(uint8_t op, const std::string& value) {
        bytes.push_back(op);
        if (op >= 0x29 && op < 0x2F) bytes.push_back(0);
        for (unsigned char ch : value)
            bytes.push_back(op >= 0x4A && op < 0x4E ? uint8_t((ch == ' ' ? 0x7F : ch) - 0x20) : ch);
        bytes.push_back(0);
    }
};
void synthetic_ui(const fs::path& root) {
    std::vector<uint8_t> image{'C','R','X','G',0,0,0,0,0xE0,1,0xF2,1,1,0,0,0,0,0,0,0};
    std::vector<uint8_t> pixels(480u * 498u * 3u, 0);
    for (int y = 378; y < 498; ++y) for (int x = 0; x < 480; ++x) {
        const size_t at = (size_t(y) * 480 + x) * 3;
        pixels[at] = 100; pixels[at + 1] = uint8_t(y >= 458 ? 180 : 40); pixels[at + 2] = 40;
    }
    for (size_t at = 0; at < pixels.size(); at += 8) {
        image.push_back(255);
        image.insert(image.end(), pixels.begin() + at, pixels.begin() + std::min(at + 8, pixels.size()));
    }
    write_file(root / "GRP/choice.CRX", image);
    Script ui;
    ui.string(0x2A, "choice");
    ui.words(0x4F, 0, 580, 1, 581); ui.string(0x3A, ":n"); ui.pair(0x17);
    for (int row = 0; row < 4; ++row) {
        const int y = 100 + row * 56;
        ui.words(0x4F, 136, y, 616, y + 40);
        ui.string(0x3A, row == 3 ? ":3" : ":" + std::to_string(row + 1));
        for (int state : {9,1,2}) {
            ui.words(0x50, 136, y, 136, y - 378); ui.words(0x52, 480, 40, 37, state);
            if (state == 1) {
                ui.words(0x50, 136, y, 136, y - 458); ui.words(0x52, 480, 40, 37, state);
            }
            ui.words(0x51, 182, y + 4, 135, state);
        }
    }
    ui.pair(0x17); ui.bytes.push_back(0);
    write_file(root / "MES/start.mes", ui.bytes);
}
struct Bounds { int left=1280, top=720, right=-1, bottom=-1, count=0; };
Bounds ink(const std::vector<uint8_t>& frame, const std::vector<uint8_t>& background, int top, int bottom) {
    Bounds result;
    for (int y = top; y < bottom; ++y) for (int x = 0; x < 1280; ++x) {
        const size_t at = (size_t(y) * 1280 + x) * 4;
        if (std::equal(frame.begin() + at, frame.begin() + at + 3, background.begin() + at)) continue;
        if (*std::min_element(frame.begin() + at, frame.begin() + at + 3) < 180) continue;
        result.left = std::min(result.left, x); result.right = std::max(result.right, x);
        result.top = std::min(result.top, y); result.bottom = std::max(result.bottom, y); ++result.count;
    }
    return result;
}
struct Fixture {
    fs::path root, data, captures;
    dc4scene::FontRenderer* font = nullptr;
    std::string name;
    std::unique_ptr<dc4scene::SceneRunner> load(const std::vector<std::string>& labels, bool dc2ty = true) {
        Script script(dc2ty);
        script.string(0x4C, "QUESTION"); script.pair(0x08, 1);
        for (size_t i = 0; i < labels.size(); ++i) {
            script.string(0x44, "branch" + std::to_string(i)); script.string(0x4D, labels[i]);
        }
        script.pair(0x09);
        for (size_t i = 0; i < labels.size(); ++i) {
            script.string(0x35, "branch" + std::to_string(i)); script.string(0x4C, "BRANCH" + std::to_string(i));
        }
        script.bytes.push_back(0);
        const auto path = root / "MES" / (dc2ty ? "choices.mes" : "dc2pc.mes");
        write_file(path, script.bytes);
        dc4::configure_resources(root.string(), data == root ? "" : data.string(), "utf-8");
        dc4::set_language(dc4::Language::Japanese);
        auto scene = std::make_unique<dc4scene::SceneRunner>(root.string());
        scene->set_font(font);
        CHECK(scene->load_ui_script((data / "MES/start.mes").string()));
        CHECK(scene->load_script(path.string()));
        CHECK(scene->advance_to_next_text());
        CHECK(!scene->advance_to_next_text()); CHECK(scene->waiting_for_choice());
        CHECK(scene->choices().size() == labels.size());
        scene->set_message_visible(false); scene->set_wait_marker(false);
        return scene;
    }
    std::vector<uint8_t> frame(dc4scene::SceneRunner& scene, dc4scene::FontRenderer* face) {
        scene.set_font(face); scene.draw(-1); return scene.frame();
    }
    void capture(const std::string& label, const std::vector<uint8_t>& pixels) {
        if (captures.empty()) return;
        fs::create_directories(captures);
        std::ofstream out(captures / (name + "-" + label + ".ppm"), std::ios::binary);
        out << "P6\n1280 720\n255\n";
        for (size_t at = 0; at < pixels.size(); at += 4) out.write(reinterpret_cast<const char*>(pixels.data() + at), 3);
        if (!out) throw std::runtime_error("Cannot write choice capture");
    }
};
const std::vector<std::string> captions{u8"○ Panggil Bos", u8"○ Panggil Pangeran", u8"○ Panggil Shinemon-san",
    u8"○ Walaupun sudah sangat terlambat, panggil semua orang untuk membicarakan rencana pernikahan bersama-sama"};
void interactions(Fixture& fixture) {
    for (int count = 1; count <= 4; ++count) {
        auto scene = fixture.load({captions.begin(), captions.begin() + count});
        std::vector<uint8_t> saved; CHECK(scene->capture_state(&saved));
        scene->set_pointer(-1, -1, false);
        const auto normal = fixture.frame(*scene, fixture.font);
        if (count == 4) fixture.capture("normal", normal);
        for (int row = 0; row < count; ++row) {
            const int y = 100 + row * 56;
            CHECK(scene->choice_at(px(136), py(y)) == row);
            CHECK(scene->choice_at(px(616) - 1, py(y + 40) - 1) == row);
            CHECK(scene->choice_at(px(136) - 1, py(y)) == -1);
            CHECK(scene->choice_at(px(616), py(y)) == -1);
            CHECK(scene->choice_at(px(200), py(y + 40)) == -1);
            scene->set_pointer(px(200), py(y + 20), false);
            const auto hovered = fixture.frame(*scene, fixture.font); CHECK(hovered != normal);
            scene->set_pointer(px(200), py(y + 20), true);
            const auto pressed = fixture.frame(*scene, fixture.font); CHECK(pressed == normal);
            CHECK(scene->waiting_for_choice() && scene->current_line() == "QUESTION");
            if (count == 4 && row == 2) { fixture.capture("hover", hovered); fixture.capture("pressed", pressed); }
            scene->set_pointer(px(200), py(y + 20), false);
            CHECK(fixture.frame(*scene, fixture.font) == hovered);
            CHECK(scene->choose(size_t(row))); CHECK(!scene->choose(size_t(row)));
            CHECK(!scene->waiting_for_time()); CHECK(scene->advance_to_next_text());
            CHECK(scene->current_line() == "BRANCH" + std::to_string(row));
            CHECK(scene->choice_at(px(200), py(y + 20)) == -1);
            CHECK(scene->restore_state(saved)); scene->set_pointer(-1, -1, false);
            CHECK(fixture.frame(*scene, fixture.font) == normal);
        }
        CHECK(scene->choice_at(px(200), py(100 + count * 56 + 20)) == -1);
    }
}
void typography(Fixture& fixture) {
    auto scene = fixture.load(captions);
    const auto frame = fixture.frame(*scene, fixture.font), background = fixture.frame(*scene, nullptr);
    for (int row = 0; row < 4; ++row) {
        const int y = 100 + row * 56;
        const auto bounds = ink(frame, background, py(y), py(y + 40));
        CHECK(bounds.count > 0);
        CHECK(bounds.left >= px(148) && bounds.left <= px(148) + 2);
        CHECK(bounds.right < px(604));
        CHECK(bounds.bottom - bounds.top + 1 <= py(28));
        CHECK(std::abs(bounds.top + bounds.bottom + 1 - py(y) - py(y + 40)) <= 3);
    }
    auto normalized = fixture.load({u8"○ Panggil Bos"});
    const auto expected = fixture.frame(*normalized, fixture.font);
    for (const std::string label : {u8"○Panggil Bos", u8"　○　Ｐａｎｇｇｉｌ　　Ｂｏｓ　", u8"○\tPanggil\r\nBos"}) {
        auto variant = fixture.load({label}); CHECK(fixture.frame(*variant, fixture.font) == expected);
    }
    const std::vector<std::string> japanese{u8"○素直にお祝いする", u8"○少し冗談を言ってみる"};
    auto original = fixture.load(japanese, false), dc2ty = fixture.load(japanese);
    CHECK(fixture.frame(*original, fixture.font) == fixture.frame(*dc2ty, fixture.font));
    fixture.capture("japanese", fixture.frame(*dc2ty, fixture.font));
    original->set_pointer(px(200), py(120), false); const auto hover = fixture.frame(*original, fixture.font);
    original->set_pointer(px(200), py(120), true); CHECK(fixture.frame(*original, fixture.font) == hover);
}
}
int main(int argc, char** argv) {
    const auto root = fs::temp_directory_path() / ("arisa32-dc2ty-choices-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        synthetic_ui(root);
        const char* installed = argc > 1 ? argv[1] : std::getenv("DC2TY_DATA");
        const fs::path data = installed && *installed ? fs::path(installed) : root;
        const fs::path captures = argc > 3 ? argv[3] : "";
        const auto fonts = fs::path(__FILE__).parent_path().parent_path() / "android/app/src/main/assets/fonts";
        const std::vector<fs::path> faces = argc > 2 ? std::vector<fs::path>{argv[2]} :
            std::vector<fs::path>{fonts / "MSGOTHIC.TTC", fonts / "PretendardJP-Bold.ttf"};
        Fixture fixture{root, data, captures};
        interactions(fixture);
        for (const auto& path : faces) {
            dc4scene::FontRenderer font;
            CHECK(font.load(path.string(), 0, 22, false));
            fixture.font = &font; fixture.name = path.stem().string();
            interactions(fixture); typography(fixture);
            std::cout << "Checked DC2TY choices with " << fixture.name << '\n';
        }
        dc4::configure_resources({}); fs::remove_all(root);
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; ++failures; }
    std::cout << "DC2TY choice checks: " << failures << " failure(s)\n";
    return failures ? 1 : 0;
}
