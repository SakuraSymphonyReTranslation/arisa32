#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace dc4ui {
// DC2PC's host-owned scene/menu boundaries use the same 16 brightness
// levels and 40 ms clock as raw 0D modes 1/2. The host supplies elapsed
// active time, so pausing never consumes the transition or its entry wait.
class Dc2pcMenuTransition {
public:
    bool active() const { return phase_ != Phase::Idle; }
    bool revealing() const { return phase_ == Phase::In; }
    void start(std::vector<uint8_t> outgoing, int native_speed, uint32_t wait_ms = 0, bool fade_out = true) {
        outgoing_update_ = {};
        outgoing_draw_ = {};
        circles_ = false;
        circle_sheet_.clear();
        frame_ = std::move(outgoing);
        step_ = 1 << std::clamp(native_speed, 0, 3);
        duration_ = 640 / step_;
        out_duration_ = fade_out ? duration_ : 0;
        wait_ = wait_ms;
        elapsed_ = 0;
        phase_ = Phase::Out;
    }
    void start_custom_outgoing(std::vector<uint8_t> outgoing, int native_speed, uint32_t wait_ms,
                               std::function<bool(uint32_t)> update,
                               std::function<void(std::vector<uint8_t>*)> draw) {
        start(std::move(outgoing), native_speed, wait_ms);
        outgoing_update_ = std::move(update);
        outgoing_draw_ = std::move(draw);
    }
    // DC2DM advmenu/title1 breaks the captured scene into moving EFF tiles.
    // Only the return-to-title host path opts in; regular DC2PC fades stay intact.
    void start_dc2dm_title(std::vector<uint8_t> outgoing, std::vector<uint8_t> sheet) {
        start(std::move(outgoing), 1, 200);
        circles_ = frame_.size() == 1280u * 720u * 4u;
        circle_sheet_ = std::move(sheet);
        out_duration_ = 1000;
    }
    // True exactly once, when the host may replace the scene behind black.
    bool update(uint32_t elapsed_ms) {
        if (phase_ == Phase::Idle || phase_ == Phase::Black) return false;
        const uint32_t previous = elapsed_;
        elapsed_ = uint32_t(std::min<uint64_t>(uint64_t(elapsed_) + elapsed_ms, UINT32_MAX));
        if (phase_ == Phase::Out && outgoing_update_) {
            if (elapsed_ < wait_ || !outgoing_update_(elapsed_ - std::max(wait_, previous))) return false;
            phase_ = Phase::Black;
            elapsed_ = 0;
            return true;
        }
        if (phase_ == Phase::Out && elapsed_ >= uint64_t(wait_) + out_duration_) {
            phase_ = Phase::Black;
            elapsed_ = 0;
            return true;
        }
        if (phase_ == Phase::In && elapsed_ >= duration_) finish();
        return false;
    }
    void reveal(std::vector<uint8_t> incoming) {
        outgoing_update_ = {};
        outgoing_draw_ = {};
        frame_ = std::move(incoming);
        elapsed_ = 0;
        phase_ = Phase::In;
    }
    void finish() {
        outgoing_update_ = {};
        outgoing_draw_ = {};
        phase_ = Phase::Idle;
        frame_.clear();
        circle_sheet_.clear();
    }
    bool draw(std::vector<uint8_t>* output) const {
        if (!active() || frame_.empty()) return false;
        *output = frame_;
        if (outgoing_draw_ && phase_ == Phase::Out && elapsed_ >= wait_) {
            outgoing_draw_(output);
            return true;
        }
        if (circles_ && phase_ == Phase::Out && elapsed_ > wait_) {
            draw_circles(output, std::min(24u, (elapsed_ - wait_) / 40));
            return true;
        }
        int level = 0;
        if (phase_ == Phase::Out) {
            level = elapsed_ <= wait_ ? 16 :
                16 - std::min(16, int((elapsed_ - wait_) / 40) * step_);
        } else if (phase_ == Phase::In) {
            level = std::min(16, int(elapsed_ / 40) * step_);
        }
        for (size_t i = 0; i + 3 < output->size(); i += 4) {
            for (size_t c = 0; c < 3; ++c) (*output)[i+c] = uint8_t(unsigned((*output)[i+c]) * level / 16);
            (*output)[i+3] = 255;
        }
        return true;
    }
private:
    void draw_circles(std::vector<uint8_t>* output, unsigned tick) const {
        std::fill(output->begin(), output->end(), 0);
        for (size_t i = 3; i < output->size(); i += 4) (*output)[i] = 255;
        uint32_t random = 0x9D72;
        const auto velocity = [&random] {
            random = random * 1664525u + 1013904223u;
            return int((random >> 16) % 9) - 4;
        };
        for (int row = 0; row < 18; ++row) for (int col = 0; col < 32; ++col) {
            const int dx = velocity() * int(tick), dy = velocity() * int(tick);
            for (int y = 0; y < 40; ++y) for (int x = 0; x < 40; ++x) {
                const int u = x * 32 / 40, v = y * 32 / 40;
                bool visible;
                if (circle_sheet_.size() == 800u * 32u * 4u) {
                    const size_t mask = (size_t(v) * 800 + tick * 32 + u) * 4;
                    visible = circle_sheet_[mask+3] != 0;
                } else {
                    const int radius = std::max(0, 16 - int(tick));
                    visible = radius && (u-16)*(u-16)+(v-16)*(v-16) < radius*radius;
                }
                const int ox = col * 40 + x + dx, oy = row * 40 + y + dy;
                if (!visible || ox < 0 || ox >= 1280 || oy < 0 || oy >= 720) continue;
                const size_t source = (size_t(row * 40 + y) * 1280 + col * 40 + x) * 4;
                const size_t dest = (size_t(oy) * 1280 + ox) * 4;
                for (size_t c = 0; c < 3; ++c) (*output)[dest+c] = frame_[source+c];
            }
        }
    }
    enum class Phase { Idle, Out, Black, In } phase_ = Phase::Idle;
    std::vector<uint8_t> frame_;
    uint32_t wait_ = 0, elapsed_ = 0, duration_ = 320, out_duration_ = 320;
    int step_ = 2;
    bool circles_ = false;
    std::vector<uint8_t> circle_sheet_;
    std::function<bool(uint32_t)> outgoing_update_;
    std::function<void(std::vector<uint8_t>*)> outgoing_draw_;
};
} // namespace dc4ui
