#include "arisa32/scene.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace dc4scene {

double BstSprite::position(int axis) const {
    if (!waves[axis].empty() && waves[axis].front().ticks) {
        const auto& wave = waves[axis].front();
        const double t = double(wave.ticks - 1) / wave.frames;
        const double angle = (wave.start + (wave.end - wave.start) * t) * .0174532777778;
        return wave.origin + wave.amplitude * (axis ? std::cos(angle) : std::sin(angle));
    }
    if (moves.empty()) return axis ? cur_y : cur_x;
    const auto& move = moves.front();
    const double t = double(move.ticks ? move.ticks - 1 : 0) / move.frames;
    return axis ? move.from_y + (move.to_y - move.from_y) * t
                : move.from_x + (move.to_x - move.from_x) * t;
}

void BstSprite::finish_motion() {
    if (!moves.empty()) { cur_x = moves.back().to_x; cur_y = moves.back().to_y; }
    moves.clear();
    waves[0].clear(); waves[1].clear();
    motion_remainder = 0;
}

bool SceneRunner::animating() const {
    return !dialogue_actions_.empty() || !camera_motion_.empty() || dc2pc_transition_active_ || dc2dm_transition_.active ||
        std::any_of(sprites_.begin(), sprites_.end(), [](const BstSprite& s) { return s.moving() || s.fade.active(); }) ||
        (is_dc4_or_dc4ph_ && (transition_active_ || !screen_effect_.empty()));
}

void SceneRunner::set_text_speed(int value) { text_speed_ = std::clamp(value, 0, 11); }

void SceneRunner::reset_sprite_motion_batch() {
    for (auto& s : sprites_) s.relative_origin_valid = false;
    sprite_wave_frames_ = 60;
    sprite_wave_axis_ = 0;
}

void SceneRunner::queue_sprite_move(size_t index, int x, int y, uint32_t frames) {
    auto& s = sprites_[index];
    if (!animation_enabled_ || !effects_enabled_) {
        s.finish_motion(); s.cur_x = x; s.cur_y = y;
    } else {
        const int from_x = s.moves.empty() ? s.cur_x : s.moves.back().to_x;
        const int from_y = s.moves.empty() ? s.cur_y : s.moves.back().to_y;
        // Native 0x4474C0 omits entries whose two velocities are zero.
        if ((from_x != x || from_y != y) && s.moves.size() < 100) {
            if (!s.moving()) s.motion_remainder = 0;
            s.moves.push_back({from_x, from_y, x, y, std::max(1u, frames), 0});
        }
    }
    scene_dirty_ = true;
}

void SceneRunner::queue_sprite_entrance(size_t index, uint16_t kind, uint32_t frames) {
    auto& s = sprites_[index];
    s.finish_motion(); s.fade = {};
    s.visible = true;
    if (animation_enabled_ && effects_enabled_) {
        const int target_x = s.cur_x;
        if (kind) s.cur_x += kind == 1 ? -20 : 20;
        queue_sprite_move(index, target_x, s.cur_y, frames);
        s.alpha = 0;
        s.fade.start_alpha = 0; s.fade.end_alpha = 255;
        s.fade.rate_per_frame = 255.0 / frames;
        s.fade.duration_frames = frames + 1;
    } else s.alpha = 255;
    scene_dirty_ = true;
}

void SceneRunner::queue_sprite_slide(uint16_t x, uint16_t y, uint16_t target, uint16_t mode) {
    if (mode & 0x20) {
        sprite_wave_frames_ = x ? x : 60;
        sprite_wave_axis_ = y ? 1 : 0;
        return;
    }
    if (mode & 0x10) {
        const size_t index = mode & 15;
        if (index >= sprites_.size() || !animation_enabled_ || !effects_enabled_ || y == target) return;
        auto& s = sprites_[index];
        auto& waves = s.waves[sprite_wave_axis_];
        if (waves.size() >= 100) return;
        if (!s.moving()) s.motion_remainder = 0;
        const int phase = sprite_wave_axis_ ? 90 : 0;
        waves.push_back({int(int16_t(x)), int(int16_t(y)) + phase,
                         int(int16_t(target)) + phase, sprite_wave_frames_, 0, 0});
        scene_dirty_ = true;
        return;
    }
    const bool relative = (target & 0x40) != 0;
    const size_t index = relative ? target & 15 : target;
    if (index >= sprites_.size()) return;
    auto& s = sprites_[index];
    if (relative && !s.relative_origin_valid) {
        s.relative_x = s.moves.empty() ? s.cur_x : s.moves.back().to_x;
        s.relative_y = s.moves.empty() ? s.cur_y : s.moves.back().to_y;
    }
    s.relative_origin_valid = relative;
    queue_sprite_move(index, int(int16_t(x)) + (relative ? s.relative_x : 0),
                      int(int16_t(y)) + (relative ? s.relative_y : 0), mode ? mode * 6u : 60u);
}

void SceneRunner::update_sprite_motion(double elapsed_ms) {
    for (auto& s : sprites_) {
        if (s.fade.active()) {
            s.fade.elapsed_frames += elapsed_ms * 60 / 1000;
            const double elapsed = s.fade.end_alpha ? std::max(0.0, std::floor(s.fade.elapsed_frames + 1e-9) - 1)
                                                    : s.fade.elapsed_frames;
            s.alpha = int(std::clamp(s.fade.start_alpha + s.fade.rate_per_frame * elapsed, 0.0, 255.0));
            if (!s.fade.active() || (s.fade.end_alpha == 0 && s.alpha <= 0)) {
                s.visible = s.fade.end_alpha != 0;
                s.alpha = 255; s.fade = {};
            }
            scene_dirty_ = true;
        }
        if (!s.moving()) continue;
        s.motion_remainder += elapsed_ms * 60;
        auto ticks = uint64_t(s.motion_remainder / 1000 + 1e-9);
        s.motion_remainder -= double(ticks) * 1000;
        s.motion_remainder = std::max(0.0, s.motion_remainder);
        while (ticks-- && s.moving()) {
            if (!s.moves.empty()) {
                auto& move = s.moves.front();
                // Native 0x449194 initializes a segment one tick before its first step.
                if (++move.ticks > move.frames) {
                    s.cur_x = move.to_x; s.cur_y = move.to_y;
                    s.moves.pop_front();
                }
            }
            for (int axis = 0; axis < 2; ++axis) {
                auto& waves = s.waves[axis];
                if (waves.empty()) continue;
                auto& wave = waves.front();
                if (!wave.ticks) wave.origin = s.position(axis);
                if (++wave.ticks > wave.frames) waves.pop_front();
            }
            scene_dirty_ = true;
        }
        if (!s.moving()) s.motion_remainder = 0;
    }
}

bool SceneRunner::collect_dialogue_command(const dc4::MesCommand& c) {
    if (!dc4_message_faces()) return false;
    if (c.op == 0xB0 && c.kind == dc4::CmdKind::Words4) {
        if (c.words[0] == 255) {
            const size_t index = std::min<size_t>(c.words[1], 9);
            if (index != dialogue_batch_index_) {
                dialogue_batch_index_ = index;
                dialogue_batches_[index].clear();
            }
        } else {
            collecting_dialogue_batch_ = !collecting_dialogue_batch_;
        }
        return true;
    }
    if (!collecting_dialogue_batch_ || c.kind == dc4::CmdKind::Text) return false;
    auto& batch = dialogue_batches_[dialogue_batch_index_];
    if (batch.size() < 10) batch.push_back(c);
    return true;
}

void SceneRunner::schedule_dialogue_actions() {
    dialogue_actions_.clear(); dialogue_action_ms_ = 0;
    if (!dc4_message_faces()) return;
    size_t prefix_bytes = 0;
    int delay = 0;
    for (size_t i = 0; i < current_line_.size();) {
        if (current_line_[i] != '@') { ++prefix_bytes; ++i; continue; }
        const size_t start = i++;
        int kind = -1, index = 0;
        if (current_line_.compare(i, 2, "bt") == 0 || current_line_.compare(i, 2, "by") == 0) {
            kind = current_line_[i + 1] == 't' ? 0 : 1;
            i += 2;
        } else if (current_line_.compare(i, 3, "cmd") == 0) {
            kind = 2; i += 3;
        } else if (i < current_line_.size() && (current_line_[i] == 'd' || current_line_[i] == 't')) {
            kind = current_line_[i++] == 'd' ? 2 : 3;
        }
        if (kind >= 0 && i < current_line_.size() && current_line_[i] >= '0' && current_line_[i] <= '9') {
            while (i < current_line_.size() && current_line_[i] >= '0' && current_line_[i] <= '9')
                index = std::min(100000, index * 10 + current_line_[i++] - '0');
            if (kind == 3) { delay = index; continue; }
            const double due = delay ? double(delay) * 100 :
                int(((prefix_bytes / 2 * 22) / (text_speed_ * 2 + 16)) * 16.659999847);
            delay = 0;
            if (index >= 1 && index <= (kind == 2 ? 10 : 4))
                dialogue_actions_.push_back({kind, index - 1, due});
            continue;
        }
        // These controls are removed by the native parser before it measures the text prefix.
        static const char* controls[] = {"muf", "lse", "wb1", "wb2", "wb3", "wb4", "wb5",
            "bi1", "bi2", "bi3", "bi4", "bi5", "bo1", "bo2", "bo3", "bo4", "bo5",
            "b1", "b2", "b3", "b4", "b5", "w1", "w2", "w3", "w4", "w5", "bc", "bf", "ht", "hy", "mu", "se"};
        bool control = false;
        for (const auto* tag : controls) {
            const size_t len = std::strlen(tag);
            if (current_line_.compare(start + 1, len, tag) != 0) continue;
            i = start + 1 + len;
            while (i < current_line_.size() && current_line_[i] >= '0' && current_line_[i] <= '9') ++i;
            if (!(len == 2 && tag[0] == 'b' && tag[1] >= '1' && tag[1] <= '5')) delay = 0;
            control = true; break;
        }
        if (!control) { i = start + 1; ++prefix_bytes; }
    }
    std::stable_sort(dialogue_actions_.begin(), dialogue_actions_.end(),
        [](const DialogueAction& a, const DialogueAction& b) { return a.due_ms < b.due_ms; });
    if (!animation_enabled_) finish_dialogue_actions();
    else update_dialogue_actions(0);
}

void SceneRunner::run_dialogue_action(int kind, int index) {
    if (kind == 2) {
        const auto commands = dialogue_batches_[size_t(index)];
        reset_sprite_motion_batch();
        size_t transfers = 0;
        for (const auto& command : commands) dispatch_command(command, transfers);
    } else if (animation_enabled_ && effects_enabled_) {
        auto& s = sprites_[size_t(index)];
        s.finish_motion();
        const int x = s.cur_x, y = s.cur_y;
        for (int offset : {8, 0, -8, 0})
            queue_sprite_move(size_t(index), x + (kind == 1 ? offset : 0), y + (kind == 0 ? offset : 0), 10);
    }
    scene_dirty_ = true;
}

void SceneRunner::update_dialogue_actions(double elapsed_ms) {
    const double end = dialogue_action_ms_ + elapsed_ms;
    while (!dialogue_actions_.empty() && dialogue_actions_.front().due_ms <= end) {
        const auto action = dialogue_actions_.front();
        dialogue_actions_.erase(dialogue_actions_.begin());
        update_sprite_motion(std::max(0.0, action.due_ms - dialogue_action_ms_));
        dialogue_action_ms_ = std::max(dialogue_action_ms_, action.due_ms);
        run_dialogue_action(action.kind, action.index);
    }
    update_sprite_motion(std::max(0.0, end - dialogue_action_ms_));
    dialogue_action_ms_ = end;
}

void SceneRunner::finish_dialogue_actions() {
    const auto actions = std::move(dialogue_actions_);
    dialogue_actions_.clear();
    for (const auto& action : actions) if (action.kind == 2) run_dialogue_action(action.kind, action.index);
}

} // namespace dc4scene
