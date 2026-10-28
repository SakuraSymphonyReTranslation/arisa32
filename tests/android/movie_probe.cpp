#include "arisa32/movie.h"
#include "arisa32/audio.h"
#include <jni.h>
extern "C" {
#include <libavcodec/jni.h>
}
#include <chrono>
#include <algorithm>
#include <sstream>
#include <thread>

extern "C" JNIEXPORT jstring JNICALL Java_MovieProbe_probe(JNIEnv* env, jclass, jstring filename, jint milliseconds, jboolean live_audio) {
    JavaVM* vm = nullptr;
    env->GetJavaVM(&vm);
    av_jni_set_java_vm(vm, nullptr);
    const char* chars = env->GetStringUTFChars(filename, nullptr);
    if (!chars) return nullptr;
    std::string path(chars);
    env->ReleaseStringUTFChars(filename, chars);
    dc4::AudioEngine audio;
    if (live_audio) audio.init();
    dc4::MoviePlayer movie;
    std::string error;
    const auto opened = std::chrono::steady_clock::now();
    const bool okay = movie.open_file(path, &audio, &error);
    const auto started = std::chrono::steady_clock::now();
    std::vector<uint8_t> first, last;
    bool copy_ok = false, stride_ok = false;
    const int w = movie.frame_width(), h = movie.frame_height();
    if (okay) {
        first.resize(size_t(w) * h * 4); last.resize(first.size());
        copy_ok = movie.copy_frame_rgba(first.data(), w, h, size_t(w) * 4);
        const size_t row = size_t(w) * 4, stride = row + 16;
        std::vector<uint8_t> padded(stride * h, 0xa5);
        stride_ok = movie.copy_frame_rgba(padded.data(), w, h, stride);
        for (int y = 0; y < h; ++y) {
            stride_ok = stride_ok && std::equal(first.begin() + y * row, first.begin() + (y + 1) * row, padded.begin() + y * stride) &&
                std::all_of(padded.begin() + y * stride + row, padded.begin() + (y + 1) * stride,
                    [](uint8_t value) { return value == 0xa5; });
        }
    }
    int64_t max_update_us = 0, total_update_us = 0;
    int updates = 0;
    auto previous = started;
    while (okay && movie.active() && std::chrono::steady_clock::now() - started < std::chrono::milliseconds(milliseconds)) {
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - previous).count();
        previous = now;
        movie.update(static_cast<uint32_t>(elapsed));
        copy_ok = movie.copy_frame_rgba(last.data(), w, h, size_t(w) * 4) && copy_ok;
        const auto cost = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - now).count();
        max_update_us = std::max(max_update_us, static_cast<int64_t>(cost));
        total_update_us += cost; ++updates;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    const auto pause_time = movie.time();
    movie.set_paused(true); movie.update(100);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const bool pause_ok = movie.time() == pause_time;
    movie.set_paused(false);
    std::ostringstream out;
    out << "file=" << path << " opened=" << okay << " dimensions=" << movie.width() << 'x' << movie.height()
        << " open_ms=" << std::chrono::duration_cast<std::chrono::milliseconds>(started-opened).count()
        << " frames=" << movie.decoded_frames() << " audio_frames=" << movie.decoded_audio_frames()
        << " audio_peak=" << movie.audio_peak() << " time=" << movie.time() << " duration=" << movie.duration()
        << " bitmap=" << w << 'x' << h << " copy_ok=" << copy_ok << " stride_ok=" << stride_ok
        << " audio_device=" << audio.device_ready() << " finished=" << movie.finished()
        << " changed=" << (!first.empty() && first != last) << " pause_ok=" << pause_ok
        << " mean_update_us=" << (updates ? total_update_us/updates : 0) << " max_update_us=" << max_update_us
        << " error=" << (error.empty() ? movie.error() : error);
    return env->NewStringUTF(out.str().c_str());
}
