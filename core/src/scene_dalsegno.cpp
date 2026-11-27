#include "arisa32/scene.h"
#include "arisa32/resource.h"
#include "arisa32/names.h"
#include "arisa32/save_paths.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <filesystem>

namespace dc4scene {

bool SceneRunner::open_dalsegno_title(int reveal_chars) {
    if (!is_dalsegno_profile_) return false;
    if (ds_break_.active()) return true;
    set_sidebar_visible(false);
    set_pointer(-1, -1, false);
    draw(reveal_chars);
    return load_script(data_ + "/MES/GOTITLE.mes");
}

void SceneRunner::start_dalsegno_break() {
    if (!animation_enabled_ || !effects_enabled_ || !visual_effects_enabled_ || variable(99, 10) == 0) return;
    constexpr int tile_size = DsBreakTransition::tile_size;
    constexpr int ticks = DsBreakTransition::ticks;
    if (!load_grp_crx("system/effbreak.crx", &ds_break_.atlas, nullptr) ||
        ds_break_.atlas.w < tile_size * ticks || ds_break_.atlas.h < tile_size) return;

    ds_break_.source = frame_;
    ds_break_.tiles.resize((kScreenW / tile_size) * ((kScreenH + tile_size - 1) / tile_size));
    for (auto& tile : ds_break_.tiles) {
        tile.vx = 32 - effect_random(64);
        tile.vy = 32 - (effect_random(64) & 48);
        tile.life = 16 + effect_random(32);
    }
    script_wait_ms_ = ticks * DsBreakTransition::tick_ms;
    script_wait_pending_ = script_wait_skippable_ = true;
}

void SceneRunner::finish_dalsegno_break() {
    if (!ds_break_.active()) return;
    ds_break_ = {};
    script_wait_ms_ = 0;
    std::fill(frame_.begin(), frame_.end(), 0);
    for (size_t pixel = 3; pixel < frame_.size(); pixel += 4) frame_[pixel] = 255;
    scene_dirty_ = true;
}

void SceneRunner::draw_dalsegno_break() {
    constexpr int tile_size = DsBreakTransition::tile_size;
    constexpr int columns = kScreenW / tile_size;
    const int tick = int(ds_break_.elapsed_ms / DsBreakTransition::tick_ms);
    const double time = tick * 0.08;
    const auto& atlas = ds_break_.atlas;
    std::fill(frame_.begin(), frame_.end(), 0);
    for (size_t pixel = 3; pixel < frame_.size(); pixel += 4) frame_[pixel] = 255;

    // EFFBREAK moves an alpha mask over the stationary capture from the preceding frame.
    for (size_t i = 0; i < ds_break_.tiles.size(); ++i) {
        const auto& tile = ds_break_.tiles[i];
        if (tick >= tile.life) continue;
        const int x = int(i % columns) * tile_size + int(4 * tile.vx * time);
        const int y = int(i / columns) * tile_size + int(-tile.vy * time - 3.92 * time * time);
        for (int row = std::max(0, -y); row < std::min(tile_size, kScreenH - y); ++row) {
            for (int col = std::max(0, -x); col < std::min(tile_size, kScreenW - x); ++col) {
                const size_t mask = (size_t(row) * atlas.w + tick * tile_size + col) * 4;
                if (!atlas.rgba[mask + 3]) continue;
                const size_t pixel = (size_t(y + row) * kScreenW + x + col) * 4;
                std::copy_n(ds_break_.source.data() + pixel, 3, frame_.data() + pixel);
            }
        }
    }
}

DalSegnoPlayback SceneRunner::dalsegno_playback() const {
    static constexpr int intervals[] = {120,100,80,65,50,40,30,24,18,12,6,0};
    return {intervals[std::clamp(variable(99, 26), 0, 11)],
            1000 + (20 - std::clamp(variable(99, 29), 0, 20)) * 150,
            variable(99, 28) != 0, variable(99, 209) != 0,
            variable(99, 210) != 0, variable(99, 30) != 0};
}

int SceneRunner::dalsegno_window_alpha() const {
    const int transparency = std::clamp(variable(100, 49), 0, 99);
    return ((99 - transparency) * 255 + 49) / 99;
}

int SceneRunner::dalsegno_audio_level(int enable_index, int volume_index) const {
    return variable(99, enable_index) ? std::clamp(variable(99, volume_index), 0, 100) : 0;
}

float SceneRunner::dalsegno_movie_gain() const {
    return dalsegno_audio_level(203, 225) * dalsegno_audio_level(200, 207) / 10000.0f;
}

void SceneRunner::sync_dalsegno_audio_setting(uint16_t bank, uint16_t index) {
    if (!audio_) return;
    if (bank == 94 || bank == 95) {
        set_character_volume(index, variable(94, index), !variable(95, index));
        return;
    }
    if (bank != 99) return;
    switch (index) {
    case 203: case 225:
        audio_->set_master_volume(dalsegno_audio_level(203, 225));
        break;
    case 14: case 7:
        audio_->set_user_volume(dc4::AudioGroup::Bgm, dalsegno_audio_level(14, 7));
        break;
    case 15: case 8:
        audio_->set_user_volume(dc4::AudioGroup::Se, dalsegno_audio_level(15, 8));
        break;
    case 5: case 9:
        audio_->set_user_volume(dc4::AudioGroup::Voice, dalsegno_audio_level(5, 9));
        break;
    case 200: case 207:
        audio_->set_movie_volume(dalsegno_audio_level(200, 207));
        break;
    }
}

void SceneRunner::sync_dalsegno_audio() {
    if (!audio_ || (mes_.version & 0xFFFF) != 0x9F9A) return;
    for (int index : {225, 7, 8, 9, 207}) sync_dalsegno_audio_setting(99, index);
    for (int index = 0; index <= 50; ++index) sync_dalsegno_audio_setting(94, index);
}

namespace {
// DSAO.exe 0x499140: Music Mode has gaps at BGM 23, 26 and 30.
constexpr int ds_bgm_ids[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,
    24,25,27,28,29,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45};

std::string catalogue_key(std::string name) {
    std::replace(name.begin(), name.end(), '\\', '/');
    name = dc4::save_lower(std::move(name));
    if (name.size() > 4 && name.substr(name.size() - 4) == ".mes") name.resize(name.size() - 4);
    return name;
}
}

void SceneRunner::award_dalsegno_scene(const std::string& script) {
    if (ds_replay_active_ || script.rfind("ds_", 0) != 0) return;
    load_dalsegno_catalogue();
    const auto id = ds_scene_ids_.find(catalogue_key(script));
    if (id != ds_scene_ids_.end() && !variable(98, id->second)) set_variable(98, id->second, 1);
}

void SceneRunner::award_dalsegno_cg(const std::string& name) {
    const auto key = catalogue_key(name);
    if (ds_replay_active_ || key.rfind("bg/ev", 0) != 0) return;
    load_dalsegno_catalogue();
    const auto id = ds_cg_ids_.find(key);
    if (id != ds_cg_ids_.end() && !variable(97, id->second)) set_variable(97, id->second, 1);
}

void SceneRunner::start_dalsegno_wipe(int parameter) {
    const bool wipe = visual_effects_enabled_ && variable(99, 10) && mask_.valid();
    if (wipe && animation_enabled_ && effects_enabled_ && !transition_pending_ && !scene_frame_.empty()) {
        transition_snap_ = scene_frame_;
        transition_pending_ = true;
    }
    transition_type_ = 21;
    start_transition(wipe ? 12 * parameter : 0, false);
}

bool SceneRunner::open_dalsegno_gallery(const std::string& action) {
    if (catalogue_key(script_name_).rfind("cg_", 0) != 0 || action.rfind("cg", 0) != 0 ||
        action.size() <= 2 || !std::all_of(action.begin() + 2, action.end(),
            [](char ch) { return ch >= '0' && ch <= '9'; })) return false;
    const auto tile = std::find_if(buttons_.begin(), buttons_.end(), [&](const auto& button) {
        return button.action == action;
    });
    if (tile == buttons_.end()) return true;
    load_dalsegno_catalogue();
    dc4::MesStream stream(mes_);
    dc4::MesCommand command;
    size_t start = 0;
    bool found = false;
    bool numeric_entry = false, wipe_routine = false;
    // The PC's cg_next table follows the numeric display labels in script order.
    while (stream.next(&command)) {
        const auto op = dc4::mes_engine_command(command).opcode();
        if (op != dc4::Opcode::LABEL) {
            if (numeric_entry && op == dc4::Opcode::JUMP && command.str == "cg_draw2")
                ds_gallery_order_.back().wipe = true;
            if (wipe_routine && op == dc4::Opcode::LOAD_SLOT && command.p1 == 16)
                ds_gallery_mask_ = command.str;
            if (wipe_routine && op == dc4::Opcode::TRANSITION && command.p1 == 21)
                ds_gallery_wipe_parameter_ = command.p2;
            continue;
        }
        numeric_entry = false;
        wipe_routine = command.str == "cg_draw2";
        if (command.str.rfind("cg", 0) != 0) continue;
        if (command.str == "cg_next") break;
        const char* end = command.str.data() + command.str.size();
        uint16_t id = 0;
        const auto parsed = std::from_chars(command.str.data() + 2, end, id);
        if (parsed.ec != std::errc{} || parsed.ptr != end || id >= ds_cg_names_.size()) continue;
        if (command.str == action) {
            start = ds_gallery_order_.size();
            found = true;
        }
        numeric_entry = true;
        ds_gallery_order_.push_back({id, false});
    }
    if (!found || !select_dalsegno_gallery_image(start, ds_gallery_order_.size()))
        close_dalsegno_gallery();
    return true;
}

bool SceneRunner::select_dalsegno_gallery_image(size_t index, size_t candidates) {
    for (size_t offset = 0; offset < candidates; ++offset) {
        const size_t position = (index + offset) % ds_gallery_order_.size();
        const auto [id, wipe] = ds_gallery_order_[position];
        if (!variable(97, id)) continue;
        Image image;
        if (!load_grp_crx(ds_cg_names_[id], &image, nullptr) || !image.valid()) continue;
        if (wipe && ds_gallery_wipe_parameter_ > 0 && visual_effects_enabled_ && variable(99, 10)) {
            snapshot_for_transition();
            load_slot(16, ds_gallery_mask_);
            start_dalsegno_wipe(ds_gallery_wipe_parameter_);
        }
        ds_gallery_index_ = position;
        ds_gallery_image_ = std::move(image);
        ++ds_gallery_revision_;
        ds_gallery_width_ = ds_gallery_height_ = 0;
        return true;
    }
    return false;
}

bool SceneRunner::advance_dalsegno_gallery() {
    if (!dalsegno_gallery_active()) return false;
    if (transition_active_) { finish_transition(); return true; }
    select_dalsegno_gallery_image(ds_gallery_index_ + 1, ds_gallery_order_.size() - 1);
    return true;
}

void SceneRunner::close_dalsegno_gallery() {
    if (dalsegno_gallery_wiping()) finish_transition();
    ds_gallery_image_ = {};
    ds_gallery_order_.clear();
    ds_gallery_mask_.clear();
    ds_gallery_wipe_parameter_ = 0;
    ds_gallery_width_ = ds_gallery_height_ = 0;
    ds_gallery_frame_.clear();
}

const std::vector<uint8_t>& SceneRunner::dalsegno_gallery_frame(int width, int height) {
    if (!dalsegno_gallery_active() || width < 1 || height < 1 || width > 8192 || height > 8192)
        return ds_gallery_frame_;
    if (width != ds_gallery_width_ || height != ds_gallery_height_) {
        fit_gallery_image(ds_gallery_image_, width, height, &ds_gallery_frame_);
        ds_gallery_width_ = width;
        ds_gallery_height_ = height;
    }
    return ds_gallery_frame_;
}

bool SceneRunner::dispatch_dalsegno_music(const dc4::MesCommand& c) {
    using K = dc4::CmdKind;
    if (c.raw_op == 0x1C && c.kind == K::Param2 && c.p1 >= 200 && c.p1 <= 202) {
        if (!ds_music_active_) {
            ds_music_active_ = true;
            ds_music_track_ = std::clamp(variable(7,98),0,41);
            ds_music_scroll_ = std::clamp(variable(9,60),0,36);
            ds_music_repeat_ = std::clamp(variable(9,30),0,2);
            if (ds_music_titles_.empty()) {
                dc4::MesFile music;
                if (music.load_file(dc4::resolve_asset(data_ + "/MES/MUSIC.mes"))) {
                    dc4::MesStream stream(music); dc4::MesCommand item; int track = -1;
                    ds_music_titles_.resize(42);
                    while (stream.next(&item)) {
                        if (item.op == 0x72) track = item.words[0];
                        else if (item.op == 0x54 && track >= 0 && track < 42) {
                            ds_music_titles_[size_t(track)] = item.str; track = -1;
                        }
                    }
                }
            }
            if (ds_music_titles_.size() != 42) {
                last_error_ = "Dal Segno Music Mode catalog could not be loaded";
                scene_end_ = true; return true;
            }
        }
        if (c.p1 == 201) {
            if (c.p2 == 255 || c.p2 == 254) ds_music_checks_.fill(c.p2 == 255);
            else if (c.p2 < 6) {
                auto& checked = ds_music_checks_[size_t(ds_music_scroll_ + c.p2)]; checked = !checked;
            }
            if (std::none_of(ds_music_checks_.begin(),ds_music_checks_.end(),[](bool check) { return check; }))
                vm_.write_variable(3,29,0);
        } else if (c.p1 == 202) {
            ds_music_building_ = false;
            vm_.write_variable(3,0,c.p2 < 6 && ds_music_checks_[size_t(ds_music_scroll_ + c.p2)] ? 1 : 0);
        } else if (c.p2 == 1 || c.p2 == 5) {
            if (c.p2 == 5) {
                ds_music_track_ = std::clamp(variable(99,254),0,41);
                if (variable(3,29)) ds_music_checks_[size_t(ds_music_track_)] = true;
            }
            play_dalsegno_music();
        } else if (c.p2 == 2 || c.p2 == 3) {
            step_dalsegno_music(c.p2 == 2 ? 1 : -1);
        } else if (c.p2 == 0) {
            if (std::none_of(ds_music_checks_.begin(),ds_music_checks_.end(),[](bool check) { return check; }))
                vm_.write_variable(3,29,0);
            const int repeat = std::clamp(variable(9,30),0,2);
            if (repeat != ds_music_repeat_ && variable(3,11) == 0) {
                const double position = audio_ ? audio_->channel_position(0) : 0;
                ds_music_repeat_ = repeat;
                if (play_dalsegno_music() && audio_) audio_->seek_channel(0,position);
            }
            ds_music_repeat_ = repeat;
        } else if (c.p2 == 4) {
            // The script has finished its six checkbox controls. Append row hit targets.
            for (int slot = 0; slot < 6; ++slot) {
                UiButton row{348,248+slot*64,718,60}; row.action = ":dsmusic" + std::to_string(slot);
                buttons_.push_back(row);
            }
            UiButton up{1118,232,30,22}, down{1118,618,30,22};
            up.action = ":dsmusicup"; down.action = ":dsmusicdown";
            buttons_.push_back(up); buttons_.push_back(down);
        }
        vm_.write_variable(99,246,ds_music_scroll_);
        vm_.write_variable(99,253,ds_music_track_);
        vm_.write_variable(99,254,ds_music_track_);
        vm_.write_variable(7,98,ds_music_track_);
        scene_dirty_ = true;
        return true;
    }
    if (!ds_music_active_) return false;
    if (c.raw_op == 0x70 && c.kind == K::Words4) {
        ds_music_building_ = true; return true;
    }
    // Native list chip/caption definitions are not ordinary button layers.
    if (ds_music_building_) return true;
    return false;
}

bool SceneRunner::play_dalsegno_music() {
    dc4::MesCommand music;
    music.op = dc4::opcode_byte(dc4::Opcode::PLAY_BGM);
    music.kind = dc4::CmdKind::Words4;
    music.words[0] = uint16_t(ds_bgm_ids[ds_music_track_]);
    music.words[1] = variable(9,30) == 0 ? 0 : 2;
    if (!scene_audio_.dispatch(music) || !scene_audio_.last_error().empty()) {
        last_error_ = scene_audio_.last_error(); vm_.write_variable(3,11,1); return false;
    }
    ds_music_has_title_ = true;
    vm_.write_variable(3,11,0);
    vm_.write_variable(99,253,ds_music_track_);
    vm_.write_variable(99,254,ds_music_track_);
    vm_.write_variable(7,98,ds_music_track_);
    return true;
}

void SceneRunner::step_dalsegno_music(int direction) {
    std::vector<int> tracks;
    for (int track = 0; track < 42; ++track)
        if (!variable(3,29) || ds_music_checks_[size_t(track)]) tracks.push_back(track);
    if (tracks.empty()) { vm_.write_variable(3,29,0); for (int track = 0; track < 42; ++track) tracks.push_back(track); }
    if (variable(9,30) == 2 && tracks.size() > 1) {
        tracks.erase(std::remove(tracks.begin(),tracks.end(),ds_music_track_),tracks.end());
        ds_music_random_ ^= ds_music_random_ << 13;
        ds_music_random_ ^= ds_music_random_ >> 17;
        ds_music_random_ ^= ds_music_random_ << 5;
        ds_music_track_ = tracks[ds_music_random_ % tracks.size()];
    } else for (int offset = 1; offset <= 42; ++offset) {
        const int track = (ds_music_track_ + direction * offset + 84) % 42;
        if (std::find(tracks.begin(),tracks.end(),track) != tracks.end()) { ds_music_track_ = track; break; }
    }
    vm_.write_variable(7,98,ds_music_track_);
}

void SceneRunner::refresh_dalsegno_music() {
    if (!ds_music_active_) return;
    vm_.write_variable(99,246,ds_music_scroll_);
    if (jump_to_label("reload")) advance_to_next_text();
    scene_dirty_ = true;
}

void SceneRunner::update_dalsegno_music() {
    if (!ds_music_active_ || !audio_ || !ds_music_has_title_ || variable(3,11) != 0 ||
        !pending_movie_.empty() || audio_->channel_playing(0)) return;
    if (variable(9,30) != 0) step_dalsegno_music(1);
    play_dalsegno_music();
    refresh_dalsegno_music();
}

bool SceneRunner::touch_dalsegno_music_scroll(int action, int x, int y) {
    if (action == 0 && x >= 1108 && x < 1158 && y >= 254 && y <= 618) ds_music_drag_ = true;
    if (!ds_music_drag_) return false;
    if (action == 3) { ds_music_drag_ = false; return true; }
    const int scroll = std::clamp(int(std::lround(double(y-255)*36/360)),0,36);
    if (scroll != ds_music_scroll_) { ds_music_scroll_ = scroll; refresh_dalsegno_music(); }
    if (action == 1) ds_music_drag_ = false;
    return true;
}

void SceneRunner::draw_dalsegno_music() {
    if (ds_music_titles_.size() != 42) return;
    if (font_ && ds_music_has_title_)
        font_->rasterize_line(sjis_to_utf16(ds_music_titles_[size_t(ds_music_track_)]),428,86,
            &frame_,kScreenW,kScreenH,22,0xFFFFFF,false,0);
    for (int slot = 0; slot < 6; ++slot) {
        const int track = ds_music_scroll_ + slot, y = 248 + slot*64;
        const bool hover = pointer_x_ >= 348 && pointer_x_ < 1066 && pointer_y_ >= y && pointer_y_ < y+60;
        const bool focus = ds_focus_ >= 0 && size_t(ds_focus_) < buttons_.size() &&
            buttons_[size_t(ds_focus_)].action == ":dsmusic" + std::to_string(slot);
        if (ds_music_has_title_ && track == ds_music_track_)
            blit_chip(UiChip{356,y,0,260,710,51,14,true},255);
        else if (hover || focus) blit_chip(UiChip{356,y,0,208,710,51,14,true},255);
        if (font_) font_->rasterize_line(sjis_to_utf16(ds_music_titles_[size_t(track)]),374,y+46,
            &frame_,kScreenW,kScreenH,22,0xFFFFFF,false,0);
        if (slot < 5) {
            auto pixel = frame_.begin() + (size_t(y + 57) * kScreenW + 356) * 4;
            constexpr uint8_t cyan[] = {130, 208, 234, 255};
            for (int x = 356; x < 1066; ++x, pixel += 4) std::copy_n(cyan, 4, pixel);
        }
    }
    blit_chip(UiChip{1118,255+ds_music_scroll_*360/36-16,751,0,32,32,14,true},255);
    // Native volume controls reveal a clipped bar, with no separate thumb.
    for (int i = 0; i < 2; ++i) {
        const int value = std::clamp(variable(99,i == 0 ? 7 : 207),0,99);
        if (value) blit_chip(UiChip{i == 0 ? 398 : 794,660,236,312,235*value/99,35,14,true},255);
    }
}

void SceneRunner::reset_dalsegno_ui() {
    if (dalsegno_gallery_active()) close_dalsegno_gallery();
    ds_captions_.clear();
    ds_has_caption_ = false;
    ds_callbacks_ = {};
    ds_modal_buttons_.clear();
    ds_modal_chips_.clear();
    captured_slider_ = -1;
    ds_focus_ = -1;
    ds_quit_requested_ = false;
    ds_name_pending_ = ds_name_confirming_ = false;
    ds_name_buttons_.clear();
    ds_music_active_ = ds_music_building_ = ds_music_drag_ = false;
    ds_music_has_title_ = false;
}

std::string SceneRunner::dalsegno_back_action() const {
    if ((mes_.version & 0xFFFF) != 0x9F9A) return {};
    if (ds_name_pending_) return ds_name_confirming_ ? ":nameno" : ":nameedit";
    if (dalsegno_gallery_active()) return ":dscgback";
    if (!ds_modal_buttons_.empty()) return ds_modal_buttons_.back().action;
    if (ds_replay_active_) return ":dsreplayexit";
    if (sidebar_visible_) return ":sidebarclose";
    if (!current_line_.empty()) return {};
    return ds_callbacks_[3];
}

bool SceneRunner::dalsegno_menu_active() const {
    return dalsegno_gallery_active() || ((mes_.version & 0xFFFF) == 0x9F9A && current_line_.empty() &&
        !buttons_.empty() && !dalsegno_dialogue_controls());
}

bool SceneRunner::dalsegno_dialogue_controls() const {
    if (ds_dialogue_buttons_.empty() || buttons_.size() != ds_dialogue_buttons_.size()) return false;
    for (size_t i = 0; i < buttons_.size(); ++i)
        if (buttons_[i].action != ds_dialogue_buttons_[i].action) return false;
    return true;
}

bool SceneRunner::handle_dalsegno_key(UiKey key, std::string* action) {
    if ((mes_.version & 0xFFFF) != 0x9F9A) return false;
    if (ds_break_.active()) {
        if (action) action->clear();
        return true;
    }
    if (dalsegno_gallery_active()) {
        if (action) *action = key == UiKey::Activate ? ":dscgnext" : "";
        return true;
    }
    if (!current_line_.empty() && !sidebar_visible_ && !ds_name_pending_ && ds_modal_buttons_.empty()) return false;
    if (dalsegno_dialogue_controls() && !sidebar_visible_ && !ds_name_pending_ && ds_modal_buttons_.empty()) return false;
    if (ds_name_confirming_) {
        if (action) action->clear();
        if (key == UiKey::Next || key == UiKey::Previous) ds_focus_ = ds_focus_ == 0 ? 1 : 0;
        else if (key == UiKey::Activate && action) *action = ds_focus_ == 0 ? ":nameyes" : ":nameno";
        return true;
    }
    auto& controls = ds_name_pending_ ? ds_name_buttons_ : sidebar_visible_ ? sidebar_buttons_ :
        ds_modal_buttons_.empty() ? buttons_ : ds_modal_buttons_;
    if (controls.empty()) return false;
    if (action) action->clear();
    if (key == UiKey::Next || key == UiKey::Previous) {
        const int count = int(controls.size());
        ds_focus_ = ds_focus_ < 0 ? (key == UiKey::Next ? 0 : count - 1) :
            (ds_focus_ + (key == UiKey::Next ? 1 : count - 1)) % count;
    } else if (ds_focus_ >= 0 && size_t(ds_focus_) < controls.size()) {
        const auto& button = controls[size_t(ds_focus_)];
        if (button.is_slider && (key == UiKey::Decrease || key == UiKey::Increase)) {
            const int value = std::clamp(variable(button.slider_bank, button.slider_index) +
                (key == UiKey::Increase ? 1 : -1), 0, button.slider_count - 1);
            vm_.write_variable(button.slider_bank, button.slider_index, value);
            sync_audio_from_slider(button.slider_bank, button.slider_index, value);
        } else if (key == UiKey::Activate && action) *action = button.action;
    }
    scene_dirty_ = true;
    return true;
}

bool SceneRunner::dispatch_dalsegno_ui(const dc4::MesCommand& c) {
    if (!c.ds) return false;
    using K = dc4::CmdKind;
    if (dispatch_dalsegno_music(c)) return true;
    if (c.kind == K::Str && c.raw_op == 0x5C) {
        if (c.str == "-") {
            sprites_[13] = {};
            scene_dirty_ = true;
        }
        return true;
    }
    if (c.raw_op==0x1A && !auto_save_path_.empty() && save_load_allowed()) {
        if (!save_state(auto_save_path_)) return false;
        return true;
    }
    if (c.raw_op == 0x1C && c.kind == K::Param2 && c.p1 == 51) {
        if (ds_replay_active_) return true;
        // Native service 51 enters the personal-information screen (0x408608).
        ds_name_pending_ = true; ds_name_confirming_ = false;
        ds_family_name_ = vm_.family_name(); ds_given_name_ = vm_.given_name();
        load_sheet(100, "SYSTEM/NAMEBACK");
        load_sheet(101, "SYSTEM/NAMECHIP");
        load_sheet(102, "SYSTEM/NAMEMENU");
        ds_name_buttons_.clear();
        for (const auto& item : std::vector<std::pair<UiButton, std::string>>{
                {UiButton{464,164,190,47}, ":namefamily"},
                {UiButton{683,164,190,47}, ":namegiven"},
                {UiButton{544,542,90,44}, ":nameconfirm"},
                {UiButton{647,542,110,44}, ":namedefault"}}) {
            auto button = item.first; button.action = item.second; ds_name_buttons_.push_back(button);
        }
        script_wait_pending_ = true; script_wait_ms_ = -1; script_wait_skippable_ = false;
        return true;
    }
    if (c.raw_op == 0x13 && c.kind == K::Param2 && c.p1 == 255) {
        // Native 0x4263B9 waits for chip animation, not another menu click.
        const double remaining = std::max({screen_effect_remaining_ms(),
            effect_pck_.active ? effect_pck_.duration_ms - effect_pck_.elapsed_ms : 0.0,
            transition_active_ ? transition_duration_ms_ - transition_elapsed_ms_ : 0.0});
        if (animation_enabled_ && remaining > 0) {
            script_wait_pending_ = true;
            script_wait_ms_ = remaining;
        }
        return true;
    }
    if (c.raw_op == 0x0F && c.kind == K::Param2) {
        ds_quit_requested_ = true;
        scene_end_ = true;
        return true;
    }
    if (c.kind == K::Words4 && (c.raw_op == 0x69 || c.raw_op == 0x7A)) {
        ds_pending_caption_ = {};
        ds_pending_caption_.x = int16_t(c.words[0]);
        ds_pending_caption_.y = int16_t(c.words[1]);
        if (c.raw_op == 0x69) {
            ds_pending_caption_.button = int(buttons_.size()) - 1;
            ds_pending_caption_.state = c.words[3];
        }
        ds_pending_caption_.preview = c.raw_op == 0x7A && c.words[2] > 0;
        ds_has_caption_ = true;
        return true;
    }
    if (ds_has_caption_ && (c.raw_op == 0x54 || c.raw_op == 0x62)) {
        ds_pending_caption_.text = c.str;
        if (ds_pending_caption_.text.compare(0, 2, "@s") == 0)
            ds_pending_caption_.text.erase(0, 2);
        std::replace(ds_pending_caption_.text.begin(), ds_pending_caption_.text.end(), '$', ' ');
        if (!c.str.empty()) ds_captions_.push_back(ds_pending_caption_);
        ds_has_caption_ = false;
        return true;
    }
    if (c.raw_op == 0x40 && c.kind == K::ParamStr && c.p1 < ds_callbacks_.size()) {
        ds_callbacks_[c.p1] = c.str;
        return true;
    }
    if (c.raw_op == 0x16 && c.kind == K::Param2 && c.p1 == 20) {
        vm_.reset_dalsegno_settings();
        sync_dalsegno_audio();
        scene_dirty_ = true;
        return true;
    }
    if (c.raw_op != 0x6C || c.kind != K::Words4) return false;
    if (ds_callbacks_[0].empty() || ds_callbacks_[1].empty()) {
        last_error_ = "Dal Segno confirmation has no Yes/No callbacks";
        scene_end_ = true;
        return true;
    }
    show_dalsegno_confirmation(int16_t(c.words[0]), int16_t(c.words[1]), c.words[3],
        ds_callbacks_[0], ds_callbacks_[1]);
    script_wait_pending_ = true;
    script_wait_ms_ = -1;
    script_wait_skippable_ = false;
    return true;
}

void SceneRunner::show_dalsegno_confirmation(int x, int y, int message,
                                             const std::string& yes, const std::string& no) {
    load_sheet(27, "SYSTEM/ADVMENUCHIP");
    ds_modal_chips_ = {UiChip{x, y, 0, 0, 700, 184, 27, true}};
    static constexpr int message_x[] = {0, 0, 0, 0, 388, 388, 388, 0, 388, 388};
    static constexpr int message_y[] = {249, 279, 309, 339, 309, 279, 339, 369, 249, 369};
    if (message < 10)
        ds_modal_chips_.push_back(UiChip{x + 160, y + 20, message_x[message],
            message_y[message], 388, 30, 27, true});
    ds_modal_chips_.push_back(UiChip{x + 336, y + 100, 700, 0, 23, 35, 27, true});
    ds_modal_buttons_.clear();
    for (int i = 0; i < 2; ++i) {
        UiButton button;
        button.x = x + (i == 0 ? 190 : 376); button.y = y + 80;
        button.w = 120; button.h = 64; button.action = i == 0 ? yes : no;
        button.state[0] = UiChip{button.x, button.y, i * 120, 185, 119, 64, 27, true};
        button.state[1] = UiChip{button.x, button.y, 240 + i * 120, 185, 119, 64, 27, true};
        button.state[2] = button.state[1];
        ds_modal_buttons_.push_back(button);
    }
    captured_slider_ = -1;
    ds_focus_ = -1;
}

bool SceneRunner::set_dalsegno_name(const std::string& family, const std::string& given) {
    if (!ds_name_pending_ || ds_name_confirming_ || family.size()>19 || given.size()>19) return false;
    dc4::MesVm validator;
    if (!validator.set_personal_names(family, given)) return false;
    ds_family_name_ = family; ds_given_name_ = given;
    scene_dirty_ = true;
    return true;
}

bool SceneRunner::handle_dalsegno_action(const std::string& action) {
    if ((mes_.version & 0xFFFF) != 0x9F9A) return false;
    if (dalsegno_gallery_active()) {
        if (action == ":dscgback") close_dalsegno_gallery();
        if (action == ":dscgnext") advance_dalsegno_gallery();
        return true;
    }
    if (ds_replay_active_) {
        if (action == ":disabled" || replay_action_disabled(action)) return true;
        if (action == ":dsreplayexit" && ds_modal_buttons_.empty()) {
            show_dalsegno_confirmation(292, 160, 8, ":dsreplayyes", ":dsreplayno");
            return true;
        }
        if (action == ":dsreplayyes" || action == ":dsreplayno") {
            if (ds_modal_buttons_.empty() || ds_modal_buttons_.front().action != ":dsreplayyes") return true;
            const bool end_replay = action == ":dsreplayyes";
            ds_modal_buttons_.clear();
            ds_modal_chips_.clear();
            ds_focus_ = -1;
            scene_dirty_ = true;
            if (end_replay) {
                if (load_script(data_ + "/MES/sc_end.mes")) advance_to_next_text();
            }
            return true;
        }
    }
    if (ds_modal_buttons_.empty() && open_dalsegno_gallery(action)) return true;
    if (action == ":sidebarclose") return set_sidebar_visible(false);
    if (ds_music_active_ && action.compare(0, 8, ":dsmusic") == 0) {
        if (action == ":dsmusicup" || action == ":dsmusicdown") {
            ds_music_scroll_ = std::clamp(ds_music_scroll_ + (action == ":dsmusicup" ? -1 : 1), 0, 36);
        } else {
            auto found = std::find_if(buttons_.begin(), buttons_.end(), [&](const auto& b) { return b.action == action; });
            if (found == buttons_.end()) return false;
            ds_music_track_ = ds_music_scroll_ + (found->y - 248) / 64;
            vm_.write_variable(99, 254, ds_music_track_);
            vm_.write_variable(7, 98, ds_music_track_);
            if (variable(3, 29)) ds_music_checks_[size_t(ds_music_track_)] = true;
            play_dalsegno_music();
        }
        refresh_dalsegno_music();
        return true;
    }
    if (!ds_name_pending_) return false;
    if (action == ":namedefault") return set_dalsegno_name("Takamura", "Atsuya");
    if (action == ":nameconfirm" && !ds_name_confirming_) {
        dc4::MesVm validator;
        if (!validator.set_personal_names(ds_family_name_, ds_given_name_)) return false;
        ds_name_confirming_ = true;
        ds_focus_ = -1;
    } else if (action == ":nameno" && ds_name_confirming_) {
        ds_name_confirming_ = false;
    } else if (action == ":nameyes" && ds_name_confirming_) {
        if (!vm_.set_personal_names(ds_family_name_, ds_given_name_)) return false;
        ds_name_pending_ = ds_name_confirming_ = false;
        ds_name_buttons_.clear();
        script_wait_pending_ = false; script_wait_ms_ = 0; script_wait_skippable_ = true;
        advance_to_next_text();
    } else return false;
    scene_dirty_ = true;
    return true;
}

bool SceneRunner::open_dalsegno_config() {
    if ((mes_.version & 0xFFFF) != 0x9F9A || ds_replay_active_ || dalsegno_gallery_active() ||
        ds_name_pending_ || !ds_modal_buttons_.empty()) return false;
    if (!current_line_.empty()) {
        set_sidebar_visible(false);
        std::vector<uint8_t> state;
        if (!capture_state(&state, &last_error_)) return false;
        if (!load_script(dc4::resolve_asset(data_ + "/MES/config.mes"))) return false;
        ds_config_return_ = std::move(state);
    } else if (has_label("config")) {
        if (!jump_to_label("config")) return false;
    } else if (!load_script(dc4::resolve_asset(data_ + "/MES/config.mes"))) return false;
    advance_to_next_text();
    return last_error_.empty();
}

bool SceneRunner::resume_dalsegno_config() {
    // Popup cancellation must stay in CONFIG; only its Back callback resumes the story.
    if (ds_config_return_.empty() || !ds_modal_buttons_.empty() || dalsegno_back_action() != "retchk") return false;
    std::map<std::pair<int, int>, int> settings;
    for (int index : {5,7,8,9,10,11,12,14,15,26,27,28,29,30,190,194,197,198,199,200,201,202,203,205,206,207,208,209,210,211,212,213,214,217,218,221,222,225,237})
        settings[{99, index}] = variable(99, index);
    for (int bank : {94, 95}) for (int i = 0; i <= 50; ++i) settings[{bank, i}] = variable(bank, i);
    settings[{100, 49}] = variable(100, 49);
    if (!restore_state(ds_config_return_)) return false;
    ds_config_return_.clear();
    for (const auto& setting : settings) set_variable(setting.first.first, setting.first.second, setting.second);
    return true;
}

std::string SceneRunner::dalsegno_text(const std::string& text) const {
    if ((mes_.version & 0xFFFF) != 0x9F9A) return text;
    if (text == "&n") return vm_.given_name();
    std::string result;
    for (size_t at = 0; at < text.size();) {
        const char* original = nullptr;
        const std::string* replacement = nullptr;
        for (const auto& item : {std::make_pair("Takamura", &vm_.family_name()), std::make_pair("Atsuya", &vm_.given_name())}) {
            const size_t length = std::char_traits<char>::length(item.first);
            const auto letter = [](unsigned char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); };
            if (text.compare(at, length, item.first) == 0 && (at == 0 || !letter(text[at-1])) &&
                (at + length == text.size() || !letter(text[at+length]))) { original = item.first; replacement = item.second; break; }
        }
        if (replacement) { result += *replacement; at += std::char_traits<char>::length(original); }
        else { result += text[at] == '$' ? ' ' : text[at]; ++at; }
    }
    return result;
}

void SceneRunner::draw_dalsegno_name() {
    blit_chip(UiChip{0,0,0,0,1280,720,100,true}, 255);
    if (font_) for (int i = 0; i < 2; ++i) {
        const auto name = sjis_to_utf16(i == 0 ? ds_family_name_ : ds_given_name_);
        const int width = font_->text_width(name, 24);
        const float size = width > 178 ? 24.f * 178 / width : 24.f;
        font_->rasterize_line(name, i == 0 ? 470 : 689, 196, &frame_, kScreenW, kScreenH, size, 0, false, 0, false, 0, 0, 1, false);
    }
    for (int i = 2; i < 4; ++i) {
        const auto& b = ds_name_buttons_[i];
        const bool hover = ds_focus_ == i || (pointer_x_ >= b.x && pointer_x_ < b.x+b.w && pointer_y_ >= b.y && pointer_y_ < b.y+b.h);
        blit_chip(UiChip{b.x,b.y,i == 2 ? 0 : 184,hover ? 44 : 0,b.w,44,101,true},255);
    }
    if (ds_name_confirming_) {
        blit_chip(UiChip{290,268,0,0,700,184,102,true},255);
        blit_chip(UiChip{450,288,0,249,388,30,102,true},255);
        for (int i = 0; i < 2; ++i) {
            const int x = 480 + i*186;
            const bool hover = ds_focus_ == i || (pointer_x_ >= x && pointer_x_ < x+120 && pointer_y_ >= 348 && pointer_y_ < 412);
            blit_chip(UiChip{x,348,i*120+(hover?240:0),185,119,64,102,true},255);
        }
    }
}

void SceneRunner::load_dalsegno_upper_menu() {
    dc4::MesFile ui;
    if (!ui.load_file(dc4::resolve_asset(data_ + "/MES/start.mes"))) return;
    load_sheet(27, "SYSTEM/ADVMENUCHIP");
    sidebar_buttons_.clear(); ds_upper_chips_.clear();
    dc4::MesStream stream(ui);
    dc4::MesCommand c;
    UiChip pending;
    bool active = false, button_chip = false;
    while (stream.next(&c)) {
        if (!active) {
            if (c.op == 0x6D && c.words[0] == 0 && c.words[1] == 0 && c.words[2] == 0 && c.words[3] == 652) active = true;
            else continue;
        }
        if (c.op == 0x18 && c.p1 == 15) break;
        if (c.op == 0x66) {
            UiButton button;
            button.x = c.words[0]; button.y = c.words[1]; button.w = c.words[2]-button.x; button.h = c.words[3]-button.y;
            sidebar_buttons_.push_back(button);
        } else if (c.op == 0x4C && !sidebar_buttons_.empty()) {
            static const std::map<std::string, std::string> actions{
                {"save", ":ds"}, {"load", ":dl"}, {"config", ":option"},
                {":aasave", ":ds"}, {":aaload", ":dl"}, {":aaconfig", ":option"},
                {"win_size", ":aawin_size"}, {"title", ":aatitle"}, {"return", ":sidebarclose"}};
            const auto found = actions.find(c.str);
            sidebar_buttons_.back().action = found == actions.end() ? c.str : found->second;
        }
        else if (c.op == 0x67 || c.op == 0x6D) {
            pending = UiChip{int16_t(c.words[0]),int16_t(c.words[1]),c.words[2],c.words[3],0,0,27,true};
            button_chip = c.op == 0x67;
        } else if (c.op == 0x6A && pending.valid) {
            pending.w = c.words[0]; pending.h = c.words[1]; pending.sheet = c.words[2];
            const int state = c.words[3] == 99 ? 0 : int(c.words[3]);
            if (button_chip && !sidebar_buttons_.empty() && state < 3) sidebar_buttons_.back().state[state] = pending;
            else if (!button_chip && (sidebar_buttons_.empty() || pending.dst_x == 980)) ds_upper_chips_.push_back(pending);
            pending.valid = false;
        }
    }
    UiButton back{980, 0, 237, 47};
    back.action = ":sidebarclose";
    back.state[0] = UiChip{980,0,711,460,235,46,27,true};
    back.state[1] = UiChip{980,0,711,508,235,46,27,true};
    back.state[2] = back.state[1];
    sidebar_buttons_.push_back(back);
}

void SceneRunner::draw_dalsegno_captions(int active_button, int state) {
    if (!font_) return;
    for (const auto& caption : ds_captions_) {
        if (caption.state != 99 && (caption.button < 0 ||
            caption.state != (caption.button == active_button ? state : 0))) continue;
        std::u16string text = sjis_to_utf16(caption.text);
        if (caption.preview && animation_enabled_) {
            const auto playback = dalsegno_playback();
            const double interval = playback.character_interval_ms;
            const double reveal_time = interval * text.size();
            const double hold_time = playback.auto_delay_ms;
            const double elapsed = std::fmod(caption.elapsed_ms, reveal_time + hold_time);
            if (interval > 0 && elapsed < reveal_time) text.resize(size_t(elapsed / interval));
        }
        size_t start = 0;
        int baseline = caption.y + 29;
        do {
            const size_t end = text.find(u'\n', start);
            font_->rasterize_line(text.substr(start, end == std::u16string::npos ? end : end - start),
                caption.x, baseline, &frame_, kScreenW, kScreenH, float(caption.size));
            if (end == std::u16string::npos) break;
            start = end + 1; baseline += 32;
        } while (start < text.size());
    }
}

void SceneRunner::draw_dalsegno_confirmation() {
    for (const auto& chip : ds_modal_chips_) blit_chip(chip, 255);
    for (size_t i = 0; i < ds_modal_buttons_.size(); ++i) {
        const auto& b = ds_modal_buttons_[i];
        const bool hover = ds_focus_ == int(i) || (pointer_x_ >= b.x && pointer_x_ < b.x + b.w &&
            pointer_y_ >= b.y && pointer_y_ < b.y + b.h);
        blit_chip(b.state[hover ? (pointer_down_ ? 2 : 1) : 0], 255);
    }
}

} // namespace dc4scene
