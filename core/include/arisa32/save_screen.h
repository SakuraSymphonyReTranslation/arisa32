// Shared CPU Save/Load screen. Geometry and art come from SAVE.mes/LOAD.mes
// and the native save-select renderer. File IO belongs to the host; actions
// are returned only after any required confirmation is accepted.
#pragma once

#include "arisa32/resource.h"
#include "arisa32/scene.h"
#include "arisa32/confirm_dialog.h"
#include <array>

namespace dc4ui {

enum class SaveMode { Save, Load };

struct SaveSlot {
    bool occupied = false;
    std::u16string timestamp;
    std::u16string excerpt;
    dc4scene::Image thumbnail;  // Any valid RGBA size; scaled into native crop.
    int character_icon = -1;    // DC2PC's authored character badge (0..15).
};

enum class SaveActionType {
    None, Back, Slot, Page, Mode, LatestLoad, AutoLoad, QuickLoad,
    Config, Title, Quit, QuickSave
};

struct SaveAction {
    SaveActionType type = SaveActionType::None;
    // Slot: zero-based 0..99. Page: 0..9. Mode: 0=Save, 1=Load.
    // Hosts retain existing save IDs by using index + 1 for storage.
    int index = -1;
};

class SaveScreen {
public:
    static constexpr int kPageCount = 10;
    static constexpr int kSlotsPerPage = 10;
    static constexpr int kSlotCount = kPageCount * kSlotsPerPage;

    // All required CRX assets must decode. Failure clears loaded state and
    // reports the asset; draw() provides a usable plain fallback screen.
    bool load(const std::string& data_dir, std::string* error = nullptr);
    bool loaded() const { return loaded_; }

    SaveMode mode() const { return mode_; }
    int page() const { return page_; }
    bool dc2pc() const { return dc2pc_; }
    int selected_icon() const { return selected_icon_; }
    bool choosing_icon() const { return choosing_icon_; }
    void set_special_slots(const SaveSlot& quick, const SaveSlot& latest) { quick_slot_=quick; latest_slot_=latest; }
    void set_mode(SaveMode mode);
    void set_page(int page);       // Clamp to 0..9.
    void change_page(int delta);   // Wrap as in the original scripts.
    void set_shortcuts(bool latest, bool automatic, bool quick);
    void set_navigation(bool config, bool title, bool quit);
    void set_language(dc4::Language lang) { language_ = lang; confirm_dialog_.set_language(lang); }
    dc4::Language language() const { return language_; }
    // Hosts update this alongside their metadata cache. Empty LOAD rows have
    // no action in the native screen; SAVE rows are always selectable.
    void set_slot_occupied(int index, bool occupied);

    // The ten rows on each page run down the left column, then the right.
    static int slot_index(int page, int visible_index);
    int slot_hit(int x, int y) const;
    SaveAction hit_test(int x, int y) const;

    void set_confirm_enabled(bool enabled) { confirm_enabled_ = enabled; }
    bool confirm_enabled() const { return confirm_enabled_; }
    bool confirming() const { return confirm_dialog_.active(); }
    void cancel_confirm() { confirm_dialog_.close(); pending_action_ = {}; choosing_icon_=false; selected_icon_=-1; }
    const ConfirmDialog& confirm_dialog() const { return confirm_dialog_; }
    ConfirmDialog& confirm_dialog() { return confirm_dialog_; }

    void pointer_down(int x, int y);
    // Returns an action only when release hits the same armed control.
    // Page/Mode actions update this controller before returning.
    SaveAction pointer_up(int x, int y);
    SaveAction activate(SaveAction action); // Keyboard/accessibility path.
    SaveAction answer_confirmation(bool yes);
    void cancel_pointer();

    // slots is indexed by absolute zero-based slot (missing entries = empty).
    // Composites the translucent screen over the supplied 1280x720 scene.
    // Hosts must restore that scene before each draw to avoid repeated blends.
    // Missing/wrong-sized input is initialized to an opaque navy background.
    // down also permits headless pressed-state previews without pointer_down.
    void draw(const std::vector<SaveSlot>& slots, int mouse_x, int mouse_y,
              bool down, const dc4scene::FontRenderer& font,
              std::vector<uint8_t>* frame) const;

private:
    void draw_dc2pc(const std::vector<SaveSlot>& slots, int x, int y, bool down,
                   const dc4scene::FontRenderer& font, std::vector<uint8_t>* frame) const;
    int icon_hit(int x, int y) const;
    SaveAction finish_confirmation(bool yes);
    int control_id(int x, int y) const;
    SaveMode mode_ = SaveMode::Save;
    int page_ = 0;
    bool loaded_ = false;
    bool dc2pc_ = false, choosing_icon_ = false;
    int selected_icon_ = -1, pressed_icon_ = -1;
    SaveSlot quick_slot_, latest_slot_;
    bool latest_ = false, automatic_ = false, quick_ = false;
    bool config_ = false, title_ = false, quit_ = false;
    bool confirm_enabled_ = false;
    std::array<bool, kSlotCount> occupied_{};
    SaveAction pressed_;
    int pressed_control_ = 0;
    bool pointer_held_ = false;
    SaveAction pending_action_;
    mutable ConfirmDialog confirm_dialog_;
    dc4scene::Image save_back_, load_back_, chip_, system_goto_, common_chip_;
    dc4::Language language_ = dc4::Language::Japanese;
};

}  // namespace dc4ui
