// Movie playback (portable MPEG-1 on desktop; FFmpeg on Android).
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace dc4 {
// Resolve movie overlays and original game containers without converting files.
std::string resolve_movie_file(const std::string& data_dir, const std::string& overlay,
                               const std::string& language, const std::string& asset);
class AudioEngine;
class MoviePlayer {
public:
    MoviePlayer();
    ~MoviePlayer();
    MoviePlayer(const MoviePlayer&) = delete;
    MoviePlayer& operator=(const MoviePlayer&) = delete;
    bool open_file(const std::string& path,
                   AudioEngine* audio = nullptr, std::string* error = nullptr);
    bool open(const std::string& data_dir, const std::string& name,
              AudioEngine* audio = nullptr, std::string* error = nullptr);
    void close();
    void update(uint32_t elapsed_ms);
    void set_paused(bool paused);
    void draw_rgba(std::vector<uint8_t>* rgba, int width, int height) const;
    // Copy the current frame directly into a host bitmap, respecting row stride.
    int frame_width() const;
    int frame_height() const;
    bool copy_frame_rgba(uint8_t* rgba, int width, int height, size_t stride) const;
    bool active() const;
    bool finished() const;
    const std::string& error() const;
    int width() const;
    int height() const;
    double time() const;
    double duration() const;
    uint64_t decoded_frames() const;
    uint64_t decoded_audio_frames() const;
    float audio_peak() const;
private:
    bool open_file_backend(const std::string& path, AudioEngine* audio,
                           std::string* error, bool hardware);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace dc4
