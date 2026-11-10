#include "arisa32/messtream.h"
#include "arisa32/mesvm.h"
#include "arisa32/movie.h"
#include "arisa32/resource.h"
#include "arisa32/save_paths.h"
#include "arisa32/scene.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {
int failures = 0;
void check(bool result, const char* expression, int line) {
    if (!result) { std::cerr << "line " << line << ": " << expression << '\n'; ++failures; }
}
#define CHECK(expression) check(bool(expression), #expression, __LINE__)

struct Script {
    std::vector<uint8_t> bytes{1,0,0,0,2,0,0,0,0x69,0x5F};
    void pair(uint8_t op, uint8_t a, uint8_t b) { bytes.insert(bytes.end(), {op,a,b}); }
    void string(uint8_t op, const std::string& value) {
        bytes.push_back(op);
        if (op >= 0x29 && op < 0x2F) bytes.push_back(0);
        for (unsigned char c : value) bytes.push_back(op >= 0x4A && op < 0x4E ? uint8_t(c - 0x20) : c);
        bytes.push_back(0);
    }
};
void write_file(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    if (!out) throw std::runtime_error("cannot write DC2TY test fixture");
}
void solid_image(const std::filesystem::path& path) {
    std::vector<uint8_t> bytes{'C','R','X','G',0,0,0,0,64,0,36,0,1,0,0,0,0,0,0,0};
    std::vector<uint8_t> pixels;
    for (int i = 0; i < 64*36; ++i) pixels.insert(pixels.end(), {32,80,160});
    for (size_t i = 0; i < pixels.size(); i += 8) {
        bytes.push_back(255);
        bytes.insert(bytes.end(), pixels.begin()+i, pixels.begin()+std::min(i+8,pixels.size()));
    }
    write_file(path, bytes);
}

void synthetic_flow(const std::filesystem::path& root) {
    solid_image(root/"GRP/bg/test.CRX");
    solid_image(root/"GRP/SYSTEM/MESSWIN.CRX");
    Script main;
    main.bytes.push_back(0);
    write_file(root/"MES/main.mes", main.bytes);
    Script opening;
    opening.string(0x4C, "BEFORE_OPENING");
    opening.string(0x35, "04");
    opening.string(0x38, "DC2TY_OP");
    opening.string(0x2E, "main");
    write_file(root/"MES/start.mes", opening.bytes);
    dc4scene::SceneRunner scene(root.string());
    scene.set_use_system_menu(true);
    CHECK(scene.load_script((root/"MES/start.mes").string(), "04"));
    CHECK(!scene.advance_to_next_text());
    CHECK(scene.current_line().empty() && scene.pending_movie() == "DC2TY_OP");
    CHECK(!scene.scene_ended() && scene.next_script().empty());
    scene.finish_movie();
    CHECK(!scene.advance_to_next_text());
    CHECK(scene.scene_ended() && scene.next_script() == "main");
    CHECK(scene.last_error().empty());

    Script entry;
    entry.string(0x2E, "sce_select");
    write_file(root/"MES/story_entry.mes", entry.bytes);
    Script selector;
    selector.string(0x30, "bg/test");
    selector.string(0x4B, "Speaker");
    selector.string(0x4C, "STORY_SELECTOR");
    selector.string(0x4C, "NEXT");
    selector.bytes.push_back(0);
    write_file(root/"MES/sce_select.mes", selector.bytes);
    CHECK(scene.load_script((root/"MES/story_entry.mes").string()));
    CHECK(scene.advance_to_next_text());
    CHECK(scene.script_name() == "sce_select.mes" && scene.next_script().empty());
    CHECK(scene.current_line() == "STORY_SELECTOR" && scene.speaker() == "Speaker");
    CHECK(scene.save_format_version() == 0x5F69);
    scene.set_variable(100, 56, 4);
    scene.set_variable(100, 5, 1);
    scene.set_message_visible(false);
    scene.draw(-1);
    const size_t at = (10*dc4scene::kScreenW+180)*4;
    CHECK(scene.frame()[at] == 160 && scene.frame()[at+1] == 80 && scene.frame()[at+2] == 32);
    CHECK(scene.frame()[0] == 0 && scene.frame()[3] == 255);
    std::vector<uint8_t> saved;
    std::string error;
    CHECK(scene.capture_state(&saved, &error));
    CHECK(scene.advance_to_next_text() && scene.current_line() == "NEXT");
    CHECK(scene.restore_state(saved, &error));
    CHECK(scene.current_line() == "STORY_SELECTOR");
    CHECK(scene.variable(100, 56) == 4 && scene.variable(100, 5) == 1);
    CHECK(!scene.capture_pc_save(&saved, &error));
    CHECK(!scene.restore_pc_save(std::vector<uint8_t>(0x2B20), &error));
    CHECK(error == "truncated or unsupported DC2TY PC save");
    CHECK(dc4::native_save_prefix(0x5F69) == "dc2ty");
    CHECK(dc4::save_slot_number("slot1.dc4sav", 0x5F69) == 1);
    CHECK(dc4::save_slot_number("DC2PC02.dat", 0x5F69) == -1);
}

void installed_corpus(const std::string& data) {
    size_t scripts = 0, commands = 0, labels = 0;
    for (const auto& item : std::filesystem::directory_iterator(std::filesystem::path(data)/"MES")) {
        if (dc4::save_lower(item.path().extension().string()) != ".mes") continue;
        dc4::MesFile file;
        CHECK(file.load_file(item.path().string()));
        if (file.version != 0x5F69) {
            std::cerr << "unexpected version: " << item.path() << '\n';
            CHECK(false); continue;
        }
        ++scripts;
        CHECK(dc4::mes_opcode_profile(file.dialect, file.version).id == dc4::MesProfileId::DC2TY);
        dc4::MesStream stream(file);
        dc4::MesCommand command;
        std::set<uint32_t> boundaries{2};
        while (stream.next(&command)) {
            ++commands;
            CHECK(command.dc2ty && !command.dc2pc && !command.dc2dm);
            boundaries.insert(stream.pos());
        }
        CHECK(stream.pos() == file.code.size());
        for (const auto& label : file.labels) { ++labels; CHECK(boundaries.count(label.offset)); }
        dc4::MesVm vm(file);
        CHECK(vm.error().empty());
    }
    CHECK(scripts > 0 && commands > 0 && labels > 0);
    std::cout << "DC2TY corpus: " << scripts << " scripts, " << commands << " commands, " << labels << " labels\n";

    dc4scene::SceneRunner opening(data);
    opening.set_use_system_menu(true);
    CHECK(opening.load_ui_script(data+"/MES/start.mes"));
    CHECK(opening.load_script(data+"/MES/start.mes", "04"));
    for (int i = 0; i < 100 && opening.pending_movie().empty() && !opening.scene_ended(); ++i)
        opening.advance_to_next_text();
    CHECK(opening.last_error().empty());
    CHECK(opening.pending_movie() == "movie\\dc2ty_op.mpg");
    opening.finish_movie();
    for (int i = 0; i < 100 && !opening.scene_ended(); ++i) opening.advance_to_next_text();
    CHECK(opening.scene_ended() && dc4::save_lower(opening.next_script()) == "main");
    CHECK(opening.last_error().empty());

    dc4::MoviePlayer movie;
    std::string movie_error;
    CHECK(movie.open(data, "movie\\dc2ty_op.mpg", nullptr, &movie_error));
    CHECK(movie.width() == 640 && movie.height() == 480);
    for (int i = 0; i < 4000 && !movie.finished(); ++i) movie.update(40);
    CHECK(movie.finished() && movie.error().empty());
    CHECK(movie.decoded_frames() > 4000 && movie.decoded_audio_frames() > 1000000);
    CHECK(movie.audio_peak() > .0001f);
    std::cout << "DC2TY opening decoded: " << movie.decoded_frames() << " video frames, "
              << movie.decoded_audio_frames() << " audio frames\n";

    dc4scene::SceneRunner story(data);
    CHECK(story.load_ui_script(data+"/MES/start.mes"));
    CHECK(story.load_script(data+"/MES/ty_myu09_d1_myu.mes"));
    bool dialogue = false;
    for (int i = 0; i < 100 && !story.scene_ended(); ++i) {
        if (story.advance_to_next_text() && !story.current_line().empty()) { dialogue = true; break; }
        if (!story.pending_movie().empty()) story.finish_movie();
    }
    CHECK(dialogue && story.last_error().empty());
    story.set_variable(100, 56, 4);
    std::vector<uint8_t> saved;
    std::string error;
    const auto line = story.current_line();
    CHECK(story.capture_state(&saved, &error));
    CHECK(story.restore_state(saved, &error));
    CHECK(story.current_line() == line && story.variable(100, 56) == 4);
}
}

int main(int argc, char** argv) {
    const auto root = std::filesystem::temp_directory_path()/
        ("arisa32-dc2ty-profile-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        synthetic_flow(root);
        const char* data = argc > 1 ? argv[1] : std::getenv("DC2TY_DATA");
        if (data && *data) installed_corpus(data);
        else std::cout << "Installed DC2TY corpus checks skipped.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; ++failures;
    }
    std::filesystem::remove_all(root);
    if (!failures) std::cout << "DC2TY profile and startup flow checks passed.\n";
    return failures ? 1 : 0;
}
