#include "arisa32/audio.h"
#include "arisa32/messtream.h"
#include "arisa32/resource.h"
#include "arisa32/scene.h"
#include "arisa32/scene_audio.h"
#include "arisa32/system_menu.h"
#include "arisa32/voice.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
void require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
uint16_t u16(const std::vector<uint8_t>& bytes, size_t at) {
    return uint16_t(bytes.at(at)) | uint16_t(bytes.at(at + 1)) << 8;
}
uint32_t u32(const std::vector<uint8_t>& bytes, size_t at) {
    return uint32_t(u16(bytes, at)) | uint32_t(u16(bytes, at + 2)) << 16;
}
std::string normalized(std::string name) {
    for (char& c : name) c = c == '\\' ? '/' : char(std::tolower(static_cast<unsigned char>(c)));
    return name;
}
bool audible(const std::vector<int16_t>& pcm) {
    return std::any_of(pcm.begin(), pcm.end(), [](int16_t sample) { return sample != 0; });
}
double energy(const std::vector<int16_t>& pcm) {
    double sum = 0;
    for (int16_t sample : pcm) sum += double(sample) * sample;
    return sum;
}
struct Decoded {
    std::vector<uint8_t> blob;
    std::vector<int16_t> pcm;
    uint32_t rate = 0;
    uint16_t channels = 0;
};
Decoded decode(dc4::VoiceArchive& archive, const dc4::VoiceEntry& entry, bool require_sound = true) {
    Decoded result;
    std::string error;
    const auto name = entry.str();
    require(archive.read_entry(entry, &result.blob, &error), name + ": " + error);
    require(result.blob.size() >= 32 && std::memcmp(result.blob.data(), "XPCM", 4) == 0,
            name + ": XPCM header");
    require(dc4::decode_xpcm_pcm(result.blob.data(), result.blob.size(), &result.pcm,
                                &result.rate, &result.channels), name + ": decode failed");
    require(result.rate > 0 && (result.channels == 1 || result.channels == 2) &&
            !result.pcm.empty() && result.pcm.size() % result.channels == 0, name + ": PCM metadata");
    if (u32(result.blob, 8) == 0x0303) {
        require(u16(result.blob, 12) == 1 && u16(result.blob, 26) == 16, name + ": PCM16 format");
        require(result.channels == u16(result.blob, 14) && result.rate == u32(result.blob, 16),
                name + ": decoded channel/rate metadata");
        require(u16(result.blob, 24) == result.channels * 2 &&
                u32(result.blob, 20) == result.rate * result.channels * 2, name + ": WAVE byte rate");
        require(result.pcm.size() * sizeof(int16_t) == u32(result.blob, 4),
                name + ": decoded length must exclude transform padding");
    }
    require(!require_sound || audible(result.pcm), name + ": decoded PCM is silent");
    return result;
}
const dc4::VoiceEntry& find(dc4::VoiceArchive& archive, const std::string& name) {
    const auto* entry = archive.find(name);
    require(entry != nullptr, "voice lookup failed: " + name);
    return *entry;
}
std::vector<int16_t> drain_voice(dc4::AudioEngine& audio) {
    std::vector<int16_t> output;
    std::array<int16_t, 4096> block{};
    const size_t limit = size_t(dc4::AudioEngine::kOutputRate) * 2 * 60;
    while (audio.voice_playing() && output.size() < limit) {
        audio.render_pcm(block.data(), block.size() / 2);
        output.insert(output.end(), block.begin(), block.end());
    }
    require(!audio.voice_playing(), "voice did not finish within one minute");
    return output;
}
void isolate_voice(dc4::AudioEngine& audio) {
    audio.set_master_volume(100);
    audio.set_user_volume(dc4::AudioGroup::Bgm, 0);
    audio.set_user_volume(dc4::AudioGroup::Se, 0);
    audio.set_user_volume(dc4::AudioGroup::Voice, 100);
    audio.set_group_volume(dc4::AudioGroup::Voice, 100);
    audio.set_group_pan(dc4::AudioGroup::Voice, 0);
}
void system_voices(dc4::VoiceArchive& archive, dc4ui::SystemMenu& menu) {
    std::set<uint32_t> decoded;
    for (int actor = 0; actor < 16; ++actor) {
        menu.set_variable(100, 56, actor);
        for (int event : {1, 2, 7, 8, 10, 14, 15, 16, 17, 18, 22, 23, 24, 25, 26, 27}) {
            const auto name = menu.system_voice(event);
            require(!name.empty(), "missing system voice mapping for actor/event " +
                    std::to_string(actor) + "/" + std::to_string(event));
            const auto& entry = find(archive, name);
            auto upper = name;
            for (char& c : upper) c = c == '/' ? '\\' : char(std::toupper(static_cast<unsigned char>(c)));
            require(find(archive, upper + ".PCM").offset == entry.offset, name + ": case/slash lookup");
            if (decoded.insert(entry.offset).second) decode(archive, entry);
        }
    }
    menu.set_variable(100, 56, 0);
    std::cout << "System voices: " << decoded.size() << " distinct original samples decoded\n";
}
void scene_and_controls(const std::string& data, dc4::VoiceArchive& archive, dc4ui::SystemMenu& menu) {
    dc4::AudioEngine audio;
    isolate_voice(audio);
    dc4scene::SceneRunner scene(data);
    scene.set_audio(&audio, &archive);
    scene.set_use_system_menu(true);
    scene.set_animation_enabled(false);
    scene.set_effects_enabled(false);
    require(scene.load_ui_script(data + "/MES/start.mes"), "load TY scene UI");
    // The early Hydrangea scene is a morning conversation, before route branches.
    require(scene.load_script(data + "/MES/hyd_0611_a2_com.mes"), "load Hydrangea morning scene");
    bool voiced_line = false;
    for (int step = 0; step < 100 && !scene.scene_ended(); ++step) {
        if (scene.waiting_for_time()) scene.finish_script_wait();
        if (!scene.pending_movie().empty()) scene.finish_movie();
        if (scene.advance_to_next_text() && !scene.current_line().empty() && !scene.last_voice().empty()) {
            voiced_line = true;
            break;
        }
        scene.finish_animations();
    }
    require(voiced_line, "real scene did not attach a voice to dialogue: " + scene.last_error());
    require(scene.voice_playing(), "real scene voice did not start: " + scene.last_voice());
    decode(archive, find(archive, scene.last_voice()));
    isolate_voice(audio);
    const auto first = drain_voice(audio);
    require(audible(first), "real dialogue mixer output is silent");
    require(scene.replay_voice() && scene.voice_playing(), "real dialogue replay did not start");
    require(drain_voice(audio) == first, "real dialogue replay differs from original playback");
    std::cout << "Scene dialogue/replay: " << scene.last_voice() << '\n';

    dc4::MesFile script;
    require(script.load_file(data + "/MES/hyd_0611_a2_com.mes"), "load morning voice commands");
    dc4::MesStream stream(script);
    dc4::MesCommand raw, otome;
    bool found_otome = false;
    while (stream.next(&raw)) {
        auto command = dc4::mes_engine_command(raw);
        const auto name = normalized(command.str);
        if (command.kind == dc4::CmdKind::Str && command.opcode() == dc4::Opcode::VOICE &&
            name.find("hyd_0611_a2_com_otm") != std::string::npos) {
            otome = std::move(command);
            found_otome = true;
            break;
        }
    }
    require(found_otome, "morning scene has no Otome voice command");
    decode(archive, find(archive, otome.str));
    dc4::SceneAudio controls(data);
    controls.set_engine(&audio, &archive);
    controls.set_profile(dc4::MesProfileId::DC2TY);
    require(controls.dispatch(otome) && controls.voice_playing(), "real Otome command playback");
    const auto normal = drain_voice(audio);
    require(audible(normal), "real Otome voice is silent at default volume");
    controls.set_character_volume(0, 100, true);
    require(controls.replay_voice(), "replay muted Otome");
    require(!audible(drain_voice(audio)), "Otome mute did not silence the real sample");
    controls.set_character_volume(0, 0);
    require(controls.replay_voice(), "replay zero-volume Otome");
    require(!audible(drain_voice(audio)), "zero actor volume did not silence the real sample");
    controls.set_character_volume(0, 50);
    require(controls.replay_voice(), "replay reduced-volume Otome");
    const auto reduced = drain_voice(audio);
    require(audible(reduced) && energy(reduced) < energy(normal), "actor volume did not reduce real PCM");
    controls.set_character_volume(0, 100);
    audio.set_user_volume(dc4::AudioGroup::Voice, 0);
    require(controls.replay_voice(), "replay with global voice disabled");
    require(!audible(drain_voice(audio)), "global voice volume did not mute real PCM");
    audio.set_user_volume(dc4::AudioGroup::Voice, 100);
    audio.set_master_volume(0);
    require(controls.replay_voice(), "replay with master disabled");
    require(!audible(drain_voice(audio)), "master volume did not mute real PCM");
    audio.set_master_volume(100);
    const auto system = decode(archive, find(archive, menu.system_voice(1)));
    for (size_t actor = 0; actor < 16; ++actor) controls.set_character_volume(actor, 100, true);
    controls.prepare_system_voice();
    require(audio.play_voice_blob(system.blob.data(), system.blob.size()), "host system voice playback");
    for (size_t actor = 0; actor < 16; ++actor) controls.set_character_volume(actor, 100, true);
    require(audible(drain_voice(audio)), "host system voice was muted by dialogue actor controls");
    require(controls.replay_voice(), "dialogue replay after host system voice");
    require(!audible(drain_voice(audio)), "dialogue replay lost its actor mute after a system voice");
}
void corpus(dc4::VoiceArchive& archive) {
    size_t failed = 0, silent = 0, checked = 0;
    uint64_t frames = 0;
    for (const auto& entry : archive.entries()) {
        try {
            const auto sample = decode(archive, entry, false);
            frames += sample.pcm.size() / sample.channels;
            if (!audible(sample.pcm)) ++silent;
        } catch (const std::exception& error) {
            if (failed++ < 20) std::cerr << error.what() << '\n';
        }
        if (++checked % 1000 == 0) std::cout << "Archive voices checked: " << checked << '\n';
    }
    std::cout << "Archive: " << checked << " entries, " << frames << " PCM frames, "
              << silent << " silent, " << failed << " failed\n";
    require(failed == 0, "original voice corpus contains decode failures");
}
} // namespace

int main(int argc, char** argv) {
    std::string data;
    if (const char* env = std::getenv("DC2TY_DATA")) data = env;
    bool all = false, positional = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--all") all = true;
        else if (!positional && std::string(argv[i]).rfind("--", 0) != 0) { data = argv[i]; positional = true; }
        else { std::cerr << "Usage: dc2ty_voice_test [AdvData] [--all]\n"; return 2; }
    }
    if (data.empty()) { std::cout << "DC2TY_DATA unset; installed voice test skipped.\n"; return 77; }
    try {
        dc4::configure_resources(data, {}, "windows-31j");
        dc4::VoiceArchive archive;
        std::string error;
        require(archive.open(data + "/VOICE.DAT", &error), "open original voice archive: " + error);
        require(archive.loaded() && !archive.entries().empty(), "original voice archive is empty");
        dc4ui::SystemMenu menu;
        require(menu.load(data, &error) && menu.is_dc2ty(), "load TY system menu: " + error);
        const auto config = menu.config();
        require(config.master_enabled && config.master_volume > 0 && config.voice_enabled && config.voice_volume > 0,
                "default TY configuration mutes voices");
        for (size_t actor = 0; actor < 16; ++actor)
            require(!config.character_muted[actor] && config.character_volume[actor] > 0, "default TY actor muted");
        system_voices(archive, menu);
        scene_and_controls(data, archive, menu);
        if (all) corpus(archive);
        std::cout << "DC2TY original voice lookup, decode, dialogue/replay and mixer controls passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
