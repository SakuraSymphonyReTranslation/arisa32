#pragma once
#include <algorithm>
#include <cstdint>
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
        frame_ = std::move(outgoing);
        step_ = 1 << std::clamp(native_speed, 0, 3);
        duration_ = 640 / step_;
        out_duration_ = fade_out ? duration_ : 0;
        wait_ = wait_ms;
        elapsed_ = 0;
        phase_ = Phase::Out;
    }
    // True exactly once, when the host may replace the scene behind black.
    bool update(uint32_t elapsed_ms) {
        if (phase_ == Phase::Idle || phase_ == Phase::Black) return false;
        elapsed_ = uint32_t(std::min<uint64_t>(uint64_t(elapsed_) + elapsed_ms, UINT32_MAX));
        if (phase_ == Phase::Out && elapsed_ >= uint64_t(wait_) + out_duration_) {
            phase_ = Phase::Black;
            elapsed_ = 0;
            return true;
        }
        if (phase_ == Phase::In && elapsed_ >= duration_) finish();
        return false;
    }
    void reveal(std::vector<uint8_t> incoming) {
        frame_ = std::move(incoming);
        elapsed_ = 0;
        phase_ = Phase::In;
    }
    void finish() {
        phase_ = Phase::Idle;
        frame_.clear();
    }
    bool draw(std::vector<uint8_t>* output) const {
        if (!active() || frame_.empty()) return false;
        *output = frame_;
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
    enum class Phase { Idle, Out, Black, In } phase_ = Phase::Idle;
    std::vector<uint8_t> frame_;
    uint32_t wait_ = 0, elapsed_ = 0, duration_ = 320, out_duration_ = 320;
    int step_ = 2;
};
} // namespace dc4ui
