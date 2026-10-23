// Android shell bridge. All VM, graphics, choices and audio dispatch stay in core.
#include "arisa32/audio.h"
#include "arisa32/log_screen.h"
#include "arisa32/save_screen.h"
#include "arisa32/scene.h"
#include "arisa32/voice.h"
#include "arisa32/resource.h"
#include "arisa32/system_menu.h"
#include "arisa32/movie.h"
#include "arisa32/bgm.h"
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>

#include <android/bitmap.h>
#include <android/log.h>
#include <jni.h>

#include <algorithm>
#include <chrono>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "dc4", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "dc4", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "dc4", __VA_ARGS__)

namespace {
using Clock = std::chrono::steady_clock;
constexpr const char* kOpening = "dc4_kyo20190204a1.mes";
constexpr int kCharacterMs = 40;

struct Engine {
    dc4::AudioEngine audio;
    dc4::VoiceArchive voice;
    dc4scene::FontRenderer font;
    std::unique_ptr<dc4scene::SceneRunner> scene;
    std::string data_dir;
    JavaVM* vm = nullptr;
    jobject activity = nullptr;
    jmethodID decode_sjis = nullptr;
    Clock::time_point line_start = Clock::now();
    size_t text_length = 0;
    bool fully_revealed = false;
    bool pointer_down = false;
    bool pressed_widget = false;
    int pressed_choice = -1;
    std::string pressed_action;
    bool paused = false;
    bool pause_saved = false;
    std::string pause_file;
    // Increments on every line/choice entry; Java builds the backlog from it.
    size_t line_serial = 0;
    // Original-look Log screen state (dc4ui::LogScreen). Rows are recorded
    // natively as lines begin; dialogue_index counts text lines for jumps.
    dc4ui::LogScreen log;
    bool log_loaded = false;
    std::vector<dc4ui::LogRow> log_rows;
    int log_scroll = 0;
    bool log_thumbs = true;
    int log_mx = -1, log_my = -1;
    int log_drag_y = -1, log_drag_base = 0;
    bool log_drag_thumb = false, log_drag_moved = false;
    int dialogue_index = -1;
    dc4ui::SaveScreen save_screen;
    std::vector<dc4ui::SaveSlot> save_slots =
        std::vector<dc4ui::SaveSlot>(dc4ui::SaveScreen::kSlotCount);
    std::vector<uint8_t> save_frame;
    int save_mx = -1, save_my = -1;
    bool save_down = false;
    dc4ui::SystemMenu menu;
    dc4ui::ConfirmDialog confirm;
    std::function<std::string()> pending_confirm;
    std::vector<uint8_t> confirm_background;
    int confirm_x=-1,confirm_y=-1;
    bool confirm_down=false;
    dc4::MoviePlayer movie;
    bool menu_open = true, in_game = false, replay = false, movie_from_scene = false;
    bool menu_return = false, current_was_read = false;
    dc4ui::SystemMenuPage return_page = dc4ui::SystemMenuPage::Title;
    Clock::time_point tick = Clock::now();
    std::string save_dir, line_key;
    std::set<std::string> read_lines;
    int menu_x = -1, menu_y = -1, log_pressed = 0;
    bool menu_down = false;
    bool menu_music = false;
    bool movie_suspended = false;
    bool movie_playing_in_java = false;
    std::string current_bgm;
};

// Java serializes native access: input/render on its render thread, lifecycle
// only after joining that thread. The audio callback uses the core mixer lock.
Engine* g_engine = nullptr;
struct LaunchConfig { std::string saves, overlay, encoding = "windows-31j", font, language; } g_config;

std::string request_confirm(Engine* e,dc4ui::ConfirmKind kind,std::function<std::string()> accepted) {
    if(e->confirm.active())return {};
    e->confirm.set_language(dc4::get_language());
    if(!e->confirm.open(kind))return accepted();
    e->pending_confirm=std::move(accepted);e->confirm_background.clear();
    e->confirm_x=e->confirm_y=-1;e->confirm_down=false;return {};
}
std::string answer_confirm(Engine* e,bool yes) {
    e->confirm.answer(yes);auto accepted=std::move(e->pending_confirm);e->pending_confirm={};
    e->confirm_background.clear();e->confirm_down=false;e->tick=Clock::now();
    return yes&&accepted?accepted():std::string{};
}

void save_profile(Engine* e) {
    if (e->save_dir.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(e->save_dir, ec);
    e->menu.save_profile(e->save_dir + "/system-profile.dat");
    std::ofstream out(e->save_dir + "/read-lines.txt", std::ios::binary);
    for (const auto& key : e->read_lines) out << key << '\n';
}

void sync_progress(Engine* e) {
    e->menu.sync_progress(*e->scene);
}

void apply_config(Engine* e) {
    const auto c = e->menu.config();
    e->audio.set_master_volume(c.master_enabled ? c.master_volume : 0);
    e->audio.set_user_volume(dc4::AudioGroup::Bgm, c.music_enabled ? c.music_volume : 0);
    e->audio.set_user_volume(dc4::AudioGroup::Se, c.se_enabled ? c.se_volume : 0);
    e->audio.set_user_volume(dc4::AudioGroup::Voice, c.voice_enabled ? c.voice_volume : 0);
    e->audio.set_movie_volume(c.movie_enabled ? c.movie_volume : 0);
    e->audio.set_duck_music_for_voice(c.duck_music_for_voice,e->menu.variable(99,206));
    if (e->scene) {
        e->scene->set_window_opacity(c.window_opacity);
        e->scene->set_effects_enabled(c.effects != 0);
        e->scene->set_effect_speed(c.effect_speed);
        for (size_t i = 0; i < c.character_volume.size(); ++i) {
            int vol = c.character_volume[i] > 0 ? c.character_volume[i] : 100;
            e->scene->set_character_volume(i, vol, c.character_muted[i]);
        }
        e->scene->set_character_volume(15, 100, false);
    }
}

std::string jstr_to_std(JNIEnv* env, jstring s) {
    if (!s) return {};
    const char* p = env->GetStringUTFChars(s, nullptr);
    std::string out = p ? p : "";
    if (p) env->ReleaseStringUTFChars(s, p);
    return out;
}

std::u16string jstr_to_utf16(JNIEnv* env, jstring s, size_t limit) {
    if (!s) return {};
    const jchar* chars = env->GetStringChars(s, nullptr);
    if (!chars) return {};
    const size_t count = std::min(size_t(env->GetStringLength(s)), limit);
    std::u16string result(reinterpret_cast<const char16_t*>(chars), count);
    env->ReleaseStringChars(s, chars);
    return result;
}

std::u16string decode_sjis(const std::string& in) {
    Engine* e = g_engine;
    if (!e || !e->vm || !e->decode_sjis) return {};
    JNIEnv* env = nullptr;
    bool attached = false;
    if (e->vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (e->vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return {};
        attached = true;
    }
    std::u16string result;
    jbyteArray arr = env->NewByteArray(jsize(in.size()));
    if (arr) {
        env->SetByteArrayRegion(arr, 0, jsize(in.size()),
                               reinterpret_cast<const jbyte*>(in.data()));
        jstring out = nullptr;
        if (!env->ExceptionCheck())
            out = static_cast<jstring>(env->CallObjectMethod(e->activity, e->decode_sjis, arr));
        if (out && !env->ExceptionCheck()) {
            const jchar* chars = env->GetStringChars(out, nullptr);
            if (chars) {
                result.assign(reinterpret_cast<const char16_t*>(chars), size_t(env->GetStringLength(out)));
                env->ReleaseStringChars(out, chars);
            }
        }
        if (out) env->DeleteLocalRef(out);
        env->DeleteLocalRef(arr);
    }
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
    if (attached) e->vm->DetachCurrentThread();
    return result;
}

void destroy_engine(JNIEnv* env) {
    Engine* e = g_engine;
    if (!e) return;
    save_profile(e);
    e->movie.close();
    e->audio.shutdown();
    dc4scene::g_sjis_decoder = nullptr;
    g_engine = nullptr;
    if (e->activity) env->DeleteGlobalRef(e->activity);
    if (!e->pause_file.empty()) std::remove(e->pause_file.c_str());
    delete e;
}

void begin_line(Engine* e) {
    e->line_start = Clock::now();
    ++e->line_serial;
    // Count rendered characters, not SJIS bytes or inline control/ruby syntax.
    e->text_length = e->scene->visible_text_length();
    e->fully_revealed = false;
    e->line_key = e->scene->script_name() + ":" + std::to_string(e->scene->script_position());
    e->current_was_read = e->read_lines.count(e->line_key) != 0;
}

int reveal_count(Engine* e) {
    if (e->fully_revealed || e->scene->waiting_for_choice()) return -1;
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - e->line_start).count();
    static constexpr int timing[] = {120,100,80,65,50,40,30,24,18,12,6,0};
    const int speed = std::clamp(e->menu.config().text_speed,0,11);
    const size_t count = speed >= 11 ? e->text_length : size_t(elapsed / timing[speed]);
    if (count >= e->text_length) { e->fully_revealed = true; return -1; }
    return int(count);
}

// Appends one Log row for the current line; also the cap + scroll reset.
void record_log_row(Engine* e, int line) {
    e->log_rows.push_back(dc4ui::capture_log_row(*e->scene, line));
    if (e->log_rows.size() > 4000)
        e->log_rows.erase(e->log_rows.begin(), e->log_rows.begin() + (e->log_rows.size() - 4000));
    size_t total = 0, snapshots = 0;
    for (auto it = e->log_rows.rbegin(); it != e->log_rows.rend(); ++it) {
        total += it->snapshot.size();
        if (++snapshots > 512 || total > 16*1024*1024) { std::vector<uint8_t>().swap(it->snapshot); }
    }
    e->log_scroll = dc4ui::LogScreen::max_scroll(e->log_rows.size());
}

bool play_row_voice(Engine* e, const std::string& name) {
    if (name.empty()) return false;
    const dc4::VoiceEntry* entry = e->voice.find(name);
    std::vector<uint8_t> blob;
    return entry && e->voice.read_entry(*entry, &blob, nullptr) &&
           e->audio.play_voice_blob(blob.data(), blob.size());
}

bool play_system_voice(Engine* e, int event_id) {
    if (!e || !e->voice.loaded()) return false;
    const auto cfg = e->menu.config();
    if (!cfg.voice_enabled || !cfg.master_enabled) return false;
    std::string asset = e->menu.system_voice(event_id);
    e->audio.set_channel_user_volume(8, 100);
    return play_row_voice(e, asset);
}

// シーンジャンプ: rebuild the opening scene settled at the row's dialogue
// index (the same mechanism the desktop shell uses). Rows after the jump
// point are discarded; advancing re-accumulates them.
std::string jump_to_row(Engine* e, int row_index) {
    if (row_index < 0 || size_t(row_index) >= e->log_rows.size()) return "Invalid history row";
    const dc4ui::LogRow& row = e->log_rows[size_t(row_index)];
    if (row.snapshot.empty()) return "This older history row no longer has a rewind snapshot";
    std::string error;
    if (!e->scene->restore_state(row.snapshot, &error)) return "Scene jump failed: " + error;
    e->dialogue_index = row.line;
    e->log_rows.resize(size_t(row_index) + 1);
    e->log_scroll = dc4ui::LogScreen::max_scroll(e->log_rows.size());
    begin_line(e);
    e->fully_revealed = true;
    e->scene->set_message_visible(true);
    e->tick = Clock::now();
    return "";
}

bool advance_scene(Engine* e) {
    if (!e->scene->pending_movie().empty() || e->scene->waiting_for_time()) return true;
    if (!e->line_key.empty() && e->read_lines.size() < 100000) e->read_lines.insert(e->line_key);
    // Current core follows 0x4F/0x50 transfers itself, preserving scene/audio.
    // Keep the existing next_script fallback for an ended runner, checking
    // every load and preventing an unbounded no-dialogue transfer loop.
    for (int jumps = 0; jumps < 64; ++jumps) {
        if (e->scene->waiting_for_choice()) { begin_line(e); return true; }
        if (e->scene->scene_ended()) {
            return false; // Script transfers happen in core; system transfers return to the menu.
        }
        const bool line = e->scene->advance_to_next_text();
        sync_progress(e);
        if (!e->scene->pending_movie().empty() || e->scene->waiting_for_time()) return true;
        if (line || e->scene->waiting_for_choice()) begin_line(e);
        if (line) {
            ++e->dialogue_index;
            record_log_row(e, e->dialogue_index);
        }
        if (line || e->scene->waiting_for_choice()) return true;
        if (e->scene->scene_ended()) return false;
    }
    LOGE("scene transfer limit reached");
    return false;
}

bool over_widget(Engine* e, int x, int y) {
    if (e->scene->waiting_for_choice() || e->scene->credits_active() || !e->scene->pending_movie().empty())
        return false;
    return e->scene->hit_test(x, y) != nullptr;
}

std::string stopped_message(Engine* e) {
    if(e->scene->last_error().empty() && !e->scene->next_script().empty()) return {};
    return e->scene->last_error().empty() ? "End of scenario" : "Scene stopped: " + e->scene->last_error();
}

const char* kFontCandidates[] = {
    "/system/fonts/NotoSerifCJK-Regular.ttc",
    "/system/fonts/NotoSerifCJKjp-Regular.otf",
    "/system/fonts/NotoSansCJK-Regular.ttc",
    "/system/fonts/NotoSansCJKjp-Regular.otf",
    "/system/fonts/NotoSansSC-Regular.otf",
    "/system/fonts/NotoSansJP-Regular.otf",
    "/system/fonts/DroidSansJapanese.ttf",
};

// Copies a scene or menu frame into an ARGB_8888 Bitmap.
bool blit_rgba(JNIEnv* env, jobject bitmap, const std::vector<uint8_t>& frame) {
    if (frame.size() != size_t(dc4scene::kScreenW) * dc4scene::kScreenH * 4) return false;
    AndroidBitmapInfo info{};
    if (AndroidBitmap_getInfo(env, bitmap, &info) != ANDROID_BITMAP_RESULT_SUCCESS ||
        info.width != dc4scene::kScreenW || info.height != dc4scene::kScreenH ||
        info.format != ANDROID_BITMAP_FORMAT_RGBA_8888 || info.stride < info.width * 4) return false;
    void* pixels = nullptr;
    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) != ANDROID_BITMAP_RESULT_SUCCESS) return false;
    // ARGB_8888 uses RGBA memory on little-endian Android. Respect row stride.
    const size_t row_bytes = size_t(dc4scene::kScreenW) * 4;
    for (int row = 0; row < dc4scene::kScreenH; ++row)
        std::memcpy(static_cast<uint8_t*>(pixels) + size_t(row) * info.stride,
                    frame.data() + size_t(row) * row_bytes, row_bytes);
    AndroidBitmap_unlockPixels(env, bitmap);
    return true;
}

bool blit_frame(JNIEnv* env, jobject bitmap) {
    Engine* e = g_engine;
    return e && e->scene && blit_rgba(env, bitmap, e->scene->frame());
}

void refresh_menu_saves(Engine* e) {
    bool latest = false;
    bool quick = false;
    bool automatic = false;
    std::error_code ec;
    if (std::filesystem::is_directory(e->save_dir, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(e->save_dir, ec)) {
            if (!entry.is_regular_file()) continue;
            std::string name = entry.path().filename().string();
            std::string ext = entry.path().extension().string();
            std::string lower_ext = ext;
            for (char& c : lower_ext) c = char(std::tolower(static_cast<unsigned char>(c)));
            std::string lower_name = name;
            for (char& c : lower_name) c = char(std::tolower(static_cast<unsigned char>(c)));

            if (lower_ext == ".dc4sav") {
                if (lower_name == "quicksave.dc4sav") quick = true;
                else if (lower_name == "autosave.dc4sav") automatic = true;
                else latest = true;
            } else if (lower_ext == ".dat") {
                if (lower_name != "system.dat" && lower_name != "system-profile.dat" && lower_name.rfind("uinst", 0) != 0) {
                    if (lower_name.find("00.dat") != std::string::npos || lower_name.find("quick") != std::string::npos) {
                        quick = true;
                    } else {
                        latest = true;
                    }
                }
            }
        }
    }
    std::string parent_dir = std::filesystem::path(e->data_dir).parent_path().string();
    for (const auto& dir : {e->save_dir, e->data_dir, parent_dir}) {
        if (dir.empty()) continue;
        for (const auto& sub : {"/system.dat", "/SAVEDATA/system.dat", "/savedata/system.dat"}) {
            if (std::filesystem::exists(dir + sub)) {
                e->menu.load_system_dat(dir + sub);
            }
        }
    }
    e->menu.set_save_available(latest, quick, automatic);
}

std::string start_scene(Engine* e, std::string name, const std::string& label = {}, bool replay = false) {
    auto next = std::make_unique<dc4scene::SceneRunner>(e->data_dir);
    next->set_use_system_menu(e->menu.loaded());
    next->set_font(&e->font);
    next->set_animation_enabled(true);
    if (std::filesystem::path(name).extension().empty()) name += ".mes";
    if (!next->load_sheets(e->data_dir + "/MES/start.mes") ||
        !next->load_ui_script(e->data_dir + "/MES/" + e->menu.scene_ui_script()) ||
        !next->show_message_window("SYSTEM/MESWIN.CRX") ||
        !next->load_script(e->data_dir + "/MES/" + name, label)) return next->last_error();
    e->menu.initialize_scene(*next,replay);
    e->movie.close(); e->movie_suspended = false;
    e->audio.stop_bgm();
    e->audio.stop_group(dc4::AudioGroup::Se);
    e->audio.stop_channel(6);
    e->audio.stop_channel(7);
    e->current_bgm.clear();
    next->set_audio(&e->audio, &e->voice);
    next->set_observer([e](const dc4::MesCommand& c, const std::string& script) { e->menu.observe(c,script); });
    e->scene = std::move(next);
    apply_config(e);
    e->line_key.clear(); e->log_rows.clear(); e->dialogue_index = -1;
    e->menu_open = false; e->menu_return = false; e->in_game = true; e->replay = replay;
    e->menu.set_in_game(true); e->tick = Clock::now();
    if (!advance_scene(e) && !e->scene->last_error().empty()) return e->scene->last_error();
    return {};
}

std::string restore_save(Engine* e, const std::string& path) {
    std::vector<uint8_t> current;
    if (!e->scene->load_state(path)) return "Load failed: " + e->scene->last_error();
    sync_progress(e);
    e->movie.close();
    e->audio.stop_group(dc4::AudioGroup::Se);
    e->audio.stop_channel(6);
    e->audio.stop_channel(7);
    e->menu_music = false;
    e->current_bgm.clear();
    e->menu_open = e->menu_return = false; e->in_game = true; e->replay = false;
    e->menu.initialize_scene(*e->scene, false);
    e->scene->set_variable(9, 7, 0);
    for (uint16_t i = 100; i <= 119; ++i) {
        e->scene->set_variable(100, i, 1);
    }
    e->line_key.clear();
    if (e->scene->current_line().empty()) {
        advance_scene(e);
    }
    begin_line(e); e->fully_revealed = true;
    e->scene->set_message_visible(true);
    e->log_rows.clear(); e->dialogue_index = 0; record_log_row(e,0);
    e->tick = Clock::now();
    return {};
}

void menu_page_changed(Engine* e, dc4ui::SystemMenuPage before);

void open_menu(Engine* e, dc4ui::SystemMenuPage page) {
    if (!e->menu.loaded()) return;
    const auto before = e->menu.page();
    e->menu_open = true; e->menu_down = false;
    e->menu_x = e->menu_y = -1;
    e->menu.set_in_game(e->in_game);
    refresh_menu_saves(e); e->menu.open(page); e->tick = Clock::now();
    menu_page_changed(e, before);
}

bool play_menu_music(Engine* e, std::string asset, bool loop) {
    if (asset.empty()) return false;
    std::replace(asset.begin(),asset.end(),'\\','/');
    if (std::filesystem::path(asset).extension().empty()) asset += ".pcm";
    if (e->current_bgm == asset && e->audio.channel_playing(0)) return true;
    int first = 0, last = 0;
    if(e->menu.scene_ui_script() != "start.mes") dc4::bgm_loop_for(asset,&first,&last);
    e->audio.set_group_volume(dc4::AudioGroup::Bgm,100);
    const bool ok = e->audio.play_bgm_file(dc4::resolve_asset(e->data_dir + "/" + asset),
        size_t(std::max(0,first)),size_t(std::max(0,last)),loop);
    if (ok) e->current_bgm = asset;
    else e->current_bgm.clear();
    return ok;
}

void return_to_title(Engine* e) {
    using P = dc4ui::SystemMenuPage;
    e->movie.close();
    e->movie_suspended = false;
    e->scene->finish_movie();
    e->audio.stop_all();
    e->current_bgm.clear();
    save_profile(e);
    if (!e->menu.loaded()) {
        e->menu_open = false;
        e->in_game = true;
        static const char* kOpeningCandidates[] = {
            "dc4_kyo20190204a1.mes", "dc4_start.mes", "gotitle.mes",
            "title_00.mes", "title_01.mes", "title_main.mes", "title.mes", "main.mes", "start.mes"
        };
        std::string opening_script;
        for (const char* cand : kOpeningCandidates) {
            std::string p = e->data_dir + "/MES/" + cand;
            FILE* test = fopen(dc4::resolve_asset(p).c_str(), "rb");
            if (test) { fclose(test); opening_script = cand; break; }
        }
        if (opening_script.empty()) opening_script = "start.mes";
        e->scene->load_script(e->data_dir + "/MES/" + opening_script);
        if (opening_script == "main.mes") {
            if (e->scene->has_label("start_main")) e->scene->jump_to_label("start_main");
            else if (e->scene->has_label("main")) e->scene->jump_to_label("main");
        }
        advance_scene(e);
        return;
    }
    e->in_game = false;
    e->replay = false;
    e->menu.set_in_game(false);
    open_menu(e, P::Title);
    e->menu_music = false;
    play_menu_music(e, e->menu.title_music(), true);
}

void menu_page_changed(Engine* e, dc4ui::SystemMenuPage before) {
    using P = dc4ui::SystemMenuPage;
    if (e->menu.page() == before) return;
    if (e->menu.page() == P::Music) {
        e->audio.stop_bgm();
        e->menu_music = false;
        e->current_bgm.clear();
        play_system_voice(e, 16);
    } else if (e->menu.page() == P::Title || e->menu.page() == P::Extras) {
        if (!e->in_game) {
            e->menu_music = false;
            play_menu_music(e, e->menu.title_music(), true);
        }
        if (e->menu.page() == P::Extras) {
            play_system_voice(e, 15);
        } else if (before != P::Title) {
            play_system_voice(e, 2);
        }
    } else if (e->menu.page() == P::Config) {
        play_system_voice(e, 14);
    } else if (e->menu.page() == P::Gallery) {
        play_system_voice(e, 15);
    } else if (e->menu.page() == P::Scenes) {
        play_system_voice(e, 17);
    } else if (before == P::Music && e->menu.page() != P::Config) {
        e->menu_music = false;
        if (!e->in_game) {
            play_menu_music(e, e->menu.title_music(), true);
        }
    }
}

static std::string to_lower_str(std::string s) {
    for (char& c : s) {
        if (c == '\\') c = '/';
        else if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
    }
    return s;
}

static bool has_suffix_ci(const std::string& str, const std::string& suffix) {
    if (str.size() < suffix.size()) return false;
    return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin(),
                      [](char a, char b) {
                          return std::tolower(static_cast<unsigned char>(a)) ==
                                 std::tolower(static_cast<unsigned char>(b));
                      });
}

static bool is_opening_movie(const std::string& name) {
    std::string lower = to_lower_str(name);
    const auto pos = lower.find_last_of("/\\");
    if (pos != std::string::npos) lower = lower.substr(pos + 1);
    return lower.find("op") != std::string::npos ||
           lower.find("opening") != std::string::npos ||
           lower.find("mv001") != std::string::npos ||
           lower.find("mv01") != std::string::npos;
}

static bool is_op3_movie(const std::string& name) {
    const std::string lower = to_lower_str(name);
    return lower.find("op3") != std::string::npos ||
           lower.find("op_3") != std::string::npos ||
           lower.find("op03") != std::string::npos ||
           lower.find("opening3") != std::string::npos ||
           lower.find("opening_3") != std::string::npos;
}

static bool has_op_identifier(const std::string& name_lower, char op_num) {
    const bool is_op_name = name_lower.find("op") != std::string::npos ||
                            name_lower.find("opening") != std::string::npos;
    if (!is_op_name) return false;
    return op_num == 0 || name_lower.find(op_num) != std::string::npos;
}

static std::vector<std::string> get_candidate_movie_dirs(const std::string& data_dir, const std::string& overlay, const std::string& language) {
    namespace fs = std::filesystem;
    std::vector<std::string> dirs;
    std::error_code ec;

    auto add_if_exists = [&](const std::string& dir) {
        if (dir.empty()) return;
        if (fs::is_directory(dir, ec)) {
            for (const auto& existing : dirs) {
                if (existing == dir) return;
            }
            dirs.push_back(dir);
        }
    };

    // Find the base game root (e.g. /storage/.../D.C.4 Da Capo 4)
    std::string root = data_dir;
    fs::path p(data_dir);
    std::string leaf = to_lower_str(p.filename().string());
    if (leaf == "advdata" || leaf == "id_data" || leaf == "eng_data") {
        if (p.has_parent_path()) {
            root = p.parent_path().string();
        }
    }

    std::string lang = to_lower_str(language);
    if (lang.empty()) {
        std::string o = to_lower_str(overlay);
        if (o.find("id_data") != std::string::npos || o.find("id") != std::string::npos) lang = "id";
        else if (o.find("eng_data") != std::string::npos || o.find("en") != std::string::npos) lang = "en";
        else lang = "jp";
    }

    static const char* kMovieSubDirs[] = {
        "/MOVIE", "/Movie", "/movie", "/MOVIES", "/Movies", "/movies"
    };

    // 1. If "id" (ID PATCH): check sibling id_Data/Movie first
    if (lang == "id") {
        for (const char* id_name : {"/id_Data", "/id_data", "/ID_DATA", "/Id_Data", "/id", "/ID"}) {
            for (const char* sub : kMovieSubDirs) {
                add_if_exists(root + id_name + sub);
                add_if_exists(data_dir + id_name + sub);
            }
        }
        if (!overlay.empty()) {
            for (const char* sub : kMovieSubDirs) {
                add_if_exists(overlay + sub);
            }
        }
    }
    // 2. If "en" (ENG PATCH): check sibling eng_Data/Movie first
    else if (lang == "en") {
        for (const char* en_name : {"/eng_Data", "/eng_data", "/ENG_DATA", "/Eng_Data", "/en_Data", "/en_data", "/en", "/EN"}) {
            for (const char* sub : kMovieSubDirs) {
                add_if_exists(root + en_name + sub);
                add_if_exists(data_dir + en_name + sub);
            }
        }
        if (!overlay.empty()) {
            for (const char* sub : kMovieSubDirs) {
                add_if_exists(overlay + sub);
            }
        }
    }

    // 3. Fallback to AdvData/Movie (and primary for "jp" / JP (Original))
    for (const char* adv : {"/AdvData", "/Advdata", "/advdata", "/ADVDATA", ""}) {
        for (const char* sub : kMovieSubDirs) {
            add_if_exists(root + adv + sub);
            add_if_exists(data_dir + adv + sub);
        }
    }

    // Guaranteed fallback
    if (dirs.empty()) {
        dirs.push_back(data_dir + "/MOVIE");
    }

    return dirs;
}

std::string resolve_movie_file(const std::string& data_dir, const std::string& overlay, const std::string& language, const std::string& asset) {
    std::string rel = asset;
    std::replace(rel.begin(), rel.end(), '\\', '/');
    if (rel.rfind("MOVIE/", 0) == 0 || rel.rfind("movie/", 0) == 0 || rel.rfind("Movie/", 0) == 0) {
        rel = rel.substr(6);
    }

    const std::filesystem::path p(rel);
    const std::string stem = p.stem().string();
    const bool is_op = is_opening_movie(rel);

    // Prioritize mpg over mp4 for Da Capo 4 & Da Capo 4 Plus Harmony compatibility
    static const char* kExts[] = {".mpg", ".MPG", ".mpeg", ".MPEG", ".mp4", ".MP4", ".ogv", ".mkv", ".avi"};

    std::vector<std::string> names;
    for (const char* ext : kExts) {
        names.push_back(stem + ext);
    }
    if (p.has_extension()) {
        names.push_back(rel);
    }

    const std::vector<std::string> search_dirs = get_candidate_movie_dirs(data_dir, overlay, language);

    // 1. Direct stem match across directories in priority order
    // (id_Data/Movie first if ID PATCH, eng_Data/Movie first if ENG PATCH, fallback to AdvData/Movie)
    for (const auto& dir : search_dirs) {
        for (const auto& name : names) {
            std::string candidate = dir + "/" + name;
            std::error_code ec;
            if (std::filesystem::is_regular_file(candidate, ec) && std::filesystem::file_size(candidate, ec) > 0) {
                LOGI("Resolved movie (patch=%s): %s", language.c_str(), candidate.c_str());
                return candidate;
            }
        }
    }

    // 2. Fuzzy match for opening videos across directories in priority order
    if (is_op) {
        char op_num = 0;
        for (char c : to_lower_str(stem)) {
            if (c >= '1' && c <= '9') { op_num = c; break; }
        }
        std::error_code ec;
        for (const auto& dir : search_dirs) {
            if (!std::filesystem::is_directory(dir, ec)) continue;
            for (const char* ext : kExts) {
                for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
                    if (!entry.is_regular_file()) continue;
                    const std::string flower = to_lower_str(entry.path().filename().string());
                    if (has_op_identifier(flower, op_num) && has_suffix_ci(flower, ext)) {
                        LOGI("Fuzzy resolved opening movie (patch=%s): %s", language.c_str(), entry.path().string().c_str());
                        return entry.path().string();
                    }
                }
            }
        }
    }

    // 3. Fallback through dc4::resolve_asset (only for non-opening movies)
    if (!is_op) {
        for (const auto& name : names) {
            for (const char* sub : {"/MOVIE/", "/movie/", "/Movie/"}) {
                std::string candidate = dc4::resolve_asset(data_dir + sub + name);
                std::error_code ec;
                if (std::filesystem::is_regular_file(candidate, ec) && std::filesystem::file_size(candidate, ec) > 0) {
                    return candidate;
                }
            }
        }
    }

    return {};
}

static bool is_mp4_container_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    uint8_t b[16] = {0};
    if (!in.read(reinterpret_cast<char*>(b), sizeof(b))) return false;
    if (b[4] == 'f' && b[5] == 't' && b[6] == 'y' && b[7] == 'p') return true;
    if (b[4] == 'm' && b[5] == 'o' && b[6] == 'o' && b[7] == 'v') return true;
    if (b[4] == 'm' && b[5] == 'd' && b[6] == 'a' && b[7] == 't') return true;
    return false;
}

bool play_movie_resource(Engine* e, const std::string& asset, bool from_scene, std::string* error) {
    e->movie_from_scene = from_scene;
    std::string full_path = resolve_movie_file(e->data_dir, g_config.overlay, g_config.language, asset);
    if (full_path.empty()) {
        if (error) *error = "movie resource not found: " + asset;
        LOGE("movie resource not found: %s in %s", asset.c_str(), e->data_dir.c_str());
        return false;
    }

    // Determine format: check extension and container signature
    std::string ext = std::filesystem::path(full_path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    const bool ext_is_mpg = (ext == ".mpg" || ext == ".mpeg");
    const bool is_mp4 = is_mp4_container_file(full_path) || (!ext_is_mpg);
    const bool is_mpg = ext_is_mpg && (!is_mp4);

    const bool is_dc4 = dc4::is_dc4_or_dc4ph_game(e->data_dir);
    const bool is_kotori = is_dc4 && (dc4::is_kotori_song_movie(asset) || (e->scene && e->scene->is_effect_movie()));

    // For genuine MPEG-1 (.mpg/.mpeg), play natively via C++ pl_mpeg decoder
    if (is_mpg) {
        if (!is_kotori) {
            if (!e->audio.channel_playing(0) || from_scene) {
                e->audio.stop_group(dc4::AudioGroup::Bgm);
            }
        }
        dc4::AudioEngine* movie_audio = is_kotori ? nullptr : &e->audio;
        if (e->movie.open_file(full_path, movie_audio, error)) {
            LOGI("movie playing natively via pl_mpeg (kotori_song=%d): %s", is_kotori, full_path.c_str());
            return true;
        }
        LOGW("native movie open failed for %s (%s), falling back to Java MediaPlayer",
             full_path.c_str(), error ? error->c_str() : "unknown");
    }

    if (e->activity && e->vm) {
        JNIEnv* env = nullptr;
        bool attached = false;
        if (e->vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
            if (e->vm->AttachCurrentThread(&env, nullptr) == JNI_OK) attached = true;
        }
        if (env) {
            jclass cls = env->GetObjectClass(e->activity);
            jmethodID play_method = env->GetMethodID(cls, "playMovie", "(Ljava/lang/String;)V");
            if (play_method) {
                if (!is_kotori) {
                    if (!e->audio.channel_playing(0) || from_scene) {
                        e->audio.stop_group(dc4::AudioGroup::Bgm);
                    }
                    e->audio.set_device_paused(true);
                }
                jstring jpath = env->NewStringUTF(full_path.c_str());
                env->CallVoidMethod(e->activity, play_method, jpath);
                env->DeleteLocalRef(jpath);
                env->DeleteLocalRef(cls);
                if (attached) e->vm->DetachCurrentThread();
                e->movie_playing_in_java = true;
                LOGI("movie playing in Java MediaPlayer: %s", full_path.c_str());
                return true;
            }
            env->DeleteLocalRef(cls);
            if (attached) e->vm->DetachCurrentThread();
        }
    }

    dc4::AudioEngine* movie_audio = is_kotori ? nullptr : (e->audio.channel_playing(0) ? nullptr : &e->audio);
    return e->movie.open_file(full_path, movie_audio, error);
}

std::string menu_action(Engine* e, const dc4ui::SystemMenuAction& action) {
    using A = dc4ui::SystemMenuActionType;
    using P = dc4ui::SystemMenuPage;
    switch (action.type) {
    case A::None: return {};
    case A::Back:
        play_system_voice(e, 11);
        if (e->in_game) { e->menu_open = false; e->tick = Clock::now(); return "close"; }
        return "quit";
    case A::Quit: return "quit";
    case A::NewGame: case A::StartScene: {
        play_system_voice(e, 7);
        auto error = start_scene(e, action.asset.empty() ? kOpening : action.asset, action.label, action.type == A::StartScene);
        return error.empty() ? "start" : error;
    }
    case A::Save: case A::Load:
        if (action.type == A::Save && !e->in_game) return {};
        play_system_voice(e, action.type == A::Save ? 12 : 13);
        e->menu_return = e->menu_open; e->return_page = e->menu.page(); e->menu_open = false;
        return action.type == A::Save ? "save" : "load";
    case A::QuickLoad: case A::AutoLoad: case A::LatestLoad: {
        play_system_voice(e, 8);
        std::string path;
        std::error_code ec;
        if (action.type == A::LatestLoad) {
            std::filesystem::file_time_type newest{};
            if (std::filesystem::is_directory(e->save_dir, ec)) {
                for (const auto& entry : std::filesystem::directory_iterator(e->save_dir, ec)) {
                    if (!entry.is_regular_file()) continue;
                    std::string lower = entry.path().filename().string();
                    for (char& c : lower) c = char(std::tolower(static_cast<unsigned char>(c)));
                    if (lower == "system.dat" || lower == "system-profile.dat" || lower.rfind("uinst", 0) == 0) continue;
                    std::string ext = entry.path().extension().string();
                    for (char& c : ext) c = char(std::tolower(static_cast<unsigned char>(c)));
                    if (ext == ".dc4sav" || ext == ".dat") {
                        const auto time = entry.last_write_time(ec);
                        if (!ec && (path.empty() || time > newest)) { path = entry.path().string(); newest = time; }
                    }
                }
            }
            if (path.empty()) return "No saved game";
        } else {
            path = e->save_dir + (action.type == A::AutoLoad ? "/autosave.dc4sav" : "/quicksave.dc4sav");
            if (!std::filesystem::is_regular_file(path, ec)) {
                if (std::filesystem::is_directory(e->save_dir, ec)) {
                    for (const auto& entry : std::filesystem::directory_iterator(e->save_dir, ec)) {
                        if (!entry.is_regular_file()) continue;
                        std::string lower = entry.path().filename().string();
                        for (char& c : lower) c = char(std::tolower(static_cast<unsigned char>(c)));
                        if (lower.find("00.dat") != std::string::npos || lower.find("quicksave") != std::string::npos) {
                            path = entry.path().string(); break;
                        }
                    }
                }
            }
        }
        auto error = restore_save(e,path); return error.empty() ? "start" : error;
    }
    case A::SettingsChanged: apply_config(e); save_profile(e); return {};
    case A::PlayVoice:
        apply_config(e);
        save_profile(e);
        e->audio.set_channel_user_volume(8, 100);
        return play_row_voice(e,action.asset) ? "" : "Voice unavailable";
    case A::PlayMusic: {
        e->menu_music = play_menu_music(e,action.asset,e->menu.variable(100,26) == 0);
        return e->menu_music ? "" : "Music unavailable: " + action.asset;
    }
    case A::StopMusic: e->audio.stop_bgm(); e->menu_music = false; e->current_bgm.clear(); return {};
    case A::Title: {
        return_to_title(e);
        return {};
    }
    case A::SeekMusic: e->audio.seek_channel_fraction(0,double(action.index)/100); return {};
    case A::PlayMovie: {
        std::string error;
        if (!play_movie_resource(e, action.asset, false, &error)) return error;
        e->tick = Clock::now(); return {};
    }
    }
    return {};
}

void tick_engine(Engine* e) {
    const auto now = Clock::now();
    const auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now-e->tick).count();
    e->tick = now;
    if (e->paused) return;
    const uint32_t elapsed = uint32_t(std::clamp<int64_t>(delta,0,250));
    if (e->movie_playing_in_java) return;
    if (!e->menu_open && !e->movie.active() && !e->scene->pending_movie().empty()) {
        std::string error;
        std::string mov = e->scene->pending_movie();
        e->menu_down = false;
        if (!play_movie_resource(e, mov, true, &error)) {
            LOGE("movie: %s", error.c_str());
            e->scene->finish_movie();
            advance_scene(e);
        }
    }
    if (e->movie.active() && !e->movie_suspended) {
        e->movie.update(elapsed);
        if (e->movie.finished()) {
            const bool from_scene = e->movie_from_scene;
            e->movie.close();
            e->movie_from_scene = false;
            if (from_scene) {
                e->scene->finish_movie();
                advance_scene(e);
            } else if (e->menu_open && e->menu.page() != dc4ui::SystemMenuPage::Music) {
                play_menu_music(e, e->menu.title_music(), true);
            }
        }
        return;
    }
    if (e->menu_open && e->menu_music && !e->audio.channel_playing(0)) {
        e->menu_music = false;
        menu_action(e,e->menu.activate("next"));
    }
    if (e->menu_open && e->menu.page() == dc4ui::SystemMenuPage::Music)
        e->menu.set_music_position(int(100*e->audio.channel_fraction(0)));
    if (e->menu_open || !e->in_game) return;
    e->scene->update(elapsed);
    if (e->scene->script_ready()) advance_scene(e);
    if (e->scene->scene_ended() && e->scene->last_error().empty()) {
        auto target = e->scene->next_script();
        std::transform(target.begin(),target.end(),target.begin(),[](unsigned char c){return char(std::tolower(c));});
        if (target.size() > 4 && target.substr(target.size()-4) == ".mes") target.resize(target.size()-4);
        if (e->menu.loaded()) {
            auto page = e->replay ? dc4ui::SystemMenuPage::Scenes : dc4ui::SystemMenuPage::Title;
            if (target == "music") page = dc4ui::SystemMenuPage::Music;
            if (target == "cgmode" || target.rfind("cg_",0)==0) page = dc4ui::SystemMenuPage::Gallery;
            if (target == "scemode" || target == "sce_end" || target == "sc_end" || target.rfind("sce_",0)==0) page = dc4ui::SystemMenuPage::Scenes;
            if (target == "config" || target == "option") page = dc4ui::SystemMenuPage::Config;
            e->in_game = false; e->audio.stop_all(); e->menu_music = false;
            save_profile(e); open_menu(e,page);
            if (page == dc4ui::SystemMenuPage::Scenes) e->menu.resume_scenes();
            if (page != dc4ui::SystemMenuPage::Music) play_menu_music(e,e->menu.title_music(),true);
        } else {
            if (!target.empty()) {
                std::string p = e->data_dir + "/MES/" + target + ".mes";
                if (e->scene->load_script(p)) {
                    e->in_game = true;
                    begin_line(e);
                    return;
                }
            }
            std::string p = e->data_dir + "/MES/main.mes";
            if (!e->scene->load_script(p)) {
                p = e->data_dir + "/MES/start.mes";
                e->scene->load_script(p);
            }
            e->in_game = true;
            begin_line(e);
        }
    }
}
}  // namespace

extern "C" {
JNIEXPORT void JNICALL
Java_org_dc4engine_Dc4Activity_dc4Configure(JNIEnv* env, jobject, jstring saves, jstring overlay, jstring encoding, jstring font, jstring language) {
    if (g_engine) return;
    g_config = {jstr_to_std(env,saves),jstr_to_std(env,overlay),jstr_to_std(env,encoding),jstr_to_std(env,font),language ? jstr_to_std(env,language) : ""};
    if (g_config.language == "id") dc4::set_language(dc4::Language::Indonesian);
    else if (g_config.language == "en") dc4::set_language(dc4::Language::English);
    else if (g_config.language == "jp") dc4::set_language(dc4::Language::Japanese);
}

JNIEXPORT jstring JNICALL
Java_org_dc4engine_Dc4Activity_dc4ValidateSaveFile(JNIEnv* env,jobject,jstring base,jstring overlay,jstring encoding,jstring path) {
    if (g_engine) return env->NewStringUTF("Close the running game before importing saves");
    const auto data = jstr_to_std(env,base);
    dc4::configure_resources(data,jstr_to_std(env,overlay),jstr_to_std(env,encoding));
    dc4scene::SceneRunner candidate(data);
    if (!candidate.load_state(jstr_to_std(env,path)))
        return env->NewStringUTF(candidate.last_error().c_str());
    return env->NewStringUTF("");
}

JNIEXPORT jboolean JNICALL
Java_org_dc4engine_Dc4Activity_dc4Init(JNIEnv* env, jobject thiz, jstring data_dir) {
    if (g_engine) return JNI_TRUE;
    // Emulate Japanese system environment (Locale Emulator)
    std::setlocale(LC_ALL, "ja_JP.UTF-8");
    setenv("LANG", "ja_JP.UTF-8", 1);
    setenv("LC_ALL", "ja_JP.UTF-8", 1);
    Engine* e = new Engine();
    g_engine = e;  // Decoder is needed during initialization/first dialogue.
    e->data_dir = jstr_to_std(env, data_dir);
    e->save_dir = g_config.saves;
    if (g_config.encoding.empty()) g_config.encoding = "windows-31j";
    dc4::configure_resources(e->data_dir,g_config.overlay,g_config.encoding);
    if (g_config.language == "id") dc4::set_language(dc4::Language::Indonesian);
    else if (g_config.language == "en") dc4::set_language(dc4::Language::English);
    else if (g_config.language == "jp") dc4::set_language(dc4::Language::Japanese);
    env->GetJavaVM(&e->vm);
    e->activity = env->NewGlobalRef(thiz);
    jclass cls = env->GetObjectClass(thiz);
    e->decode_sjis = env->GetMethodID(cls, "decodeSjis", "([B)Ljava/lang/String;");
    env->DeleteLocalRef(cls);
    if (!e->activity || !e->decode_sjis) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        LOGE("cannot initialize Java text decoder");
        destroy_engine(env);
        return JNI_FALSE;
    }
    dc4scene::g_sjis_decoder = &decode_sjis;

    std::string err;
    if (e->voice.open(e->data_dir + "/VOICE.DAT", &err))
        LOGI("VOICE.DAT: %zu entries", e->voice.entries().size());
    else LOGE("VOICE.DAT: %s", err.c_str());
    if (!e->audio.init()) LOGE("audio init failed; continuing without device output");

    bool menu_ok = e->menu.load(e->data_dir, &err);
    bool font_ok = false;
    if (!g_config.font.empty()) {
        const int face_index = dc4::is_latin() && e->menu.scene_ui_script() == "start.mes" ? 2 : 1;
        font_ok = e->font.load(g_config.font, face_index, 22.0f);
        if (!font_ok) font_ok = e->font.load(g_config.font, 0, 22.0f);
        if (font_ok) LOGI("font: %s", g_config.font.c_str());
    }
    for (const char* path : kFontCandidates) {
        if (font_ok) break;
        if (e->font.load(path, 0, 22.0f)) {
            LOGI("font: %s", path);
            font_ok = true;
            break;
        }
    }
    if (!font_ok) LOGE("no CJK font found; text will be blank");
    e->log_loaded = e->log.load(e->data_dir);
    e->log.set_latin_mode(dc4::is_latin());
    if (!e->log_loaded) LOGE("log screen artwork unavailable");
    if (!menu_ok) {
        LOGI("system menus unavailable (%s); using in-game flow", err.c_str());
        e->menu_open = false;
        e->in_game = true;
    } else {
        std::string parent_dir = std::filesystem::path(e->data_dir).parent_path().string();
        for (const auto& dir : {e->save_dir, e->data_dir, parent_dir}) {
            if (dir.empty()) continue;
            for (const auto& sub : {"/system.dat", "/SAVEDATA/system.dat", "/savedata/system.dat"}) {
                if (std::filesystem::exists(dir + sub)) {
                    e->menu.load_system_dat(dir + sub);
                }
            }
        }
        if (std::filesystem::exists(e->data_dir + "/system-profile.dat")) {
            e->menu.load_profile(e->data_dir + "/system-profile.dat");
        }
        if (std::filesystem::exists(e->save_dir + "/system-profile.dat")) {
            e->menu.load_profile(e->save_dir + "/system-profile.dat");
        }
    }
    if (menu_ok) e->confirm.load(e->data_dir);
    if (!e->save_dir.empty() && !std::filesystem::exists(e->save_dir + "/system-profile.dat")) {
        save_profile(e);
    }
    std::ifstream read_file(e->save_dir + "/read-lines.txt",std::ios::binary);
    std::string read_key;
    while (e->read_lines.size() < 100000 && std::getline(read_file,read_key))
        if (read_key.size() <= 300) e->read_lines.insert(read_key);

    e->scene.reset(new dc4scene::SceneRunner(e->data_dir));
    e->scene->set_use_system_menu(menu_ok);
    e->scene->set_latin_mode(dc4::is_latin());
    e->scene->set_font(&e->font);
    e->scene->set_audio(&e->audio, &e->voice);
    e->scene->set_animation_enabled(true);
    e->scene->set_observer([e](const dc4::MesCommand& c,const std::string& script){e->menu.observe(c,script);});

    e->scene->load_sheets(e->data_dir + "/MES/start.mes");
    e->scene->load_ui_script(e->data_dir + "/MES/" + e->menu.scene_ui_script());
    e->scene->show_message_window("SYSTEM/MESWIN.CRX");

    static const char* kOpeningCandidates[] = {
        "dc4_kyo20190204a1.mes",
        "dc4_start.mes",
        "gotitle.mes",
        "title_00.mes",
        "title_01.mes",
        "title_main.mes",
        "title.mes",
        "main.mes",
        "start.mes"
    };
    std::string opening_script;
    for (const char* cand : kOpeningCandidates) {
        std::string p = e->data_dir + "/MES/" + cand;
        FILE* test = fopen(dc4::resolve_asset(p).c_str(), "rb");
        if (test) {
            fclose(test);
            opening_script = cand;
            break;
        }
    }
    if (opening_script.empty()) opening_script = "start.mes";

    if (!e->scene->load_script(e->data_dir + "/MES/" + opening_script)) {
        LOGE("scene initialization failed (%s): %s", e->data_dir.c_str(), e->scene->last_error().c_str());
        destroy_engine(env);
        return JNI_FALSE;
    }
    if (menu_ok) {
        apply_config(e);
        open_menu(e, dc4ui::SystemMenuPage::Title);
        play_menu_music(e, e->menu.title_music(), true);
        LOGI("title ready");
    } else {
        e->audio.set_master_volume(100);
        e->audio.set_user_volume(dc4::AudioGroup::Bgm, 80);
        e->audio.set_user_volume(dc4::AudioGroup::Se, 80);
        e->audio.set_user_volume(dc4::AudioGroup::Voice, 100);
        for (int i = 0; i < 51; ++i) e->scene->set_character_volume(i, 100, false);
        e->menu_open = false;
        e->in_game = true;
        if (opening_script == "main.mes") {
            if (e->scene->has_label("start_main")) {
                e->scene->jump_to_label("start_main");
            } else if (e->scene->has_label("main")) {
                e->scene->jump_to_label("main");
            }
        }
        advance_scene(e);
        LOGI("in-game flow ready (%s)", opening_script.c_str());
    }
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_org_dc4engine_Dc4Activity_dc4Quit(JNIEnv* env, jobject) {
    destroy_engine(env);
}

JNIEXPORT jstring JNICALL
Java_org_dc4engine_Dc4Activity_dc4OpenMenu(JNIEnv* env,jobject,jstring name) {
    Engine* e = g_engine;
    if (!e) return env->NewStringUTF("Engine is not running");
    const auto page = jstr_to_std(env,name);
    if (page == "title") {
        return_to_title(e);
        return env->NewStringUTF("");
    }
    if (!e->menu.loaded()) return env->NewStringUTF("");
    if (page == "resume") {
        if (e->menu_return) { open_menu(e,e->return_page); e->menu_return = false; }
        e->tick = Clock::now(); return env->NewStringUTF("");
    }
    if (page == "back") return env->NewStringUTF(menu_action(e,e->menu.back()).c_str());
    using P = dc4ui::SystemMenuPage;
    if (e->movie.active()) { e->movie_suspended = true; e->movie.set_paused(true); }
    open_menu(e,page == "gallery" ? P::Gallery : page == "music" ? P::Music :
        page == "scenes" ? P::Scenes : page == "extras" ? P::Extras : P::Config);
    return env->NewStringUTF("");
}

JNIEXPORT jboolean JNICALL
Java_org_dc4engine_Dc4Activity_dc4MenuActive(JNIEnv*,jobject) {
    return g_engine && (g_engine->menu_open || g_engine->movie.active() || g_engine->movie_playing_in_java);
}

JNIEXPORT jboolean JNICALL
Java_org_dc4engine_Dc4Activity_dc4MenuFrame(JNIEnv* env,jobject,jobject bitmap) {
    Engine* e = g_engine;
    if (!e || e->paused) return JNI_FALSE;
    tick_engine(e);
    std::vector<uint8_t> frame;
    if (e->movie.active() && !e->movie_suspended) e->movie.draw_rgba(&frame,1280,720);
    else if (e->menu_open) e->menu.draw(e->menu_x,e->menu_y,e->menu_down,e->font,&frame);
    else { e->scene->draw(reveal_count(e)); frame = e->scene->frame(); }
    return blit_rgba(env,bitmap,frame);
}

JNIEXPORT jstring JNICALL
Java_org_dc4engine_Dc4Activity_dc4MenuTouch(JNIEnv* env,jobject,jint action,jint x,jint y) {
    Engine* e = g_engine;
    if (!e || e->paused) return env->NewStringUTF("");
    e->menu_x = x; e->menu_y = y;
    if (e->movie.active() && !e->movie_suspended) {
        if (action == 0) e->menu_down = true;
        else if (action == 3) e->menu_down = false;
        else if (action == 1 && e->menu_down) {
            e->menu_down = false;
            // Prevent accidental instant skip from the touch-up of the previous dialog/button.
            // Require at least 1.0 second of playback before allowing touch-to-skip.
            if (e->movie.time() >= 1.0) {
                const bool from_scene = e->movie_from_scene;
                const double movie_time = e->movie.time();
                const double movie_duration = e->movie.duration();
                e->movie.close();
                e->movie_from_scene = false;
                if (e->audio.channel_playing(0)) {
                    const double remaining = movie_duration - movie_time;
                    if (remaining > 0) {
                        const double current_pos = e->audio.channel_position_seconds(0);
                        e->audio.seek_channel_seconds(0, current_pos + remaining);
                    }
                }
                if (from_scene) {
                    e->scene->finish_movie();
                    advance_scene(e);
                } else if (e->menu_open && e->menu.page() != dc4ui::SystemMenuPage::Music) {
                    play_menu_music(e, e->menu.title_music(), true);
                }
            }
        }
        return env->NewStringUTF("");
    }
    std::string result;
    const auto before = e->menu.page();
    if (action == 0) {e->menu_down = true; e->menu.pointer_down(x,y);}
    else if (action == 2 && e->menu_down) result = menu_action(e,e->menu.pointer_move(x,y));
    else if (action == 1) {e->menu_down = false; result = menu_action(e,e->menu.pointer_up(x,y));}
    else if (action == 3) {e->menu_down = false; e->menu.cancel_pointer();}
    if (e->menu_open) menu_page_changed(e,before);
    if (e->movie_suspended && (!e->menu_open || e->menu.page() != dc4ui::SystemMenuPage::Config)) {
        e->movie_suspended = false; e->movie.set_paused(false); e->tick = Clock::now();
    }
    return env->NewStringUTF(result.c_str());
}

JNIEXPORT jstring JNICALL
Java_org_dc4engine_Dc4Activity_dc4RequestConfirm(JNIEnv* env,jobject,jstring requested) {
    Engine* e=g_engine;if(!e||e->paused)return env->NewStringUTF("");
    const auto action=jstr_to_std(env,requested);
    dc4ui::ConfirmKind kind;
    if(action=="quick-save"){
        if(!e->in_game)return env->NewStringUTF("Start a game before saving");
        kind=dc4ui::ConfirmKind::QuickSave;
    }else if(action=="quick-load"){
        if(!std::filesystem::is_regular_file(e->save_dir+"/quicksave.dc4sav"))return env->NewStringUTF("No quick save");
        kind=dc4ui::ConfirmKind::QuickLoad;
    }else if(action=="title")kind=dc4ui::ConfirmKind::Title;
    else if(action=="quit")kind=dc4ui::ConfirmKind::Quit;
    else return env->NewStringUTF("");
    return env->NewStringUTF(request_confirm(e,kind,[action]{return action+"-confirmed";}).c_str());
}

JNIEXPORT jboolean JNICALL
Java_org_dc4engine_Dc4Activity_dc4ConfirmActive(JNIEnv*,jobject) {
    return g_engine&&g_engine->confirm.active();
}

JNIEXPORT jboolean JNICALL
Java_org_dc4engine_Dc4Activity_dc4CancelConfirmation(JNIEnv*,jobject) {
    Engine* e=g_engine;if(!e)return JNI_FALSE;
    if(e->confirm.active()){answer_confirm(e,false);return JNI_TRUE;}
    if(e->menu.confirming()){e->menu.answer_confirmation(false);return JNI_TRUE;}
    if(e->save_screen.confirming()){e->save_screen.cancel_confirm();return JNI_TRUE;}
    return JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_org_dc4engine_Dc4Activity_dc4ConfirmFrame(JNIEnv* env,jobject,jobject bitmap) {
    Engine* e=g_engine;if(!e||!e->confirm.active())return;
    if(e->confirm_background.empty()){
        AndroidBitmapInfo info{};void* pixels=nullptr;
        if(AndroidBitmap_getInfo(env,bitmap,&info)!=ANDROID_BITMAP_RESULT_SUCCESS||info.width!=1280||info.height!=720||
           info.format!=ANDROID_BITMAP_FORMAT_RGBA_8888||info.stride<1280*4||
           AndroidBitmap_lockPixels(env,bitmap,&pixels)!=ANDROID_BITMAP_RESULT_SUCCESS)return;
        e->confirm_background.resize(1280*720*4);
        for(int y=0;y<720;++y)std::memcpy(e->confirm_background.data()+size_t(y)*1280*4,
            static_cast<const uint8_t*>(pixels)+size_t(y)*info.stride,1280*4);
        AndroidBitmap_unlockPixels(env,bitmap);
    }
    auto frame=e->confirm_background;
    e->confirm.draw(e->confirm_x,e->confirm_y,e->confirm_down,e->font,&frame);blit_rgba(env,bitmap,frame);
}

JNIEXPORT jstring JNICALL
Java_org_dc4engine_Dc4Activity_dc4ConfirmTouch(JNIEnv* env,jobject,jint action,jint x,jint y) {
    Engine* e=g_engine;if(!e||e->paused||!e->confirm.active())return env->NewStringUTF("");
    e->confirm_x=x;e->confirm_y=y;
    if(action==0){e->confirm_down=true;e->confirm.pointer_down(x,y);}
    else if(action==3){e->confirm_down=false;e->confirm.cancel_pointer();}
    else if(action==1){
        e->confirm_down=false;auto result=e->confirm.pointer_up(x,y);
        if(result!=dc4ui::ConfirmResult::None)return env->NewStringUTF(answer_confirm(e,result==dc4ui::ConfirmResult::Yes).c_str());
    }
    return env->NewStringUTF("");
}

JNIEXPORT jstring JNICALL
Java_org_dc4engine_Dc4Activity_dc4PlaybackSettings(JNIEnv* env,jobject) {
    Engine* e = g_engine;
    if (!e) return env->NewStringUTF("");
    const auto c = e->menu.config();
    const int delay = (21-std::clamp(c.auto_wait,0,20))*100 + int(e->text_length)*(12-std::clamp(c.auto_speed,0,11))*8;
    const std::string result = std::to_string(delay) + ";" +
        (c.wait_for_voice ? "1" : "0") + ";" + (c.stop_auto_at_choice ? "1" : "0") + ";" +
        (c.stop_skip_at_choice ? "1" : "0") + ";" +
        ((c.skip_unread || e->current_was_read) ? "1" : "0");
    return env->NewStringUTF(result.c_str());
}

// Tap dialogue once to finish revealing it, then again to advance.
JNIEXPORT jboolean JNICALL
Java_org_dc4engine_Dc4Activity_dc4Advance(JNIEnv*, jobject) {
    Engine* e = g_engine;
    if (!e || !e->scene || e->paused || e->menu_open) return JNI_FALSE;
    if (e->movie.active()) {
        const double movie_time = e->movie.time();
        const double movie_duration = e->movie.duration();
        e->movie.close();
        if (e->audio.channel_playing(0)) {
            const double remaining = movie_duration - movie_time;
            if (remaining > 0) {
                const double current_pos = e->audio.channel_position_seconds(0);
                e->audio.seek_channel_seconds(0, current_pos + remaining);
            }
        }
        e->scene->finish_movie();
        return advance_scene(e);
    }
    if (e->scene->waiting_for_time()) {
        if (e->scene->effect_pck_active()) {
            return JNI_TRUE;
        }
        e->scene->finish_script_wait(); return advance_scene(e);
    }
    if (!e->scene->message_visible()) {
        e->scene->set_message_visible(true);
        return JNI_TRUE;
    }
    if (e->scene->waiting_for_choice()) return JNI_TRUE;
    if (reveal_count(e) >= 0) { e->fully_revealed = true; return JNI_TRUE; }
    if (e->menu.config().stop_voice_on_advance) e->audio.stop_group(dc4::AudioGroup::Voice);
    return advance_scene(e) ? JNI_TRUE : JNI_FALSE;
}

// Android MotionEvent action values: DOWN=0, UP=1, MOVE=2, CANCEL=3.
// Coordinates have already been inverse-mapped from the letterboxed viewport.
JNIEXPORT jstring JNICALL
Java_org_dc4engine_Dc4Activity_dc4Touch(JNIEnv* env, jobject thiz, jint action, jint x, jint y) {
    Engine* e = g_engine;
    if (!e || !e->scene || e->paused) return env->NewStringUTF("");
    e->scene->set_pointer(x, y, action == 0 || (action == 2 && e->pointer_down));
    if (action == 0) {
        e->pointer_down = true;
        e->pressed_choice = e->scene->choice_at(x, y);
        const auto* tag = e->scene->hit_test(x, y);
        e->pressed_action = tag ? *tag : "";
        e->pressed_widget = (tag != nullptr) || over_widget(e, x, y);
        return env->NewStringUTF("");
    }
    if (action == 2) {
        if (e->scene->waiting_for_choice()) {
            e->scene->set_pointer(x, y, e->pointer_down);
        }
        return env->NewStringUTF("");
    }
    const bool was_down = e->pointer_down;
    e->pointer_down = false;
    if (action != 1 || !was_down) {
        e->pressed_action.clear();
        e->pressed_widget = false;
        e->pressed_choice = -1;
        return env->NewStringUTF("");
    }
    std::string message;
    if (e->scene->waiting_for_choice()) {
        const int choice = e->scene->choice_at(x, y);
        if (choice >= 0 && (e->pressed_choice < 0 || choice == e->pressed_choice) && e->scene->choose(size_t(choice))) {
            e->pressed_choice = -1;
            if (!advance_scene(e)) message = stopped_message(e);
        }
    } else if (!e->pressed_action.empty() || e->pressed_widget || over_widget(e, x, y)) {
        const auto* tag = e->scene->hit_test(x, y);
        if (tag && *tag == e->pressed_action) {
            if (*tag == ":voice") {
                message = e->scene->replay_voice() ? ":voice" : "No voice on this line";
            } else if (*tag == ":n") {
                e->scene->toggle_taskbar();
            } else if (*tag == ":off" || *tag == ":close" || *tag == ":clear" || *tag == ":change") {
                e->scene->set_message_visible(!e->scene->message_visible());
            } else if (*tag == ":auto" || *tag == ":rapid" || *tag == ":skip" ||
                       *tag == ":backlog" || *tag == ":qs" || *tag == ":ql" ||
                       *tag == ":ds" || *tag == ":dl" || *tag == ":option" || *tag == ":aatitle" ||
                       *tag == ":aaload" || *tag == ":aasave" || *tag == ":aaqload" ||
                       *tag == ":aaqsave" || *tag == ":aaskip" || *tag == ":qsave" ||
                       *tag == ":full" || *tag == ":menu" || *tag == ":change" ||
                       *tag == ":up" || *tag == ":down" || *tag == ":aalog") {
                // Shell-level actions (auto/skip/backlog/save UI) dispatch in Java.
                e->pressed_action.clear();
                e->pressed_widget = false;
                e->pressed_choice = -1;
                return env->NewStringUTF(tag->c_str());
            } else if (!tag->empty() && (*tag)[0] != ':') {
                if (e->scene->has_label(*tag)) {
                    e->scene->jump_to_label(*tag);
                    advance_scene(e);
                } else if (*tag == "end" || *tag == "quit") {
                    e->pressed_action.clear();
                    e->pressed_widget = false;
                    e->pressed_choice = -1;
                    return env->NewStringUTF("quit");
                } else {
                    std::string p = e->data_dir + "/MES/" + *tag + ".mes";
                    if (e->scene->load_script(p)) {
                        advance_scene(e);
                    }
                }
            } else message = "Not available on Android: " + *tag;
        }
    } else if (x >= 0 && x < dc4scene::kScreenW && y >= 0 && y < dc4scene::kScreenH) {
        if (!Java_org_dc4engine_Dc4Activity_dc4Advance(env, thiz)) message = stopped_message(e);
    }
    e->pressed_action.clear();
    e->pressed_widget = false;
    e->pressed_choice = -1;
    return env->NewStringUTF(message.c_str());
}

// Pause the audio device while retaining mixer and decoder state in memory.
JNIEXPORT jstring JNICALL
Java_org_dc4engine_Dc4Activity_dc4SetPaused(JNIEnv* env, jobject, jboolean paused, jstring checkpoint) {
    Engine* e = g_engine;
    if (!e || !e->scene || e->paused == bool(paused)) return env->NewStringUTF("");
    std::string message;
    e->pointer_down = false;
    e->pressed_action.clear();
    e->pressed_widget = false;
    e->pressed_choice = -1;
    e->scene->set_pointer(-1, -1, false);
    e->menu.cancel_pointer(); e->menu_down = false;
    e->save_screen.cancel_pointer(); e->save_down = false;
    if(paused){answer_confirm(e,false);e->menu.answer_confirmation(false);e->save_screen.cancel_confirm();}
    e->fully_revealed = true;
    e->movie.set_paused(paused || e->movie_suspended);
    if (!e->audio.set_device_paused(paused)) message = "Audio device pause/resume failed";
    e->paused = paused; e->tick = Clock::now();
    if (paused) save_profile(e);
    if (!message.empty()) LOGE("%s", message.c_str());
    return env->NewStringUTF(message.c_str());
}

JNIEXPORT void JNICALL
Java_org_dc4engine_Dc4Activity_dc4Render(JNIEnv* env, jobject, jobject bitmap) {
    Engine* e = g_engine;
    if (!e || !e->scene || e->paused) return;
    tick_engine(e);
    if (e->movie.active() || e->menu_open) {
        std::vector<uint8_t> frame;
        if (e->movie.active()) e->movie.draw_rgba(&frame,1280,720);
        else e->menu.draw(e->menu_x,e->menu_y,e->menu_down,e->font,&frame);
        blit_rgba(env,bitmap,frame); return;
    }
    const int reveal = reveal_count(e);
    e->scene->set_wait_marker(reveal < 0 && !e->scene->waiting_for_choice() && !e->scene->scene_ended());
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
    e->scene->set_wait_frame(int((now / 120) % 4));
    e->scene->draw(reveal);
    blit_frame(env, bitmap);
}

JNIEXPORT jstring JNICALL
Java_org_dc4engine_Dc4Activity_dc4CurrentLine(JNIEnv* env, jobject) {
    Engine* e = g_engine;
    if (!e || !e->scene) return env->NewStringUTF("");
    // Never feed Shift-JIS bytes to NewStringUTF (which expects modified UTF-8).
    const auto text = e->scene->visible_text();
    return env->NewString(reinterpret_cast<const jchar*>(text.data()), jsize(text.size()));
}

// "line;waitingChoice;voicePlaying;animating;revealed;messageVisible;ended"
JNIEXPORT jstring JNICALL
Java_org_dc4engine_Dc4Activity_dc4Status(JNIEnv* env, jobject) {
    Engine* e = g_engine;
    if (!e || !e->scene) return env->NewStringUTF("0;0;0;0;1;0;1");
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%zu;%d;%d;%d;%d;%d;%d",
                  e->line_serial,
                  e->scene->waiting_for_choice() ? 1 : 0,
                  e->scene->voice_playing() ? 1 : 0,
                  (e->scene->animating() || e->scene->waiting_for_time() || e->movie.active() || e->movie_playing_in_java) ? 1 : 0,
                  (e->fully_revealed || reveal_count(e) < 0) ? 1 : 0,
                  e->scene->message_visible() ? 1 : 0,
                  e->scene->scene_ended() ? 1 : 0);
    return env->NewStringUTF(buf);
}

JNIEXPORT jstring JNICALL
Java_org_dc4engine_Dc4Activity_dc4Speaker(JNIEnv* env, jobject) {
    Engine* e = g_engine;
    if (!e || !e->scene) return env->NewStringUTF("");
    const auto name = dc4scene::sjis_to_utf16(e->scene->speaker());
    return env->NewString(reinterpret_cast<const jchar*>(name.data()), jsize(name.size()));
}

// One skip step: finish the typewriter reveal and advance, as the desktop
// shell's skip loop does (reveal-complete + advance at 100 ms cadence).
JNIEXPORT jboolean JNICALL
Java_org_dc4engine_Dc4Activity_dc4SkipAdvance(JNIEnv*, jobject) {
    Engine* e = g_engine;
    if (!e || !e->scene || e->paused) return JNI_FALSE;
    if (e->menu_open || e->movie.active() || e->scene->waiting_for_time()) return JNI_TRUE;
    if (!e->menu.config().skip_unread && !e->current_was_read) return JNI_FALSE;
    if (e->scene->scene_ended()) return JNI_FALSE;
    if (!e->scene->message_visible()) {
        e->scene->set_message_visible(true);
        return JNI_TRUE;
    }
    if (e->scene->waiting_for_choice()) return JNI_TRUE;
    e->fully_revealed = true;
    e->scene->finish_animations();
    return advance_scene(e) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_org_dc4engine_Dc4Activity_dc4Save(JNIEnv* env, jobject, jstring path) {
    Engine* e = g_engine;
    if (!e || !e->scene || e->paused || !e->in_game) return env->NewStringUTF("Start a game before saving");
    if (!e->scene->save_state(jstr_to_std(env, path)))
        return env->NewStringUTF(("Save failed: " + e->scene->last_error()).c_str());
    sync_progress(e); save_profile(e);
    return env->NewStringUTF("");
}

JNIEXPORT jstring JNICALL
Java_org_dc4engine_Dc4Activity_dc4Load(JNIEnv* env, jobject, jstring path) {
    Engine* e = g_engine;
    if (!e || !e->scene || e->paused) return env->NewStringUTF("Engine is not running");
    const auto error = restore_save(e,jstr_to_std(env,path));
    return env->NewStringUTF(error.c_str());
}

// Renders one clean frame (message window hidden) for the save thumbnail.
JNIEXPORT void JNICALL
Java_org_dc4engine_Dc4Activity_dc4RenderPreview(JNIEnv* env, jobject, jobject bitmap) {
    Engine* e = g_engine;
    if (!e || !e->scene || e->paused) return;
    const bool visible = e->scene->message_visible();
    e->scene->set_message_visible(false);
    e->scene->set_wait_marker(false);
    e->scene->draw(-1);
    e->scene->set_message_visible(visible);
    blit_frame(env, bitmap);
}

// ------------------------------------------------------ Save/Load screen

JNIEXPORT jstring JNICALL
Java_org_dc4engine_Dc4Activity_dc4SlotsOpen(JNIEnv* env, jobject, jboolean loading,
                                          jboolean latest, jboolean quick) {
    Engine* e = g_engine;
    if (!e || !e->scene || e->paused) return env->NewStringUTF("Engine is not running");
    std::string error;
    if (!e->save_screen.loaded() && !e->save_screen.load(e->data_dir, &error))
        LOGE("save screen artwork: %s", error.c_str());
    e->save_screen.set_language(dc4::get_language());
    e->save_screen.set_mode(loading ? dc4ui::SaveMode::Load : dc4ui::SaveMode::Save);
    e->save_screen.set_shortcuts(latest, false, quick);
    e->save_screen.set_navigation(true,true,true);
    if (e->menu_open) { e->menu_return = true; e->return_page = e->menu.page(); }
    e->menu_open = false;
    e->save_screen.cancel_pointer();
    e->save_mx = e->save_my = -1;
    e->save_down = false;
    play_system_voice(e, loading ? 13 : 12);
    // A missing sheet uses the core's functional fallback layout.
    return env->NewStringUTF(error.empty() ? "" : "Save/Load artwork unavailable; using fallback layout");
}

// Java owns emulator save paths and PNG decoding; only bounded metadata and
// thumbnail pixels cross into the shared renderer, never native game saves.
static bool copy_thumbnail_bitmap(JNIEnv* env, jobject thumbnail, dc4scene::Image* out_thumb) {
    if (!env || !thumbnail || !out_thumb) return false;
    AndroidBitmapInfo info{};
    if (AndroidBitmap_getInfo(env, thumbnail, &info) != ANDROID_BITMAP_RESULT_SUCCESS ||
        info.width == 0 || info.height == 0) return false;
    void* pixels = nullptr;
    if (AndroidBitmap_lockPixels(env, thumbnail, &pixels) != ANDROID_BITMAP_RESULT_SUCCESS) return false;

    out_thumb->w = 128;
    out_thumb->h = 72;
    out_thumb->rgba.resize(128 * 72 * 4, 255);
    uint8_t* dst = out_thumb->rgba.data();

    if (info.format == ANDROID_BITMAP_FORMAT_RGBA_8888) {
        for (int y = 0; y < 72; ++y) {
            const uint32_t sy = uint32_t(y * info.height / 72);
            const uint8_t* src_row = static_cast<const uint8_t*>(pixels) + size_t(sy) * info.stride;
            for (int x = 0; x < 128; ++x) {
                const uint32_t sx = uint32_t(x * info.width / 128);
                const uint8_t* p = src_row + sx * 4;
                uint8_t* d = dst + (y * 128 + x) * 4;
                d[0] = p[0]; d[1] = p[1]; d[2] = p[2]; d[3] = p[3];
            }
        }
    } else if (info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
        for (int y = 0; y < 72; ++y) {
            const uint32_t sy = uint32_t(y * info.height / 72);
            const uint16_t* src_row = reinterpret_cast<const uint16_t*>(
                static_cast<const uint8_t*>(pixels) + size_t(sy) * info.stride);
            for (int x = 0; x < 128; ++x) {
                const uint32_t sx = uint32_t(x * info.width / 128);
                const uint16_t p = src_row[sx];
                uint8_t* d = dst + (y * 128 + x) * 4;
                d[0] = uint8_t(((p >> 11) & 0x1F) * 255 / 31);
                d[1] = uint8_t(((p >> 5) & 0x3F) * 255 / 63);
                d[2] = uint8_t((p & 0x1F) * 255 / 31);
                d[3] = 255;
            }
        }
    }

    AndroidBitmap_unlockPixels(env, thumbnail);
    return true;
}

JNIEXPORT void JNICALL
Java_org_dc4engine_Dc4Activity_dc4SlotInfo(JNIEnv* env, jobject, jint index,
                                          jboolean occupied, jstring timestamp,
                                          jstring excerpt, jobject thumbnail) {
    Engine* e = g_engine;
    if (!e || index < 0 || index >= dc4ui::SaveScreen::kSlotCount) return;
    dc4ui::SaveSlot slot;
    slot.occupied = occupied;
    slot.timestamp = jstr_to_utf16(env, timestamp, 64);
    slot.excerpt = jstr_to_utf16(env, excerpt, 256);
    if (occupied && thumbnail) {
        copy_thumbnail_bitmap(env, thumbnail, &slot.thumbnail);
    }
    e->save_screen.set_slot_occupied(index, occupied);
    e->save_slots[size_t(index)] = std::move(slot);
}

JNIEXPORT jboolean JNICALL
Java_org_dc4engine_Dc4Activity_dc4SlotsFrame(JNIEnv* env, jobject, jobject bitmap) {
    Engine* e = g_engine;
    if (!e || !e->scene || e->paused) return JNI_FALSE;
    e->tick = Clock::now();
    // LOADBACK/SAVEBACK are translucent. Start from the scene every frame;
    // reusing the already composed menu would darken it on each redraw.
    const bool message_visible = e->scene->message_visible();
    e->scene->set_message_visible(false);
    e->scene->set_wait_marker(false);
    e->scene->draw(-1);
    e->scene->set_message_visible(message_visible);
    e->save_frame = e->scene->frame();
    e->save_screen.draw(e->save_slots, e->save_mx, e->save_my, e->save_down,
                        e->font, &e->save_frame);
    return blit_rgba(env, bitmap, e->save_frame) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_org_dc4engine_Dc4Activity_dc4SlotsTouch(JNIEnv* env, jobject, jint action, jint x, jint y) {
    Engine* e = g_engine;
    if (!e || e->paused) return env->NewStringUTF("");
    e->save_mx = x; e->save_my = y;
    if (action == 0) {
        e->save_down = true;
        e->save_screen.pointer_down(x, y);
    } else if (action == 3) {
        e->save_down = false;
        e->save_mx = e->save_my = -1;
        e->save_screen.cancel_pointer();
    } else if (action == 1) {
        e->save_down = false;
        const auto selected = e->save_screen.pointer_up(x, y);
        using Action = dc4ui::SaveActionType;
        switch (selected.type) {
        case Action::Slot:
            return env->NewStringUTF(("slot:" + std::to_string(selected.index)).c_str());
        case Action::Mode:
            play_system_voice(e, selected.index == 1 ? 13 : 12);
            return env->NewStringUTF(selected.index == 1 ? "load" : "save");
        case Action::Back:
            play_system_voice(e, 11);
            return env->NewStringUTF("close");
        case Action::LatestLoad: return env->NewStringUTF("latest");
        case Action::QuickLoad: return env->NewStringUTF("quick");
        case Action::Config: return env->NewStringUTF("config");
        case Action::Title: return env->NewStringUTF("title");
        case Action::Quit: return env->NewStringUTF("quit");
        default: break;  // Page changes are applied by the shared controller.
        }
    }
    return env->NewStringUTF("");
}

// ------------------------------------------------------------ Log screen

// Opens the Log screen: scroll snaps to the newest row (native behavior).
JNIEXPORT void JNICALL
Java_org_dc4engine_Dc4Activity_dc4LogOpen(JNIEnv*, jobject) {
    Engine* e = g_engine;
    if (!e) return;
    e->log_scroll = dc4ui::LogScreen::max_scroll(e->log_rows.size());
    e->log_mx = e->log_my = -1;
    e->log_drag_y = -1;
    e->log_drag_moved = false;
    e->log_pressed = 0;
    e->tick = Clock::now();
}

// Drops all rows (used after loading a save).
JNIEXPORT void JNICALL
Java_org_dc4engine_Dc4Activity_dc4LogClear(JNIEnv*, jobject) {
    Engine* e = g_engine;
    if (!e) return;
    e->log_rows.clear();
    e->log_scroll = 0;
    if (e->in_game && !e->scene->current_line().empty()) record_log_row(e,e->dialogue_index);
}

// Renders the current scene frame with the Log screen composed over it.
JNIEXPORT jboolean JNICALL
Java_org_dc4engine_Dc4Activity_dc4LogFrame(JNIEnv* env, jobject, jobject bitmap) {
    Engine* e = g_engine;
    if (!e || !e->scene || !e->log_loaded) return JNI_FALSE;
    e->tick = Clock::now();
    const bool visible = e->scene->message_visible();
    e->scene->set_message_visible(false);
    e->scene->set_wait_marker(false);
    e->scene->draw(-1);
    e->scene->set_message_visible(visible);
    AndroidBitmapInfo info{};
    if (AndroidBitmap_getInfo(env, bitmap, &info) != ANDROID_BITMAP_RESULT_SUCCESS ||
        info.width != dc4scene::kScreenW || info.height != dc4scene::kScreenH ||
        info.format != ANDROID_BITMAP_FORMAT_RGBA_8888 || info.stride < info.width * 4) return JNI_FALSE;
    void* pixels = nullptr;
    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) != ANDROID_BITMAP_RESULT_SUCCESS) return JNI_FALSE;
    const size_t row_bytes = size_t(dc4scene::kScreenW) * 4;
    std::vector<uint8_t> overlay(e->scene->frame().begin(), e->scene->frame().end());
    e->log.set_latin_mode(dc4::is_latin());
    e->log.draw(e->log_rows, e->log_scroll, e->log_thumbs, e->log_mx, e->log_my, e->font, &overlay);
    for (int row = 0; row < dc4scene::kScreenH; ++row)
        std::memcpy(static_cast<uint8_t*>(pixels) + size_t(row) * info.stride,
                    overlay.data() + size_t(row) * row_bytes, row_bytes);
    AndroidBitmap_unlockPixels(env, bitmap);
    return JNI_TRUE;
}

// Log screen input. Returns "" (nothing to do), "close", "check", "jump",
// "voice", or an error message to notify. Drag scrolling is handled here.
// Android MotionEvent action values: DOWN=0, UP=1, MOVE=2, CANCEL=3.
JNIEXPORT jstring JNICALL
Java_org_dc4engine_Dc4Activity_dc4LogTouch(JNIEnv* env, jobject, jint action, jint x, jint y) {
    Engine* e = g_engine;
    if (!e || !e->log_loaded) return env->NewStringUTF("");
    e->log_mx = x;
    e->log_my = y;
    const size_t count = e->log_rows.size();
    const auto control_at = [&](int px,int py) {
        if (e->log.back_hit(px,py)) return 1;
        if (e->log.checkbox_hit(px,py)) return 2;
        const int jump = e->log.jump_hit(px,py,e->log_scroll,count);
        if (jump >= 0) return 1000+jump;
        const int voice = e->log.voice_hit(px,py,e->log_scroll,count);
        return voice >= 0 ? 10000+voice : 0;
    };
    if (action == 0) {
        e->log_pressed = control_at(x,y);
        e->log_drag_y = y;
        e->log_drag_base = e->log_scroll;
        e->log_drag_moved = false;
        e->log_drag_thumb = e->log.thumb_hit(x, y, e->log_scroll, count);
        return env->NewStringUTF("");
    }
    if (action == 2) {
        if (e->log_drag_y < 0) return env->NewStringUTF("");
        if (std::abs(y - e->log_drag_y) > 12) e->log_drag_moved = true;
        if (e->log_drag_thumb) {
            e->log_scroll = e->log.thumb_scroll(y, count);
        } else {
            // Swipe: drag down shows newer rows (top index decreases).
            const int delta = (y - e->log_drag_y) / dc4ui::LogScreen::kRowPitch;
            e->log_scroll = std::clamp(e->log_drag_base - delta, 0, dc4ui::LogScreen::max_scroll(count));
        }
        return env->NewStringUTF("");
    }
    const bool had_press = e->log_drag_y >= 0;
    e->log_drag_y = -1;
    e->log_mx = e->log_my = -1;
    if (action != 1 || !e->log_drag_moved) {
        e->log_drag_thumb = false;
        if (action != 1) return env->NewStringUTF("");
    } else {
        e->log_drag_thumb = false;
        return env->NewStringUTF("");
    }
    const int pressed = e->log_pressed;
    e->log_pressed = 0;
    if (!had_press || pressed == 0 || control_at(x,y) != pressed) return env->NewStringUTF("");
    if (e->log.back_hit(x, y)) return env->NewStringUTF("close");
    if (e->log.checkbox_hit(x, y)) {
        e->log_thumbs = !e->log_thumbs;
        return env->NewStringUTF("check");
    }
    const int jump = e->log.jump_hit(x, y, e->log_scroll, count);
    if (jump >= 0) {
        return env->NewStringUTF(request_confirm(e,dc4ui::ConfirmKind::SceneJump,[e,jump]{
            const auto error=jump_to_row(e,jump);return error.empty()?std::string("jump"):error;
        }).c_str());
    }
    const int voice = e->log.voice_hit(x, y, e->log_scroll, count);
    if (voice >= 0) {
        if (!play_row_voice(e, e->log_rows[size_t(voice)].voice))
            return env->NewStringUTF("No voice on this row");
        return env->NewStringUTF("voice");
    }
    return env->NewStringUTF("");
}

// Forwarding exports for Arisa32Activity
JNIEXPORT void JNICALL
Java_org_dc4engine_Arisa32Activity_dc4Configure(JNIEnv* env, jobject thiz, jstring saves, jstring overlay, jstring encoding, jstring font, jstring language) {
    Java_org_dc4engine_Dc4Activity_dc4Configure(env, thiz, saves, overlay, encoding, font, language);
}
JNIEXPORT jstring JNICALL
Java_org_dc4engine_Arisa32Activity_dc4ValidateSaveFile(JNIEnv* env, jobject thiz, jstring base, jstring overlay, jstring encoding, jstring path) {
    return Java_org_dc4engine_Dc4Activity_dc4ValidateSaveFile(env, thiz, base, overlay, encoding, path);
}
JNIEXPORT jboolean JNICALL
Java_org_dc4engine_Arisa32Activity_dc4Init(JNIEnv* env, jobject thiz, jstring data_dir) {
    return Java_org_dc4engine_Dc4Activity_dc4Init(env, thiz, data_dir);
}
JNIEXPORT void JNICALL
Java_org_dc4engine_Arisa32Activity_dc4Quit(JNIEnv* env, jobject thiz) {
    Java_org_dc4engine_Dc4Activity_dc4Quit(env, thiz);
}
JNIEXPORT jstring JNICALL
Java_org_dc4engine_Arisa32Activity_dc4OpenMenu(JNIEnv* env, jobject thiz, jstring name) {
    return Java_org_dc4engine_Dc4Activity_dc4OpenMenu(env, thiz, name);
}
JNIEXPORT jboolean JNICALL
Java_org_dc4engine_Arisa32Activity_dc4MenuActive(JNIEnv* env, jobject thiz) {
    return Java_org_dc4engine_Dc4Activity_dc4MenuActive(env, thiz);
}
JNIEXPORT jboolean JNICALL
Java_org_dc4engine_Arisa32Activity_dc4MenuFrame(JNIEnv* env, jobject thiz, jobject bitmap) {
    return Java_org_dc4engine_Dc4Activity_dc4MenuFrame(env, thiz, bitmap);
}
JNIEXPORT jstring JNICALL
Java_org_dc4engine_Arisa32Activity_dc4MenuTouch(JNIEnv* env, jobject thiz, jint action, jint x, jint y) {
    return Java_org_dc4engine_Dc4Activity_dc4MenuTouch(env, thiz, action, x, y);
}
JNIEXPORT jstring JNICALL
Java_org_dc4engine_Arisa32Activity_dc4RequestConfirm(JNIEnv* env, jobject thiz, jstring requested) {
    return Java_org_dc4engine_Dc4Activity_dc4RequestConfirm(env, thiz, requested);
}
JNIEXPORT jboolean JNICALL
Java_org_dc4engine_Arisa32Activity_dc4ConfirmActive(JNIEnv* env, jobject thiz) {
    return Java_org_dc4engine_Dc4Activity_dc4ConfirmActive(env, thiz);
}
JNIEXPORT jboolean JNICALL
Java_org_dc4engine_Arisa32Activity_dc4CancelConfirmation(JNIEnv* env, jobject thiz) {
    return Java_org_dc4engine_Dc4Activity_dc4CancelConfirmation(env, thiz);
}
JNIEXPORT void JNICALL
Java_org_dc4engine_Arisa32Activity_dc4ConfirmFrame(JNIEnv* env, jobject thiz, jobject bitmap) {
    Java_org_dc4engine_Dc4Activity_dc4ConfirmFrame(env, thiz, bitmap);
}
JNIEXPORT jstring JNICALL
Java_org_dc4engine_Arisa32Activity_dc4ConfirmTouch(JNIEnv* env, jobject thiz, jint action, jint x, jint y) {
    return Java_org_dc4engine_Dc4Activity_dc4ConfirmTouch(env, thiz, action, x, y);
}
JNIEXPORT jstring JNICALL
Java_org_dc4engine_Arisa32Activity_dc4PlaybackSettings(JNIEnv* env, jobject thiz) {
    return Java_org_dc4engine_Dc4Activity_dc4PlaybackSettings(env, thiz);
}
JNIEXPORT jboolean JNICALL
Java_org_dc4engine_Arisa32Activity_dc4Advance(JNIEnv* env, jobject thiz) {
    return Java_org_dc4engine_Dc4Activity_dc4Advance(env, thiz);
}
JNIEXPORT jstring JNICALL
Java_org_dc4engine_Arisa32Activity_dc4Touch(JNIEnv* env, jobject thiz, jint action, jint x, jint y) {
    return Java_org_dc4engine_Dc4Activity_dc4Touch(env, thiz, action, x, y);
}
JNIEXPORT jstring JNICALL
Java_org_dc4engine_Arisa32Activity_dc4SetPaused(JNIEnv* env, jobject thiz, jboolean paused, jstring checkpoint) {
    return Java_org_dc4engine_Dc4Activity_dc4SetPaused(env, thiz, paused, checkpoint);
}
JNIEXPORT void JNICALL
Java_org_dc4engine_Arisa32Activity_dc4Render(JNIEnv* env, jobject thiz, jobject bitmap) {
    Java_org_dc4engine_Dc4Activity_dc4Render(env, thiz, bitmap);
}
JNIEXPORT jstring JNICALL
Java_org_dc4engine_Arisa32Activity_dc4CurrentLine(JNIEnv* env, jobject thiz) {
    return Java_org_dc4engine_Dc4Activity_dc4CurrentLine(env, thiz);
}
JNIEXPORT jstring JNICALL
Java_org_dc4engine_Arisa32Activity_dc4Status(JNIEnv* env, jobject thiz) {
    return Java_org_dc4engine_Dc4Activity_dc4Status(env, thiz);
}
JNIEXPORT jstring JNICALL
Java_org_dc4engine_Arisa32Activity_dc4Speaker(JNIEnv* env, jobject thiz) {
    return Java_org_dc4engine_Dc4Activity_dc4Speaker(env, thiz);
}
JNIEXPORT jboolean JNICALL
Java_org_dc4engine_Arisa32Activity_dc4SkipAdvance(JNIEnv* env, jobject thiz) {
    return Java_org_dc4engine_Dc4Activity_dc4SkipAdvance(env, thiz);
}
JNIEXPORT jstring JNICALL
Java_org_dc4engine_Arisa32Activity_dc4Save(JNIEnv* env, jobject thiz, jstring path) {
    return Java_org_dc4engine_Dc4Activity_dc4Save(env, thiz, path);
}
JNIEXPORT jstring JNICALL
Java_org_dc4engine_Arisa32Activity_dc4Load(JNIEnv* env, jobject thiz, jstring path) {
    return Java_org_dc4engine_Dc4Activity_dc4Load(env, thiz, path);
}
JNIEXPORT void JNICALL
Java_org_dc4engine_Arisa32Activity_dc4RenderPreview(JNIEnv* env, jobject thiz, jobject bitmap) {
    Java_org_dc4engine_Dc4Activity_dc4RenderPreview(env, thiz, bitmap);
}
JNIEXPORT jstring JNICALL
Java_org_dc4engine_Arisa32Activity_dc4SlotsOpen(JNIEnv* env, jobject thiz, jboolean loading, jboolean latest, jboolean quick) {
    return Java_org_dc4engine_Dc4Activity_dc4SlotsOpen(env, thiz, loading, latest, quick);
}
JNIEXPORT void JNICALL
Java_org_dc4engine_Arisa32Activity_dc4SlotInfo(JNIEnv* env, jobject thiz, jint index, jboolean occupied, jstring timestamp, jstring excerpt, jobject thumbnail) {
    Java_org_dc4engine_Dc4Activity_dc4SlotInfo(env, thiz, index, occupied, timestamp, excerpt, thumbnail);
}
JNIEXPORT jboolean JNICALL
Java_org_dc4engine_Arisa32Activity_dc4SlotsFrame(JNIEnv* env, jobject thiz, jobject bitmap) {
    return Java_org_dc4engine_Dc4Activity_dc4SlotsFrame(env, thiz, bitmap);
}
JNIEXPORT jstring JNICALL
Java_org_dc4engine_Arisa32Activity_dc4SlotsTouch(JNIEnv* env, jobject thiz, jint action, jint x, jint y) {
    return Java_org_dc4engine_Dc4Activity_dc4SlotsTouch(env, thiz, action, x, y);
}
JNIEXPORT void JNICALL
Java_org_dc4engine_Arisa32Activity_dc4LogOpen(JNIEnv* env, jobject thiz) {
    Java_org_dc4engine_Dc4Activity_dc4LogOpen(env, thiz);
}
JNIEXPORT void JNICALL
Java_org_dc4engine_Arisa32Activity_dc4LogClear(JNIEnv* env, jobject thiz) {
    Java_org_dc4engine_Dc4Activity_dc4LogClear(env, thiz);
}
JNIEXPORT jboolean JNICALL
Java_org_dc4engine_Arisa32Activity_dc4LogFrame(JNIEnv* env, jobject thiz, jobject bitmap) {
    return Java_org_dc4engine_Dc4Activity_dc4LogFrame(env, thiz, bitmap);
}
JNIEXPORT jstring JNICALL
Java_org_dc4engine_Arisa32Activity_dc4LogTouch(JNIEnv* env, jobject thiz, jint action, jint x, jint y) {
    return Java_org_dc4engine_Dc4Activity_dc4LogTouch(env, thiz, action, x, y);
}
JNIEXPORT void JNICALL
Java_org_dc4engine_Dc4Activity_dc4MovieCompleted(JNIEnv*, jobject) {
    Engine* e = g_engine;
    if (!e) return;
    e->movie_playing_in_java = false;
    e->audio.set_device_paused(false);
    if (e->movie_from_scene && e->scene) {
        e->scene->finish_movie();
        advance_scene(e);
    } else {
        e->tick = Clock::now();
    }
}
JNIEXPORT void JNICALL
Java_org_dc4engine_Arisa32Activity_dc4MovieCompleted(JNIEnv* env, jobject thiz) {
    Java_org_dc4engine_Dc4Activity_dc4MovieCompleted(env, thiz);
}
}  // extern "C"
