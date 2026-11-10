#include "arisa32/audio.h"
#include "arisa32/scene.h"
#include "arisa32/scene_audio.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <thread>
#include <vector>

static void check(bool pass, const char* message) {
    if (!pass) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static std::vector<int16_t> render(dc4::AudioEngine& audio, size_t frames) {
    std::vector<int16_t> output(frames * 2);
    audio.render_pcm(output.data(), frames);
    return output;
}
static void put32(std::vector<uint8_t>& bytes, size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) bytes[at + i] = uint8_t(v >> (8 * i));
}

static void dc2dm_voice_prefetch() {
    using namespace dc4;
    const auto directory = std::filesystem::temp_directory_path() /
        ("arisa32-audio-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    check(std::filesystem::create_directory(directory), "create voice fixture directory");
    for (int voice = 0; voice < 2; ++voice) {
        std::vector<uint8_t> bytes(36);
        bytes[0] = 'X'; bytes[1] = 'P'; bytes[2] = 'C'; bytes[3] = 'M';
        put32(bytes, 4, 8);
        bytes[12] = 1; bytes[14] = 1;
        put32(bytes, 16, 44100); put32(bytes, 20, 44100);
        bytes[24] = 1; bytes[26] = 8;
        for (int i = 0; i < 8; ++i) bytes[28 + i] = uint8_t(129 + voice * 16 + i);
        std::ofstream file(directory / (voice ? "next.pcm" : "current.pcm"), std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
        check(bool(file), "write voice fixture");
    }
    AudioEngine audio;
    SceneAudio scene(directory.string());
    scene.set_engine(&audio, nullptr);
    auto command = [](uint8_t opcode, const char* name) {
        MesCommand raw;
        raw.dc2dm = true; raw.kind = CmdKind::Str; raw.op = opcode; raw.str = name;
        return mes_engine_command(raw);
    };
    const auto prefetch = command(0x37, "next");
    check(scene.dispatch(prefetch) && !scene.voice_playing() && scene.last_voice_name().empty(),
          "DC2DM prefetch alone does not start or retain a voice");
    check(scene.dispatch(command(0x35, "current")) && scene.voice_playing(),
          "DC2DM immediate voice starts playback");
    check(render(audio, 2)[2] == 512, "current voice plays its first two samples");
    for (int frame = 2; frame < 8; ++frame) {
        const double position = audio.channel_position(8);
        check(scene.dispatch(prefetch) && audio.channel_position(8) == position &&
              scene.last_voice_name() == "current", "prefetch preserves playback position and replay voice");
        check(render(audio, 1)[0] == (frame + 1) * 256, "repeated prefetch leaves current samples uninterrupted");
    }
    check(!scene.voice_playing() && render(audio, 2)[0] == 0, "immediate voice completes without looping");
    check(scene.dispatch(command(0x36, "next")) && scene.last_voice_name() == "next",
          "DC2DM deferred native voice becomes the replay voice when played");
    check(render(audio, 8)[0] == 17 * 256 && !scene.voice_playing(),
          "prefetched voice plays once only at its playback command");
    check(scene.dispatch(prefetch) && !scene.voice_playing(), "later prefetch does not restart completed playback");
    check(scene.replay_voice() && render(audio, 8)[0] == 17 * 256 && !scene.voice_playing(),
          "explicit replay still plays the retained voice once");
    MesCommand next_line;
    next_line.dc2dm = true; next_line.kind = CmdKind::Param2; next_line.op = 0x06;
    next_line = mes_engine_command(next_line);
    check(scene.dispatch(command(0x36, "current")), "start voice before advancing the dialogue");
    render(audio, 2);
    check(scene.dispatch(next_line) && scene.last_voice_name().empty() && !scene.replay_voice() &&
          audio.channel_position(8) == 2, "advancing clears the line replay association without restarting audio");
    check(scene.dispatch(prefetch) && scene.last_voice_name().empty(), "narration does not inherit a prefetched voice");
    SceneAudio headless(directory.string());
    check(headless.dispatch(prefetch) && headless.last_voice_name().empty(), "headless prefetch retains no voice");
    check(headless.dispatch(command(0x36, "current")) && headless.dispatch(prefetch) &&
          headless.last_voice_name() == "current", "headless prefetch preserves explicit playback association");
    check(headless.dispatch(next_line) && headless.last_voice_name().empty(), "headless advance clears the voiced line");
    std::vector<uint8_t> script{1,0,0,0,3,0,0,0,0,0,0x72,0x9D,0};
    const auto voice_command = [&](uint8_t op, const std::string& name) {
        script.push_back(op);
        script.insert(script.end(), name.begin(), name.end());
        script.push_back(0);
    };
    const auto dialogue = [&](const std::string& line) {
        script.push_back(0x4F);
        for (unsigned char ch : line) script.push_back(uint8_t(ch - 0x20));
        script.insert(script.end(), {0,0x06,0,0});
    };
    voice_command(0x37, "next"); dialogue("FIRST");
    voice_command(0x36, "current"); voice_command(0x37, "next"); dialogue("CURRENT");
    voice_command(0x37, "next"); dialogue("NARRATION");
    script.push_back(0x4E);
    for (unsigned char ch : std::string("Yoshiyuki")) script.push_back(uint8_t(ch - 0x20));
    script.push_back(0); dialogue("MC");
    voice_command(0x36, "next"); voice_command(0x37, "current"); dialogue("NEXT");
    script.push_back(0);
    std::filesystem::create_directories(directory / "MES");
    const auto path = directory / "MES/voice-sequence.mes";
    {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(script.data()), std::streamsize(script.size()));
        check(bool(file), "write voice sequence fixture");
    }
    AudioEngine sequence_audio;
    dc4scene::SceneRunner runner(directory.string());
    runner.set_audio(&sequence_audio, nullptr);
    check(runner.load_script(path.string()), "load DC2DM voice sequence");
    check(runner.advance_to_next_text() && runner.current_line() == "FIRST" &&
          runner.last_voice().empty() && !runner.voice_playing(), "runner ignores narration prefetch");
    check(runner.advance_to_next_text() && runner.current_line() == "CURRENT" &&
          runner.last_voice() == "current" && runner.voice_playing(), "runner retains played voice through prefetch");
    render(sequence_audio, 8);
    check(!runner.voice_playing() && runner.last_voice() == "current" && runner.replay_voice(),
          "completed line keeps its own replay available");
    render(sequence_audio, 2);
    check(runner.advance_to_next_text() && runner.current_line() == "NARRATION" &&
          runner.last_voice().empty() && sequence_audio.channel_position(8) == 2,
          "runner advances to narration without restarting or associating another voice");
    std::vector<uint8_t> silent_line;
    check(runner.capture_state(&silent_line) && !runner.replay_voice(), "capture narration with no replay");
    check(runner.advance_to_next_text() && runner.speaker() == "Yoshiyuki" &&
          runner.last_voice().empty() && !runner.replay_voice(), "unvoiced Yoshiyuki cannot replay another character");
    check(runner.advance_to_next_text() && runner.current_line() == "NEXT" &&
          runner.last_voice() == "next" && render(sequence_audio, 8)[0] == 17 * 256 && !runner.voice_playing(),
          "runner plays deferred voice once at its own dialogue");
    check(runner.restore_state(silent_line) && runner.current_line() == "NARRATION" &&
          runner.last_voice().empty() && !runner.replay_voice(), "restoring DC2DM narration keeps replay unavailable");
    for (uint8_t version : {0xAA, 0xAB}) {
        std::filesystem::create_directories(directory / "MES");
        const auto modern_path = directory / "MES/modern-voice-sequence.mes";
        script = {1,0,0,0,3,0,0,0,0,0,0xB6,version,0};
        const auto text = [&](uint8_t op, const std::string& line) {
            script.push_back(op);
            for (unsigned char ch : line) script.push_back(uint8_t(ch - 0x20));
            script.push_back(0);
        };
        voice_command(0x4D, "current"); text(0x6A, "Nino"); text(0x6B, "VOICED");
        script.insert(script.end(), {0x06,0,0}); text(0x6B, "NARRATION");
        script.insert(script.end(), {0x06,0,0}); text(0x6A, "Ichito"); text(0x6B, "MC");
        script.insert(script.end(), {0x06,0,0});
        voice_command(0x4D, "next"); text(0x6A, "Nino"); text(0x6B, "NEXT");
        script.push_back(0);
        {
            std::ofstream file(modern_path, std::ios::binary);
            file.write(reinterpret_cast<const char*>(script.data()), std::streamsize(script.size()));
        }
        AudioEngine mixer;
        dc4scene::SceneRunner modern(directory.string());
        modern.set_audio(&mixer, nullptr);
        check(modern.load_script(modern_path.string()) && modern.advance_to_next_text(), "load modern voiced line");
        check(modern.last_voice() == "current" && modern.replay_voice(), "voiced line can replay");
        render(mixer, 2);
        check(modern.advance_to_next_text() && modern.last_voice().empty() && !modern.replay_voice() &&
              mixer.channel_position(8) == 2, "modern narration clears replay without cutting off playback");
        std::vector<uint8_t> narration;
        check(modern.capture_state(&narration), "save unvoiced narration");
        check(modern.advance_to_next_text() && modern.speaker() == "Ichito" && modern.last_voice().empty(),
              "unvoiced MC does not inherit Nino's voice");
        check(modern.advance_to_next_text() && modern.last_voice() == "next" && modern.replay_voice(),
              "next voiced line uses its own recording");
        check(modern.restore_state(narration) && modern.last_voice().empty() && !modern.replay_voice(),
              "restoring narration keeps replay unavailable");
    }
    std::filesystem::remove_all(directory);
}

static void deferred_channel_adoption() {
    using namespace dc4;
    AudioEngine authored, staged, live, expected;
    const std::vector<int16_t> samples{10000, 12000, 16000, 18000};
    check(authored.play_pcm(0, samples, 22050, 1, {true}), "prepare saved track");
    authored.set_group_volume(AudioGroup::Bgm, 60, 100);
    authored.fade_group_pan(AudioGroup::Bgm, -5000, 100);
    render(authored, 31);
    const auto saved = authored.capture_snapshot();
    AudioPreparedChannel track;
    track.channel = 0; track.pcm = samples; track.rate = 22050;
    track.channels = 1; track.options = {true};
    track.position_frames = saved.channels[0].position_frames;
    check(staged.restore_channels({track}, saved.groups) &&
          expected.restore_channels({track}, saved.groups), "stage fully validated saved audio");
    check(live.play_pcm(0, {1000}, 44100, 1, {true}), "outgoing menu track");
    for (auto* mixer : {&live, &expected}) {
        mixer->set_master_volume(80);
        mixer->set_user_volume(AudioGroup::Bgm, 70);
        mixer->set_channel_user_volume(0, 60);
    }
    staged.set_master_volume(10);
    staged.set_user_volume(AudioGroup::Bgm, 20);
    staged.set_channel_user_volume(0, 30);
    check(render(live, 16) == std::vector<int16_t>(32, 336),
          "preparing a load leaves outgoing audio audible until commit");
    const auto waiting = staged.capture_snapshot();
    check(waiting.channels[0].position_frames == saved.channels[0].position_frames &&
          waiting.groups[0].volume_remaining_frames == saved.groups[0].volume_remaining_frames,
          "detached playback position and volume ramp remain frozen before commit");
    live.adopt_channels_from(staged);
    check(live.master_volume() == 80 && live.user_volume(AudioGroup::Bgm) == 70 &&
          live.channel_position(0) == saved.channels[0].position_frames,
          "commit preserves live controls and adopts exact saved position");
    check(render(live, 256) == render(expected, 256),
          "committed samples preserve source position, ramps and live channel volume");
    check(render(staged, 1) == std::vector<int16_t>(2, 6),
          "source retains its controls and usable retired channels");
    live.adopt_channels_from(live);
    check(render(live, 64) == render(expected, 64), "self adoption is a no-op");
    for (auto* mixer : {&live, &expected})
        check(mixer->play_pcm(1, {3000}, 44100, 1, {true}), "start track after adoption");
    check(render(live, 64) == render(expected, 64),
          "new channels inherit the adopted script group ramps");
}

static void dc2dm_retained_title_bgm() {
    using namespace dc4;
    const auto directory = std::filesystem::temp_directory_path() /
        ("arisa32-title-audio-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto write = [&](const std::filesystem::path& relative, const std::vector<uint8_t>& bytes) {
        const auto path = directory / relative;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
        check(bool(file), "write retained title audio fixture");
    };
    std::vector<uint8_t> names(772 + 4 + 2 * 192 + 12);
    put32(names, 0, 6); put32(names, 772, 2);
    for (size_t id = 0; id < 2; ++id) {
        const std::string name = id ? "BGM\\STORY" : "BGM\\TITLE";
        for (size_t i = 0; i < name.size(); ++i) names[776 + id * 192 + i] = uint8_t(name[i]);
        std::vector<uint8_t> pcm(28 + 16);
        pcm[0] = 'X'; pcm[1] = 'P'; pcm[2] = 'C'; pcm[3] = 'M';
        put32(pcm, 4, 16); pcm[12] = 1; pcm[14] = 1;
        put32(pcm, 16, 44100); put32(pcm, 20, 44100);
        pcm[24] = 1; pcm[26] = 8;
        for (size_t i = 0; i < 16; ++i) pcm[28 + i] = uint8_t(129 + id * 32 + i);
        write(id ? "BGM/STORY.pcm" : "BGM/TITLE.pcm", pcm);
    }
    write("GRP/NAMES.DAT", names);
    auto command = [](uint16_t id, uint16_t mode) {
        MesCommand raw;
        raw.dc2dm = true; raw.kind = CmdKind::Words4; raw.op = 0x52;
        raw.words[0] = id; raw.words[1] = mode;
        return mes_engine_command(raw);
    };
    AudioEngine live, expected;
    const auto title = (directory / "BGM/TITLE.pcm").string();
    for (auto* audio : {&live, &expected}) {
        check(audio->play_bgm_file(title), "start retained title fixture");
        audio->set_master_volume(80);
        audio->set_user_volume(AudioGroup::Bgm, 90);
        audio->set_group_volume(AudioGroup::Bgm, 70, 100);
        audio->fade_group_pan(AudioGroup::Bgm, 2500, 100);
        render(*audio, 7);
    }
    SceneAudio scene(directory.string());
    scene.set_engine(&live, nullptr);
    check(!scene.adopt_playing_bgm("BGM/unknown") && scene.current_bgm() == -1,
          "unknown title asset cannot claim the live channel");
    check(scene.adopt_playing_bgm("bgm/title.PCM"), "match retained title name across case, slash and extension");
    const auto adopted = scene.capture_snapshot();
    check(adopted.bgm_id == 0 && adopted.bgm_mode == 1 && !adopted.bgm_alternate &&
          adopted.bgm_playing && adopted.bgm_position_frames == 7 && adopted.bgm_volume == 70 &&
          adopted.saved_bgm_volume == -1 && adopted.groups[0].volume_remaining_frames == 4403 &&
          adopted.groups[0].pan_remaining_frames == 4403,
          "adoption records current title position and in-progress script controls");
    check(scene.dispatch(command(0, 1)) && !live.channel_playing(1) && live.channel_position(0) == 7,
          "native same-ID mode1 keeps title channel without a second fading copy");
    check(render(live, 64) == render(expected, 64),
          "native selector BGM preserves every sample, user control and ramp");
    AudioEngine restored;
    SceneAudio restored_scene(directory.string());
    restored_scene.set_engine(&restored, nullptr);
    restored.set_master_volume(80); restored.set_user_volume(AudioGroup::Bgm, 90);
    check(restored_scene.restore_snapshot(scene.capture_snapshot()) &&
          render(restored, 64) == render(expected, 64), "retained title metadata can restore matching playback");
    check(scene.dispatch(command(0, 0)) && live.channel_playing(1) && live.channel_position(0) == 0,
          "native explicit restart still replaces the adopted title channel");
    render(live, 3);
    check(scene.dispatch(command(1, 1)) && scene.current_bgm() == 1 &&
          live.channel_position(0) == 0 && live.channel_position(1) == 3,
          "native different-ID mode1 still starts its new track");
    live.stop_bgm();
    check(!scene.adopt_playing_bgm("BGM/TITLE"), "stopped channel cannot be adopted");
    SceneAudio headless(directory.string()), missing((directory / "missing").string());
    missing.set_engine(&live, nullptr);
    check(!headless.adopt_playing_bgm("BGM/TITLE") && !missing.adopt_playing_bgm("BGM/TITLE"),
          "adoption requires both an audio engine and its names catalogue");

    std::vector<uint8_t> script{1,0,0,0,3,0,0,0,0,0,0x72,0x9D,0,
                              0x52,0,0,1,0,0,0,0,0,0x4F,0x2D,0x25,0x2E,0x35,0,
                              0x06,0,0,0x0D,1,0,0};
    const auto path = directory / "MES/selector.mes";
    write("MES/selector.mes", script);
    dc4scene::SceneRunner runner(directory.string());
    runner.set_audio(&live, nullptr);
    check(runner.load_script(path.string()), "load seeded selector audio fixture");
    check(live.play_bgm_file(title), "restart host title before selector handoff");
    render(live, 7);
    check(!runner.seed_dc2dm_bgm("BGM/TITLE"), "unseeded scene cannot inherit host title metadata");
    check(runner.seed_dc2dm_presentation(runner.frame()) && runner.seed_dc2dm_bgm("BGM/TITLE"),
          "DC2DM scene accepts audio only with its pending presentation seed");
    check(runner.advance_to_next_text() && runner.current_line() == "MENU" &&
          live.channel_position(0) == 7 && !live.channel_playing(1),
          "native raw52 through SceneRunner preserves the retained title");
    runner.advance_to_next_text();
    check(!runner.seed_dc2dm_bgm("BGM/TITLE"), "authored transition consumes permission to seed title metadata");
    for (uint16_t version : {0xAAB6, 0xABB6, 0x9969}) {
        script = {1,0,0,0,3,0,0,0,0,0,uint8_t(version),uint8_t(version >> 8),0,0};
        write("MES/other.mes", script);
        check(runner.load_script((directory / "MES/other.mes").string()) &&
              !runner.seed_dc2dm_presentation(runner.frame()) && !runner.seed_dc2dm_bgm("BGM/TITLE"),
              "other game profiles cannot adopt DC2DM title presentation or BGM");
    }
    std::filesystem::remove_all(directory);
}

static void dc2ty_character_voice_controls() {
    using namespace dc4;
    const auto directory = std::filesystem::temp_directory_path() /
        ("arisa32-ty-audio-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    check(std::filesystem::create_directory(directory), "create TY voice fixture directory");
    std::vector<uint8_t> bytes(36);
    bytes[0] = 'X'; bytes[1] = 'P'; bytes[2] = 'C'; bytes[3] = 'M';
    put32(bytes, 4, 8);
    bytes[12] = 1; bytes[14] = 1;
    put32(bytes, 16, 44100); put32(bytes, 20, 44100);
    bytes[24] = 1; bytes[26] = 8;
    for (int i = 0; i < 8; ++i) bytes[28 + i] = uint8_t(129 + i);
    const auto write_voice = [&](const std::string& name) {
        const auto path = directory / (name + ".pcm");
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
        check(bool(file), "write TY voice fixture");
    };
    const auto command = [](const std::string& name, bool ty = true) {
        MesCommand raw;
        raw.dc2ty = ty; raw.kind = CmdKind::Str; raw.op = ty ? 0x36 : 0x4D; raw.str = name;
        return mes_engine_command(raw);
    };
    const char* actors[] = {"otm","yme","kko","anz","akn","mhr","myk","yki",
                            "mik","myu","skr","sgn","wtr","kmk","mki","hrm"};
    AudioEngine audio;
    SceneAudio scene(directory.string());
    scene.set_engine(&audio, nullptr);
    for (size_t id = 0; id < 16; ++id) {
        const std::string name = std::string("VOICE/ty_myu01_d1_myu/ty_myu01_d1_myu_") + actors[id] + "01";
        const std::string actor = "TY actor " + std::to_string(id) + " (" + actors[id] + ")";
        write_voice(name);
        scene.set_character_volume(id, 100, true);
        check(scene.dispatch(command(name)), (actor + " dispatch").c_str());
        check(scene.voice_playing(), (actor + " starts playback: " + scene.last_error()).c_str());
        const int first_sample = render(audio, 1)[0];
        check(first_sample == 0, (actor + " mute sample=" + std::to_string(first_sample)).c_str());
        scene.set_character_volume(id, 100);
        check(render(audio, 1)[0] == 512, "unmuting the active TY actor updates its current voice");
        scene.set_character_volume(id, 0);
        check(scene.replay_voice() && render(audio, 1)[0] == 0,
              "zero TY character volume remains silent on replay");
        scene.set_character_volume(id, 100);
    }
    const std::string upper = "VOICE/ty_myu01_d1_myu/ty_myu01_d1_myu_OTM02";
    const std::string unknown = "VOICE/ty_myu01_d1_myu/ty_myu01_d1_myu_xyz01";
    const std::string system = "VOICE/SYS/sys_otm_02";
    const std::string ordinary = "VOICE/asystem/line_hrm01";
    const std::string modern = "VOICE/modern/line_asu01";
    for (const auto& name : {upper, unknown, system, ordinary, modern}) write_voice(name);
    scene.set_character_volume(0, 100, true);
    check(scene.dispatch(command(upper)) && render(audio, 1)[0] == 0, "TY actor matching ignores case");
    scene.set_character_volume(15, 100, true);
    check(scene.dispatch(command(unknown)) && render(audio, 1)[0] == 0,
          "unknown TY actor uses native slot15 and preserves its mute");
    const auto muted_snapshot = scene.capture_snapshot();
    AudioEngine restored_audio;
    SceneAudio restored(directory.string());
    restored.set_engine(&restored_audio, nullptr);
    restored.set_profile(MesProfileId::DC2TY);
    restored.set_character_volume(15, 0);
    check(restored.restore_snapshot(muted_snapshot) && restored.replay_voice() &&
          render(restored_audio, 1)[0] == 0, "restored TY dialogue replay preserves actor zero volume");
    check(restored.restore_snapshot(muted_snapshot, true) && render(restored_audio, 1)[0] == 0,
          "explicit TY snapshot replay applies actor controls");
    check(scene.dispatch(command(system)) && render(audio, 1)[0] == 256,
          "TY sys path bypasses muted dialogue actors");
    for (size_t id = 0; id < 16; ++id) scene.set_character_volume(id, 100, true);
    check(render(audio, 1)[0] == 512, "reapplying settings leaves a scripted TY system voice audible");
    check(scene.dispatch(command(ordinary)) && render(audio, 1)[0] == 0,
          "sys must begin a path component to bypass TY character mute");
    for (int channel = 9; channel < 12; ++channel)
        check(audio.play_pcm(channel, {1000}, 44100, 1, {true}), "seed all dialogue voice channels");
    scene.set_character_volume(15, 100, true);
    check(render(audio, 1)[0] == 0, "actor mute covers all four voice channels");
    scene.prepare_system_voice();
    check(render(audio, 1)[0] == 3768, "system voice preparation resets all four channel gains");
    audio.stop_group(AudioGroup::Voice);
    check(audio.play_pcm(8, {1000}, 44100, 1, {true}), "start direct host system voice");
    for (size_t id = 0; id < 16; ++id) scene.set_character_volume(id, 100, true);
    check(render(audio, 1)[0] == 1000, "host system voice is not remuted by the last dialogue actor");
    check(scene.replay_voice() && render(audio, 1)[0] == 0,
          "dialogue replay reclaims actor controls after a host system voice");
    scene.set_character_volume(0, 60);
    check(scene.dispatch(command(modern, false)) && render(audio, 1)[0] > 0,
          "switching out of TY retains the DC4 ASU alias");
    scene.set_character_volume(0, 100, true);
    check(render(audio, 1)[0] == 0, "DC4 alias still responds to its own actor control");
    std::filesystem::remove_all(directory);
}

int main(int argc, char** argv) {
    using namespace dc4;
    dc2dm_voice_prefetch();
    deferred_channel_adoption();
    dc2dm_retained_title_bgm();
    dc2ty_character_voice_controls();
    AudioEngine audio;
    check(audio.play_pcm(0, {100, 200, 300, 400, 500}, 44100, 1, {true, 1, 4}), "loop PCM load");
    auto output = render(audio, 8);
    const int16_t looped[] = {100, 200, 300, 400, 200, 300, 400, 200};
    for (size_t i = 0; i < 8; ++i)
        check(output[2 * i] == looped[i] && output[2 * i + 1] == looped[i], "exclusive loop end and intro");
    check(audio.seek_channel(0, 1.5), "seek fractional source frame");
    check(render(audio, 1)[0] == 250, "seek keeps interpolation fraction");
    double position = audio.channel_position(0);
    check(!audio.seek_channel(0, 1000) && audio.channel_position(0) == position,
          "invalid seek leaves playback unchanged");
    check(!audio.seek_channel(0, std::numeric_limits<double>::quiet_NaN()), "reject NaN source position");
    audio.set_group_volume(AudioGroup::Bgm, 50, 100);
    audio.fade_group_pan(AudioGroup::Bgm, 5000, 100);
    render(audio, 31);
    auto mixer = audio.capture_snapshot();
    auto expected_mix = render(audio, 256);
    AudioPreparedChannel prepared;
    prepared.channel = 0;
    prepared.pcm = {100, 200, 300, 400, 500};
    prepared.rate = 44100;
    prepared.channels = 1;
    prepared.options = {true, 1, 4};
    prepared.position_frames = mixer.channels[0].position_frames;
    check(audio.restore_channels({prepared}, mixer.groups), "restore prepared mixer snapshot");
    check(render(audio, 256) == expected_mix, "restored source position and ramps produce identical samples");
    position = audio.channel_position(0);
    auto invalid_groups = mixer.groups;
    invalid_groups[0].pan = std::numeric_limits<double>::infinity();
    check(!audio.restore_channels({prepared}, invalid_groups) && audio.channel_position(0) == position,
          "invalid controls do not mutate playback");
    auto invalid_track = prepared;
    invalid_track.position_frames = 1e30;
    check(!audio.restore_channels({invalid_track}, mixer.groups) && audio.channel_position(0) == position,
          "out-of-source snapshot does not mutate playback");
    check(!audio.restore_channels({prepared, prepared}, mixer.groups) && audio.channel_position(0) == position,
          "duplicate channels do not partially replace playback");
    audio.stop_all();
    audio.set_group_volume(AudioGroup::Bgm, 100);
    audio.set_group_pan(AudioGroup::Bgm, 0);

    // The dispatcher may stage a group pan before starting a voice this slice.
    audio.set_group_pan(AudioGroup::Voice, 0);
    audio.fade_group_pan(AudioGroup::Voice, 10000, 10);
    audio.play_pcm(8, {1000}, 44100, 1, {true});
    check(render(audio, 1)[0] == 1000, "new voice begins at pan ramp start");
    render(audio, 441);
    output = render(audio, 1);
    check(output[0] == 0 && output[1] == 1000, "new voice inherits pending pan ramp");
    audio.stop_all();
    audio.set_group_pan(AudioGroup::Voice, 0);

    check(audio.play_pcm(8, {0, 1000}, 22050, 1), "22050 mono load");
    output = render(audio, 4);
    check(output[0] == 0 && output[2] == 500 && output[4] == 1000 && output[6] == 1000,
          "native-rate interpolation and mono upmix");
    check(!audio.voice_playing(), "voice completes at source-rate duration");
    check(render(audio, 1)[0] == 0, "completed channel is silent");
    check(!audio.play_pcm(12, {1, 2}, 44100, 2), "reject out-of-range channel");
    check(!audio.play_pcm(0, {1}, 44100, 2), "reject partial stereo frame");
    check(!audio.play_pcm(0, {1}, 0, 1), "reject zero sample rate");

    audio.play_pcm(0, {30000}, 44100, 1, {true});
    audio.play_pcm(8, {30000}, 44100, 1, {true});
    check(render(audio, 1)[0] == 32767, "mix saturates instead of wrapping");
    audio.stop_group(AudioGroup::Voice);
    check(audio.channel_playing(0) && !audio.voice_playing(), "voice stop preserves BGM");
    audio.set_group_volume(AudioGroup::Bgm, 50);
    check(std::abs(int(render(audio, 1)[0]) - 1687) <= 1, "native 50 percent is -25 dB");
    audio.set_group_volume(AudioGroup::Bgm, 100);
    audio.set_group_pan(AudioGroup::Bgm, 10000);
    output = render(audio, 1);
    check(output[0] == 0 && output[1] == 30000, "positive DirectSound pan attenuates left");
    audio.set_group_pan(AudioGroup::Bgm, 0);
    audio.fade_group_pan(AudioGroup::Bgm, -10000, 10);
    render(audio, 441);
    output = render(audio, 1);
    check(output[0] == 30000 && output[1] == 0, "pan ramp reaches target");
    audio.set_group_pan(AudioGroup::Bgm, 0);
    audio.fade_out_bgm(10);
    check(!audio.channel_playing(0) && audio.channel_playing(1), "outgoing music uses channel 1");
    audio.play_pcm(0, {1000}, 44100, 1, {true});
    render(audio, 441);
    check(!audio.channel_playing(1) && audio.channel_playing(0), "outgoing fade preserves new BGM");
    audio.stop_all();

    audio.play_pcm(2, {1}, 44100, 1, {true});
    audio.play_pcm(6, {1}, 44100, 1, {true});
    check(audio.free_channel(AudioGroup::Se) == 3, "one-shot SE skips looping slot 2");
    audio.stop_group(AudioGroup::Se);
    check(!audio.channel_playing(2) && audio.channel_playing(6), "SE group stop preserves system effects");
    audio.stop_all();

    std::vector<uint8_t> pcm8(30);
    pcm8[0] = 'X'; pcm8[1] = 'P'; pcm8[2] = 'C'; pcm8[3] = 'M';
    put32(pcm8, 4, 2);
    pcm8[12] = 1; pcm8[14] = 1;  // PCM mono
    put32(pcm8, 16, 22050); put32(pcm8, 20, 22050);
    pcm8[24] = 1; pcm8[26] = 8; pcm8[28] = 0; pcm8[29] = 255;
    std::vector<int16_t> samples;
    uint32_t rate = 0; uint16_t channels = 0;
    check(decode_xpcm_pcm(pcm8.data(), pcm8.size(), &samples, &rate, &channels), "PCM8 decode");
    check(rate == 22050 && channels == 1 && samples[0] == -32768 && samples[1] == 32512,
          "PCM8 unsigned to signed conversion");
    check(!decode_xpcm_pcm(pcm8.data(), 29, &samples, &rate, &channels), "reject truncated payload");

    // Concurrent replacement used to invalidate the callback's vector storage.
    std::thread callback([&audio] { for (int i = 0; i < 1000; ++i) render(audio, 64); });
    for (int i = 0; i < 1000; ++i) {
        check(audio.play_pcm(8, {int16_t(i), int16_t(-i)}, 22050, 1), "concurrent voice replace");
        audio.stop_group(AudioGroup::Voice);
    }
    callback.join();
    audio.stop_all();

    if (argc > 1) {
        SceneAudio scene(argv[1]);
        VoiceArchive voices;
        std::string error;
        check(scene.load_names(&error), error.c_str());
        check(scene.bgm_names().size() > 1 && !scene.se_names().empty(), "real NAMES.DAT tables");
        check(voices.open(std::string(argv[1]) + "/VOICE.DAT", &error), error.c_str());
        scene.set_engine(&audio, &voices);
        MesCommand command;
        command.kind = CmdKind::Words4; command.op = 0x70; command.words[0] = 1;
        scene.dispatch(command);
        check(audio.channel_playing(0) && scene.current_bgm() == 1, "script BGM resolves and decodes");
        command.op = 0x71; command.words[0] = 0;
        scene.dispatch(command);
        check(audio.group_playing(AudioGroup::Se), "SE silent entry decodes");
        command.kind = CmdKind::Str; command.op = 0x4D;
        command.str = "VOICE\\system\\system_asa011";
        scene.dispatch(command);
        check(scene.voice_playing(), "script voice archive entry decodes");
        audio.stop_group(AudioGroup::Voice);
        check(scene.replay_voice() && scene.voice_playing(), "replay remembers archive blob");
        audio.stop_group(AudioGroup::Voice);
        command = MesCommand{};
        command.kind = CmdKind::Words4; command.op = 0x71;
        command.words[0] = 5; command.words[1] = 1;
        scene.dispatch(command);
        command.kind = CmdKind::Param2; command.op = 0x05; command.p1 = 50;
        scene.dispatch(command);
        command.kind = CmdKind::Words4; command.op = 0xAF;
        command.words[0] = uint16_t(-25); command.words[1] = 25;
        command.words[2] = 60; command.words[3] = 0;
        scene.dispatch(command);
        render(audio, 1235);
        auto scene_snapshot = scene.capture_snapshot();
        check(scene_snapshot.bgm_id == 1 && scene_snapshot.bgm_playing &&
              scene_snapshot.bgm_position_frames > 0 && scene_snapshot.looped_se.size() == 1 &&
              scene_snapshot.looped_se[0].id == 5 && scene_snapshot.looped_se[0].position_frames > 0,
              "scene snapshot records BGM and looping SE source positions");
        check(!scene_snapshot.last_voice_name.empty(), "finished voice name remains replayable");
        expected_mix = render(audio, 256);
        scene.reset_state();
        check(scene.restore_snapshot(scene_snapshot, false, &error), error.c_str());
        check(!scene.voice_playing(), "restore keeps finished voice silent by default");
        check(render(audio, 256) == expected_mix, "real tracks restore with identical next mixed samples");
        position = audio.channel_position(0);
        auto bad_snapshot = scene_snapshot;
        bad_snapshot.looped_se[0].position_frames = 1e30;
        check(!scene.restore_snapshot(bad_snapshot, false, &error) && audio.channel_position(0) == position &&
              scene.current_bgm() == 1, "invalid source position preserves live scene audio");
        bad_snapshot = scene_snapshot; bad_snapshot.version = 0;
        check(!scene.restore_snapshot(bad_snapshot, false, &error) && error.find("version") != std::string::npos,
              "old audio snapshot fails explicitly");
        bad_snapshot = scene_snapshot; bad_snapshot.last_voice_name = "VOICE\\missing\\not_present";
        check(!scene.restore_snapshot(bad_snapshot, false, &error) && audio.channel_position(0) == position,
              "missing voice asset preserves live scene audio");
        check(scene.restore_snapshot(scene_snapshot, true, &error) && scene.voice_playing(),
              "explicit restore replay starts the retained voice");
        audio.stop_group(AudioGroup::Voice);
        audio.stop_channel(0);
        auto completed_snapshot = scene.capture_snapshot();
        check(!completed_snapshot.bgm_playing && scene.restore_snapshot(completed_snapshot, false, &error) &&
              !audio.channel_playing(0), "inactive BGM is not restarted by restore");

        SceneAudio headless(argv[1]);
        command = MesCommand{};
        command.kind = CmdKind::Words4; command.op = 0x70;
        command.words[0] = 1; command.words[1] = 2;
        headless.dispatch(command);
        command.op = 0x71; command.words[0] = 5; command.words[1] = 1;
        headless.dispatch(command);
        command.kind = CmdKind::Str; command.op = 0x4D;
        command.str = "VOICE\\system\\system_asa011";
        headless.dispatch(command);
        command.kind = CmdKind::Param2; command.op = 0x05; command.p1 = 75;
        headless.dispatch(command);
        command.kind = CmdKind::Words4; command.op = 0xAF;
        command.words[0] = uint16_t(-30); command.words[1] = 20;
        command.words[2] = 42; command.words[3] = 1;
        headless.dispatch(command);
        auto logical = headless.capture_snapshot();
        check(logical.bgm_id == 1 && logical.bgm_mode == 2 && logical.bgm_playing &&
              logical.looped_se.size() == 1 && !logical.last_voice_name.empty() &&
              logical.bgm_volume == 75 && logical.groups[1].pan == -3000 &&
              logical.groups[1].pan_target == 2000, "headless dispatcher preserves logical audio state");
        check(headless.restore_snapshot(logical, false, &error), error.c_str());
        check(headless.capture_snapshot().last_voice_name == logical.last_voice_name &&
              headless.capture_snapshot().bgm_mode == 2, "headless snapshot survives validation and restore");
        SceneAudio missing_data(std::string(argv[1]) + "/missing-game-data");
        check(!missing_data.restore_snapshot(logical, false, &error) && error.find("NAMES.DAT") != std::string::npos,
              "missing source data has an explanatory restore failure");
        scene.reset_state();
        check(!audio.group_playing(AudioGroup::Bgm) && !scene.voice_playing(), "new-game audio reset");
        size_t decoded_effects = 0;
        for (const auto& file : std::filesystem::directory_iterator(std::string(argv[1]) + "/SE")) {
            if (!file.is_regular_file()) continue;
            std::ifstream input(file.path(), std::ios::binary);
            std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
            check(decode_xpcm_pcm(bytes.data(), bytes.size(), &samples, &rate, &channels),
                  file.path().string().c_str());
            ++decoded_effects;
        }
        std::printf("real assets: %zu BGM names, %zu SE names, %zu SE files decoded, voice replay OK\n",
                    scene.bgm_names().size(), scene.se_names().size(), decoded_effects);
    }
    std::puts("audio tests passed");
    return 0;
}
