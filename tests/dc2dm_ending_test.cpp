#include "arisa32/scene.h"

#include <algorithm>
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
void word(std::vector<uint8_t>& bytes, uint16_t value) {
    bytes.push_back(uint8_t(value)); bytes.push_back(uint8_t(value >> 8));
}
void dword(std::vector<uint8_t>& bytes, uint32_t value) {
    word(bytes, uint16_t(value)); word(bytes, uint16_t(value >> 16));
}
void write(const fs::path& path, const std::vector<uint8_t>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    if (!file) throw std::runtime_error("cannot write ending fixture");
}
std::vector<uint8_t> image(int width, int height, int color, bool transparent = false) {
    std::vector<uint8_t> bytes{'C','R','X','G',0,0,0,0};
    word(bytes, uint16_t(width)); word(bytes, uint16_t(height));
    word(bytes, 1); word(bytes, 0); word(bytes, transparent ? 1 : 0); word(bytes, 0);
    const int channels = transparent ? 4 : 3;
    const size_t size = size_t(width) * height * channels;
    for (size_t at = 0; at < size; at += 8) {
        bytes.push_back(255);
        for (size_t i = at; i < std::min(at + 8, size); ++i)
            bytes.push_back(transparent ? (i % 4 == 0 ? 255 : 0) : i % 3 == size_t(2 - color) ? 220 : 20);
    }
    return bytes;
}
std::vector<uint8_t> archive(size_t count, const std::vector<uint8_t>& first, const std::vector<uint8_t>& rest) {
    std::vector<uint8_t> bytes;
    dword(bytes, uint32_t(count));
    const uint32_t start = uint32_t(4 + count * 8);
    for (size_t i = 0; i < count; ++i) {
        dword(bytes, start + (i ? uint32_t(first.size()) : 0));
        dword(bytes, uint32_t(i ? rest.size() : first.size()));
    }
    bytes.insert(bytes.end(), first.begin(), first.end());
    bytes.insert(bytes.end(), rest.begin(), rest.end());
    return bytes;
}
void string(std::vector<uint8_t>& bytes, uint8_t opcode, const std::string& value, bool encoded = false) {
    bytes.push_back(opcode);
    for (unsigned char ch : value) bytes.push_back(encoded ? uint8_t(ch - 0x20) : ch);
    bytes.push_back(0);
}
void script(const fs::path& path, const std::vector<uint8_t>& code) {
    std::vector<uint8_t> bytes{1,0,0,0,3,0,0,0,0,0,0x72,0x9D,0};
    bytes.insert(bytes.end(), code.begin(), code.end()); bytes.push_back(0);
    write(path, bytes);
}
void fixture_files(const fs::path& root) {
    write(root / "GRP/ED/GED.PCK", archive(16, image(512,600,0), image(512,600,1)));
    write(root / "GRP/ED/GEDANIME.PCK", archive(47, image(800,600,1), image(800,600,1)));
    const auto animation = archive(90, image(400,300,0), image(400,300,2));
    for (int i = 0; i < 15; ++i)
        write(root / ("GRP/ED/ANIME" + (i < 10 ? "0" + std::to_string(i) : std::to_string(i)) + ".PCK"), animation);
    write(root / "GRP/ED/PHOTO.CRX", image(400,300,0,true));
    std::vector<uint8_t> timer{0x1B,208,0};
    string(timer,0x4F,"BEFORE",true); string(timer,0x38,"ending");
    script(root / "MES/timer.mes",timer);
    std::vector<uint8_t> ending{0x1B,209,0};
    string(ending,0x4F,"AFTER",true); string(ending,0x34,"skip_end");
    string(ending,0x4F,"SKIPPED",true);
    script(root / "MES/ending.mes",ending);
    script(root / "MES/start.mes",{});
}
uint8_t pixel(const dc4scene::SceneRunner& scene, int x, int y, int channel) {
    return scene.frame()[(size_t(y) * 1280 + x) * 4 + channel];
}
void fixture(const fs::path& root) {
    fixture_files(root);
    dc4scene::SceneRunner scene(root.string());
    scene.set_animation_enabled(true);
    CHECK(scene.load_script((root / "MES/timer.mes").string()));
    CHECK(scene.advance_to_next_text() && scene.current_line() == "BEFORE");
    scene.update(120000);
    std::vector<uint8_t> snapshot;
    CHECK(scene.capture_state(&snapshot));
    CHECK(snapshot.size() > 24 && snapshot[8] == 6);
    CHECK(!scene.advance_to_next_text() && scene.credits_active() && scene.waiting_for_time());
    scene.draw(-1);
    CHECK(scene.last_error().empty());
    CHECK(pixel(scene,150,250,0) == 220 && pixel(scene,150,250,2) == 20);
    CHECK(pixel(scene,99,250,0) == 0 && pixel(scene,600,250,0) == 0);
    CHECK(pixel(scene,150,199,0) == 0 && pixel(scene,150,575,0) == 0);
    scene.finish_script_wait();
    CHECK(scene.credits_active() && scene.waiting_for_time());
    CHECK(!scene.capture_state(&snapshot));
    scene.update(1000); scene.draw(-1);
    CHECK(pixel(scene,150,250,2) == 220);
    CHECK(pixel(scene,800,719,0) == 220 && pixel(scene,800,700,0) == 0);
    scene.update(6553); scene.draw(-1);
    CHECK(pixel(scene,150,250,2) == 220 && pixel(scene,150,250,1) == 20);
    scene.update(134); scene.draw(-1);
    CHECK(pixel(scene,150,250,2) == 120 && pixel(scene,150,250,1) == 119);
    scene.update(355000);
    CHECK(!scene.credits_active() && scene.script_ready() && scene.variable(100,29) == 1);
    CHECK(scene.advance_to_next_text() && scene.current_line() == "AFTER");

    CHECK(scene.restore_state(snapshot));
    CHECK(scene.current_line() == "BEFORE");
    CHECK(!scene.advance_to_next_text() && scene.credits_active());
    scene.update(355000);
    CHECK(scene.credits_active());
    scene.update(1000);
    CHECK(!scene.credits_active() && scene.script_ready());

    CHECK(scene.load_script((root / "MES/ending.mes").string()));
    scene.set_variable(100,29,1);
    CHECK(!scene.advance_to_next_text() && scene.credits_active());
    scene.finish_script_wait();
    CHECK(!scene.credits_active() && !scene.waiting_for_time());
    CHECK(scene.advance_to_next_text() && scene.current_line() == "SKIPPED");

    CHECK(scene.load_script((root / "MES/ending.mes").string()));
    CHECK(!scene.advance_to_next_text());
    CHECK(scene.jump_to_label("skip_end") && !scene.credits_active());
    CHECK(scene.capture_state(&snapshot) && snapshot[8] == 5);
    CHECK(scene.load_script((root / "MES/timer.mes").string()));
    CHECK(scene.advance_to_next_text());
    scene.update(500000);
    CHECK(scene.advance_to_next_text() && scene.current_line() == "AFTER");
    CHECK(!scene.credits_active() && scene.variable(100,29) == 1);
    auto invalid_photo = image(400,300,0,true);
    invalid_photo[8] = invalid_photo[9] = 255;
    write(root / "GRP/ED/PHOTO.CRX", invalid_photo);
    CHECK(scene.load_script((root / "MES/ending.mes").string()));
    scene.advance_to_next_text();
    CHECK(!scene.credits_active() && !scene.last_error().empty());
    write(root / "GRP/ED/GED.PCK", {255,255,255,255,0,0,0,0,0,0,0,0});
    CHECK(scene.load_script((root / "MES/ending.mes").string()));
    scene.advance_to_next_text();
    CHECK(!scene.credits_active() && !scene.last_error().empty());
}
void capture(const dc4scene::SceneRunner& scene, const std::string& name) {
    const char* directory = std::getenv("DC2DM_ENDING_CAPTURES");
    if (!directory || !*directory) return;
    fs::create_directories(directory);
    std::ofstream file(fs::path(directory) / (name + ".ppm"), std::ios::binary);
    file << "P6\n1280 720\n255\n";
    for (size_t i = 0; i < scene.frame().size(); i += 4)
        file.write(reinterpret_cast<const char*>(scene.frame().data() + i), 3);
    CHECK(bool(file));
}
void installed_game(const fs::path& data) {
    dc4scene::SceneRunner scene(data.string());
    scene.set_animation_enabled(true); scene.set_use_system_menu(true);
    CHECK(scene.load_ui_script((data / "MES/start.mes").string()));
    CHECK(scene.set_sidebar_visible(true) && scene.sidebar_visible());
    CHECK(scene.set_sidebar_visible(false) && !scene.sidebar_visible());
    CHECK(scene.load_script((data / "MES/MED.mes").string()));
    CHECK(!scene.advance_to_next_text() && scene.credits_active());
    CHECK(!scene.set_sidebar_visible(true) && !scene.sidebar_visible());
    CHECK(scene.pending_movie().empty());
    for (int i = 0; i < 47; ++i) {
        const auto target = uint32_t(i * 10106 + 1000);
        scene.update(i ? 10106 : target); scene.draw(-1);
        CHECK(scene.credits_active() && scene.last_error().empty());
        CHECK(pixel(scene,50,100,0) == 0 && pixel(scene,50,100,1) == 0);
        if (i == 0 || i == 1 || i == 18 || i == 46) capture(scene, "dc2dm-ending-card-" + std::to_string(i));
    }
    scene.update(15000);
    CHECK(!scene.credits_active() && scene.script_ready() && scene.variable(100,29) == 1);
    CHECK(scene.set_sidebar_visible(true) && scene.sidebar_visible());
    CHECK(scene.set_sidebar_visible(false) && !scene.sidebar_visible());
    std::string background;
    std::vector<std::string> backgrounds;
    bool transferred_to_ed = false, drew_final_photo = false;
    const auto observe = [&](const dc4::MesCommand& command, const std::string&) {
        if (command.op == 0x33) {
            background = command.str;
            backgrounds.push_back(background);
        }
        if (command.op == 0x38 && command.str == "ED") transferred_to_ed = true;
    };
    scene.set_observer(observe);
    for (int i = 0; i < 32 && !scene.scene_ended(); ++i) {
        scene.advance_to_next_text();
        CHECK(scene.last_error().empty());
        if (scene.waiting_for_time()) scene.update(10000);
        scene.draw(-1);
        if (background == "BG\\EV_SUC37A") {
            // Original EV_SUC37A.CRX, independently scaled to the host canvas.
            const int samples[][5]{{320,180,7,14,31}, {640,360,16,18,15}, {960,540,70,175,230}};
            for (const auto& sample : samples)
                for (int channel = 0; channel < 3; ++channel)
                    CHECK(std::abs(int(pixel(scene,sample[0],sample[1],channel)) - sample[channel + 2]) <= 1);
            capture(scene,"dc2dm-ending-final-photo");
            drew_final_photo = true;
        }
    }
    CHECK(scene.scene_ended() && scene.last_error().empty());
    CHECK(transferred_to_ed && scene.next_script() == "MAIN");
    CHECK(drew_final_photo);
    for (const auto* name : {"SYSTEM\\GED02", "SYSTEM\\GED03", "SYSTEM\\GED04", "SYSTEM\\GED05", "SYSTEM\\GED06", "BG\\EV_SUC37A"})
        CHECK(std::find(backgrounds.begin(), backgrounds.end(), name) != backgrounds.end());

    dc4scene::SceneRunner replay(data.string());
    replay.set_animation_enabled(true); replay.set_use_system_menu(true);
    replay.set_observer(observe);
    CHECK(replay.load_script((data / "MES/MED.mes").string()));
    replay.set_variable(9,7,1); replay.set_variable(100,29,1);
    backgrounds.clear(); transferred_to_ed = false;
    CHECK(!replay.advance_to_next_text() && replay.credits_active());
    replay.finish_script_wait();
    CHECK(!replay.credits_active());
    for (int i = 0; i < 32 && !replay.scene_ended(); ++i) {
        replay.advance_to_next_text();
        CHECK(replay.last_error().empty());
        if (replay.waiting_for_time()) replay.update(10000);
    }
    CHECK(replay.scene_ended() && replay.next_script() == "SC_END");
    CHECK(!transferred_to_ed);
    CHECK(std::find(backgrounds.begin(), backgrounds.end(), "SYSTEM\\GED02") == backgrounds.end());
    CHECK(std::find(backgrounds.begin(), backgrounds.end(), "SYSTEM\\GED05") != backgrounds.end());
}
}
int main() {
    const auto root = fs::temp_directory_path() / ("arisa32-dc2dm-ending-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        fixture(root);
        if (const char* data = std::getenv("DC2DM_DATA")) installed_game(data);
        fs::remove_all(root);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; ++failures;
    }
    std::cout << "DC2DM ending checks: " << (failures ? "FAILED" : "passed") << '\n';
    return failures ? 1 : 0;
}
