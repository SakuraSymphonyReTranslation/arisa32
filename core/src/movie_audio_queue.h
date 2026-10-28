// Timestamped stereo PCM waiting to enter the bounded movie mixer.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <utility>
#include <vector>

namespace dc4::detail {
class MovieAudioQueue {
public:
    explicit MovieAudioQueue(uint32_t rate, size_t capacity)
        : rate_(rate), capacity_(capacity) {}

    bool push(double timestamp, std::vector<float> stereo) {
        if (!std::isfinite(timestamp) || timestamp < 0 || timestamp > 24 * 3600 ||
            stereo.size() % 2 || stereo.size() / 2 > capacity_ - buffered_) return false;
        if (stereo.empty()) return true;
        buffered_ += stereo.size() / 2;
        chunks_.push_back({sample_at(timestamp), std::move(stereo), 0});
        discard_elapsed();
        return true;
    }

    // A silent interval already presented by the host must not replay when
    // demuxing eventually supplies older audio. Keep future samples intact.
    void advance_to(double timestamp) {
        end_ = std::max(end_, sample_at(timestamp));
        discard_elapsed();
    }
    void reset(double timestamp = 0) {
        chunks_.clear(); buffered_ = 0; end_ = sample_at(timestamp);
    }
    bool empty() const { return chunks_.empty(); }
    size_t buffered_frames() const { return buffered_; }
    double end_time() const { return double(end_) / rate_; }
    bool filled_through(double timestamp) const { return end_ >= sample_at(timestamp); }

    // Fill only the requested lead and available mixer space. Long gaps use
    // this small zero block, never an allocation proportional to their length.
    template<class Sink>
    bool feed(double until, size_t available, Sink&& sink) {
        const int64_t limit = sample_at(until);
        static constexpr std::array<float, 2048> silence{};
        discard_elapsed();
        while (!chunks_.empty() && end_ < limit && available) {
            auto& chunk = chunks_.front();
            const int64_t position = chunk.start + int64_t(chunk.offset);
            size_t count = size_t(std::min<int64_t>(limit - end_, int64_t(available)));
            if (position > end_) {
                count = std::min({count, size_t(position - end_), silence.size() / 2});
                if (!sink(silence.data(), count)) return false;
            } else {
                count = std::min(count, chunk.stereo.size() / 2 - chunk.offset);
                if (!sink(chunk.stereo.data() + chunk.offset * 2, count)) return false;
                chunk.offset += count; buffered_ -= count;
            }
            end_ += int64_t(count); available -= count;
            discard_elapsed();
        }
        return true;
    }

private:
    struct Chunk { int64_t start; std::vector<float> stereo; size_t offset; };
    uint32_t rate_;
    size_t capacity_, buffered_ = 0;
    int64_t end_ = 0;
    std::deque<Chunk> chunks_;

    int64_t sample_at(double time) const {
        return int64_t(std::llround(std::max(0.0, time) * rate_));
    }
    void discard_elapsed() {
        while (!chunks_.empty()) {
            auto& chunk = chunks_.front();
            const size_t frames = chunk.stereo.size() / 2;
            const size_t skipped = size_t(std::min<int64_t>(frames,
                std::max<int64_t>(0, end_ - chunk.start)));
            if (skipped > chunk.offset) {
                buffered_ -= skipped - chunk.offset; chunk.offset = skipped;
            }
            if (chunk.offset < frames) break;
            chunks_.pop_front();
        }
    }
};
} // namespace dc4::detail
