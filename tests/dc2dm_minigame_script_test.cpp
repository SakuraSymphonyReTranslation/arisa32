#include "arisa32/dc2dm_minigame.h"
#include "arisa32/scene.h"

#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
int failures = 0;
void check(bool value, const char* expression, int line) {
    if (!value) { std::cerr << line << ": " << expression << '\n'; ++failures; }
}
#define CHECK(value) check(bool(value), #value, __LINE__)
using Key = dc4scene::Dc2dmMinigame::Key;

void score_markers() {
    using dc4scene::dc2dm_minigame_score_text;
    CHECK(dc2dm_minigame_score_text("Wins:\x81\x96 Losses:\x81\x97", 12, 3) ==
          "Wins:\x82\x50\x82\x51 Losses:\x82\x52");
    CHECK(dc2dm_minigame_score_text("\x81\x97/\x81\x96/\x81\x96", 255, 0) ==
          "\x82\x4f/\x82\x51\x82\x54\x82\x54/\x82\x51\x82\x54\x82\x54");
    CHECK(dc2dm_minigame_score_text("\xef\xbc\x8a/\xef\xbc\xa0", 12, 3) ==
          "\xef\xbc\x91\xef\xbc\x92/\xef\xbc\x93");
    CHECK(dc2dm_minigame_score_text("ASCII * @ remains", 12, 3) == "ASCII * @ remains");
    CHECK(dc2dm_minigame_score_text("\x83\x81\x96", 12, 3) == "\x83\x81\x96");
}

void other_profiles() {
    const auto directory = std::filesystem::temp_directory_path() /
        ("arisa32-minigame-isolation-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    for (uint16_t version : {0xAAB6, 0xABB6, 0x9969}) {
        const bool classic = version == 0x9969;
        std::vector<uint8_t> bytes = classic ? std::vector<uint8_t>{1,0,0,0,2,0,0,0,0x69,0x99} :
            std::vector<uint8_t>{1,0,0,0,3,0,0,0,0,0,uint8_t(version),uint8_t(version >> 8),0};
        bytes.insert(bytes.end(), {0x1E,200,0,0x1E,201,0,0x1E,200,1,0x1E,201,1});
        bytes.push_back(classic ? 0x4C : 0x6B);
        const std::string text = "\x81\x96/\x81\x97";
        for (unsigned char c : text) bytes.push_back(uint8_t(c - 0x20));
        bytes.insert(bytes.end(), {0,0});
        const auto script = directory / (std::to_string(version) + ".mes");
        { std::ofstream file(script, std::ios::binary);
          file.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size())); }
        dc4scene::SceneRunner scene(directory.string());
        CHECK(scene.load_script(script.string()));
        scene.set_variable(3, 0, 99);
        scene.set_variable(3, 98, 1);
        scene.set_variable(100, 16, 12); scene.set_variable(100, 17, 3);
        CHECK(scene.advance_to_next_text());
        CHECK(!scene.minigame_active() && !scene.waiting_for_time());
        CHECK(scene.variable(3, 0) == 99);
        CHECK(scene.variable(100, 16) == 12 && scene.variable(100, 17) == 3);
        CHECK(scene.current_line() == text && scene.save_load_allowed());
        std::filesystem::remove(script);
    }
    std::filesystem::remove(directory);
}

void return_branch(const std::filesystem::path& data, int kind, bool repeated,
                   int wins, int losses, const std::vector<int>& expected_voices) {
    dc4scene::SceneRunner scene(data.string());
    scene.set_use_system_menu(true);
    const int first_flag = kind == 0 ? 14 : 15;
    const int score_index = kind == 0 ? 16 : 18;
    const int other_index = kind == 0 ? 18 : 16;
    const auto script = data / "MES" / (kind == 0 ? "mgjk.mes" : "reversi.mes");
    CHECK(scene.load_script(script.string()));
    scene.set_variable(100, first_flag, repeated ? 1 : 0);
    scene.set_variable(100, score_index, wins);
    scene.set_variable(100, score_index + 1, losses);
    scene.set_variable(100, other_index, 241);
    scene.set_variable(100, other_index + 1, 242);
    scene.set_variable(3, 0, 99);

    int launch_count = 0, comparison_count = 0;
    std::vector<std::string> return_scripts;
    std::string raw_line;
    scene.set_observer([&](const dc4::MesCommand& command, const std::string&) {
        if (command.op == 0x1B && command.p1 == 200) {
            CHECK(command.p2 == kind);
            ++launch_count;
        }
        if (command.op == 0x1B && command.p1 == 201) {
            CHECK(command.p2 == kind);
            ++comparison_count;
        }
        if (command.op == 0x38) return_scripts.push_back(command.str);
        if (command.op == 0x4F && command.kind == dc4::CmdKind::Text) raw_line = command.str;
    });
    CHECK(!scene.advance_to_next_text());
    CHECK(scene.last_error().empty());
    CHECK(launch_count == 1 && scene.minigame_active() && scene.waiting_for_time());
    CHECK(scene.variable(100, first_flag) == (repeated ? 1 : 0));
    if (!scene.minigame_active()) return;
    scene.minigame_key(Key::Cancel);
    CHECK(scene.minigame()->confirming_exit());
    scene.minigame_key(Key::Left);
    scene.minigame_key(Key::Accept);
    CHECK(!scene.minigame_active() && scene.script_ready());

    std::vector<int> observed_voices;
    int score_lines = 0;
    for (int step = 0; step < 30 && !scene.scene_ended(); ++step) {
        const bool dialogue = scene.advance_to_next_text();
        CHECK(scene.last_error().empty());
        if (scene.waiting_for_time()) scene.finish_script_wait();
        if (!dialogue) continue;
        const auto& voice = scene.last_voice();
        const auto marker = voice.find("_OTM");
        CHECK(marker != std::string::npos);
        if (marker != std::string::npos) observed_voices.push_back(std::stoi(voice.substr(marker + 4)));
        CHECK(!scene.speaker().empty());
        if (raw_line.find("\x81\x96") != std::string::npos ||
            raw_line.find("\xef\xbc\x8a") != std::string::npos) {
            ++score_lines;
            CHECK(scene.variable(3, 98) == kind + 1);
            CHECK(scene.variable(3, 0) == (wins > losses ? 1 : wins < losses ? 2 : 0));
            CHECK(scene.current_line().find("\x81\x96") == std::string::npos);
            CHECK(scene.current_line().find("\x81\x97") == std::string::npos);
            CHECK(scene.current_line().find("\xef\xbc\x8a") == std::string::npos);
            CHECK(scene.current_line().find("\xef\xbc\xa0") == std::string::npos);
            const bool utf8 = raw_line.find("\xef\xbc\x8a") != std::string::npos;
            const char* twelve = utf8 ? "\xef\xbc\x91\xef\xbc\x92" : "\x82\x50\x82\x51";
            const char* three = utf8 ? "\xef\xbc\x93" : "\x82\x52";
            const char* twenty_seven = utf8 ? "\xef\xbc\x92\xef\xbc\x97" : "\x82\x51\x82\x56";
            CHECK(scene.current_line().find(wins == 12 ? twelve : wins == 3 ? three : twenty_seven) != std::string::npos);
            CHECK(scene.current_line().find(losses == 12 ? twelve : losses == 3 ? three : twenty_seven) != std::string::npos);
        } else {
            CHECK(scene.current_line() == raw_line);
        }
    }
    if (observed_voices != expected_voices) {
        std::cerr << "kind=" << kind << " repeated=" << repeated << " score=" << wins << ':' << losses << " voices:";
        for (int voice : observed_voices) std::cerr << ' ' << voice;
        std::cerr << '\n';
    }
    CHECK(observed_voices == expected_voices);
    CHECK(scene.scene_ended() && scene.next_script() == "MAIN");
    CHECK(comparison_count == (repeated ? 1 : 0));
    CHECK(score_lines == (repeated ? 1 : 0));
    CHECK(scene.variable(100, first_flag) == 1);
    CHECK(scene.variable(100, score_index) == wins && scene.variable(100, score_index + 1) == losses);
    CHECK(scene.variable(100, other_index) == 241 && scene.variable(100, other_index + 1) == 242);
    CHECK(scene.variable(3, 98) == 0);
    if (!repeated) CHECK(scene.variable(3, 0) == 99);
    CHECK((return_scripts == std::vector<std::string>{"MINIGAME", "MAIN"}));
}

void installed_game(const std::filesystem::path& data) {
    for (int kind = 0; kind < 2; ++kind) {
        dc4scene::SceneRunner scene(data.string());
        const int score_index = kind == 0 ? 16 : 18;
        CHECK(scene.load_script((data / "MES" / (kind == 0 ? "mgjk.mes" : "reversi.mes")).string()));
        scene.set_variable(100, score_index, 12);
        scene.set_variable(100, score_index + 1, 3);
        CHECK(!scene.advance_to_next_text());
        CHECK(scene.minigame_active() && !scene.save_load_allowed());
        if (kind == 0) {
            for (int round = 0; round < 64 && scene.minigame()->wins() + scene.minigame()->losses() == 15; ++round)
                scene.minigame_key(Key::Accept);
            CHECK(scene.minigame()->wins() + scene.minigame()->losses() == 16);
            CHECK(scene.variable(100, score_index) == 12 && scene.variable(100, score_index + 1) == 3);
        }
        scene.finish_replay();
        CHECK(!scene.minigame_active() && !scene.waiting_for_time() && scene.save_load_allowed());
        CHECK(scene.variable(100, score_index) == 12 && scene.variable(100, score_index + 1) == 3);
    }
    {
        dc4scene::SceneRunner scene(data.string());
        CHECK(scene.load_script((data / "MES" / "mgjk.mes").string()));
        scene.set_variable(100, 16, 12); scene.set_variable(100, 17, 3);
        CHECK(!scene.advance_to_next_text());
        for (int round = 0; round < 64 && scene.minigame()->wins() + scene.minigame()->losses() == 15; ++round)
            scene.minigame_key(Key::Accept);
        const int wins = scene.minigame()->wins(), losses = scene.minigame()->losses();
        CHECK(wins + losses == 16);
        scene.minigame_key(Key::Next); scene.minigame_key(Key::Accept);
        CHECK(!scene.minigame_active());
        CHECK(scene.variable(100, 16) == wins && scene.variable(100, 17) == losses);
    }
    return_branch(data, 0, false, 12, 3, {8,9,10,11,12});
    return_branch(data, 1, false, 3, 12, {14,15,16,17,18});
    return_branch(data, 0, true, 12, 3, {22,25,26,29});
    return_branch(data, 0, true, 3, 12, {23,27,28,29});
    return_branch(data, 0, true, 27, 27, {24,29});
    return_branch(data, 1, true, 12, 3, {31,32,35,36,39});
    return_branch(data, 1, true, 3, 12, {31,33,37,38,39});
    return_branch(data, 1, true, 27, 27, {31,34,39});
}
}

int main() {
    score_markers();
    try {
        other_profiles();
        if (const char* data = std::getenv("DC2DM_DATA")) installed_game(data);
        else std::cout << "DC2DM_DATA unset; installed MES branches skipped\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; ++failures;
    }
    std::cout << "DC2DM minigame script checks: " << (failures ? "FAILED" : "passed") << '\n';
    return failures ? 1 : 0;
}
