// Android movie backend: decode original game files, never transcode them.
#include "arisa32/movie.h"
#include "arisa32/audio.h"
#include "arisa32/resource.h"
#include "movie_audio_queue.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}
#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <filesystem>
#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace dc4 {
namespace {
constexpr int kRate = AudioEngine::kOutputRate;
constexpr int kMaxWidth = 4096, kMaxHeight = 2304;
constexpr size_t kVideoBytes = 12 * 1024 * 1024;
constexpr size_t kPacketBytes = 2 * 1024 * 1024;
const char* hardware_decoder(AVCodecID codec) {
    switch (codec) {
    case AV_CODEC_ID_H264: return "h264_mediacodec";
    case AV_CODEC_ID_HEVC: return "hevc_mediacodec";
    case AV_CODEC_ID_MPEG2VIDEO: return "mpeg2_mediacodec";
    case AV_CODEC_ID_MPEG4: return "mpeg4_mediacodec";
    case AV_CODEC_ID_VP8: return "vp8_mediacodec";
    case AV_CODEC_ID_VP9: return "vp9_mediacodec";
    default: return nullptr;
    }
}
std::string ff_error(int code) {
    char message[AV_ERROR_MAX_STRING_SIZE]{};
    av_strerror(code, message, sizeof(message));
    return message;
}
}
struct MoviePlayer::Impl {
    AVFormatContext* format = nullptr;
    AVCodecContext* video = nullptr;
    AVCodecContext* sound = nullptr;
    AVPacket* packet = nullptr;
    AVFrame* scratch = nullptr;
    SwsContext* scaler = nullptr;
    mutable SwsContext* viewport_scaler = nullptr;
    SwrContext* resampler = nullptr;
    struct VideoFrame { AVFrame* frame; double pts; };
    std::deque<VideoFrame> pending;
    AVFrame* due_video = nullptr;
    std::deque<AVPacket*> deferred_video;
    size_t deferred_bytes = 0;
    std::vector<uint8_t> pixels;
    std::vector<float> samples;
    detail::MovieAudioQueue audio_pending{kRate, size_t(kRate) * 4};
    AudioEngine* audio = nullptr;
    std::string failure;
    int video_index = -1, audio_index = -1, width = 0, height = 0;
    int output_width = 0, output_height = 0;
    size_t video_queue = 4;
    double clock = 0, duration = 0, origin = 0, video_end = 0, audio_end = 0, audio_base = 0;
    double audio_decoded_end = 0;
    double last_video_pts = 0, frame_step = 1.0 / 30;
    uint64_t frames = 0, audio_frames = 0;
    float peak = 0;
    bool done = true, paused = false, audio_clock = false, eof = false;
    bool video_flushed = false, audio_flushed = false;
    bool video_drained = false, audio_drained = false, resampler_drained = false;
    int64_t video_drain_progress = 0, audio_drain_progress = 0;
    bool packet_pending = false, demux_blocked = false;
    bool hardware = false, try_hardware = true;
    ~Impl() {
        if (audio) audio->end_movie_audio();
        av_frame_free(&due_video);
        for (auto& f : pending) av_frame_free(&f.frame);
        for (auto& p : deferred_video) av_packet_free(&p);
        sws_freeContext(scaler);
        sws_freeContext(viewport_scaler);
        swr_free(&resampler);
        av_frame_free(&scratch);
        av_packet_free(&packet);
        avcodec_free_context(&video);
        avcodec_free_context(&sound);
        avformat_close_input(&format);
    }
    bool fail(const std::string& reason) { failure = reason; done = true; return false; }
    bool open_codec(int index, AVCodecContext** context) {
        const auto* params = format->streams[index]->codecpar;
        const char* hardware_name = index == video_index && try_hardware ? hardware_decoder(params->codec_id) : nullptr;
        const auto* decoder = hardware_name ? avcodec_find_decoder_by_name(hardware_name) : nullptr;
        const bool using_hardware = decoder != nullptr;
        if (!decoder) decoder = avcodec_find_decoder(params->codec_id);
        if (!decoder) return fail(std::string("movie codec is not included in this build: ") + avcodec_get_name(params->codec_id));
        *context = avcodec_alloc_context3(decoder);
        if (!*context) return fail("cannot allocate movie decoder");
        int result = avcodec_parameters_to_context(*context, params);
        if (result < 0) return fail(ff_error(result));
        (*context)->max_pixels = int64_t(kMaxWidth) * kMaxHeight;
        (*context)->thread_count = using_hardware ? 1 : 2;
        (*context)->thread_type = FF_THREAD_SLICE;
        AVDictionary* options = nullptr;
        // JNI enumerates hardware codecs and excludes Android software codecs.
        if (using_hardware) av_dict_set(&options, "ndk_codec", "0", 0);
        result = avcodec_open2(*context, decoder, &options);
        av_dict_free(&options);
        if (result >= 0 && index == video_index) hardware = using_hardware;
#ifdef __ANDROID__
        if (result >= 0) __android_log_print(ANDROID_LOG_INFO, "dc4", "movie %s decoder=%s mode=%s",
            index == video_index ? "video" : "audio", decoder->name, using_hardware ? "MediaCodec" : "software");
#endif
        return result >= 0 || fail("cannot open movie decoder: " + ff_error(result));
    }
    double pts(AVFrame* frame, int stream, double fallback) const {
        if (frame->best_effort_timestamp == AV_NOPTS_VALUE) return fallback;
        const double value = frame->best_effort_timestamp * av_q2d(format->streams[stream]->time_base) - origin;
        return std::isfinite(value) ? std::max(0.0, value) : fallback;
    }
    bool stage_samples(int count, double timestamp) {
        if (count <= 0) return true;
        audio_frames += count;
        audio_decoded_end = std::max(audio_decoded_end, timestamp + double(count) / kRate);
        for (int i = 0; i < count * 2; ++i) peak = std::max(peak, std::abs(samples[i]));
        samples.resize(size_t(count) * 2);
        if (!audio_pending.push(timestamp, std::move(samples)))
            return fail("decoded movie audio exceeded its buffer limit");
        return true;
    }
    bool feed_audio(double target) {
        // The mixer holds at most one second of this producer's audio. The
        // timestamp queue retains future PCM without allocating long silences.
        const size_t queued = audio_clock ? audio->movie_audio_queued() : 0;
        const size_t available = queued < size_t(kRate) ? size_t(kRate) - queued : 0;
        if (!audio_pending.feed(target + .3, available, [&](const float* data, size_t count) {
                return !audio_clock || audio->queue_movie_audio(data, count);
            })) return fail("movie audio queue exceeded its limit");
        audio_end = audio_pending.end_time();
        return true;
    }
    bool audio_frame(AVFrame* frame) {
        if (frame->sample_rate <= 0 || frame->sample_rate > 192000 ||
            frame->ch_layout.nb_channels < 1 || frame->ch_layout.nb_channels > 8 ||
            frame->nb_samples < 0 || frame->nb_samples > 262144)
            return fail("invalid decoded movie audio format");
        const bool continuing = resampler != nullptr;
        if (!resampler) {
            AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
            int result = swr_alloc_set_opts2(&resampler, &stereo, AV_SAMPLE_FMT_FLT, kRate,
                &frame->ch_layout, AVSampleFormat(frame->format), frame->sample_rate, 0, nullptr);
            if (result < 0 || !resampler || (result = swr_init(resampler)) < 0)
                return fail("cannot initialize movie audio resampler: " + ff_error(result));
        }
        double timestamp = pts(frame, audio_index, audio_decoded_end);
        // swr_convert first emits samples retained from the preceding input.
        // Their position precedes this frame's PTS by the resampler delay.
        if (frame->best_effort_timestamp != AV_NOPTS_VALUE)
            timestamp = std::max(0.0, timestamp -
                double(swr_get_delay(resampler, frame->sample_rate)) / frame->sample_rate);
        // Coarse container timestamps (notably ASF) must not introduce tiny
        // gaps/overlaps in a continuous soundtrack. Keep real delays intact.
        if (continuing && std::abs(timestamp - audio_decoded_end) <= .025)
            timestamp = audio_decoded_end;
        const int capacity = swr_get_out_samples(resampler, frame->nb_samples);
        if (capacity < 0 || capacity > 524288) return fail("invalid resampled movie audio size");
        samples.resize(size_t(capacity) * 2);
        uint8_t* out = reinterpret_cast<uint8_t*>(samples.data());
        int count = swr_convert(resampler, &out, capacity,
            const_cast<const uint8_t**>(frame->extended_data), frame->nb_samples);
        if (count < 0) return fail("cannot decode movie audio: " + ff_error(count));
        return stage_samples(count, timestamp);
    }
    bool receive(AVCodecContext* decoder, bool is_video) {
        for (;;) {
            const int result = avcodec_receive_frame(decoder, scratch);
            if (result == AVERROR_EOF) {
                (is_video ? video_drained : audio_drained) = true;
                return true;
            }
            if (result == AVERROR(EAGAIN)) return true;
            if (result < 0) return fail("cannot decode movie frame: " + ff_error(result));
            if (is_video && video_flushed) video_drain_progress = av_gettime_relative();
            if (!is_video && audio_flushed) audio_drain_progress = av_gettime_relative();
            bool okay = true;
            if (is_video) {
                if (scratch->width < 16 || scratch->height < 16 || scratch->width > kMaxWidth ||
                    scratch->height > kMaxHeight || scratch->width != width || scratch->height != height)
                    okay = fail("invalid or changing movie dimensions");
                else {
                    AVFrame* frame = av_frame_clone(scratch);
                    if (!frame) okay = fail("cannot retain movie frame");
                    else {
                        const double timestamp = pts(frame, video_index, last_video_pts);
                        pending.push_back({frame, timestamp});
                        last_video_pts = timestamp + frame_step;
                        video_end = std::max(video_end, last_video_pts);
                    }
                }
            } else okay = audio_frame(scratch);
            av_frame_unref(scratch);
            if (!okay) return false;
        }
    }
    bool send(AVCodecContext* decoder, AVPacket* input, bool is_video) {
        int result = avcodec_send_packet(decoder, input);
        if (result == AVERROR(EAGAIN)) {
            if (!receive(decoder, is_video)) return false;
            result = avcodec_send_packet(decoder, input);
        }
        if (result < 0 && result != AVERROR_EOF)
            return fail("invalid movie packet: " + ff_error(result));
        return receive(decoder, is_video);
    }
    bool pump(double target) {
        // Demux only a short lead. Never load a whole video or soundtrack.
        demux_blocked = false;
        for (int packets = 0; packets < 128; ++packets) {
            collect_due(target);
            if (!feed_audio(target)) return false;
            if (pending.size() < video_queue && !deferred_video.empty()) {
                AVPacket* retained = deferred_video.front(); deferred_video.pop_front();
                deferred_bytes -= retained->size;
                const bool okay = send(video, retained, true);
                av_packet_free(&retained);
                if (!okay) return false;
                continue;
            }
            if (eof) break;
            const bool video_full = pending.size() >= video_queue;
            const bool audio_ready = !sound || audio_pending.filled_through(target + .3);
            // MPEG program streams can place the next audio packet behind many
            // video packets. Retain compressed packets while filling audio so a
            // full decoded-frame queue cannot deadlock the audio-driven clock.
            if ((video_full && audio_ready) ||
                (audio_clock && audio->movie_audio_queued() >= size_t(kRate))) break;
            if (video_end >= target + .3 && audio_ready) break;
            if (!packet_pending) {
                const int result = av_read_frame(format, packet);
                if (result == AVERROR_EOF) { eof = true; break; }
                if (result < 0) return fail("cannot read movie packet: " + ff_error(result));
            }
            bool okay = true;
            if (packet->stream_index == video_index) {
                if (video_full) {
                    if (packet->size < 0 || size_t(packet->size) > kPacketBytes)
                        okay = fail("movie video packet exceeds the compressed-buffer limit");
                    else if (size_t(packet->size) > kPacketBytes - deferred_bytes) {
                        // Keep this already-read packet until there is room;
                        // reading onward would reorder or drop video packets.
                        packet_pending = demux_blocked = true;
                        break;
                    }
                    else {
                        AVPacket* retained = av_packet_clone(packet);
                        if (!retained) okay = fail("cannot retain movie packet");
                        else { deferred_video.push_back(retained); deferred_bytes += retained->size; }
                    }
                } else okay = send(video, packet, true);
            }
            else if (packet->stream_index == audio_index) okay = send(sound, packet, false);
            av_packet_unref(packet);
            packet_pending = false;
            if (!okay) return false;
        }
        if (eof && deferred_video.empty() && !video_flushed) {
            video_flushed = true;
            video_drain_progress = av_gettime_relative();
            if (!send(video, nullptr, true)) return false;
        }
        // MediaCodec can return EAGAIN after accepting its end-of-stream packet.
        if (video_flushed && !video_drained && !receive(video, true)) return false;
        if (eof && sound && !audio_flushed) {
            audio_flushed = true;
            audio_drain_progress = av_gettime_relative();
            if (!send(sound, nullptr, false)) return false;
        }
        if (audio_flushed && !audio_drained && !receive(sound, false)) return false;
        const int64_t now = av_gettime_relative();
        if ((video_flushed && !video_drained && now - video_drain_progress > 5 * AV_TIME_BASE) ||
            (audio_flushed && !audio_drained && now - audio_drain_progress > 5 * AV_TIME_BASE))
            return fail("movie decoder stalled while draining its final frames");
        if (audio_drained && !resampler_drained) {
            resampler_drained = true;
            if (resampler) {
                const int capacity = swr_get_out_samples(resampler, 0);
                if (capacity > 0 && capacity <= 524288) {
                    samples.resize(size_t(capacity) * 2);
                    uint8_t* out = reinterpret_cast<uint8_t*>(samples.data());
                    int count = swr_convert(resampler, &out, capacity, nullptr, 0);
                    if (count < 0) return fail("cannot drain movie audio resampler: " + ff_error(count));
                    if (!stage_samples(count, audio_decoded_end)) return false;
                }
            }
        }
        return feed_audio(target);
    }
    void collect_due(double target) {
        while (!pending.empty() && (pending.front().pts <= target + .001 || !frames)) {
            auto next = pending.front(); pending.pop_front();
            av_frame_free(&due_video);
            due_video = next.frame;
            ++frames;
        }
    }
    bool display(double target) {
        collect_due(target);
        AVFrame* selected = due_video;
        due_video = nullptr;
        // Convert only the newest due frame; late frames never waste RGBA work.
        if (selected) {
            scaler = sws_getCachedContext(scaler, width, height, AVPixelFormat(selected->format),
                output_width, output_height, AV_PIX_FMT_RGBA, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
            if (!scaler) { av_frame_free(&selected); return fail("cannot convert movie video format"); }
            uint8_t* out[] = {pixels.data(), nullptr, nullptr, nullptr};
            int strides[] = {output_width * 4, 0, 0, 0};
            const int result = sws_scale(scaler, selected->data, selected->linesize, 0, height, out, strides);
            av_frame_free(&selected);
            if (result != output_height) return fail("cannot convert movie video frame");
        }
        return true;
    }
    bool recover_software() {
        if (!hardware) return false;
        // A vendor decoder can fail after startup. Rewind to a keyframe in
        // the same original file and resume software decoding at the clock.
        const int64_t timestamp = int64_t((clock + origin) * AV_TIME_BASE);
        const int seek = avformat_seek_file(format, -1, INT64_MIN, timestamp, timestamp, AVSEEK_FLAG_BACKWARD);
        if (seek < 0) return fail("cannot resume movie in software: " + ff_error(seek));
#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_WARN, "dc4", "movie MediaCodec failed at %.3fs; resuming in software: %s", clock, failure.c_str());
#endif
        for (auto& retained : pending) av_frame_free(&retained.frame);
        pending.clear();
        av_frame_free(&due_video);
        for (auto& retained : deferred_video) av_packet_free(&retained);
        deferred_video.clear(); deferred_bytes = 0;
        av_packet_unref(packet);
        packet_pending = demux_blocked = false;
        avcodec_free_context(&video);
        if (sound) avcodec_flush_buffers(sound);
        swr_free(&resampler);
        try_hardware = false;
        if (!open_codec(video_index, &video)) return false;
        if (audio && !audio->begin_movie_audio(kRate)) return fail("cannot resume movie audio");
        audio_base = audio_end = video_end = last_video_pts = clock;
        audio_decoded_end = clock;
        audio_pending.reset(clock);
        eof = video_flushed = audio_flushed = done = false;
        video_drained = audio_drained = resampler_drained = false;
        video_drain_progress = audio_drain_progress = 0;
        failure.clear();
        return pump(clock) && display(clock);
    }
};

MoviePlayer::MoviePlayer() : impl_(new Impl) {}
MoviePlayer::~MoviePlayer() = default;
void MoviePlayer::close() {
    const std::string failure = impl_->failure;
    impl_.reset(new Impl);
    impl_->failure = failure;
}
bool MoviePlayer::open_file(const std::string& path, AudioEngine* audio, std::string* error) {
    if (open_file_backend(path, audio, error, true)) return true;
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_WARN, "dc4", "movie decoder retry in software: %s", impl_->failure.c_str());
#endif
    return open_file_backend(path, audio, error, false);
}
bool MoviePlayer::open_file_backend(const std::string& path, AudioEngine* audio, std::string* error, bool hardware) {
    close();
    impl_->failure.clear();
    impl_->try_hardware = hardware;
    auto fail = [&](std::string reason) {
        impl_->failure = reason; close();
        if (error) *error = reason;
        return false;
    };
    std::error_code ec;
    const auto length = std::filesystem::file_size(path, ec);
    if (ec || length < 32 || length > 2048ull * 1024 * 1024)
        return fail("movie is missing, empty, or too large");
    impl_->format = avformat_alloc_context();
    if (!impl_->format) return fail("cannot allocate movie demuxer");
    impl_->format->probesize = 2 * 1024 * 1024;
    impl_->format->max_analyze_duration = 5 * AV_TIME_BASE;
    AVDictionary* options = nullptr;
    av_dict_set(&options, "protocol_whitelist", "file", 0);
    av_dict_set(&options, "format_whitelist", "asf,mpeg,mpegts,mpegvideo,mov,avi,matroska,webm,ogg", 0);
    int result = avformat_open_input(&impl_->format, path.c_str(), nullptr, &options);
    av_dict_free(&options);
    if (result < 0) return fail("cannot open original movie: " + ff_error(result));
    result = avformat_find_stream_info(impl_->format, nullptr);
    if (result < 0) return fail("cannot read movie streams: " + ff_error(result));
    impl_->video_index = av_find_best_stream(impl_->format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (impl_->video_index < 0) return fail("movie has no supported video stream");
    const auto* stream = impl_->format->streams[impl_->video_index];
    impl_->width = stream->codecpar->width; impl_->height = stream->codecpar->height;
    if (impl_->width < 16 || impl_->height < 16 || impl_->width > kMaxWidth || impl_->height > kMaxHeight)
        return fail("unsupported movie dimensions");
    // Leave small originals unscaled so FFmpeg can use its NEON YUV converter.
    // The Android canvas scales the bitmap to the screen on the GPU.
    const double scale = std::min({1.0, 1280.0 / impl_->width, 720.0 / impl_->height});
    impl_->output_width = std::max(1, int(impl_->width * scale));
    impl_->output_height = std::max(1, int(impl_->height * scale));
    // Retained decoded YUV frames stay under a small budget even for 1080p.
    impl_->video_queue = std::clamp(kVideoBytes / (size_t(impl_->width) * impl_->height * 2), size_t(2), size_t(12));
    if (!impl_->open_codec(impl_->video_index, &impl_->video)) return fail(impl_->failure);
    impl_->audio_index = av_find_best_stream(impl_->format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (impl_->audio_index >= 0 && !impl_->open_codec(impl_->audio_index, &impl_->sound))
        return fail(impl_->failure);
    if (impl_->format->start_time != AV_NOPTS_VALUE) impl_->origin = double(impl_->format->start_time) / AV_TIME_BASE;
    else if (stream->start_time != AV_NOPTS_VALUE) impl_->origin = stream->start_time * av_q2d(stream->time_base);
    if (impl_->format->duration != AV_NOPTS_VALUE) impl_->duration = double(impl_->format->duration) / AV_TIME_BASE;
    if (!std::isfinite(impl_->duration) || impl_->duration < 0 || impl_->duration > 4 * 3600)
        return fail("invalid movie duration");
    AVRational fps = av_guess_frame_rate(impl_->format, const_cast<AVStream*>(stream), nullptr);
    if (fps.num > 0 && fps.den > 0) impl_->frame_step = std::clamp(av_q2d(av_inv_q(fps)), 1.0/120, 1.0);
    impl_->packet = av_packet_alloc(); impl_->scratch = av_frame_alloc();
    if (!impl_->packet || !impl_->scratch) return fail("cannot allocate movie buffers");
    impl_->pixels.assign(size_t(impl_->output_width) * impl_->output_height * 4, 0);
    for (size_t i = 3; i < impl_->pixels.size(); i += 4) impl_->pixels[i] = 255;
    if (audio && impl_->sound) {
        if (!audio->begin_movie_audio(kRate)) return fail("cannot initialize movie audio stream");
        impl_->audio = audio;
        impl_->audio_clock = audio->device_ready();
    }
    impl_->done = false;
    if (!impl_->pump(0) || !impl_->display(0)) return fail(impl_->failure);
    // MediaCodec is asynchronous; give its first output time to arrive before
    // deciding that the original stream needs the software fallback.
    for (int retry = 0; impl_->hardware && !impl_->frames && retry < 100; ++retry) {
        av_usleep(5000);
        if (!impl_->receive(impl_->video, true) || !impl_->pump(0) || !impl_->display(0)) return fail(impl_->failure);
    }
    if (!impl_->frames) return fail("movie contains no decodable video frame");
    if (error) error->clear();
    return true;
}
bool MoviePlayer::open(const std::string& data_dir, const std::string& name, AudioEngine* audio, std::string* error) {
    std::string relative = name;
    std::replace(relative.begin(), relative.end(), '\\', '/');
    if (relative.rfind("MOVIE/", 0) == 0 || relative.rfind("movie/", 0) == 0) relative.erase(0, 6);
    const auto p = std::filesystem::path(relative);
    if (relative.empty() || relative.find('\0') != std::string::npos || relative.find("..") != std::string::npos ||
        relative.find(':') != std::string::npos || p.is_absolute() || relative.size() > 260) {
        if (error) *error = "invalid movie resource name";
        return false;
    }
    if (!p.has_extension()) relative += ".mpg";
    std::error_code ec;
    for (const auto& directory : {data_dir + "/MOVIE/", data_dir + "/movie/", data_dir + "/AdvData/MOVIE/", data_dir + "/"}) {
        const auto path = resolve_asset(directory + relative);
        if (std::filesystem::is_regular_file(path, ec)) return open_file(path, audio, error);
    }
    if (error) *error = "movie resource not found: " + name;
    return false;
}
void MoviePlayer::update(uint32_t elapsed_ms) {
    if (!active() || impl_->paused) return;
    const double wall = std::min(elapsed_ms, uint32_t(250)) / 1000.0;
    double target = impl_->clock + wall;
    if (impl_->audio_clock) {
        const double played = impl_->audio->movie_audio_time();
        target = std::max(impl_->clock, impl_->audio_base + played);
        // A soundtrack can end before the last video packet, or start after
        // more video than the compressed queue can hold. Present that silence
        // while freeing video space, then trim late PCM against this timeline.
        if ((impl_->eof || impl_->demux_blocked) && impl_->audio_pending.empty() &&
            !impl_->audio->movie_audio_queued()) {
            target = std::max(target, impl_->clock + wall);
            impl_->audio_base = target - played;
            impl_->audio_pending.advance_to(target);
            impl_->audio_end = impl_->audio_pending.end_time();
        }
    }
    impl_->clock = std::min(target, impl_->clock + .5);
    impl_->collect_due(impl_->clock);
    if (!impl_->pump(impl_->clock) || !impl_->display(impl_->clock)) {
        if (!impl_->recover_software()) impl_->done = true;
    }
    if (impl_->eof && impl_->video_drained && (!impl_->sound || impl_->audio_drained) &&
        impl_->deferred_video.empty() && !impl_->packet_pending && impl_->pending.empty() &&
        impl_->audio_pending.empty() && impl_->clock + 1.0 / kRate >= std::max(impl_->video_end, impl_->audio_end) &&
        (!impl_->audio_clock || !impl_->audio->movie_audio_queued())) impl_->done = true;
    if (impl_->done && impl_->audio) impl_->audio->end_movie_audio();
}
void MoviePlayer::set_paused(bool paused) {
    if (impl_->paused && !paused) {
        const int64_t now = av_gettime_relative();
        if (impl_->video_flushed && !impl_->video_drained) impl_->video_drain_progress = now;
        if (impl_->audio_flushed && !impl_->audio_drained) impl_->audio_drain_progress = now;
    }
    impl_->paused = paused;
    if (impl_->audio) impl_->audio->pause_movie_audio(paused);
}
void MoviePlayer::draw_rgba(std::vector<uint8_t>* rgba, int width, int height) const {
    if (!rgba || width <= 0 || height <= 0 || width > 8192 || height > 8192) return;
    if (width == impl_->output_width && height == impl_->output_height && !impl_->pixels.empty()) {
        *rgba = impl_->pixels;
        return;
    }
    rgba->assign(size_t(width) * height * 4, 0);
    for (size_t i = 3; i < rgba->size(); i += 4) (*rgba)[i] = 255;
    if (impl_->pixels.empty()) return;
    const double scale = std::min(double(width) / impl_->output_width, double(height) / impl_->output_height);
    const int w = std::max(1, int(impl_->output_width * scale)), h = std::max(1, int(impl_->output_height * scale));
    const int x0 = (width-w)/2, y0 = (height-h)/2;
    if (w == impl_->output_width && h == impl_->output_height) {
        for (int y = 0; y < h; ++y)
            std::copy_n(impl_->pixels.data() + size_t(y)*w*4, size_t(w)*4,
                rgba->data() + (size_t(y+y0)*width+x0)*4);
        return;
    }
    impl_->viewport_scaler = sws_getCachedContext(impl_->viewport_scaler,
        impl_->output_width, impl_->output_height, AV_PIX_FMT_RGBA,
        w, h, AV_PIX_FMT_RGBA, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (!impl_->viewport_scaler) return;
    const uint8_t* source[] = {impl_->pixels.data(), nullptr, nullptr, nullptr};
    int source_stride[] = {impl_->output_width * 4, 0, 0, 0};
    uint8_t* destination[] = {rgba->data() + (size_t(y0)*width+x0)*4, nullptr, nullptr, nullptr};
    int destination_stride[] = {width * 4, 0, 0, 0};
    sws_scale(impl_->viewport_scaler, source, source_stride, 0, impl_->output_height, destination, destination_stride);
}
bool MoviePlayer::active() const { return impl_->format && !impl_->done; }
int MoviePlayer::frame_width() const { return impl_->output_width; }
int MoviePlayer::frame_height() const { return impl_->output_height; }
bool MoviePlayer::copy_frame_rgba(uint8_t* rgba, int width, int height, size_t stride) const {
    if (!rgba || impl_->pixels.empty() || width != frame_width() || height != frame_height() ||
        stride < size_t(width) * 4) return false;
    const size_t row = size_t(width) * 4;
    if (stride == row) std::memcpy(rgba, impl_->pixels.data(), row * height);
    else for (int y = 0; y < height; ++y)
        std::memcpy(rgba + size_t(y) * stride, impl_->pixels.data() + size_t(y) * row, row);
    return true;
}
bool MoviePlayer::finished() const { return impl_->done; }
const std::string& MoviePlayer::error() const { return impl_->failure; }
int MoviePlayer::width() const { return impl_->width; }
int MoviePlayer::height() const { return impl_->height; }
double MoviePlayer::time() const { return impl_->clock; }
double MoviePlayer::duration() const { return impl_->duration; }
uint64_t MoviePlayer::decoded_frames() const { return impl_->frames; }
uint64_t MoviePlayer::decoded_audio_frames() const { return impl_->audio_frames; }
float MoviePlayer::audio_peak() const { return impl_->peak; }
} // namespace dc4
