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
void write_file(const fs::path& path, const std::vector<uint8_t>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    if (!file) throw std::runtime_error("cannot write credits fixture");
}
std::vector<uint8_t> page(int height, bool blue) {
    std::vector<uint8_t> bytes{'C','R','X','G',0,0,0,0};
    word(bytes, 1280); word(bytes, uint16_t(height));
    word(bytes, 1); word(bytes, 0); word(bytes, 0); word(bytes, 0);
    const size_t size = size_t(1280) * height * 3;
    for (size_t offset = 0; offset < size; offset += 8) {
        bytes.push_back(255);
        for (size_t i = offset; i < std::min(offset + 8, size); ++i)
            bytes.push_back(i % 3 == 1 ? 40 : (i % 3 == (blue ? 0 : 2) ? 220 : 20));
    }
    return bytes;
}
void write_archive(const fs::path& root, int height) {
    const auto red = page(height, false), blue = page(height, true);
    std::vector<uint8_t> bytes;
    dword(bytes, 2); dword(bytes, 20); dword(bytes, uint32_t(red.size()));
    dword(bytes, uint32_t(20 + red.size())); dword(bytes, uint32_t(blue.size()));
    bytes.insert(bytes.end(), red.begin(), red.end());
    bytes.insert(bytes.end(), blue.begin(), blue.end());
    write_file(root / "GRP/ED/ED.PCK", bytes);
}
void write_script(const fs::path& root) {
    std::vector<uint8_t> bytes{1,0,0,0,3,0,0,0,0,0,0xB6,0xAB,0,0x82};
    word(bytes, 3); word(bytes, 0); word(bytes, 4); word(bytes, 0);
    bytes.insert(bytes.end(), {0x1E,9,0,0x6B});
    for (unsigned char c : std::string("AFTER")) bytes.push_back(uint8_t(c - 0x20));
    bytes.insert(bytes.end(), {0,0});
    write_file(root / "MES/credits.mes", bytes);
}
uint8_t channel(const dc4scene::SceneRunner& scene, int y, int c) {
    return scene.frame()[(size_t(y) * 1280 + 640) * 4 + c];
}
void fixture(const fs::path& root, int height) {
    write_archive(root, height); write_script(root);
    dc4scene::SceneRunner scene(root.string());
    scene.set_animation_enabled(true);
    CHECK(scene.load_script((root / "MES/credits.mes").string()));
    CHECK(!scene.advance_to_next_text());
    CHECK(scene.credits_active() && scene.waiting_for_time());
    if (!scene.credits_active()) return;
    scene.draw(-1); CHECK(scene.last_error().empty());
    CHECK(channel(scene, 0, 0) == 220 && channel(scene, 719, 0) == 220);
    scene.update(1000); scene.draw(-1);
    CHECK(channel(scene, 359, 0) == 220 && channel(scene, 360, 2) == 220);
    scene.update(1000); scene.draw(-1);
    CHECK(channel(scene, 0, 2) == 220 && channel(scene, 719, 2) == 220);
    scene.update(4999); CHECK(scene.credits_active());
    scene.update(1667); scene.draw(-1);
    CHECK(channel(scene, 0, 2) > 100 && channel(scene, 0, 2) < 120);
    scene.update(1668);
    CHECK(!scene.credits_active() && !scene.waiting_for_time() && scene.script_ready());
    CHECK(scene.advance_to_next_text() && scene.current_line() == "AFTER");
    CHECK(scene.load_script((root / "MES/credits.mes").string()));
    CHECK(!scene.advance_to_next_text() && scene.credits_active());
    scene.finish_script_wait();
    CHECK(!scene.credits_active() && !scene.waiting_for_time());
    CHECK(scene.advance_to_next_text() && scene.current_line() == "AFTER");
}
void capture(const dc4scene::SceneRunner& scene, const std::string& name) {
    const char* directory = std::getenv("DC4PH_CREDITS_CAPTURE_DIR");
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
    scene.set_animation_enabled(true);
    scene.set_use_system_menu(true);
    CHECK(scene.load_script((data / "MES/dc4_dc99990101a.mes").string(), "end"));
    scene.set_variable(9, 7, 0);
    for (int i = 0; i < 32 && !scene.credits_active() && !scene.scene_ended(); ++i) {
        scene.advance_to_next_text();
        CHECK(scene.last_error().empty());
        if (!scene.last_error().empty() || scene.credits_active()) break;
        scene.update(10000);
    }
    CHECK(scene.credits_active() && scene.waiting_for_time());
    if (!scene.credits_active()) return;
    CHECK(scene.variable(3, 0) == 125);
    scene.draw(-1); CHECK(scene.last_error().empty());
    const auto first = scene.frame(); capture(scene, "credits-start");
    scene.update(5000); scene.draw(-1);
    CHECK(scene.last_error().empty() && scene.frame() != first);
    capture(scene, "credits-5s");
    scene.update(115000); scene.draw(-1);
    CHECK(scene.credits_active() && scene.waiting_for_time());
    CHECK(scene.last_error().empty()); capture(scene, "credits-120s");
    scene.update(8000);
    CHECK(!scene.credits_active() && !scene.waiting_for_time() && scene.script_ready());
    for (int i = 0; i < 16 && !scene.scene_ended(); ++i) {
        scene.advance_to_next_text(); scene.update(10000);
    }
    CHECK(scene.scene_ended() && scene.last_error().empty());
    CHECK(scene.next_script() == "MAIN");
}
}
int main() {
    const auto root = fs::temp_directory_path() / ("arisa32-credits-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        fixture(root / "720", 720);
        fixture(root / "721", 721);
        if (const char* data = std::getenv("DC4PH_DATA")) installed_game(data);
        fs::remove_all(root);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; ++failures;
    }
    std::cout << "Credits checks: " << (failures ? "FAILED" : "passed") << '\n';
    return failures ? 1 : 0;
}
