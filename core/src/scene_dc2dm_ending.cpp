#include "arisa32/scene.h"
#include "arisa32/resource.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace dc4scene {
namespace {
struct Pan { int x, y, end_x, end_y, animation; };
// DC2DM.EXE 0x43F630 selects one crop or ANIME index for each GEDANIME card.
constexpr std::array<Pan, 47> pans{{
    {0,0,0,0,0}, {168,0,800,0,-1}, {240,16,240,16,-1}, {240,16,240,16,-1},
    {308,100,308,540,-1}, {220,300,220,100,-1}, {0,0,0,0,1}, {0,0,0,0,2},
    {330,100,0,100,-1}, {0,0,0,0,3}, {80,20,80,20,-1}, {80,20,80,20,-1},
    {80,20,80,20,-1}, {256,120,256,120,-1}, {256,120,256,120,-1},
    {356,10,0,10,-1}, {110,36,110,36,-1}, {110,36,110,36,-1},
    {0,120,800,120,-1}, {160,0,160,600,-1}, {0,0,0,0,4},
    {100,100,684,100,-1}, {0,0,0,0,5}, {110,30,110,30,-1},
    {110,30,110,30,-1}, {0,0,0,0,6}, {312,0,312,360,-1},
    {136,240,136,0,-1}, {80,90,80,90,-1}, {80,90,80,90,-1},
    {0,0,0,0,7}, {0,0,0,0,8}, {340,40,340,40,-1}, {340,40,340,40,-1},
    {400,20,256,20,-1}, {0,0,0,0,9}, {512,60,40,60,-1}, {0,0,0,0,10},
    {220,60,220,512,-1}, {0,0,0,0,11}, {548,120,368,120,-1}, {0,0,0,0,12},
    {336,0,336,294,-1}, {96,0,96,576,-1}, {0,120,1024,120,-1},
    {0,0,0,0,13}, {0,0,0,0,14}
}};

uint32_t u32(const uint8_t* bytes) {
    return uint32_t(bytes[0]) | uint32_t(bytes[1]) << 8 |
           uint32_t(bytes[2]) << 16 | uint32_t(bytes[3]) << 24;
}

bool read_at(std::ifstream& file, uint32_t offset, uint8_t* bytes, size_t count) {
    file.seekg(offset);
    return bool(file.read(reinterpret_cast<char*>(bytes), std::streamsize(count)));
}

// Clip in the original 1024x576 canvas, then sample at the host's 1.25 scale.
void paint(std::vector<uint8_t>& frame, const Image& image, int dx, int dy,
           int left, int top, int right, int bottom, int alpha = 255) {
    if (!image.valid() || alpha <= 0) return;
    const int x0 = std::max({0, int(std::ceil(left * 1.25)), int(std::ceil(dx * 1.25))});
    const int y0 = std::max({0, int(std::ceil(top * 1.25)), int(std::ceil(dy * 1.25))});
    const int x1 = std::min({kScreenW, int(std::ceil(right * 1.25)), int(std::ceil((dx + image.w) * 1.25))});
    const int y1 = std::min({kScreenH, int(std::ceil(bottom * 1.25)), int(std::ceil((dy + image.h) * 1.25))});
    for (int y = y0; y < y1; ++y) {
        const double v = std::clamp((y + 0.5) / 1.25 - dy - 0.5, 0.0, double(image.h - 1));
        const int iy = int(v), ny = std::min(iy + 1, image.h - 1);
        const double fy = v - iy;
        for (int x = x0; x < x1; ++x) {
            const double u = std::clamp((x + 0.5) / 1.25 - dx - 0.5, 0.0, double(image.w - 1));
            const int ix = int(u), nx = std::min(ix + 1, image.w - 1);
            const double fx = u - ix;
            const auto* a = image.rgba.data() + (size_t(iy) * image.w + ix) * 4;
            const auto* b = image.rgba.data() + (size_t(iy) * image.w + nx) * 4;
            const auto* c = image.rgba.data() + (size_t(ny) * image.w + ix) * 4;
            const auto* d = image.rgba.data() + (size_t(ny) * image.w + nx) * 4;
            int sample[4];
            for (int k = 0; k < 4; ++k)
                sample[k] = int(std::lround((a[k] * (1 - fx) + b[k] * fx) * (1 - fy) +
                                          (c[k] * (1 - fx) + d[k] * fx) * fy));
            const int opacity = sample[3] * alpha / 255;
            auto* dst = frame.data() + (size_t(y) * kScreenW + x) * 4;
            for (int k = 0; k < 3; ++k) dst[k] = uint8_t((sample[k] * opacity + dst[k] * (255 - opacity)) / 255);
        }
    }
}
}

bool SceneRunner::EndingArchive::open(const std::string& name, size_t max_entries,
                                     int width, int height, std::string* error) {
    path = dc4::resolve_asset(name);
    entries.clear();
    std::error_code ec;
    const auto length = std::filesystem::file_size(path, ec);
    std::ifstream file(path, std::ios::binary);
    const auto fail = [&] {
        *error = "missing or invalid DC2DM ending archive: " + name;
        entries.clear();
        return false;
    };
    uint8_t header[20];
    if (ec || length > 128 * 1024 * 1024 || length < 12 || !read_at(file, 0, header, 4)) return fail();
    const uint32_t count = u32(header);
    if (!count || count > max_entries || uint64_t(count) * 8 + 4 > length) return fail();
    for (uint32_t i = 0; i < count; ++i) {
        if (!read_at(file, 4 + 8 * i, header, 8)) return fail();
        const auto offset = u32(header), size = u32(header + 4);
        if (offset < 4 + count * 8 || offset > length || size < 20 || size > length - offset ||
            !read_at(file, offset, header, sizeof(header))) return fail();
        const int w = header[8] | header[9] << 8, h = header[10] | header[11] << 8;
        if (std::memcmp(header, "CRXG", 4) || !w || !h || w > 2048 || h > 2048 ||
            (width && width != w) || (height && height != h)) return fail();
        entries.emplace_back(offset, size);
    }
    return true;
}

bool SceneRunner::EndingArchive::decode(size_t index, Image* image, std::string* error) const {
    if (index >= entries.size()) return false;
    const auto [offset, size] = entries[index];
    std::ifstream file(path, std::ios::binary);
    std::vector<uint8_t> bytes(size);
    dc4::CrxInfo info;
    if (!read_at(file, offset, bytes.data(), size)) {
        *error = "cannot read DC2DM ending frame: " + path;
        return false;
    }
    if (!dc4::crx_decode(bytes.data(), bytes.size(), &image->rgba, &info, error)) return false;
    image->w = info.width; image->h = info.height;
    return true;
}

bool SceneRunner::start_dc2dm_ending() {
    dc2dm_ending_ = {};
    const double remaining = 470000 - (dc2dm_ending_reference_valid_ ? dc2dm_ending_reference_elapsed_ms_ : 0);
    if (remaining <= 0) {
        vm_.write_variable(100, 29, 1);
        dc2dm_ending_reference_valid_ = false;
        dc2dm_ending_reference_elapsed_ms_ = 0;
        return true;
    }
    auto& ending = dc2dm_ending_;
    if (!ending.credits.open(data_ + "/GRP/ED/GED.PCK", 512, 512, 600, &last_error_) ||
        !ending.montage.open(data_ + "/GRP/ED/GEDANIME.PCK", pans.size(), 0, 0, &last_error_)) {
        ending = {};
        return false;
    }
    const auto photo_path = dc4::resolve_asset(data_ + "/GRP/ED/PHOTO.CRX");
    std::ifstream photo(photo_path, std::ios::binary | std::ios::ate);
    const auto photo_size = photo.tellg();
    if (!photo || photo_size < 20 || photo_size > 4 * 1024 * 1024) {
        last_error_ = "missing or invalid DC2DM ending photo overlay";
        ending = {};
        return false;
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(photo_size));
    dc4::CrxInfo info;
    if (!read_at(photo, 0, bytes.data(), bytes.size()) ||
        std::memcmp(bytes.data(), "CRXG", 4) ||
        (bytes[8] | bytes[9] << 8) != 400 || (bytes[10] | bytes[11] << 8) != 300) {
        last_error_ = "missing or invalid DC2DM ending photo overlay";
        ending = {};
        return false;
    }
    if (!dc4::crx_decode(bytes.data(), bytes.size(), &ending.photo.rgba, &info, &last_error_)) {
        ending = {};
        return false;
    }
    ending.photo.w = info.width; ending.photo.h = info.height;
    ending.card_ms = std::max(1.0, std::floor((remaining + 5000) / ending.montage.entries.size()));
    ending.credits_speed = std::max(1, int(ending.credits.entries.size() * 600000.0 / remaining));
    ending.duration_ms = std::max(ending.card_ms * ending.montage.entries.size(),
        std::ceil(ending.credits.entries.size() * 600000.0 / ending.credits_speed)) + 1000.0 / 60;
    ending.active = true;
    scene_dirty_ = true;
    return true;
}

void SceneRunner::finish_dc2dm_ending(bool skipped) {
    if (skipped) {
        scene_audio_.stop_for_movie();
        if (vm_.has_label("skip_end")) vm_.jump("skip_end");
    }
    vm_.write_variable(100, 29, 1);
    dc2dm_ending_ = {};
    dc2dm_ending_reference_valid_ = false;
    dc2dm_ending_reference_elapsed_ms_ = 0;
    scene_dirty_ = true;
}

void SceneRunner::draw_dc2dm_ending() {
    auto& ending = dc2dm_ending_;
    std::fill(frame_.begin(), frame_.end(), 0);
    for (size_t at = 3; at < frame_.size(); at += 4) frame_[at] = 255;
    const int scroll = std::min(int(ending.credits.entries.size() * 600),
                               int(ending.elapsed_ms * ending.credits_speed / 1000));
    const size_t page = size_t(scroll / 600);
    for (auto it = ending.pages.begin(); it != ending.pages.end();) {
        if (it->first + 1 < page || it->first > page) it = ending.pages.erase(it);
        else ++it;
    }
    for (size_t i = page ? page - 1 : 0; i <= page && i < ending.credits.entries.size(); ++i) {
        auto [it, inserted] = ending.pages.try_emplace(i);
        if (inserted && !ending.credits.decode(i, &it->second, &last_error_)) continue;
        paint(frame_, it->second, 480, int((i + 1) * 600) - scroll, 480, 0, 992, 576);
    }
    const size_t card = std::min(ending.montage.entries.size() - 1, size_t(ending.elapsed_ms / ending.card_ms));
    const double local_ms = ending.elapsed_ms - card * ending.card_ms;
    const int fade_tick = std::min(16, int(local_ms * 60 / 1000));
    const size_t first = card && fade_tick < 16 ? card - 1 : card;
    for (auto it = ending.cards.begin(); it != ending.cards.end();) {
        if (it->first < first || it->first > card) it = ending.cards.erase(it);
        else ++it;
    }
    for (size_t i = first; i <= card; ++i) {
        auto [it, inserted] = ending.cards.try_emplace(i);
        auto& cached = it->second;
        const auto& pan = pans[i];
        const double age = ending.elapsed_ms - i * ending.card_ms;
        int crop_x = pan.x, crop_y = pan.y;
        if (pan.animation >= 0) {
            if (inserted) {
                const std::string number = pan.animation < 10 ? "0" + std::to_string(pan.animation) : std::to_string(pan.animation);
                if (!cached.animation.open(data_ + "/GRP/ED/ANIME" + number + ".PCK", 512, 400, 300, &last_error_)) continue;
            }
            if (cached.animation.entries.empty()) continue;
            const double frame_ms = std::max(1.0, std::floor((ending.card_ms + 1000) / cached.animation.entries.size()));
            const int frame = std::min(int(cached.animation.entries.size()) - 1, int(age / frame_ms));
            if (frame != cached.frame) {
                cached.frame = frame;
                if (!cached.animation.decode(size_t(frame), &cached.image, &last_error_)) continue;
            }
        } else {
            if (inserted && !ending.montage.decode(i, &cached.image, &last_error_)) continue;
            const int end_x = std::clamp(pan.end_x, 0, std::max(0, cached.image.w - 400));
            const int end_y = std::clamp(pan.end_y, 0, std::max(0, cached.image.h - 300));
            const double progress = std::min(1.0, age / (ending.card_ms + 1000));
            crop_x = int(pan.x + (end_x - pan.x) * progress);
            crop_y = int(pan.y + (end_y - pan.y) * progress);
        }
        const int alpha = i == card && first != card ? fade_tick * 255 / 16 : 255;
        paint(frame_, cached.image, 80 - crop_x, 160 - crop_y, 80, 160, 480, 460, alpha);
    }
    paint(frame_, ending.photo, 80, 160, 80, 160, 480, 460);
}
}
