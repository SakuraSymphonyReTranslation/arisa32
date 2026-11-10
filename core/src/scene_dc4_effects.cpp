#include "arisa32/scene.h"

#include <algorithm>
#include <cmath>

namespace dc4scene {
namespace {
constexpr int kDensityCounts[] = {0, 1, 3, 6, 9, 12, 20};
constexpr int kAtlasFrames[] = {7, 7, 6, 8, 5, 8};

int native_random(uint32_t& state, int range) {
    state = state * 214013u + 2531011u;
    return int((state >> 16) & 0x7fff) % range;
}

void draw_atlas_sprite(std::vector<uint8_t>& dst, const Image& atlas,
                       int sx, int sy, int width, int height, float x, float y,
                       float pivot_x, float pivot_y, float scale, float angle, float opacity,
                       bool white = false) {
    if (!atlas.valid() || width <= 0 || height <= 0 || scale <= 0 || opacity <= 0 || sx < 0 || sy < 0 ||
        sx + width > atlas.w || sy + height > atlas.h) return;
    const float cosine = std::cos(angle), sine = std::sin(angle);
    float min_x = float(kScreenW), min_y = float(kScreenH), max_x = 0, max_y = 0;
    for (int i = 0; i < 4; ++i) {
        const float u = ((i & 1) ? width : 0) - pivot_x;
        const float v = ((i & 2) ? height : 0) - pivot_y;
        const float dx = x + pivot_x + scale * (cosine * u - sine * v);
        const float dy = y + pivot_y + scale * (sine * u + cosine * v);
        min_x = std::min(min_x, dx); max_x = std::max(max_x, dx);
        min_y = std::min(min_y, dy); max_y = std::max(max_y, dy);
    }
    const int left = std::clamp(int(std::floor(min_x)), 0, kScreenW);
    const int right = std::clamp(int(std::ceil(max_x)), 0, kScreenW);
    const int top = std::clamp(int(std::floor(min_y)), 0, kScreenH);
    const int bottom = std::clamp(int(std::ceil(max_y)), 0, kScreenH);
    const float du = cosine / scale, dv = -sine / scale;
    const float opacity_scale = opacity / 255.f;
    for (int dy = top; dy < bottom; ++dy) {
        const float rx = left + .5f - x - pivot_x, ry = dy + .5f - y - pivot_y;
        float u = (cosine * rx + sine * ry) / scale + pivot_x - .5f;
        float v = (-sine * rx + cosine * ry) / scale + pivot_y - .5f;
        for (int dx = left; dx < right; ++dx, u += du, v += dv) {
            if (u < -.5f || v < -.5f || u >= width - .5f || v >= height - .5f) continue;
            const float cu = std::max(u, 0.f), cv = std::max(v, 0.f);
            const int ix = int(cu), iy = int(cv);
            const int nx = std::min(ix + 1, width - 1), ny = std::min(iy + 1, height - 1);
            const float fx = cu - ix, fy = cv - iy;
            const auto* p0 = &atlas.rgba[(size_t(sy + iy) * atlas.w + sx + ix) * 4];
            const auto* p1 = &atlas.rgba[(size_t(sy + iy) * atlas.w + sx + nx) * 4];
            const auto* p2 = &atlas.rgba[(size_t(sy + ny) * atlas.w + sx + ix) * 4];
            const auto* p3 = &atlas.rgba[(size_t(sy + ny) * atlas.w + sx + nx) * 4];
            if (!(p0[3] | p1[3] | p2[3] | p3[3])) continue;
            auto* pixel = &dst[(size_t(dy) * kScreenW + dx) * 4];
            if (white) {
                if (opacity == 1 && (p0[3] & p1[3] & p2[3] & p3[3]) == 255) {
                    pixel[0] = pixel[1] = pixel[2] = pixel[3] = 255;
                    continue;
                }
                const float upper = p0[3] + fx * (int(p1[3]) - p0[3]);
                const float lower = p2[3] + fx * (int(p3[3]) - p2[3]);
                const float alpha = (upper + fy * (lower - upper)) * opacity_scale;
                for (int channel = 0; channel < 3; ++channel)
                    pixel[channel] = uint8_t(std::clamp(int(pixel[channel] + (255 - pixel[channel]) * alpha + .5f), 0, 255));
                pixel[3] = 255;
                continue;
            }
            const float a0 = (1 - fx) * (1 - fy) * p0[3] * opacity_scale;
            const float a1 = fx * (1 - fy) * p1[3] * opacity_scale;
            const float a2 = (1 - fx) * fy * p2[3] * opacity_scale;
            const float a3 = fx * fy * p3[3] * opacity_scale;
            const float alpha = a0 + a1 + a2 + a3;
            for (int channel = 0; channel < 3; ++channel) {
                const float rgb = a0 * p0[channel] + a1 * p1[channel] + a2 * p2[channel] + a3 * p3[channel];
                pixel[channel] = uint8_t(std::clamp(int(rgb + (1 - alpha) * pixel[channel] + .5f), 0, 255));
            }
            pixel[3] = 255;
        }
    }
}
} // namespace

int SceneRunner::effect_random(int range) {
    return native_random(effect_random_state_, range);
}

bool SceneRunner::dispatch_dc4_effect_command(const dc4::MesCommand& c) {
    if (!is_dc4_or_dc4ph_ || c.dc2dm || c.classic_dc2() ||
        (mes_.version != 0xAAB6 && mes_.version != 0xABB6)) return false;
    if (c.opcode() == dc4::Opcode::SCREEN_EFFECT) {
        start_screen_effect(c.p1, c.p2);
        return true;
    }
    // Native 0x28 uses the same five particle layers for petals and fairy lights.
    if (c.p1 == 7 || c.p1 == 16) return false;
    sakura_ = {};
    sakura_sheet_ = {};
    fairy_lights_ = {};
    fairy_light_sheet_ = {};
    if (c.p1 == 13 && c.p2 % 10 != 0) {
        if (load_grp_crx("SYSTEM/EFFYOUSEI", &fairy_light_sheet_, nullptr)) {
            fairy_lights_.active = true;
            fairy_lights_.parameter = c.p2;
            fairy_lights_.initialize(effect_random_state_);
        }
    } else if ((c.p1 == 1 || c.p1 == 9 || c.p1 == 11) && c.p2 % 10 != 0) {
        const auto sheet = sheets_.find(28);
        if (sheet != sheets_.end() && sheet->second.valid()) {
            sakura_sheet_ = sheet->second;
            sakura_.active = true;
            sakura_.type = c.p1;
            sakura_.parameter = c.p2;
            sakura_.initialize(effect_random_state_);
        }
    }
    scene_dirty_ = true;
    return true;
}

void FairyLightSystem::reset_particle(size_t index, int depth, uint32_t& random_state) {
    const auto random = [&](int range) { return native_random(random_state, range); };
    auto& p = particles[index];
    p = {};
    p.x = float(random(1280)); p.y = float(random(720));
    p.vy = (random(4) + 1) * .2f;
    if (random(2) == 0) p.vy = -p.vy;
    p.amplitude = float(random(32) + 28);
    p.drift = random(3);
    if (random(2) == 0) p.drift = -p.drift;
    p.row = depth + random(2);
    p.phase = float(random(360));
}

void FairyLightSystem::initialize(uint32_t& random_state) {
    const int count = kDensityCounts[std::clamp(parameter % 10, 0, 6)];
    particles.resize(size_t(count) * 5);
    tick_remainder = 0;
    for (size_t i = 0; i < particles.size(); ++i)
        reset_particle(i, 4 - int(i) / count, random_state);
}

void FairyLightSystem::update(uint32_t elapsed_ms, uint32_t& random_state) {
    if (!active) return;
    const int count = int(particles.size()) / 5;
    tick_remainder += double(elapsed_ms) * 60;
    while (tick_remainder >= 1000) {
        tick_remainder -= 1000;
        for (size_t i = 0; i < particles.size(); ++i) {
            auto& p = particles[i];
            p.phase = std::fmod(p.phase + .01f, 2 * 3.14159265358979323846f);
            p.y -= p.vy;
            p.alpha += p.brightening ? .005f : -.005f;
            if (p.alpha >= .8f) { p.alpha = .8f; p.brightening = false; }
            if (p.alpha <= 0) { p.alpha = 0; p.brightening = true; }
            if (++p.cell_ticks > 30) {
                p.cell_ticks = 0;
                p.cell = (p.cell + 1) % kAtlasFrames[p.row];
            }
            const float x = p.x + std::sin(p.phase) * p.amplitude + p.drift / 10.f;
            if (x < -28 || x > 1280 || p.y < -28 || p.y > 748)
                reset_particle(i, 4 - int(i) / count, random_state);
        }
    }
}

void FairyLightSystem::draw(std::vector<uint8_t>& frame, const Image& sheet) const {
    if (!active) return;
    for (const auto& p : particles)
        draw_atlas_sprite(frame, sheet, p.cell * 32, p.row * 28, 28,
            std::min(28, sheet.h - p.row * 28),
            p.x + std::sin(p.phase) * p.amplitude + p.drift / 10.f, p.y,
            0, 0, 1, 0, p.alpha);
}

void SakuraSystem::reset_particle(size_t index, int depth, uint32_t& random_state) {
    const auto random = [&](int range) { return native_random(random_state, range); };
    auto& p = particles[index];
    p = {};
    p.scale = depth == 0 ? 1.f : (10 - depth) / 10.f;
    p.x = float(random(1280));
    const int screen = random(3);
    p.x += screen == 1 ? -1280 : screen == 2 ? 1280 : 0;
    p.y = float(-random(720));
    p.vy = (random(parameter % 10 == 6 ? 4 : 10) + 2) / 1.5f;
    p.amplitude = float(random(32) + 28);
    p.row = depth + random(2);
    p.phase = float(random(360));
    p.cell = random(kAtlasFrames[p.row]);
    p.alpha = uint8_t((.5f + random(3) / 10.f - depth * .02f) * 255);
}

void SakuraSystem::initialize(uint32_t& random_state) {
    const int count = kDensityCounts[std::clamp(parameter % 10, 0, 6)];
    particles.resize(size_t(count) * 5);
    tick_remainder = 0;
    for (size_t i = 0; i < particles.size(); ++i)
        reset_particle(i, 4 - int(i) / count, random_state);
}

void SakuraSystem::update(uint32_t elapsed_ms, uint32_t& random_state) {
    if (!active) return;
    const int count = int(particles.size()) / 5;
    const int wind = parameter < 100 ? parameter / 10 : -(parameter % 100) / 10;
    tick_remainder += double(elapsed_ms) * 60;
    while (tick_remainder >= 1000) {
        tick_remainder -= 1000;
        for (size_t i = 0; i < particles.size(); ++i) {
            auto& p = particles[i];
            p.x += wind;
            p.phase += .05f;
            p.y += p.vy;
            if (++p.cell_ticks > 18) {
                p.cell_ticks = 0;
                p.cell = (p.cell + 1) % kAtlasFrames[p.row];
            }
            if (p.y >= 720) reset_particle(i, 4 - int(i) / count, random_state);
        }
    }
}

void SakuraSystem::draw(std::vector<uint8_t>& frame, const Image& sheet) const {
    if (!active) return;
    for (const auto& p : particles)
        draw_atlas_sprite(frame, sheet, p.cell * 32, p.row * 28, 28, 28,
                          p.x + std::sin(p.phase) * p.amplitude, p.y,
                          0, 0, p.scale, 0, p.alpha / 255.f);
}

void SceneRunner::update_sakura(uint32_t elapsed_ms) {
    if ((!sakura_.active && !fairy_lights_.active) || !animation_enabled_ || !effects_enabled_ || !visual_effects_enabled_) return;
    sakura_.update(elapsed_ms, effect_random_state_);
    fairy_lights_.update(elapsed_ms, effect_random_state_);
    scene_dirty_ = true;
}

void SceneRunner::draw_sakura() {
    if (!animation_enabled_ || !effects_enabled_ || !visual_effects_enabled_) return;
    sakura_.draw(frame_, sakura_sheet_);
    fairy_lights_.draw(frame_, fairy_light_sheet_);
}

void SceneRunner::start_screen_effect(int type, int duration) {
    screen_effect_.clear();
    if (!animation_enabled_ || !effects_enabled_ || !screen_effect_atlas_.valid() ||
        (type < 0 || type > 2) || duration <= 0) return;
    if (type == 2) {
        ScreenEffectSprite sprite;
        sprite.from_x = sprite.to_x = 280;
        sprite.pivot_x = 360; sprite.pivot_y = 355;
        sprite.width = 732; sprite.height = 720;
        sprite.scale_phase_ms = duration * 200.0;
        sprite.duration_ms = sprite.scale_phase_ms * 2 + 500;
        screen_effect_.push_back(sprite);
        return;
    }
    const auto large = [&](float from, float to) {
        ScreenEffectSprite sprite;
        sprite.from_x = from; sprite.to_x = to;
        sprite.pivot_x = 360; sprite.pivot_y = 355;
        sprite.width = 732; sprite.height = 720;
        sprite.duration_ms = duration * 200.0;
        screen_effect_.push_back(sprite);
    };
    large(-732, type == 0 ? 1536.f : 1288.f);
    if (type == 1) large(1288, -732);
    const int count = type == 0 ? 53 : 52;
    for (int i = 0; i < count; ++i) {
        ScreenEffectSprite sprite;
        const bool reverse = type == 1 && i >= 27;
        sprite.from_x = float(reverse ? effect_random(1280) + 2080 : -effect_random(1280) - 800);
        sprite.to_x = reverse ? -800.f : type == 0 ? 1536.f : 1280.f;
        sprite.y = float(effect_random(848) - 128);
        sprite.sx = 720; sprite.width = 278; sprite.height = 256;
        sprite.scale = (effect_random(80) + 20) / 100.f;
        sprite.duration_ms = (duration + effect_random(3)) * 200.0;
        sprite.alpha = .5f + effect_random(5) / 10.f;
        sprite.fade = true;
        screen_effect_.push_back(sprite);
    }
}

void SceneRunner::update_screen_effect(uint32_t elapsed_ms) {
    for (auto& sprite : screen_effect_) sprite.elapsed_ms += elapsed_ms;
    screen_effect_.erase(std::remove_if(screen_effect_.begin(), screen_effect_.end(),
        [](const ScreenEffectSprite& sprite) { return sprite.elapsed_ms >= sprite.duration_ms; }), screen_effect_.end());
}

double SceneRunner::screen_effect_remaining_ms() const {
    double remaining = 0;
    for (const auto& sprite : screen_effect_)
        remaining = std::max(remaining, sprite.duration_ms - sprite.elapsed_ms);
    return remaining;
}

void SceneRunner::draw_screen_effect() {
    if (screen_effect_.empty()) return;
    // The original sakura atlas has white RGB throughout; retain colored replacement assets too.
    bool white = true;
    for (size_t i = 0; i < screen_effect_atlas_.rgba.size(); i += 4) {
        const auto* p = &screen_effect_atlas_.rgba[i];
        if (p[3] && (p[0] != 255 || p[1] != 255 || p[2] != 255)) { white = false; break; }
    }
    for (const auto& sprite : screen_effect_) {
        const float t = float(sprite.elapsed_ms / sprite.duration_ms);
        float scale = sprite.scale, angle = t * 3 * 3.14159265358979323846f;
        if (sprite.scale_phase_ms > 0) {
            // Native type 2 queues 1% -> 500% -> 550% -> 0%, with a 30-frame middle phase.
            const double phase = sprite.scale_phase_ms, elapsed = sprite.elapsed_ms;
            if (elapsed < phase) scale = .01f + 4.99f * float(elapsed / phase);
            else if (elapsed < phase + 500) scale = 5.f + .5f * float((elapsed - phase) / 500);
            else scale = 5.5f * float(1 - (elapsed - phase - 500) / phase);
            angle = 0;
        }
        draw_atlas_sprite(frame_, screen_effect_atlas_, sprite.sx, sprite.sy, sprite.width, sprite.height,
            sprite.from_x + (sprite.to_x - sprite.from_x) * t, sprite.y,
            sprite.pivot_x, sprite.pivot_y, scale, angle,
            sprite.alpha * (sprite.fade ? 1 - t : 1), white);
    }
}

void SceneRunner::start_transition(int frames, bool scale_speed) {
    if (!animation_enabled_ || !effects_enabled_ || !transition_pending_ || transition_snap_.empty() || frames <= 0) {
        transition_pending_ = false;
        return;
    }
    transition_pending_ = false;
    transition_kind_ = transition_type_;
    transition_elapsed_ms_ = 0;
    // Native mask wipes and 0x38 sprites bypass the effect-speed preference.
    if (scale_speed) {
        if (effect_speed_ < 5) frames += int(double(frames / 2) / 5 * (5 - effect_speed_));
        if (effect_speed_ > 5) frames -= int(double(frames / 2) / 6 * (effect_speed_ - 5));
    }
    transition_duration_ms_ = std::max(1, frames) * 1000.0 / 60.0;
    transition_active_ = true;
    scene_dirty_ = true;
}

void SceneRunner::draw_transition(uint8_t* dst) {
    if (!transition_active_ || !dst || transition_snap_.size() != frame_.size()) return;
    const double t = std::clamp(transition_elapsed_ms_ / transition_duration_ms_, 0.0, 1.0);
    const bool masked = transition_kind_ == 21 && mask_.valid();
    const int threshold = int(std::lround(255 - 510 * t));
    const int fade_alpha = int(std::lround(255 * (1 - t)));
    for (int y = 0; y < kScreenH; ++y) {
        for (int x = 0; x < kScreenW; ++x) {
            int old_alpha = fade_alpha;
            if (masked) {
                // Native mode 3 subtracts 510/frames from the old screenshot's alpha.
                const int mx = std::min(x, mask_.w - 1), my = std::min(y, mask_.h - 1);
                old_alpha = std::clamp(int(mask_.rgba[(size_t(my) * mask_.w + mx) * 4 + 2]) + threshold, 0, 255);
            }
            const size_t offset = (size_t(y) * kScreenW + x) * 4;
            for (int channel = 0; channel < 3; ++channel)
                dst[offset + channel] = uint8_t((transition_snap_[offset + channel] * old_alpha + dst[offset + channel] * (255 - old_alpha)) / 255);
        }
    }
}
} // namespace dc4scene
