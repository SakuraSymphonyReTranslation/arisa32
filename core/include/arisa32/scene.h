// Portable scene runner for the D.C.4 reimplementation.
//
// Renders into a software RGBA framebuffer (kScreenW x kScreenH) so the same
// code runs on the desktop shell, in headless tests and on Android — the only
// platform-specific part left is presenting the finished frame.
//
// Executed graphics commands (see docs/render-progress.md for evidence/limits):
//   0x0F u8,u8   select transition mode
//   0x3C u8+str  load an image into a slot (1..3 background, 4..8 characters,
//                19..23 the same characters via the parallel path, 25..27 cuts)
//   0x8B u16x4   sprite scale + centre position
//   0x49 str     commit a background and its queued character changes
//   0x9D u16x4   scripted camera framing
// VM control flow and audio are handled by MesVm and SceneAudio.
#pragma once
#include "arisa32/audio.h"
#include "arisa32/crm.h"
#include "arisa32/crx.h"
#include "arisa32/mesfile.h"
#include "arisa32/messtream.h"
#include "arisa32/mesvm.h"
#include "arisa32/scene_audio.h"
#include "arisa32/voice.h"
#include "arisa32/dc2dm_minigame.h"

#include <cstdint>
#include <array>
#include <deque>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace dc4scene {

constexpr int kScreenW = 1280;
constexpr int kScreenH = 720;

// Decodes a byte string in code page 932 to UTF-16. Windows uses
// ::MultiByteToWideChar; other platforms install their own decoder through
// g_sjis_decoder (the Android shell routes it through Java's "Shift_JIS").
using SjisDecoder = std::u16string (*)(const std::string&);
extern SjisDecoder g_sjis_decoder;
std::u16string sjis_to_utf16(const std::string& s);

struct Image;
constexpr char16_t kInlineDash = u'\u2014';
constexpr char16_t kInlineMusicNote = u'\u266a';

// stb_truetype wrapper rasterizing one UTF-16 line into an RGBA buffer.
class FontRenderer {
public:
    FontRenderer() = default;
    ~FontRenderer();
    FontRenderer(const FontRenderer&) = delete;
    FontRenderer& operator=(const FontRenderer&) = delete;
    bool load(const std::string& path, int font_index, float pixel_size, bool native = true);
    bool is_pretendard_jp_bold() const { return pretendard_jp_bold_; }
    bool is_ms_gothic() const { return ms_gothic_; }
    // Renders white glyphs starting at x0 with the given baseline.
    // pixel_size overrides the font's own size (the engine uses 22px dialogue
    // and 18px names from one face). A dark outline is drawn under the glyphs.
    // output_scale maps logical coordinates and advances to output pixels;
    // glyphs are rasterized at that resolution, with a one-pixel black outline.
    // outlined=false keeps native labels that have no shadow or border plain.
    void rasterize_line(const std::u16string& text, int x0, int baseline,
                        std::vector<uint8_t>* rgba, int buf_w, int buf_h,
                        float pixel_size = 0, uint32_t rgb = 0xFFFFFF,
                        bool wide = false, uint32_t outline_rgb = 0,
                        bool fixed_grid = false, int tracking = 0,
                        float raster_size = 0, float output_scale = 1,
                        bool outlined = true, const Image* inline_dash = nullptr,
                        const Image* inline_note = nullptr) const;
    int glyph_advance(char16_t ch, float pixel_size = 0, bool wide = false,
                      bool fixed_grid = false, int tracking = 0,
                      const Image* inline_dash = nullptr, const Image* inline_note = nullptr) const;
    int text_width(const std::u16string& text, float pixel_size = 0, bool wide = false,
                   bool fixed_grid = false, int tracking = 0,
                   const Image* inline_dash = nullptr, const Image* inline_note = nullptr) const;
    std::pair<int, int> text_vertical_bounds(const std::u16string& text, float pixel_size = 0) const;

private:
    struct Glyph {
        int w = 0, h = 0, x = 0, y = 0, advance = 0;
        std::vector<uint8_t> coverage;
    };
    const Glyph& glyph(char16_t ch, int size, bool wide) const;
    mutable std::map<std::pair<int, char16_t>, Glyph> glyphs_;
    mutable std::map<int, void*> native_fonts_;
    void* native_dc_ = nullptr;
    std::u16string native_family_;
    std::vector<uint8_t> font_data_;
    void* font_ = nullptr;   // ::stbtt_fontinfo*
    float scale_ = 0;
    float pixel_size_ = 0;
    bool pretendard_jp_bold_ = false;
    bool ms_gothic_ = false;
};

struct Image {
    std::string name;
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;
    bool native_placement = false;
    float native_x = 0, native_y = 0, native_scale = 1;
    int native_alpha = 255;
    bool valid() const { return !rgba.empty(); }
};

void fit_gallery_image(const Image& image, int width, int height, std::vector<uint8_t>* frame);

struct SakuraSystem {
    struct Particle {
        float x = 0, y = 0, vy = 0, scale = 1, phase = 0, amplitude = 0;
        int row = 0, cell = 0, cell_ticks = 0;
        uint8_t alpha = 255;
    };
    bool active = false;
    int type = 0, parameter = 0;
    double tick_remainder = 0;
    std::vector<Particle> particles;
    void initialize(uint32_t& random_state);
    void update(uint32_t elapsed_ms, uint32_t& random_state);
    void draw(std::vector<uint8_t>& frame, const Image& sheet) const;
private:
    void reset_particle(size_t index, int depth, uint32_t& random_state);
};

struct FairyLightSystem {
    struct Particle {
        float x = 0, y = 0, vy = 0, phase = 0, amplitude = 0, alpha = 0;
        int drift = 0, row = 0, cell = 0, cell_ticks = 0;
        bool brightening = true;
    };
    bool active = false;
    int parameter = 0;
    double tick_remainder = 0;
    std::vector<Particle> particles;
    void initialize(uint32_t& random_state);
    void update(uint32_t elapsed_ms, uint32_t& random_state);
    void draw(std::vector<uint8_t>& frame, const Image& sheet) const;
private:
    void reset_particle(size_t index, int depth, uint32_t& random_state);
};

// One of the engine's BST sprite records.
struct BstSprite {
    std::string name;      // last script name, for "--" reuse
    std::string image;     // last resolved reference
    std::string expression; // DC2PC partial face overlay; body reference stays intact
    Image img;
    float scale = 1.0f;    // 0x8B operand 0, percent
    int x = kScreenW / 2, y = kScreenH / 2;
    // 0x8B mode-0 "current" position. The native pins the frame point (x, y)
    // at screen (cur + (x, y)): at scale 1 the canvas centre simply lands at
    // cur + (inner + size/2), and the 0x8B position acts as the zoom anchor.
    int cur_x = 0, cur_y = 0;
    // Canvas centre in the 1280-tall BST frame (inner offset + size/2).
    float frame_cx = 0, frame_cy = 0;
    int canvas_w = 0, canvas_h = 0;
    int alpha = 255;
    bool visible = false;
    int z = 0;
    int priority = 50;     // Native 0x8B mode 0x20; lower values draw first.
    struct Move {
        int from_x = 0, from_y = 0, to_x = 0, to_y = 0;
        uint32_t frames = 1, ticks = 0;
    };
    struct Wave {
        int amplitude = 0, start = 0, end = 0;
        uint32_t frames = 60, ticks = 0;
        double origin = 0;
    };
    std::deque<Move> moves;
    std::array<std::deque<Wave>, 2> waves;
    double motion_remainder = 0;
    bool relative_origin_valid = false;
    int relative_x = 0, relative_y = 0;
    bool moving() const { return !moves.empty() || !waves[0].empty() || !waves[1].empty(); }
    double position(int axis) const;
    void finish_motion();
    // 0x8C mode 0x80: the effect layer (a BST\CUT* cut-in) fades out through
    // its wipe mask; sub_447B00 queues alpha 255 -> 0 at 510/frames per frame,
    // so the sprite is fully transparent halfway through the duration.
    struct Fade {
        int start_alpha = 255, end_alpha = 0;
        double rate_per_frame = 0;
        double elapsed_frames = 0, duration_frames = 0;
        bool active() const { return duration_frames > 0 && elapsed_frames < duration_frames; }
    } fade;
};

// PCK animation effect (AdvData\GRP\EFF\ef*.PCK)
struct EffectPck {
    std::string name;
    std::vector<uint8_t> archive;
    std::vector<std::pair<uint32_t, uint32_t>> entries;
    bool active = false;
    double elapsed_ms = 0;
    double duration_ms = 0;
    int current_frame = -1;
    Image current_img;
    int x = 0, y = 0;
    int loop = 0;

    bool valid() const { return !entries.empty() && !archive.empty(); }
    void clear() {
        name.clear();
        archive.clear();
        entries.clear();
        active = false;
        elapsed_ms = 0;
        duration_ms = 0;
        current_frame = -1;
        current_img = Image{};
        x = 0;
        y = 0;
        loop = 0;
    }
};

// A UI chip: one blit from a sheet. 0x73 queues it on a widget, 0x7A draws it
// outright; 0x77 supplies the size, the sheet id and the state (99 = normal,
// 1 = hover, 2 = pressed).
struct UiChip {
    int dst_x = 0, dst_y = 0, src_x = 0, src_y = 0;
    int w = 0, h = 0;
    int sheet = 0;
    bool valid = false;
};

// One COMU phone-chat timeline entry (MesVM case 171 / text case 109).
// Bubble rows crop GRP/SYSTEM/COMUCHIP.CRX at (420, 81*contact)..(740, ...+81)
// — avatar, name plate and blank bubble in one chip; stamps crop
// GRP/SYSTEM/comustamp.CRX at (80*stamp/8, 64*stamp%8), 79x63 per cell.
struct ComuItem {
    int contact = 0;        // contact row 0..10; ignored when stamp >= 0
    int stamp = -1;         // 0-based stamp cell when >= 0
    std::string text;       // SJIS bubble text; empty for stamps
};

// One interactive widget from the UI script (0x72 hit rect + 0x54 action).
struct UiButton {
    int x = 0, y = 0, w = 0, h = 0;
    std::string action;
    UiChip state[3];
    std::array<std::vector<UiChip>, 3> layers;
    bool is_slider = false;
    int slider_bank = -1;
    int slider_index = 0;
    int slider_count = 0;
    int slider_left = 0;
    int slider_right = 0;
    bool has_knob = false;
    UiChip knob[2];
    UiChip slider_caption;
};

// Message-window layout, measured against a 1:1 (1280x720) real screenshot.
//
// The speaker name is *plain outlined text*, not a plate — the cyan
// parallelogram stored in MESWIN.CRX at (338,197) is the choice-selection
// box. Default dialogue cells advance 22px (11px for halfwidth characters).
struct MesWinLayout {
    int bg_x = 0, bg_y = 540, bg_w = kScreenW, bg_h = kScreenH - 540;
    int bg_src_x = 0, bg_src_y = 540;
    int text_x = 283, text_w = 660, text_baseline = 594;
    int name_x = 283, name_baseline = 540, name_font = 18;
    int font_size = 22;
    int line_spacing = 36;
    int max_lines = 2;
    // Patched MESWIN sheet: four 32x32 frames, 33px apart, starting at (36,0).
    int gear_x = 36, gear_y = 0, gear_w = 32, gear_h = 32;
};

struct DalSegnoPlayback {
    int character_interval_ms;
    int auto_delay_ms;
    bool wait_for_voice;
    bool stop_auto_at_choice;
    bool stop_skip_at_choice;
    bool skip_unread;
};

class SceneRunner {
public:
    explicit SceneRunner(const std::string& data_dir);
    ~SceneRunner();

    void set_font(const FontRenderer* font, const FontRenderer* textbox_font = nullptr) {
        font_ = font;
        textbox_font_ = textbox_font ? textbox_font : font;
    }
    void set_debug(bool on) { debug_ = on; }
    void set_audio(dc4::AudioEngine* audio, dc4::VoiceArchive* voice);

    bool load_script(const std::string& mes_path, const std::string& entry_label = {});
    bool open_dalsegno_title(int reveal_chars = -1);
    // Seed the first authored DC2DM transition after load_script, before advance.
    bool seed_dc2dm_presentation(const std::vector<uint8_t>& rgba);
    bool start_dc2ty_title_exit(const std::vector<uint8_t>& outgoing);
    bool update_dc2ty_title_exit(uint32_t elapsed_ms);
    void draw_dc2ty_title_exit(std::vector<uint8_t>* outgoing);
    // After seeding presentation, adopt the title asset the host kept playing on channel 0.
    bool seed_dc2dm_bgm(const std::string& asset);
    bool jump_to_label(std::string label);
    bool has_label(const std::string& label) const { return vm_.has_label(label); }
    const std::string& script_name() const { return script_name_; }
    using CommandObserver = std::function<void(const dc4::MesCommand&, const std::string&)>;
    void set_observer(CommandObserver observer) { observer_ = std::move(observer); }
    void set_auto_save_path(std::string path) { auto_save_path_=std::move(path); }
    const std::string& pending_movie() const { return pending_movie_; }
    bool is_effect_movie() const { return pending_movie_is_effect_; }
    void finish_movie() { pending_movie_.clear(); pending_movie_is_effect_ = false; }
    // Hosts keep updating while a script wait is pending, then advance once
    // script_ready() is true. Input can finish a skippable wait/credit sequence.
    bool waiting_for_time() const {
        return script_wait_pending_ && (script_wait_ms_ == kDc2dmVoiceWait
            ? dc2dm_voice_audible() : script_wait_ms_ != 0);
    }
    bool script_ready() const { return !sidebar_visible_ && !reviewing_dialogue() &&
        !dalsegno_confirmation_active() && script_wait_pending_ && !waiting_for_time(); }
    void finish_script_wait();
    bool credits_active() const { return credits_active_ || dc2dm_ending_.active; }
    bool effect_pck_active() const { return effect_pck_.active; }
    void set_window_opacity(int percent);
    void set_effects_enabled(bool enabled);
    void set_visual_effects_enabled(bool enabled) { visual_effects_enabled_ = enabled; scene_dirty_ = true; }
    void set_dialogue_font_size(int size);
    void set_effect_speed(int native_0_to_11);
    void set_text_speed(int native_0_to_11);
    void set_character_volume(size_t id, int percent, bool muted = false) { scene_audio_.set_character_volume(id, percent, muted); }
    void prepare_system_voice() { scene_audio_.prepare_system_voice(); }
    void set_latin_mode(bool latin);
    bool latin_mode() const { return latin_mode_; }
    void set_use_system_menu(bool use) { use_system_menu_ = use; }
    bool use_system_menu() const { return use_system_menu_; }
    // Advances to (and including) the next dialogue line. Speaker names
    // (0x6A) are consumed on the way and stored separately.
    bool advance_to_next_text();

    const std::string& current_line() const { return current_line_; }
    const std::string& speaker() const { return speaker_; }
    // DC2PC's textbox history is a presentation cursor; the live VM stays put.
    bool browse_dialogue(int direction);
    bool reviewing_dialogue() const { return dialogue_cursor_ >= 0; }
    void return_to_dialogue() { dialogue_cursor_ = -1; }
    const std::string& displayed_line() const;
    const std::string& displayed_speaker() const;
    bool set_sidebar_visible(bool visible);
    bool handle_dc2pc_back();
    bool sidebar_visible() const { return sidebar_visible_; }
    bool scene_ended() const { return scene_end_; }
    const std::string& next_script() const { return next_script_; }
    const std::string& last_error() const { return last_error_; }
    bool waiting_for_choice() const { return ds_modal_buttons_.empty() && vm_.waiting_for_choice(); }
    const std::vector<dc4::MesChoice>& choices() const { return vm_.choices(); }
    bool choose(size_t index) {
        if (sidebar_visible_ || reviewing_dialogue() || !waiting_for_choice()) return false;
        bool ok = vm_.choose(index);
        if (ok) scene_dirty_ = true;
        return ok;
    }
    int choice_at(int x, int y) const;
    bool handle_slider_touch(int action, int x, int y, std::string* triggered_action = nullptr);
    enum class UiKey { Next, Previous, Activate, Decrease, Increase };
    bool handle_dalsegno_key(UiKey key, std::string* action);
    bool dalsegno_menu_active() const;
    std::string dalsegno_back_action() const;
    bool dalsegno_name_pending() const { return ds_name_pending_; }
    bool dalsegno_name_confirming() const { return ds_name_confirming_; }
    const std::string& dalsegno_family_name() const { return ds_family_name_; }
    const std::string& dalsegno_given_name() const { return ds_given_name_; }
    bool set_dalsegno_name(const std::string& family, const std::string& given);
    bool handle_dalsegno_action(const std::string& action);
    bool dalsegno_replay_active() const { return ds_replay_active_; }
    bool dalsegno_confirmation_active() const { return !ds_modal_buttons_.empty(); }
    bool dalsegno_gallery_active() const { return ds_gallery_image_.valid(); }
    bool dalsegno_gallery_wiping() const { return dalsegno_gallery_active() && transition_active_; }
    const Image& dalsegno_gallery_image() const { return ds_gallery_image_; }
    uint32_t dalsegno_gallery_revision() const { return ds_gallery_revision_; }
    bool advance_dalsegno_gallery();
    void close_dalsegno_gallery();
    const std::vector<uint8_t>& dalsegno_gallery_frame(int width, int height);
    void recover_dalsegno_progress(const std::set<std::string>& read_lines, const std::string& save_dir);
    bool open_dalsegno_config();
    bool resume_dalsegno_config();
    bool load_dalsegno_system(const std::string& path, std::string* error = nullptr);
    bool load_dalsegno_profile(const std::string& path, std::string* error = nullptr);
    bool save_dalsegno_profile(const std::string& directory, std::string* error = nullptr);
    bool quit_requested() const { return ds_quit_requested_; }
    bool dalsegno_music_active() const { return ds_music_active_; }
    int dalsegno_music_track() const { return ds_music_track_; }
    int dalsegno_music_scroll() const { return ds_music_scroll_; }
    const std::vector<std::string>& dalsegno_music_titles() const { return ds_music_titles_; }
    bool dalsegno_music_checked(int track) const { return track >= 0 && track < 42 && ds_music_checks_[size_t(track)]; }
    void sync_audio_from_slider(int bank, int index, int val);
    DalSegnoPlayback dalsegno_playback() const;
    float dalsegno_movie_gain() const;
    std::u16string visible_text() const;
    size_t visible_text_length() const;
    void set_pointer(int x, int y, bool down) {
        if (x != pointer_x_ || y != pointer_y_ || down != pointer_down_) ds_focus_ = -1;
        pointer_x_ = x; pointer_y_ = y; pointer_down_ = down;
    }
    bool minigame_active() const;
    void minigame_pointer(int action, int x, int y);
    void minigame_key(Dc2dmMinigame::Key key);
    const Dc2dmMinigame* minigame() const { return minigame_.get(); }
    void set_message_visible(bool visible) { message_visible_ = visible; scene_dirty_ = true; }
    bool message_visible() const { return message_visible_; }
    void set_taskbar_visible(bool visible) { taskbar_visible_ = visible; scene_dirty_ = true; }
    bool taskbar_visible() const { return taskbar_visible_; }
    void toggle_taskbar() { taskbar_visible_ = !taskbar_visible_; scene_dirty_ = true; }
    bool replay_voice();
    bool voice_playing() const;
    // Voice file attached to the current line ("" when silent), for backlog rows.
    const std::string& last_voice() const { return scene_audio_.last_voice_name(); }
    // Default/headless execution settles effects immediately. Interactive hosts
    // opt in, feed elapsed milliseconds, and may finish effects while skipping.
    void set_animation_enabled(bool enabled);
    void update(uint32_t elapsed_ms);
    void finish_animations(bool finish_screen_effect = true);
    bool animating() const;
    float camera_scale() const { return camera_scale_; }
    // Native .dat saves for supported games; snapshots for legacy/checkpoints.
    bool save_state(const std::string& path);
    bool load_state(const std::string& path);
    bool save_load_allowed() const;
    // Restore normal persistence when a supported replay returns to its menu.
    void finish_replay();
    bool capture_state(std::vector<uint8_t>* bytes, std::string* error = nullptr);
    bool capture_pc_save(std::vector<uint8_t>* bytes, std::string* error = nullptr);
    uint32_t save_format_version() const { return mes_.version; }
    bool restore_state(const std::vector<uint8_t>& bytes, std::string* error = nullptr);
    bool restore_pc_save(const std::vector<uint8_t>& bytes, std::string* error = nullptr);
    int variable(uint16_t bank, uint16_t index) const { return vm_.read_variable(bank, index); }
    uint32_t script_position() const { return vm_.pos(); }
    uint32_t dc4_progress_awards() const { return vm_.dc4_progress_awards(); }
    void set_variable(uint16_t bank, uint16_t index, int value) {
        vm_.write_variable(bank, index, value);
        if (dc2dm_ui_ && bank == 9 && index == 7 && value != 0) sidebar_visible_ = false;
        if (is_dalsegno_profile_ && bank == 8) scene_dirty_ = true;
    }

    // Loads the message-window UI script (MES/win_size.mes) and the sheets it
    // blits from (0x3D registers a sheet at index slot + 37).
    bool load_sheets(const std::string& mes_path);
    bool load_ui_script(const std::string& mes_path);
    bool show_message_window(const std::string& path);

    // Renders the current state into the frame buffer (RGBA, kScreenW*kScreenH*4).
    void draw(int reveal_chars);
    // Fresh scene-only preview, before transitions/message UI. Does not advance
    // the VM or animation clocks; dimensions must fit the logical canvas.
    std::vector<uint8_t> capture_thumbnail(int width, int height);
    // Shows the next-page gear after the text while waiting for a click.
    void set_wait_marker(bool on) { wait_marker_ = on; }
    void set_wait_frame(int frame) { wait_frame_ = frame; }
    const std::vector<uint8_t>& frame() const { return frame_; }

    // Returns the action tag of the widget under (x, y), or nullptr.
    const std::string* hit_test(int x, int y) const;

    // Test hooks.
    void dump_sprites() const;
    const BstSprite& sprite(size_t i) const { return sprites_[i]; }
    const std::vector<UiButton>& buttons() const { return buttons_; }

    bool load_grp_crx(const std::string& rel, Image* out, std::string* err) const;

private:
    bool mes_exists(const std::string& name) const;
    void load_slot(int slot, const std::string& raw);
    void load_bst(int idx, const std::string& name, int slot);
    bool dispatch_dc2dm_message_face(const dc4::MesCommand& command);
    bool dc4_message_faces() const;
    void release_dc4_message_face(size_t index);
    bool dispatch_dc4_message_face(const dc4::MesCommand& command);
    void draw_dc4_message_face();
    void draw_dc2dm_message_face();
    void release_dc2dm_sprite(size_t index);
    bool load_bst_expression(int idx, const std::string& name);
    void load_preload(int slot, const std::string& name);
    void load_layer(int layer, const std::string& path);
    void load_sheet(int id, const std::string& path);
    void set_sprite_transform(uint16_t scale_pct, uint16_t x, uint16_t y,
                              uint16_t mode);
    void draw_message_window(int reveal_chars);
    void load_dc2pc_sidebar();
    void reset_dialogue_history();
    void record_dialogue();
    void blit_chip(const UiChip& ch, int alpha);
    void draw_text_line(const std::string& sjis, int reveal);
    void draw_choices();
    void load_dc2pc_choices(dc4::MesStream& stream);
    void draw_dc2pc_choices();
    UiButton dc2pc_choice_button(size_t index) const;
    void apply_camera(uint16_t mode, uint16_t scale_pct, uint16_t x, uint16_t y);
    void apply_dalsegno_camera(uint16_t mode, uint16_t scale_pct, uint16_t x, uint16_t y);
    void update_dalsegno_camera(double elapsed_ms, bool finish = false);
    void reset_camera();
    void queue_sprite_slide(uint16_t x, uint16_t y, uint16_t target, uint16_t mode);
    void queue_sprite_move(size_t index, int x, int y, uint32_t frames);
    void queue_sprite_entrance(size_t index, uint16_t kind, uint32_t frames);
    void update_sprite_motion(double elapsed_ms);
    void reset_sprite_motion_batch();
    bool collect_dialogue_command(const dc4::MesCommand& command);
    void schedule_dialogue_actions();
    void run_dialogue_action(int kind, int index);
    void update_dialogue_actions(double elapsed_ms);
    void finish_dialogue_actions();
    // 0x8C: effect-layer fade (MesVM case 140 -> sub_447B00).
    void queue_sprite_effect(uint16_t w0, uint16_t w1, uint16_t w2, uint16_t w3);
    // COMU phone chat: 0xAB modes and 0x6D messages (MesVM case 171/109).
    void comu_command(uint16_t w0, uint16_t w1, uint16_t w2, uint16_t w3);
    void comu_message(const std::string& str);
    void draw_comu();
    bool start_credits(uint32_t duration_ms);
    void draw_credits();
    bool start_dc2dm_ending();
    void draw_dc2dm_ending();
    void finish_dc2dm_ending(bool skipped);
    bool load_pck_effect(const std::string& rel);
    void start_pck_effect(uint16_t w0, uint16_t w1, uint16_t w2);
    void update_pck_effect(double elapsed_ms);
    bool decode_pck_frame(size_t index, Image* out);
    // 0xB4/0x36/0x37 transition frame markers (see docs/vm-progress.md).
    void mark_transition_frame(int kind);
    void start_transition(int frames = 30, bool scale_speed = true);
    void finish_transition();
    void start_dalsegno_wipe(int parameter);
    void snapshot_for_transition();
    void draw_transition(uint8_t* dst);
    void compose_scene();
    void start_dc2pc_transition(int mode);
    void update_dc2pc_transition(uint32_t elapsed_ms);
    void finish_dc2pc_transition();
    void draw_dc2pc_transition();
    bool start_dc2ty_transition(int mode, int parameter);
    void tick_dc2ty_transition();
    void draw_dc2ty_circles();
    bool start_dc2dm_transition(int mode, int speed);
    void draw_dc2dm_scene_controls();
    void clear_dc2dm_scene_controls();
    void snapshot_dc2dm_transition();
    void finish_dc2dm_transition();
    void draw_dc2dm_transition();
    bool particle_effects_visible() const;
    void start_dalsegno_lens(int density);
    void draw_dalsegno_lens();
    void update_sakura(uint32_t elapsed_ms);
    void draw_sakura();
    bool dispatch_dc4_effect_command(const dc4::MesCommand& c);
    int effect_random(int range);
    void start_screen_effect(int type, int duration);
    void update_screen_effect(uint32_t elapsed_ms);
    void draw_screen_effect();
    double screen_effect_remaining_ms() const;

    bool dispatch_command(const dc4::MesCommand& c, size_t& transfers);
    bool dispatch_dalsegno_ui(const dc4::MesCommand& c);
    void load_dalsegno_upper_menu();
    void draw_dalsegno_name();
    void draw_dalsegno_character_counters();
    std::string dalsegno_text(const std::string& text) const;
    void draw_dalsegno_captions(int active_button, int state);
    void draw_dalsegno_confirmation();
    void start_dalsegno_break();
    void finish_dalsegno_break();
    void draw_dalsegno_break();
    void show_dalsegno_confirmation(int x, int y, int message, const std::string& yes, const std::string& no);
    void load_dalsegno_catalogue();
    void award_dalsegno_scene(const std::string& script);
    void award_dalsegno_cg(const std::string& name);
    bool open_dalsegno_gallery(const std::string& action);
    bool select_dalsegno_gallery_image(size_t index, size_t candidates);
    void reset_dalsegno_ui();
    bool dispatch_dalsegno_music(const dc4::MesCommand& c);
    bool play_dalsegno_music();
    void step_dalsegno_music(int direction);
    void refresh_dalsegno_music();
    void draw_dalsegno_music();
    int dalsegno_window_alpha() const;
    int dalsegno_audio_level(int enable_index, int volume_index) const;
    void sync_dalsegno_audio();
    void sync_dalsegno_audio_setting(uint16_t bank, uint16_t index);
    void update_dalsegno_music();
    bool touch_dalsegno_music_scroll(int action, int x, int y);
    bool restore_dc2pc_pc_save(const std::vector<uint8_t>& bytes, std::string* error, bool ty = false);
    bool restore_dc2dm_pc_save(const std::vector<uint8_t>& bytes, std::string* error);
    bool capture_dalsegno_pc_save(std::vector<uint8_t>* bytes, std::string* error);
    bool restore_dalsegno_pc_save(const std::vector<uint8_t>& bytes, std::string* error);
    void reset_restore_transients();
    bool dispatch_timing_command(const dc4::MesCommand& c);
    void sync_minigame();
    bool dispatch_flow_command(const dc4::MesCommand& c, size_t& transfers);
    bool dispatch_ui_chip_command(const dc4::MesCommand& c);
    bool dispatch_layer_command(const dc4::MesCommand& c);
    bool dispatch_sprite_command(const dc4::MesCommand& c);
    bool dispatch_camera_command(const dc4::MesCommand& c);
    bool dispatch_text_command(const dc4::MesCommand& c);
    bool replay_action_disabled(const std::string& action) const;

    using SakuraParticle = SakuraSystem::Particle;
    SakuraSystem sakura_;
    Image sakura_sheet_;
    FairyLightSystem fairy_lights_;
    Image fairy_light_sheet_;
    struct LensParameters {
        int x = 0, y = 0;
        int start_angle = 0, end_angle = -1;
        uint32_t duration_ms = 0;
    } ds_lens_parameters_;
    struct LensReflection {
        int radius_x, radius_y, source_x, size;
        float scale, alpha;
    };
    struct {
        LensParameters parameters;
        uint32_t elapsed_ms = 0;
        Image sheet;
        std::vector<LensReflection> reflections;
    } ds_lens_;
    uint32_t effect_random_state_ = 1;
    struct ScreenEffectSprite {
        float from_x = 0, to_x = 0, y = 0;
        float pivot_x = 0, pivot_y = 0, scale = 1, alpha = 1;
        int sx = 0, sy = 0, width = 0, height = 0;
        double elapsed_ms = 0, duration_ms = 0;
        double scale_phase_ms = 0;
        bool fade = false;
    };
    std::vector<ScreenEffectSprite> screen_effect_;
    struct DsPetalAtlas {
        static constexpr int width = 499, height = 360;
        std::string name;
        std::vector<uint8_t> alpha, coverage;
        bool load(const Image& image);
        void stamp(const ScreenEffectSprite& sprite, float progress);
        void composite(std::vector<uint8_t>& frame) const;
    } ds_petals_;
    EffectPck effect_pck_;
    std::string data_;
    std::string script_name_;
    CommandObserver observer_;
    std::string auto_save_path_;
    std::string pending_movie_, input_skip_label_;
    bool pending_movie_is_effect_ = false;
    bool is_dc4_or_dc4ph_ = false;
    bool is_dalsegno_profile_ = false;
    bool is_dc2pc_profile_ = false;
    bool visual_effects_enabled_ = true;
    int dialogue_font_size_ = 22;
    bool dc4_replay_profile_ = false;
    static constexpr double kDc2dmVoiceWait = -2;
    bool dc2dm_voice_audible() const;
    double script_wait_ms_ = 0;
    std::unique_ptr<Dc2dmMinigame> minigame_;
    bool script_wait_pending_ = false;
    bool script_wait_skippable_ = true;
    double dc2pc_clock_ms_ = 0, dc2pc_wait_reference_ms_ = 0;
    bool dc2pc_wait_reference_valid_ = false;
    bool credits_active_ = false;
    double credits_elapsed_ms_ = 0, credits_duration_ms_ = 0, credits_scroll_ms_ = 0;
    std::vector<uint8_t> credits_archive_;
    std::vector<std::pair<uint32_t, uint32_t>> credits_entries_;
    std::map<size_t, Image> credits_pages_;
    struct EndingArchive {
        std::string path;
        std::vector<std::pair<uint32_t, uint32_t>> entries;
        bool open(const std::string& name, size_t max_entries, int width, int height, std::string* error);
        bool decode(size_t index, Image* image, std::string* error) const;
    };
    struct EndingCard {
        EndingArchive animation;
        Image image;
        int frame = -1;
    };
    struct Dc2dmEnding {
        bool active = false;
        double elapsed_ms = 0, duration_ms = 0, card_ms = 0;
        int credits_speed = 0;
        EndingArchive credits, montage;
        std::map<size_t, Image> pages;
        std::map<size_t, EndingCard> cards;
        Image photo;
    } dc2dm_ending_;
    bool dc2dm_ending_reference_valid_ = false;
    double dc2dm_ending_reference_elapsed_ms_ = 0;
    dc4::MesFile mes_;
    dc4::MesVm vm_;
    dc4::SceneAudio scene_audio_;
    std::vector<BstSprite> sprites_ = std::vector<BstSprite>(16);
    // The native effect layer (VM+0x688): the last BST\CUT* sprite loaded,
    // which 0x8C mode 0x80 fades out.
    int effect_sprite_ = 12;
    // COMU phone chat state. The timeline persists across AB 98 (session end)
    // and is cleared by AB 65535 at every scenario start (sub_4581F0).
    bool comu_visible_ = false, comu_group_ = false;
    std::vector<ComuItem> comu_items_;
    Image comu_chips_, comu_stamps_;
    std::map<int, Image> layers_;
    std::map<int, Image> sheets_;          // 0x3D slot + 37, and 0x3C slot raw
    std::vector<UiChip> static_chips_;     // 0x7A blits (the bar capsule)
    std::vector<UiChip> scene_chips_;      // 0x7A blits executed by the scene
    int scene_chip_origin_x_ = 0, scene_chip_origin_y_ = 0;
    int scene_chip_source_x_ = 0, scene_chip_source_y_ = 0;
    UiChip pending_scene_chip_;
    bool pending_scene_blit_ = false;
    bool pending_chip_is_button_ = false;
    std::vector<UiButton> buttons_;        // 0x72/0x54/0x73/0x77 widgets
    std::vector<UiButton> ds_dialogue_buttons_;
    bool dalsegno_dialogue_controls() const;
    UiButton pending_button_;
    bool pending_has_button_ = false;
    struct DalSegnoCaption {
        int x = 0, y = 0, size = 20;
        int button = -1, state = 99;
        bool preview = false;
        std::string text;
        double elapsed_ms = 0;
    };
    DalSegnoCaption ds_pending_caption_;
    bool ds_has_caption_ = false;
    std::vector<DalSegnoCaption> ds_captions_;
    std::array<std::string, 16> ds_callbacks_;
    std::vector<UiButton> ds_modal_buttons_;
    std::vector<UiChip> ds_modal_chips_;
    int ds_focus_ = -1;
    bool ds_quit_requested_ = false;
    bool ds_name_pending_ = false, ds_name_confirming_ = false;
    std::string ds_family_name_ = "Takamura", ds_given_name_ = "Atsuya";
    std::vector<UiButton> ds_name_buttons_;
    std::vector<UiChip> ds_upper_chips_;
    std::vector<uint8_t> ds_config_return_;
    bool ds_music_active_ = false, ds_music_building_ = false, ds_music_drag_ = false;
    bool ds_music_has_title_ = false;
    int ds_music_track_ = 0, ds_music_scroll_ = 0, ds_music_repeat_ = 0;
    std::vector<std::string> ds_music_titles_;
    std::array<bool, 42> ds_music_checks_{};
    uint32_t ds_music_random_ = 0xD5A032;
    std::vector<uint8_t> ds_system_template_;
    std::vector<std::string> ds_cg_names_;
    std::map<std::string, uint16_t> ds_cg_ids_, ds_scene_ids_;
    bool ds_replay_active_ = false;
    int ds_replay_route_ = 0, ds_replay_page_ = 0;
    struct DsGalleryEntry { uint16_t id; bool wipe; };
    std::vector<DsGalleryEntry> ds_gallery_order_;
    std::string ds_gallery_mask_;
    int ds_gallery_wipe_parameter_ = 0;
    size_t ds_gallery_index_ = 0;
    uint32_t ds_gallery_revision_ = 0;
    Image ds_gallery_image_;
    int ds_gallery_width_ = 0, ds_gallery_height_ = 0;
    std::vector<uint8_t> ds_gallery_frame_;
    Image ds_inline_bar_, ds_inline_note_;
    int ds_face_x_ = 4, ds_face_y_ = 567;
    unsigned ds_counter_ticks_ = 0;
    MesWinLayout layout_;
    bool dc2pc_ui_ = false;
    std::vector<UiButton> dc2pc_scene_buttons_;
    std::string dc2pc_back_label_;
    bool dc2pc_button_wait_ = false;
    bool dc2pc_advmenu_call_ = false;
    bool dc2dm_ui_ = false;
    size_t dc2dm_toolbar_count_ = 0;
    bool sidebar_visible_ = false;
    bool sidebar_replay_ = false;
    UiChip sidebar_background_;
    std::vector<UiButton> sidebar_buttons_;
    struct ChoiceUi {
        UiButton button;
        std::vector<UiChip> state[3];
        int text_x = 0, text_y = 0;
    };
    std::vector<ChoiceUi> dc2pc_choices_;
    std::vector<ChoiceUi> dc2dm_toolbar_;
    int ui_bar_top_ = 690;
    Image meswin_;
    Image mask_;           // Wipe texture in Dal Segno slot 16 or DC4/PH slot 21.
    Image screen_effect_atlas_; // 0x3C slot 22, animated by 0x38.
    std::vector<uint8_t> frame_;
    std::vector<uint8_t> scene_frame_;
    bool scene_dirty_ = true;
    int transition_type_ = 0;
    bool preserve_scene_ = false;
    std::vector<bool> pending_sprites_ = std::vector<bool>(16, false);
    float camera_scale_ = 1.0f;
    float camera_x_ = 512, camera_y_ = 288;
    struct CameraMotion {
        float from_scale = 1, to_scale = 1;
        float from_x = 512, from_y = 288, to_x = 512, to_y = 288;
        double elapsed_ms = 0, duration_ms = 0;
    };
    std::vector<CameraMotion> camera_motion_;
    std::map<int, CameraMotion> ds_background_motion_;
    struct DsCameraLayer {
        float base_x = 0, base_y = 0, base_scale = 1;
        CameraMotion motion;
    };
    std::map<int, DsCameraLayer> ds_camera_layers_;
    uint16_t camera_duration_ = 5; // MES 0x99, native default 5 * 12 updates
    bool animation_enabled_ = false;
    bool effects_enabled_ = true;
    int effect_speed_ = 5;
    int text_speed_ = 5;
    uint32_t sprite_wave_frames_ = 60;
    int sprite_wave_axis_ = 0;
    struct DialogueAction { int kind = 0, index = 0; double due_ms = 0; };
    std::vector<DialogueAction> dialogue_actions_;
    double dialogue_action_ms_ = 0;
    std::array<std::vector<dc4::MesCommand>, 10> dialogue_batches_;
    bool collecting_dialogue_batch_ = false;
    size_t dialogue_batch_index_ = 0;
    // Scene transition: 0x0F(32) snapshots the composed frame and selects the
    // effect; 0xB4 after the layer commit starts a crossfade to the new scene.
    std::vector<uint8_t> transition_snap_;
    int transition_kind_ = 0;
    bool transition_active_ = false;
    bool transition_pending_ = false;
    double transition_elapsed_ms_ = 0, transition_duration_ms_ = 0;
    struct DsBreakTile {
        int vx = 0, vy = 0, life = 0;
    };
    struct DsBreakTransition {
        static constexpr int tile_size = 32, ticks = 24;
        static constexpr double tick_ms = 1000.0 / 60;
        std::vector<uint8_t> source;
        std::vector<DsBreakTile> tiles;
        Image atlas;
        double elapsed_ms = 0;
        bool active() const { return !source.empty(); }
    } ds_break_;
    // DC2PC raw0D presents a pending composition. Keep the displayed pixels
    // separately: raw30 must not replace the source of an outgoing fade.
    std::vector<uint8_t> dc2pc_displayed_frame_, dc2pc_transition_target_;
    bool dc2pc_present_pending_ = false, dc2pc_black_ = false;
    bool dc2ty_draw_pending_ = false;
    bool dc2pc_transition_active_ = false;
    int dc2pc_transition_mode_ = 0, dc2pc_transition_speed_ = 0;
    double dc2pc_transition_elapsed_ms_ = 0, dc2pc_transition_duration_ms_ = 0;
    struct Dc2tyCircleTile {
        int origin_x = 0, origin_y = 0, x = 0, y = 0;
        int vx = 0, vy = 0, life = 0, age = 0, time = 0;
        bool advance();
    };
    struct Dc2tyTransition {
        int step = 1, progress = 0, tick_ms = 40;
        std::vector<int16_t> mask;
        std::vector<Dc2tyCircleTile> tiles;
        Image source, atlas, canvas;
    } dc2ty_transition_;
    struct Dc2dmTransition {
        std::vector<uint8_t> source, target, initial_source;
        bool active = false, pending = false, black = false;
        int mode = 0, step = 1, limit = 510, tick_ms = 34;
        double elapsed_ms = 0, duration_ms = 0;
    } dc2dm_transition_;
    Image dc2dm_transition_mask_;
    std::string current_line_;
    std::string speaker_;
    struct Dialogue { std::string text, name; };
    std::deque<Dialogue> dialogue_history_;
    int dialogue_cursor_ = -1;
    bool preserving_dialogue_history_ = false;
    std::string effect_movie_;
    bool scene_end_ = false;
    std::string next_script_;
    std::string last_error_;
    const FontRenderer* font_ = nullptr;
    const FontRenderer* textbox_font_ = nullptr;
    dc4::AudioEngine* audio_ = nullptr;
    dc4::VoiceArchive* voice_ = nullptr;
    bool debug_ = false;
    bool wait_marker_ = false;
    int wait_frame_ = 0;
    int pointer_x_ = -1, pointer_y_ = -1;
    bool pointer_down_ = false;
    bool message_visible_ = true;
    bool taskbar_visible_ = true;
    UiButton open_button_;
    int window_opacity_ = 50;
    bool latin_mode_ = false;
    bool use_system_menu_ = true;
    int captured_slider_ = -1;
};

}  // namespace dc4scene
