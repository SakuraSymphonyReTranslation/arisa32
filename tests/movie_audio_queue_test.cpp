#include "movie_audio_queue.h"
#include <iostream>

namespace {
int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; ++failures; } } while (false)
std::vector<float> tone(size_t frames, float value) { return std::vector<float>(frames * 2, value); }
auto collect(std::vector<float>& output) {
    return [&output](const float* data, size_t count) {
        output.insert(output.end(), data, data + count * 2); return true;
    };
}
void delayed_soundtrack() {
    constexpr int rate = 44100;
    dc4::detail::MovieAudioQueue queue(rate, rate * 4);
    CHECK(queue.push(5.0, tone(rate / 10, .5f)));
    std::vector<float> output;
    for (int step = 1; step <= 51; ++step) {
        const auto before = output.size();
        CHECK(queue.feed(step / 10.0, rate / 10, collect(output)));
        CHECK(output.size() - before <= size_t(rate / 10 * 2));
        CHECK(queue.buffered_frames() <= size_t(rate / 10));
    }
    CHECK(output.size() == size_t(rate * 51 / 10 * 2));
    CHECK(std::all_of(output.begin(), output.begin() + rate * 5 * 2, [](float x) { return x == 0; }));
    CHECK(std::all_of(output.begin() + rate * 5 * 2, output.end(), [](float x) { return x == .5f; }));
    CHECK(queue.empty());
}
void bounded_feed_and_retry() {
    dc4::detail::MovieAudioQueue queue(1000, 100);
    CHECK(queue.push(10.0, tone(100, 1)));
    CHECK(!queue.push(10.1, tone(1, 2)));
    const double before = queue.end_time();
    CHECK(!queue.feed(20, 17, [](const float*, size_t) { return false; }));
    CHECK(queue.end_time() == before && queue.buffered_frames() == 100);
    std::vector<float> output;
    CHECK(queue.feed(20, 17, collect(output)));
    CHECK(output.size() == 34 && queue.buffered_frames() == 100);
    CHECK(std::all_of(output.begin(), output.end(), [](float x) { return x == 0; }));
    queue.advance_to(10.0);
    output.clear();
    CHECK(queue.feed(20, 50, collect(output)));
    CHECK(output == tone(50, 1) && queue.buffered_frames() == 50);
    CHECK(queue.push(10.1, tone(50, 2)));
    CHECK(queue.buffered_frames() == 100);
}
void overlaps_and_gaps() {
    dc4::detail::MovieAudioQueue queue(1000, 1000);
    CHECK(queue.push(0.0, tone(100, 1)));
    CHECK(queue.push(.05, tone(100, 2)));
    CHECK(queue.push(.20, tone(100, 3)));
    std::vector<float> output;
    CHECK(queue.feed(1.0, 1000, collect(output)));
    auto expected = tone(100, 1);
    for (const auto& segment : {tone(50, 2), tone(50, 0), tone(100, 3)})
        expected.insert(expected.end(), segment.begin(), segment.end());
    CHECK(output == expected && queue.empty());
}
void silent_progress_and_recovery() {
    dc4::detail::MovieAudioQueue queue(1000, 1000);
    CHECK(queue.push(0, tone(100, 1)));
    std::vector<float> output;
    CHECK(queue.feed(.1, 100, collect(output)));
    // The audio stream ends while the host continues presenting buffered video.
    queue.advance_to(1.0);
    std::vector<float> late;
    for (int sample = 0; sample < 300; ++sample) { late.push_back(float(sample)); late.push_back(float(sample)); }
    CHECK(queue.push(.8, late));
    output.clear();
    CHECK(queue.feed(1.1, 100, collect(output)));
    CHECK(output == std::vector<float>(late.begin() + 400, late.end()));
    CHECK(queue.empty() && queue.end_time() == 1.1);
    // A software decoder resumes from an earlier keyframe at a new mixer base.
    CHECK(queue.push(5.0, tone(100, 9)));
    queue.reset(2.0);
    CHECK(queue.empty() && queue.buffered_frames() == 0);
    CHECK(queue.push(1.9, tone(200, 4)));
    output.clear();
    CHECK(queue.feed(2.1, 100, collect(output)));
    CHECK(output == tone(100, 4) && queue.empty());
}
void fractional_sample_lead() {
    dc4::detail::MovieAudioQueue queue(44100, 44100);
    CHECK(queue.push(0, tone(44100, .5f)));
    std::vector<float> output;
    // 0.332 seconds is 14641.2 samples: demuxing must stop after the rounded
    // lead is filled, even though its end time is fractionally below the target.
    CHECK(queue.feed(.332, 44100, collect(output)));
    CHECK(output.size() == 14641 * 2);
    CHECK(queue.filled_through(.332));
    CHECK(!queue.filled_through(.3321));
}
}
int main() {
    delayed_soundtrack(); bounded_feed_and_retry(); overlaps_and_gaps(); silent_progress_and_recovery();
    fractional_sample_lead();
    return failures ? 1 : 0;
}
