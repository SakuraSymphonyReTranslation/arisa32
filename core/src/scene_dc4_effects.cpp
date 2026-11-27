#include "arisa32/scene.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

namespace dc4scene {
namespace {
constexpr int kDensityCounts[] = {0, 1, 3, 6, 9, 12, 20};
constexpr int kAtlasFrames[] = {7, 7, 6, 8, 5, 8};

#if defined(__ARM_NEON)
uint8x8_t divide_by_255(uint16x8_t v) {
    return vshrn_n_u16(vaddq_u16(vaddq_u16(v, vdupq_n_u16(1)), vshrq_n_u16(v, 8)), 8);
}

void blend_mask_row(uint8_t* dst, const uint8_t* old, const uint8_t* mask, int threshold) {
    const auto add = vdup_n_u8(uint8_t(std::max(0, threshold)));
    const auto sub = vdup_n_u8(uint8_t(std::max(0, -threshold)));
    for (int x = 0; x < kScreenW; x += 8, dst += 32, old += 32, mask += 32) {
        const auto a = vqsub_u8(vqadd_u8(vld4_u8(mask).val[2], add), sub);
        const uint64_t bits = vget_lane_u64(vreinterpret_u64_u8(a), 0);
        if (!bits) continue;
        auto result = vld4_u8(dst);
        const auto source = vld4_u8(old);
        for (int c = 0; c < 3; ++c)
            result.val[c] = divide_by_255(vmlal_u8(vmull_u8(source.val[c], a), result.val[c], vmvn_u8(a)));
        vst4_u8(dst, result);
    }
}

void whiten_eight(uint8_t* dst, uint8x8_t coverage) {
    auto out = vld4_u8(dst);
    for (int c = 0; c < 3; ++c) {
        const auto dark = vmull_u8(vmvn_u8(out.val[c]), vmvn_u8(coverage));
        out.val[c] = vmvn_u8(divide_by_255(vaddq_u16(dark, vdupq_n_u16(127))));
    }
    vst4_u8(dst, out);
}
#endif

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
    if (c.opcode() == dc4::Opcode::LENS_PARAMETERS) {
        if (!c.ds || !is_dalsegno_profile_) return false;
        if (c.words[0] == 0) {
            ds_lens_parameters_.x = int16_t(c.words[1]);
            ds_lens_parameters_.y = int16_t(c.words[2]);
        } else {
            ds_lens_parameters_.start_angle = int16_t(c.words[1]);
            ds_lens_parameters_.end_angle = int16_t(c.words[2]);
            ds_lens_parameters_.duration_ms = c.words[3] * 200u;
        }
        return true;
    }
    if (c.ds) {
        if (is_dalsegno_profile_ && c.opcode() == dc4::Opcode::SCREEN_EFFECT) {
            start_screen_effect(c.p2, c.p1);
            return true;
        }
        if (!is_dalsegno_profile_ || c.opcode() != dc4::Opcode::EFFECT_SETUP) return false;
        if (c.p1 == 15) {
            start_dalsegno_lens(c.p2);
            return true;
        }
        if (c.p1 != 0 && c.p1 != 13) return false;
        ds_lens_ = {};
    } else if (!is_dc4_or_dc4ph_ || c.dc2dm || c.classic_dc2() ||
               (mes_.version != 0xAAB6 && mes_.version != 0xABB6)) return false;
    if (c.opcode() == dc4::Opcode::SCREEN_EFFECT) {
        start_screen_effect(c.p1, c.p2);
        return true;
    }
    // Petals and fairy lights share five particle layers.
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

void SceneRunner::start_dalsegno_lens(int density) {
    ds_lens_ = {};
    const int count = 4 * std::clamp(density, 0, 5);
    if (!count || !load_grp_crx("SYSTEM/EFFLENS", &ds_lens_.sheet, nullptr)) return;
    ds_lens_.parameters = ds_lens_parameters_;
    for (int i = 0; i < count; ++i) {
        const int radius_x = 64 * i + 8 + effect_random(20);
        const int radius_y = 64 * i + 8 + effect_random(20);
        const float scale = (300 + effect_random(400)) / 100.f;
        const int cell = effect_random(3);
        constexpr int offsets[] = {140, 180, 200}, sizes[] = {40, 20, 8};
        ds_lens_.reflections.push_back({radius_x, radius_y, offsets[cell], sizes[cell],
                                        scale, effect_random(10) / 100.f});
    }
}

void SceneRunner::draw_dalsegno_lens() {
    if (ds_lens_.reflections.empty() || !particle_effects_visible()) return;
    const auto& p = ds_lens_.parameters;
    float degrees = float(p.start_angle);
    if (p.end_angle != -1 && p.duration_ms)
        degrees += (p.end_angle - p.start_angle) * (float(ds_lens_.elapsed_ms) / p.duration_ms);
    const float radians = degrees * (3.14159265358979323846f / 180.f);
    const float dx = std::sin(radians), dy = std::cos(radians);
    for (const auto& r : ds_lens_.reflections)
        draw_atlas_sprite(frame_, ds_lens_.sheet, r.source_x, 0, r.size, r.size,
                          p.x + dx * r.radius_x, p.y + dy * r.radius_y,
                          r.size / 2.f, r.size / 2.f, r.scale, 0, r.alpha);
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

bool SceneRunner::particle_effects_visible() const {
    return animation_enabled_ && effects_enabled_ && visual_effects_enabled_ &&
        (!is_dalsegno_profile_ || variable(99, 10) != 0);
}

void SceneRunner::update_sakura(uint32_t elapsed_ms) {
    if ((!sakura_.active && !fairy_lights_.active) || !particle_effects_visible()) return;
    sakura_.update(elapsed_ms, effect_random_state_);
    fairy_lights_.update(elapsed_ms, effect_random_state_);
    scene_dirty_ = true;
}

void SceneRunner::draw_sakura() {
    if (!particle_effects_visible()) return;
    sakura_.draw(frame_, sakura_sheet_);
    fairy_lights_.draw(frame_, fairy_light_sheet_);
}

void SceneRunner::start_screen_effect(int type, int duration) {
    screen_effect_.clear();
    const bool ds = is_dalsegno_profile_;
    if (!animation_enabled_ || !effects_enabled_ || duration <= 0 || type < 0) return;
    if (ds ? (ds_petals_.alpha.empty() || !visual_effects_enabled_ || type > 1)
           : (!screen_effect_atlas_.valid() || type > 2)) return;
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
    const bool long_sweep = !ds && type == 0;
    large(-732, long_sweep ? 1536.f : 1288.f);
    if (type == 1) large(1288, -732);
    const int count = (ds ? 33 : 53) - type;
    for (int i = 0; i < count; ++i) {
        ScreenEffectSprite sprite;
        const bool reverse = type == 1 && i >= (ds ? 17 : 27);
        sprite.from_x = float(reverse ? effect_random(1280) + 2080 : -effect_random(1280) - 800);
        sprite.to_x = reverse ? -800.f : long_sweep ? 1536.f : 1280.f;
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
    if (is_dalsegno_profile_) {
        if (!visual_effects_enabled_) return;
        auto& coverage = ds_petals_.coverage;
        coverage.resize((kScreenW / 2 + 1) * (kScreenH / 2 + 1));
        std::fill(coverage.begin(), coverage.end(), 0);
        for (const auto& sprite : screen_effect_)
            ds_petals_.stamp(sprite, float(sprite.elapsed_ms / sprite.duration_ms));
        constexpr int stride = kScreenW / 2 + 1;
        for (int y = 0; y < kScreenH / 2; ++y)
            coverage[(y + 1) * stride - 1] = coverage[(y + 1) * stride - 2];
        std::copy_n(coverage.end() - 2 * stride, stride, coverage.end() - stride);
        ds_petals_.composite(frame_);
        return;
    }
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

bool SceneRunner::DsPetalAtlas::load(const Image& image) {
    if (!image.valid() || image.w < 2 * width || image.h < 2 * height) return false;
    alpha.resize(width * height);
    for (int y = 0; y < height; ++y) {
        const auto* row = image.rgba.data() + size_t(2 * y) * image.w * 4 + 3;
        for (int x = 0; x < width; ++x, row += 8)
            alpha[y * width + x] = uint8_t((row[0] + row[4] + row[4 * image.w] + row[4 * image.w + 4] + 2) / 4);
    }
    return true;
}

void SceneRunner::DsPetalAtlas::stamp(const ScreenEffectSprite& s, float t) {
    const float angle = t * 3 * 3.14159265358979323846f;
    const float cs = std::cos(angle), sn = std::sin(angle);
    const float px = s.pivot_x * .5f, py = s.pivot_y * .5f;
    const float cx = (s.from_x + t * (s.to_x - s.from_x)) * .5f + px, cy = s.y * .5f + py;
    const int w = s.width / 2, h = s.height / 2;
    const float mx = cx + s.scale * (cs * (w * .5f - px) - sn * (h * .5f - py));
    const float my = cy + s.scale * (sn * (w * .5f - px) + cs * (h * .5f - py));
    const float rx = .5f * s.scale * (std::abs(cs) * w + std::abs(sn) * h);
    const float ry = .5f * s.scale * (std::abs(sn) * w + std::abs(cs) * h);
    const int left = std::max(0, int(std::floor(mx - rx))), right = std::min(kScreenW / 2, int(std::ceil(mx + rx)));
    const int top = std::max(0, int(std::floor(my - ry))), bottom = std::min(kScreenH / 2, int(std::ceil(my + ry)));
    if (left >= right || top >= bottom) return;
    const int opacity = int(std::lround(255 * s.alpha * (s.fade ? 1 - t : 1)));
    const int du = int(std::lround(65536 * cs / s.scale)), dv = int(std::lround(-65536 * sn / s.scale));
    const auto* source = alpha.data() + s.sx / 2;
    for (int y = top; y < bottom; ++y) {
        const float dx = left + .5f - cx, dy = y + .5f - cy;
        int u = int(65536 * (px + (cs * dx + sn * dy) / s.scale));
        int v = int(65536 * (py + (cs * dy - sn * dx) / s.scale));
        auto* dst = coverage.data() + y * (kScreenW / 2 + 1) + left;
        for (int x = left; x < right; ++x, u += du, v += dv, ++dst) {
            if (unsigned(u) >= unsigned(w << 16) || unsigned(v) >= unsigned(h << 16)) continue;
            const int a = source[(v >> 16) * width + (u >> 16)] * opacity / 255;
            *dst += uint8_t(((255 - *dst) * a + 127) / 255);
        }
    }
}

void SceneRunner::DsPetalAtlas::composite(std::vector<uint8_t>& frame) const {
    constexpr int stride = kScreenW / 2 + 1;
#if !defined(__ARM_NEON)
    const auto whiten = [](uint8_t* p, int a) {
        for (int i = 0; i < 3; ++i) p[i] += uint8_t(((255 - p[i]) * a + 127) / 255);
    };
#endif
    for (int y = 0; y < kScreenH / 2; ++y) {
        const auto* upper = coverage.data() + y * stride;
        const auto* lower = upper + stride;
        auto* dst = frame.data() + size_t(2 * y) * kScreenW * 4;
#if defined(__ARM_NEON)
        for (int x = 0; x < kScreenW / 2; x += 8, dst += 64) {
            const auto a = vld1_u8(upper + x), b = vld1_u8(upper + x + 1);
            const auto c = vld1_u8(lower + x), d = vld1_u8(lower + x + 1);
            if (!vget_lane_u64(vreinterpret_u64_u8(vorr_u8(vorr_u8(a, b), vorr_u8(c, d))), 0)) continue;
            const auto top = vzip_u8(a, vrhadd_u8(a, b));
            const auto sum = vaddq_u16(vaddl_u8(a, b), vaddl_u8(c, d));
            const auto bottom = vzip_u8(vrhadd_u8(a, c), vrshrn_n_u16(sum, 2));
            for (int i = 0; i < 2; ++i) {
                whiten_eight(dst + 32 * i, top.val[i]);
                whiten_eight(dst + kScreenW * 4 + 32 * i, bottom.val[i]);
            }
        }
#else
        for (int x = 0; x < kScreenW / 2; ++x, dst += 8) {
            const int a = upper[x], b = upper[x + 1], c = lower[x], d = lower[x + 1];
            if (!(a | b | c | d)) continue;
            auto* next = dst + kScreenW * 4;
            if ((a & b & c & d) == 255) {
                std::memset(dst, 255, 8);
                std::memset(next, 255, 8);
                continue;
            }
            whiten(dst, a); whiten(dst + 4, (a + b + 1) / 2);
            whiten(next, (a + c + 1) / 2); whiten(next + 4, (a + b + c + d + 2) / 4);
        }
#endif
    }
}

void SceneRunner::start_transition(int frames, bool scale_speed) {
    if (!animation_enabled_ || !effects_enabled_ || !transition_pending_ || transition_snap_.empty() || frames <= 0) {
        transition_pending_ = false;
        if (!transition_active_) transition_snap_.clear();
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

void SceneRunner::finish_transition() {
    transition_active_ = transition_pending_ = false;
    transition_elapsed_ms_ = transition_duration_ms_ = 0;
    transition_snap_.clear();
    transition_snap_.shrink_to_fit();
}

void SceneRunner::draw_transition(uint8_t* dst) {
    if (!transition_active_ || !dst || transition_snap_.size() != frame_.size()) return;
    const double t = std::clamp(transition_elapsed_ms_ / transition_duration_ms_, 0.0, 1.0);
    const bool masked = transition_kind_ == 21 && mask_.valid();
    const int threshold = int(std::lround(255 - 510 * t));
    const int fade_alpha = int(std::lround(255 * (1 - t)));
#if defined(__ARM_NEON)
    if (masked && mask_.w >= kScreenW && mask_.h >= kScreenH) {
        for (int y = 0; y < kScreenH; ++y)
            blend_mask_row(dst + size_t(y) * kScreenW * 4, transition_snap_.data() + size_t(y) * kScreenW * 4,
                           mask_.rgba.data() + size_t(y) * mask_.w * 4, threshold);
        return;
    }
#endif
    for (int y = 0; y < kScreenH; ++y) {
        for (int x = 0; x < kScreenW; ++x) {
            int old_alpha = fade_alpha;
            if (masked) {
                // Native mode 3 subtracts 510/frames from the old screenshot's alpha.
                const int mx = std::min(x, mask_.w - 1), my = std::min(y, mask_.h - 1);
                old_alpha = std::clamp(int(mask_.rgba[(size_t(my) * mask_.w + mx) * 4 + 2]) + threshold, 0, 255);
            }
            const size_t offset = (size_t(y) * kScreenW + x) * 4;
            if (old_alpha == 0) continue;
            if (old_alpha == 255) {
                std::memcpy(dst + offset, transition_snap_.data() + offset, 3);
                continue;
            }
            for (int channel = 0; channel < 3; ++channel)
                dst[offset + channel] = uint8_t((transition_snap_[offset + channel] * old_alpha + dst[offset + channel] * (255 - old_alpha)) / 255);
        }
    }
}
} // namespace dc4scene
