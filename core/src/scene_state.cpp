#include "arisa32/scene.h"
#include "arisa32/resource.h"
#include "arisa32/bgm.h"
#include "arisa32/mem_safe.h"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace dc4scene {
namespace {
constexpr size_t kMaxSave = 2 * 1024 * 1024;
constexpr char kMagic[8] = {'D','C','4','S','A','V','E','\0'};
uint64_t checksum(const std::vector<uint8_t>& bytes) {
    uint64_t value = 14695981039346656037ull;
    for (uint8_t b : bytes) { value ^= b; value *= 1099511628211ull; }
    return value;
}
struct Writer {
    std::vector<uint8_t> bytes;
    void u64(uint64_t value) { for (int i = 0; i < 8; ++i) bytes.push_back(uint8_t(value >> (i * 8))); }
    void u32(uint32_t value) { for (int i = 0; i < 4; ++i) bytes.push_back(uint8_t(value >> (i * 8))); }
    void number(double value) { uint64_t bits; std::memcpy(&bits, &value, 8); u64(bits); }
    void flag(bool value) { bytes.push_back(value ? 1 : 0); }
    void str(const std::string& value) {
        if (value.size() > 65536) throw std::runtime_error("save string too large");
        u32(uint32_t(value.size())); bytes.insert(bytes.end(), value.begin(), value.end());
    }
};
struct Reader {
    const std::vector<uint8_t>& bytes;
    size_t at = 0;
    void need(size_t count) const { if (at > bytes.size() || count > bytes.size() - at) throw std::runtime_error("truncated save"); }
    uint64_t u64() { need(8); uint64_t value = 0; for (int i = 0; i < 8; ++i) value |= uint64_t(bytes[at++]) << (i * 8); return value; }
    uint32_t u32() { need(4); uint32_t value = 0; for (int i = 0; i < 4; ++i) value |= uint32_t(bytes[at++]) << (i * 8); return value; }
    int32_t integer() { uint32_t bits = u32(); int32_t result; std::memcpy(&result, &bits, 4); return result; }
    double number() { uint64_t bits = u64(); double value; std::memcpy(&value, &bits, 8); if (!std::isfinite(value)) throw std::runtime_error("invalid save number"); return value; }
    bool flag() { need(1); const auto value = bytes[at++]; if (value > 1) throw std::runtime_error("invalid save flag"); return value != 0; }
    uint32_t count(uint32_t limit) { const auto value = u32(); if (value > limit) throw std::runtime_error("save collection too large"); return value; }
    std::string str(size_t limit = 65536) {
        size_t size = count(uint32_t(limit)); need(size);
        std::string value(reinterpret_cast<const char*>(bytes.data() + at), size); at += size;
        if (value.find('\0') != std::string::npos) throw std::runtime_error("embedded NUL in save string");
        return value;
    }
};
bool resource_name(const std::string& name, bool allow_separator = true) {
    if (name.empty() || name.size() > 255 || name.find(':') != std::string::npos ||
        name.find("..") != std::string::npos || name[0] == '/' || name[0] == '\\') return false;
    if (!allow_separator && name.find_first_of("/\\") != std::string::npos) return false;
    return true;
}
void write_vm(Writer& out, const dc4::MesVmSnapshot& vm) {
    out.u32(vm.version); out.u64(vm.script_fingerprint); out.u32(vm.pc);
    out.u32(uint32_t(vm.variables.size()));
    for (const auto& item : vm.variables) { out.u32(item.first.first); out.u32(item.first.second); out.u32(uint32_t(item.second)); }
    out.str(vm.entry_label); out.u32(uint32_t(vm.choices.size()));
    for (const auto& choice : vm.choices) { out.str(choice.label); out.str(choice.text); }
    out.flag(vm.ended); out.flag(vm.transferring); out.flag(vm.waiting_for_choice);
}
dc4::MesVmSnapshot read_vm(Reader& in) {
    dc4::MesVmSnapshot vm;
    vm.version = in.u32(); vm.script_fingerprint = in.u64(); vm.pc = in.u32();
    const auto count = in.count(8192);
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t bank = in.u32(), index = in.u32(); const int32_t value = in.integer();
        if (bank > 65535 || index > 65535 || !vm.variables.emplace(std::make_pair(uint16_t(bank), uint16_t(index)), value).second)
            throw std::runtime_error("invalid or duplicate saved variable");
    }
    vm.entry_label = in.str(256);
    const auto choices = in.count(64);
    for (uint32_t i = 0; i < choices; ++i) { dc4::MesChoice choice; choice.label = in.str(256); choice.text = in.str(); vm.choices.push_back(std::move(choice)); }
    vm.ended = in.flag(); vm.transferring = in.flag(); vm.waiting_for_choice = in.flag();
    return vm;
}
void write_audio(Writer& out, const dc4::SceneAudioSnapshot& audio) {
    out.u32(audio.version); out.u32(uint32_t(audio.bgm_id)); out.u32(uint32_t(audio.bgm_mode));
    out.flag(audio.bgm_alternate); out.flag(audio.bgm_playing); out.number(audio.bgm_position_frames);
    out.u32(uint32_t(audio.looped_se.size()));
    for (const auto& se : audio.looped_se) { out.u32(uint32_t(se.id)); out.u32(uint32_t(se.channel)); out.number(se.position_frames); }
    out.str(audio.last_voice_name); out.u32(uint32_t(audio.bgm_volume)); out.u32(uint32_t(audio.saved_bgm_volume));
    for (const auto& group : audio.groups) {
        out.number(group.volume_db); out.number(group.volume_target_db); out.u64(group.volume_remaining_frames);
        out.number(group.pan); out.number(group.pan_target); out.u64(group.pan_remaining_frames);
    }
}
dc4::SceneAudioSnapshot read_audio(Reader& in) {
    dc4::SceneAudioSnapshot audio;
    audio.version = in.u32(); audio.bgm_id = in.integer(); audio.bgm_mode = in.integer();
    audio.bgm_alternate = in.flag(); audio.bgm_playing = in.flag(); audio.bgm_position_frames = in.number();
    auto count = in.count(3);
    for (uint32_t i = 0; i < count; ++i) { dc4::SceneLoopedSeSnapshot se; se.id = in.integer(); se.channel = in.integer(); se.position_frames = in.number(); audio.looped_se.push_back(se); }
    audio.last_voice_name = in.str(260); audio.bgm_volume = in.integer(); audio.saved_bgm_volume = in.integer();
    for (auto& group : audio.groups) {
        group.volume_db = in.number(); group.volume_target_db = in.number(); group.volume_remaining_frames = in.u64();
        group.pan = in.number(); group.pan_target = in.number(); group.pan_remaining_frames = in.u64();
    }
    return audio;
}
} // namespace

bool SceneRunner::capture_state(std::vector<uint8_t>* bytes, std::string* capture_error) {
    try {
        if (!bytes) throw std::runtime_error("missing snapshot destination");
        if (!pending_movie_.empty() || script_wait_pending_)
            throw std::runtime_error("wait for the current movie or timed sequence to finish before saving");
        dc4::MesVmSnapshot snapshot;
        std::string error;
        if (!vm_.capture_snapshot(&snapshot, &error)) throw std::runtime_error(error);
        if (!resource_name(script_name_, false)) throw std::runtime_error("no scenario is loaded");
        Writer out;
        out.str(script_name_); write_vm(out, snapshot);
        out.str(current_line_); out.str(speaker_); out.flag(scene_end_); out.flag(message_visible_); out.flag(preserve_scene_);
        out.u32(uint32_t(transition_type_)); out.number(camera_scale_); out.number(camera_x_); out.number(camera_y_);
        out.u32(uint32_t(layers_.size()));
        for (const auto& layer : layers_) { out.u32(uint32_t(layer.first)); out.str(layer.second.name); }
        out.u32(uint32_t(sprites_.size()));
        for (const auto& sprite : sprites_) {
            out.str(sprite.name); out.str(sprite.image); out.number(sprite.scale);
            out.u32(uint32_t(sprite.x)); out.u32(uint32_t(sprite.y)); out.u32(uint32_t(sprite.alpha)); out.u32(uint32_t(sprite.z)); out.flag(sprite.visible);
            out.u32(uint32_t(sprite.cur_x)); out.u32(uint32_t(sprite.cur_y));
        }
        write_audio(out, scene_audio_.capture_snapshot());
        out.u32(camera_duration_); out.u32(uint32_t(camera_motion_.size()));
        for (const auto& motion : camera_motion_) {
            out.number(motion.from_scale); out.number(motion.to_scale);
            out.number(motion.from_x); out.number(motion.from_y);
            out.number(motion.to_x); out.number(motion.to_y);
            out.number(motion.elapsed_ms); out.number(motion.duration_ms);
        }
        // v4: COMU phone chat state (visibility + timeline in scene order).
        out.flag(comu_visible_);
        out.u32(uint32_t(comu_items_.size()));
        for (const auto& item : comu_items_) {
            out.u32(uint32_t(item.contact));
            out.u32(uint32_t(item.stamp));
            out.str(item.text);
        }
        if (out.bytes.size() > kMaxSave) throw std::runtime_error("save exceeds size limit");
        Writer header;
        header.bytes.insert(header.bytes.end(), kMagic, kMagic + 8);
        header.u32(4); header.u32(uint32_t(out.bytes.size())); header.u64(checksum(out.bytes));
        header.bytes.insert(header.bytes.end(), out.bytes.begin(), out.bytes.end());
        *bytes = std::move(header.bytes);
        if (capture_error) capture_error->clear();
        return true;
    } catch (const std::exception& error) {
        if (capture_error) *capture_error = error.what();
        return false;
    }
}

bool SceneRunner::save_state(const std::string& filename) {
    try {
        std::vector<uint8_t> bytes;
        std::string error;
        if (!capture_state(&bytes, &error)) throw std::runtime_error(error);
        const std::filesystem::path target(filename), temporary = target.string() + ".tmp";
        if (!target.parent_path().empty()) std::filesystem::create_directories(target.parent_path());
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file) throw std::runtime_error("cannot create save: " + temporary.string());
        file.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
        file.flush();
        if (!file) throw std::runtime_error("cannot write complete save");
        file.close();
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("cannot replace save (Windows error " + std::to_string(GetLastError()) + ")");
#else
        std::filesystem::rename(temporary, target);
#endif
        last_error_.clear();
        return true;
    } catch (const std::exception& error) { last_error_ = error.what(); return false; }
}

bool SceneRunner::load_state(const std::string& filename) {
    try {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file) throw std::runtime_error("save not found: " + filename);
        const auto size = file.tellg();
        if (size < 24) throw std::runtime_error("invalid save size");
        file.seekg(0);
        std::vector<uint8_t> bytes(size_t(size), 0);
        if (!file.read(reinterpret_cast<char*>(bytes.data()), size)) throw std::runtime_error("cannot read complete save");
        std::string error;
        bool ok = false;
        if (bytes.size() >= 8 && !std::memcmp(bytes.data(), kMagic, 8)) {
            ok = restore_state(bytes, &error);
        } else {
            ok = restore_pc_save(bytes, &error);
        }
        if (!ok) throw std::runtime_error(error);
        last_error_.clear();
        return true;
    } catch (const std::exception& error) { last_error_ = error.what(); return false; }
}

bool SceneRunner::restore_pc_save(const std::vector<uint8_t>& bytes, std::string* restore_error) {
    try {
        if (bytes.size() < 0x1000) throw std::runtime_error("save file too small to be a PC save");

        std::string script;
        for (size_t i = 8; i < 8 + 64 && i < bytes.size(); ++i) {
            if (bytes[i] == 0) break;
            script.push_back(char(bytes[i]));
        }
        if (script.empty()) throw std::runtime_error("no scenario name in PC save");
        if (script.find('.') == std::string::npos) script += ".mes";

        std::string resolved = dc4::find_file_ci(data_ + "/MES", script);
        if (resolved.empty()) resolved = script;

        uint32_t pc = 0;
        if (bytes.size() >= 0xb04) {
            std::memcpy(&pc, bytes.data() + 0xb00, 4);
        }

        std::string bg;
        if (bytes.size() >= 0xd8 + 64) {
            for (size_t i = 0xd8; i < 0xd8 + 64; ++i) {
                if (bytes[i] == 0) break;
                char c = char(bytes[i]);
                if (c == '\\') c = '/';
                bg.push_back(c);
            }
        }

        vm_.reset();
        if (!load_script(data_ + "/MES/" + resolved)) {
            throw std::runtime_error("cannot load script: " + resolved + " (" + last_error_ + ")");
        }

        if (bytes.size() >= 0x184ec + 1100) {
            for (uint16_t b = 0; b <= 10; ++b) {
                for (uint16_t idx = 0; idx < 100; ++idx) {
                    uint8_t val = bytes[0x184ec + b * 100 + idx];
                    set_variable(b, idx, val);
                }
            }
        }
        // Force ReplayMode / Scenario Mode flag to 0 for story saves so branch doesn't exit to SCE_END
        set_variable(9, 7, 0);

        // Ensure all route clear / branch flags in Bank 100 are active so choices are NEVER skipped
        for (uint16_t i = 100; i <= 119; ++i) {
            set_variable(100, i, 1);
        }

        layers_.clear();
        for (auto& s : sprites_) s = BstSprite{};
        // Only clear character / scene scratch sheets (< 37). Keep UI sheets (>= 37) intact!
        for (auto it = sheets_.begin(); it != sheets_.end(); ) {
            if (it->first < 37) {
                it = sheets_.erase(it);
            } else {
                ++it;
            }
        }
        if (sheets_.find(37) == sheets_.end()) {
            std::string start_mes = dc4::find_file_ci(data_ + "/MES", "start.mes");
            if (!start_mes.empty()) load_sheets(data_ + "/MES/" + start_mes);
            std::string win_size = dc4::find_file_ci(data_ + "/MES", "win_size.mes");
            if (!win_size.empty()) load_ui_script(data_ + "/MES/" + win_size);
            if (sheets_.find(37) == sheets_.end()) {
                show_message_window("SYSTEM/MESWIN.CRX");
            }
        }
        if (buttons_.empty()) {
            std::string win_size = dc4::find_file_ci(data_ + "/MES", "win_size.mes");
            if (!win_size.empty()) load_ui_script(data_ + "/MES/" + win_size);
        }
        scene_chips_.clear();
        camera_scale_ = 1.0f;
        camera_x_ = 0.0f;
        camera_y_ = 0.0f;
        camera_duration_ = 5;
        camera_motion_.clear();
        current_line_.clear();
        speaker_.clear();
        std::fill(pending_sprites_.begin(), pending_sprites_.end(), false);

        uint32_t final_seek_pc = pc;
        if (pc >= 3 && pc < mes_.code.size()) {
            bool prev_anim = animation_enabled_;
            animation_enabled_ = false;

            std::string target_bg;
            std::vector<std::string> target_slots(sprites_.size());
            std::vector<int> target_slot_ids(sprites_.size(), 0);

            dc4::MesStream scan(mes_);
            dc4::MesCommand c;
            while (scan.next(&c)) {
                if (c.pos > pc) break;
                if (c.pos == pc) {
                    if (c.kind == dc4::CmdKind::Text) {
                        if (c.op == 0x6A || c.op == 0x4A || c.op == 0x4D) {
                            speaker_ = c.str;
                        } else {
                            current_line_ = c.str;
                            final_seek_pc = scan.pos();
                            break;
                        }
                    } else {
                        break;
                    }
                }
                if (c.op == 0) continue;

                using K = dc4::CmdKind;
                if (c.kind == K::Words4 && c.op == 0x8B) {
                    set_sprite_transform(c.words[0], c.words[1], c.words[2], c.words[3]);
                } else if (c.kind == K::Words4 && c.op == 0x8E) {
                    queue_sprite_slide(c.words[0], c.words[1], c.words[2], c.words[3]);
                } else if (c.kind == K::Words4 && c.op == 0x8C) {
                    queue_sprite_effect(c.words[0], c.words[1], c.words[2], c.words[3]);
                } else if (c.kind == K::Words4 && c.op == 0x99) {
                    camera_duration_ = c.words[0];
                } else if (c.kind == K::Words4 && c.op == 0x9D && c.words[0] < 2) {
                    apply_camera(c.words[0], c.words[1], c.words[2], c.words[3]);
                } else if (c.kind == K::ParamStr && c.op == 0x3C) {
                    int slot = c.p1;
                    int idx = -1;
                    if (slot >= 4 && slot <= 8) idx = slot - 4;
                    else if (slot >= 25 && slot <= 27) idx = 12 + (slot - 25);
                    if (c.str == "-") {
                        if (idx >= 0 && size_t(idx) < sprites_.size()) {
                            sprites_[idx].visible = false;
                            pending_sprites_[idx] = true;
                            target_slots[idx].clear();
                        } else if (slot <= 1) {
                            target_bg.clear();
                        }
                    } else if (slot <= 1) {
                        target_bg = c.str;
                    } else if (idx >= 0 && size_t(idx) < sprites_.size()) {
                        std::string name = c.str;
                        while (!name.empty() && ((unsigned char)name.front() < 0x20 || name.front() == '!')) {
                            name.erase(name.begin());
                        }
                        bool is_dash = name.size() >= 2 && name[0] == '-' && name[1] == '-';
                        if (!is_dash) {
                            sprites_[idx].name = name;
                        }
                        sprites_[idx].visible = true;
                        sprites_[idx].z = (slot >= 25) ? 2 : (slot >= 19 ? 1 : 0);
                        target_slots[idx] = sprites_[idx].name;
                        target_slot_ids[idx] = slot;
                        if (slot >= 4 && slot <= 8) pending_sprites_[slot - 4] = true;
                    }
                } else if (c.kind == K::ParamStr && (c.op == 0x3F || c.op == 0x40 || c.op == 0x41)) {
                    int idx = -1;
                    if (c.op == 0x3F) {
                        if (c.p1 < 5) idx = int(c.p1);
                    } else if (c.op == 0x40) {
                        idx = (effect_sprite_ >= 0 && size_t(effect_sprite_) < sprites_.size()) ? effect_sprite_ : 12;
                    } else if (c.op == 0x41) {
                        if (c.p1 < 5) idx = int(c.p1);
                    }
                    if (idx >= 0 && size_t(idx) < sprites_.size()) {
                        if (c.str == "-" || c.str == "-n") {
                            sprites_[size_t(idx)].visible = false;
                            target_slots[size_t(idx)].clear();
                            pending_sprites_[size_t(idx)] = false;
                        } else if (c.str == ".") {
                            if (!target_slots[size_t(idx)].empty()) {
                                sprites_[size_t(idx)].visible = true;
                                pending_sprites_[size_t(idx)] = true;
                            }
                        } else if (c.str == "--") {
                            if (!target_slots[size_t(idx)].empty()) {
                                sprites_[size_t(idx)].visible = true;
                                pending_sprites_[size_t(idx)] = true;
                            } else {
                                sprites_[size_t(idx)].visible = false;
                                pending_sprites_[size_t(idx)] = false;
                            }
                        }
                    }
                } else if (c.kind == K::Str && (c.op == 0x49 || c.op == 0x30)) {
                    if (c.str == ":n") { preserve_scene_ = true; continue; }
                    if (c.str == ":r") { preserve_scene_ = false; continue; }
                    if (c.str.find('\\') != std::string::npos || c.str.find('/') != std::string::npos) {
                        target_bg = c.str;
                        scene_chips_.clear();
                        if (!preserve_scene_) {
                            for (size_t i = 0; i < sprites_.size(); ++i) {
                                if (!pending_sprites_[i]) {
                                    sprites_[i].visible = false;
                                    target_slots[i].clear();
                                }
                            }
                        }
                        std::fill(pending_sprites_.begin(), pending_sprites_.end(), false);
                    }
                } else if (c.kind == K::Param2 && c.op == 0x37) {
                    std::fill(pending_sprites_.begin(), pending_sprites_.end(), false);
                } else if (c.kind == K::Words4 && c.op == 0xB4) {
                    std::fill(pending_sprites_.begin(), pending_sprites_.end(), false);
                } else if (c.kind == K::Text) {
                    std::fill(pending_sprites_.begin(), pending_sprites_.end(), false);
                    if (c.op == 0x6A || c.op == 0x4A || c.op == 0x4D) {
                        speaker_ = c.str;
                    } else if (c.op == 0x6B || c.op == 0x4B || c.op == 0x4E) {
                        current_line_ = c.str;
                    }
                }
            }

            if (!target_bg.empty()) {
                load_layer(1, target_bg);
            }
            for (size_t i = 0; i < sprites_.size(); ++i) {
                if (sprites_[i].visible && !target_slots[i].empty()) {
                    load_bst(int(i), target_slots[i], target_slot_ids[i]);
                    sprites_[i].visible = true;
                }
            }

            finish_animations();
            animation_enabled_ = prev_anim;
            vm_.seek_to_pc(final_seek_pc);
        }

        if (layers_.find(1) == layers_.end() && !bg.empty()) {
            load_layer(1, bg);
        }

        if (audio_) {
            std::string bgm;
            if (bytes.size() >= 0xa38 + 64) {
                for (size_t i = 0xa38; i < 0xa38 + 64; ++i) {
                    if (bytes[i] == 0) break;
                    char c = char(bytes[i]);
                    if (c == '\\') c = '/';
                    bgm.push_back(c);
                }
            }
            if (!bgm.empty()) {
                if (std::filesystem::path(bgm).extension().empty()) bgm += ".pcm";
                int first = 0, last = 0;
                std::string stem = std::filesystem::path(bgm).filename().string();
                dc4::bgm_loop_for(stem, &first, &last);
                audio_->set_group_volume(dc4::AudioGroup::Bgm, 100);
                std::string resolved_bgm = dc4::resolve_asset(data_ + "/" + bgm);
                if (!audio_->play_bgm_file(resolved_bgm, size_t(std::max(0, first)), size_t(std::max(0, last)), true)) {
                    audio_->stop_bgm();
                }
            } else {
                audio_->stop_bgm();
            }
        }

        frame_.assign(size_t(kScreenW) * kScreenH * 4, 0);
        scene_frame_.clear();

        scene_dirty_ = true;
        script_name_ = resolved;
        scene_end_ = false;
        message_visible_ = true;
        taskbar_visible_ = true;
        preserve_scene_ = false;
        next_script_.clear();
        last_error_.clear();
        pointer_x_ = pointer_y_ = -1;
        pointer_down_ = false;
        pending_movie_.clear();
        input_skip_label_.clear();
        script_wait_pending_ = false;
        script_wait_ms_ = 0;
        credits_active_ = false;
        effect_pck_.clear();
        transition_active_ = transition_pending_ = false;
        transition_snap_.clear();
        scene_chips_.clear();
        pending_scene_blit_ = false;
        if (restore_error) restore_error->clear();
        return true;
    } catch (const std::exception& error) {
        if (restore_error) *restore_error = error.what();
        return false;
    }
}

bool SceneRunner::restore_state(const std::vector<uint8_t>& bytes, std::string* restore_error) {
    try {
        if (bytes.size() < 24 || bytes.size() > kMaxSave + 24) throw std::runtime_error("invalid save size");
        if (std::memcmp(bytes.data(), kMagic, 8)) throw std::runtime_error("not a dc4engine save");
        Reader header{bytes, 8};
        const auto version = header.u32();
        if (version != 1 && version != 2 && version != 3 && version != 4)
            throw std::runtime_error("unsupported save version");
        if (header.u32() != bytes.size() - 24) throw std::runtime_error("invalid save payload length");
        const uint64_t expected_checksum = header.u64();
        std::vector<uint8_t> payload(bytes.begin() + 24, bytes.end());
        if (checksum(payload) != expected_checksum) throw std::runtime_error("save checksum mismatch");
        Reader in{payload};
        const std::string script = in.str(255);
        if (!resource_name(script, false)) throw std::runtime_error("invalid saved scenario path");
        const auto vm = read_vm(in);
        const std::string line = in.str(), speaker = in.str(1024);
        const bool ended = in.flag(), message_visible = in.flag(), preserve_scene = in.flag();
        const int transition = in.integer(); const double camera_scale = in.number();
        const double camera_x = version == 1 ? in.integer() : in.number();
        const double camera_y = version == 1 ? in.integer() : in.number();
        if (transition < 0 || transition > 255 || camera_scale < 1 || camera_scale > 655.35 ||
            camera_x < -127 || camera_x > 1151 || camera_y < -71 || camera_y > 647)
            throw std::runtime_error("invalid saved camera or transition");
        // Rehydrate and validate in an independent runner. A broken save never
        // destroys the currently running scene, its VM, or its resource state.
        SceneRunner candidate(data_);
        if (!candidate.load_script(data_ + "/MES/" + script)) throw std::runtime_error(candidate.last_error_);
        std::string error;
        if (!candidate.vm_.restore_snapshot(vm, &error)) throw std::runtime_error(error);
        const auto layers = in.count(64);
        for (uint32_t i = 0; i < layers; ++i) {
            const int index = in.integer(); const std::string name = in.str(255);
            if (index < 0 || index > 255 || !resource_name(name) || candidate.layers_.count(index))
                throw std::runtime_error("invalid saved image layer");
            candidate.load_layer(index, name);
            if (!candidate.layers_.count(index)) throw std::runtime_error("saved image is unavailable: " + name);
        }
        if (in.count(16) != 16) throw std::runtime_error("invalid saved sprite count");
        for (size_t i = 0; i < 16; ++i) {
            const std::string name = in.str(255), image = in.str(255);
            const double scale = in.number();
            const int x = in.integer(), y = in.integer(), alpha = in.integer(), z = in.integer();
            const bool visible = in.flag();
            const int cur_x = version >= 3 ? in.integer() : 0;
            const int cur_y = version >= 3 ? in.integer() : 0;
            if ((!name.empty() && !resource_name(name)) || (!image.empty() && !resource_name(image)) ||
                scale <= 0 || scale > 655.35 || x < -32768 || x > 65535 || y < -32768 || y > 65535 || alpha < 0 || alpha > 255 || z < 0 || z > 2)
                throw std::runtime_error("invalid saved sprite");
            if (!image.empty()) {
                candidate.load_bst(int(i), image, z == 2 ? 25 : z == 1 ? 19 : 4);
                if (!candidate.sprites_[i].img.valid()) throw std::runtime_error("saved sprite is unavailable: " + image);
            }
            auto& sprite = candidate.sprites_[i];
            sprite.name = name; sprite.image = image; sprite.scale = float(scale); sprite.x = x; sprite.y = y; sprite.alpha = alpha; sprite.z = z; sprite.visible = visible;
            sprite.cur_x = cur_x; sprite.cur_y = cur_y;
        }
        const auto audio = read_audio(in);
        uint16_t camera_duration = 5;
        std::vector<CameraMotion> camera_motion;
        if (version >= 2) {
            const auto duration = in.u32();
            if (duration > 65535) throw std::runtime_error("invalid saved camera duration");
            camera_duration = uint16_t(duration);
            const auto count = in.count(10); // native queue limit
            for (uint32_t i = 0; i < count; ++i) {
                CameraMotion motion;
                motion.from_scale = float(in.number()); motion.to_scale = float(in.number());
                motion.from_x = float(in.number()); motion.from_y = float(in.number());
                motion.to_x = float(in.number()); motion.to_y = float(in.number());
                motion.elapsed_ms = in.number(); motion.duration_ms = in.number();
                auto valid_scale = [](float scale) { return scale >= 1 && scale <= 655.35f; };
                auto valid_pivot = [](float x, float y) { return x >= -127 && x <= 1151 && y >= -71 && y <= 647; };
                if (!valid_scale(motion.from_scale) || !valid_scale(motion.to_scale) ||
                    !valid_pivot(motion.from_x, motion.from_y) || !valid_pivot(motion.to_x, motion.to_y) ||
                    motion.duration_ms <= 0 || motion.duration_ms > 65535.0 * 200 ||
                    motion.elapsed_ms < 0 || motion.elapsed_ms >= motion.duration_ms ||
                    (i && motion.elapsed_ms != 0))
                    throw std::runtime_error("invalid saved camera animation");
                camera_motion.push_back(motion);
            }
        }
        bool restored_comu_visible = false;
        std::vector<ComuItem> restored_comu;
        if (version >= 4) {
            const bool comu_visible = in.flag();
            const auto items = in.count(200);
            std::vector<ComuItem> restored;
            restored.reserve(items);
            for (uint32_t i = 0; i < items; ++i) {
                ComuItem item;
                item.contact = in.integer();
                item.stamp = in.integer();
                item.text = in.str(255);
                if (item.contact < -1 || item.contact > 10 || item.stamp < -1 || item.stamp > 63)
                    throw std::runtime_error("invalid saved chat entry");
                restored.push_back(std::move(item));
            }
            restored_comu_visible = comu_visible;
            restored_comu = std::move(restored);
        }
        if (in.at != payload.size()) throw std::runtime_error("unexpected trailing save data");
        if (!scene_audio_.restore_snapshot(audio, false, &error)) throw std::runtime_error(error);
        comu_visible_ = restored_comu_visible;
        comu_group_ = false;
        comu_items_ = std::move(restored_comu);
        mes_ = std::move(candidate.mes_);
        vm_.reset(); vm_.bind(mes_); vm_.restore_snapshot(vm);
        layers_.swap(candidate.layers_); sprites_.swap(candidate.sprites_);
        scene_dirty_ = true;
        script_name_ = script; current_line_ = line; speaker_ = speaker; transition_type_ = transition;
        camera_scale_ = float(camera_scale); camera_x_ = float(camera_x); camera_y_ = float(camera_y);
        camera_duration_ = camera_duration; camera_motion_ = std::move(camera_motion);
        if (!animation_enabled_) finish_animations();
        scene_end_ = ended; message_visible_ = message_visible; preserve_scene_ = preserve_scene;
        next_script_.clear(); last_error_.clear(); pointer_x_ = pointer_y_ = -1; pointer_down_ = false;
        pending_movie_.clear(); input_skip_label_.clear();
        script_wait_pending_ = false; script_wait_ms_ = 0;
        credits_active_ = false; credits_elapsed_ms_ = credits_duration_ms_ = 0;
        credits_archive_.clear(); credits_entries_.clear(); credits_pages_.clear();
        effect_pck_.clear();
        transition_active_ = transition_pending_ = false; transition_snap_.clear();
        scene_chips_.clear(); pending_scene_blit_ = false;
        if (restore_error) restore_error->clear();
        return true;
    } catch (const std::exception& error) {
        if (restore_error) *restore_error = error.what();
        return false;
    }
}
} // namespace dc4scene
