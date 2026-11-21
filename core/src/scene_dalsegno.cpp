#include "arisa32/scene.h"
#include "arisa32/resource.h"
#include <algorithm>
#include <cmath>

namespace dc4scene {

namespace {
// DSAO.exe 0x499140: Music Mode has gaps at BGM 23, 26 and 30.
constexpr int ds_bgm_ids[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,
    24,25,27,28,29,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45};
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
            sync_audio_from_slider(99,7,variable(99,7));
            sync_audio_from_slider(99,207,variable(99,207));
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
    }
    blit_chip(UiChip{1118,255+ds_music_scroll_*360/36-16,751,0,32,32,14,true},255);
    // Native volume controls reveal a clipped bar, with no separate thumb.
    for (int i = 0; i < 2; ++i) {
        const int value = std::clamp(variable(99,i == 0 ? 7 : 207),0,99);
        if (value) blit_chip(UiChip{i == 0 ? 398 : 794,660,236,312,235*value/99,35,14,true},255);
    }
}

void SceneRunner::reset_dalsegno_ui() {
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
    if (sidebar_visible_) return ":sidebarclose";
    if (!current_line_.empty()) return {};
    if (!ds_modal_buttons_.empty()) return ds_modal_buttons_.back().action;
    return ds_callbacks_[3];
}

bool SceneRunner::handle_dalsegno_key(UiKey key, std::string* action) {
    if ((mes_.version & 0xFFFF) != 0x9F9A || (!current_line_.empty() && !sidebar_visible_ && !ds_name_pending_)) return false;
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
    if (c.raw_op==0x1A && !auto_save_path_.empty() && save_load_allowed()) {
        if (!save_state(auto_save_path_)) return false;
        return true;
    }
    if (c.raw_op == 0x1C && c.kind == K::Param2 && c.p1 == 51) {
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
        ds_pending_caption_.preview = c.raw_op == 0x7A && c.words[2] > 0;
        ds_has_caption_ = true;
        return true;
    }
    if (ds_has_caption_ && (c.raw_op == 0x54 || c.raw_op == 0x62)) {
        ds_pending_caption_.text = c.str;
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
        for (int index : {225, 7, 8, 9, 207}) sync_audio_from_slider(99, index, variable(99, index));
        scene_dirty_ = true;
        return true;
    }
    if (c.raw_op != 0x6C || c.kind != K::Words4) return false;
    if (ds_callbacks_[0].empty() || ds_callbacks_[1].empty()) {
        last_error_ = "Dal Segno confirmation has no Yes/No callbacks";
        scene_end_ = true;
        return true;
    }
    // start.mes control group 6 uses ADVMENUCHIP for the common native dialog.
    const int x = int16_t(c.words[0]), y = int16_t(c.words[1]);
    load_sheet(27, "SYSTEM/ADVMENUCHIP");
    ds_modal_chips_ = {UiChip{x, y, 0, 0, 700, 184, 27, true}};
    static constexpr int message_x[] = {0, 0, 0, 0, 388, 388, 388, 0, 388, 388};
    static constexpr int message_y[] = {249, 279, 309, 339, 309, 279, 339, 369, 249, 369};
    if (c.words[3] < 10)
        ds_modal_chips_.push_back(UiChip{x + 160, y + 20, message_x[c.words[3]],
            message_y[c.words[3]], 388, 30, 27, true});
    ds_modal_chips_.push_back(UiChip{x + 336, y + 100, 700, 0, 23, 35, 27, true});
    ds_modal_buttons_.clear();
    for (int i = 0; i < 2; ++i) {
        UiButton button;
        button.x = x + (i == 0 ? 190 : 376); button.y = y + 80;
        button.w = 120; button.h = 64; button.action = ds_callbacks_[i];
        button.state[0] = UiChip{button.x, button.y, i * 120, 185, 119, 64, 27, true};
        button.state[1] = UiChip{button.x, button.y, 240 + i * 120, 185, 119, 64, 27, true};
        button.state[2] = button.state[1];
        ds_modal_buttons_.push_back(button);
    }
    captured_slider_ = -1;
    script_wait_pending_ = true;
    script_wait_ms_ = -1;
    script_wait_skippable_ = false;
    return true;
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
    if ((mes_.version & 0xFFFF) != 0x9F9A || ds_name_pending_ || !ds_modal_buttons_.empty()) return false;
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
    for (int index : {225,7,8,9,207}) sync_audio_from_slider(99, index, variable(99, index));
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

void SceneRunner::draw_dalsegno_captions() {
    if (!font_) return;
    for (const auto& caption : ds_captions_) {
        std::u16string text = sjis_to_utf16(caption.text);
        if (caption.preview && animation_enabled_) {
            const int speed = std::clamp(variable(99, 26), 0, 11);
            static constexpr int intervals[] = {120,100,80,65,50,40,30,24,18,12,6,0};
            const double interval = intervals[speed];
            const double reveal_time = interval * text.size();
            const double hold_time = 1000 + (20 - std::clamp(variable(99, 27), 0, 20)) * 150;
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
