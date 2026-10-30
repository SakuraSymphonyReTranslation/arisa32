#include "arisa32/save_screen.h"
#include "arisa32/resource.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <utility>

#include "arisa32/crm.h"
#include "arisa32/crx.h"

namespace dc4ui {
namespace {

constexpr int kWidth = dc4scene::kScreenW;
constexpr int kHeight = dc4scene::kScreenH;
constexpr size_t kFrameBytes = size_t(kWidth) * kHeight * 4;
constexpr int kPageSourceX[] = {2, 44, 86, 128, 170, 211, 253, 295, 337, 378};
constexpr int kPageWidth[] = {41, 41, 41, 41, 40, 41, 41, 41, 40, 39};

bool contains(int x, int y, int left, int top, int right, int bottom) {
    return x >= left && x < right && y >= top && y < bottom;
}

bool same_action(SaveAction a, SaveAction b) {
    return a.type == b.type && a.index == b.index;
}

bool valid_image(const dc4scene::Image& image) {
    return image.w > 0 && image.h > 0 &&
           size_t(image.w) <= std::numeric_limits<size_t>::max() / 4 / size_t(image.h) &&
           image.rgba.size() >= size_t(image.w) * size_t(image.h) * 4;
}

bool read_crx(const std::string& data_dir, const char* name,
              int min_width, int min_height, dc4scene::Image* image,
              std::string* error) {
    const std::string dir = data_dir + "/GRP/SYSTEM";
    const std::string resolved = dc4::find_file_ci(dir, name);
    std::ifstream in(dc4::resolve_asset(dir + "/" + (resolved.empty() ? name : resolved)), std::ios::binary);
    if (!in) {
        if (error) *error = std::string("Cannot open SYSTEM/") + name;
        return false;
    }
    const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in),
                                     std::istreambuf_iterator<char>()};
    dc4::CrxInfo info;
    std::string decode_error;
    if (!dc4::crx_decode(bytes.data(), bytes.size(), &image->rgba, &info, &decode_error)) {
        if (error) *error = std::string("Cannot decode SYSTEM/") + name + ": " + decode_error;
        return false;
    }
    image->name = name;
    image->w = info.width;
    image->h = info.height;
    if (!valid_image(*image) || image->w < min_width || image->h < min_height) {
        if (error) *error = std::string("Invalid Save/Load artwork dimensions: SYSTEM/") + name;
        return false;
    }
    return true;
}

void blend(const uint8_t* source, uint8_t* dest) {
    const int alpha = source[3];
    for (int c = 0; c < 3; ++c)
        dest[c] = uint8_t((int(source[c]) * alpha + int(dest[c]) * (255 - alpha)) / 255);
    dest[3] = 255;
}

void blit(const dc4scene::Image& image, int sx, int sy, int width, int height,
          int dx, int dy, std::vector<uint8_t>* frame) {
    if (!valid_image(image)) return;
    const int left = std::max({0, -sx, -dx});
    const int top = std::max({0, -sy, -dy});
    const int right = std::min({width, image.w - sx, kWidth - dx});
    const int bottom = std::min({height, image.h - sy, kHeight - dy});
    if (left >= right || top >= bottom) return;
    for (int y = top; y < bottom; ++y) {
        const uint8_t* source = image.rgba.data() + (size_t(sy + y) * image.w + sx + left) * 4;
        uint8_t* dest = frame->data() + (size_t(dy + y) * kWidth + dx + left) * 4;
        for (int x = left; x < right; ++x, source += 4, dest += 4)
            blend(source, dest);
    }
}

void fill_rect(int x, int y, int width, int height, uint32_t rgb,
               std::vector<uint8_t>* frame) {
    for (int py = std::max(0, y); py < std::min(kHeight, y + height); ++py) {
        for (int px = std::max(0, x); px < std::min(kWidth, x + width); ++px) {
            uint8_t* dest = frame->data() + (size_t(py) * kWidth + px) * 4;
            dest[0] = uint8_t(rgb >> 16);
            dest[1] = uint8_t(rgb >> 8);
            dest[2] = uint8_t(rgb);
            dest[3] = 255;
        }
    }
}

int char_width(char16_t ch, int size) {
    return ch < 0x100 || (ch >= 0xFF61 && ch <= 0xFF9F) ? size / 2 : size;
}

int occupied_excerpt_size(const std::u16string& text) {
    // Native @i/@y/@r selects 18/16/12px by the chapter's SJIS length,
    // including the two-byte prefix. Host excerpts use the same thresholds.
    size_t bytes = 2;
    for (char16_t ch : text) {
        bytes += ch < 0x80 || (ch >= 0xFF61 && ch <= 0xFF9F) ? 1 : 2;
        if (bytes > 34) return 12;
    }
    return bytes > 30 ? 16 : 18;
}

// Native row text has a 270px clipping area. Flatten host excerpts into its
// single chapter line, shortening long dialogue without touching save data.
std::u16string clipped_text(const std::u16string& input, int width, int size) {
    std::u16string text;
    int used = 0;
    for (char16_t ch : input) {
        if (ch == u'\r' || ch == u'\n' || ch == u'\t') ch = u' ';
        const int advance = char_width(ch, size);
        if (used + advance > width) {
            while (!text.empty() && used + size > width) {
                used -= char_width(text.back(), size);
                text.pop_back();
            }
            text.push_back(u'\u2026');
            break;
        }
        text.push_back(ch);
        used += advance;
    }
    return text;
}

void draw_text(const dc4scene::FontRenderer& font, const std::u16string& text,
               int x, int top, int width, int size, std::vector<uint8_t>* frame,
               uint32_t rgb = 0xFFFFFF, uint32_t outline = 0) {
    if (text.empty() || width <= 0) return;
    dc4scene::Image pixels;
    pixels.w = width + 4;
    pixels.h = size + 12;
    pixels.rgba.assign(size_t(pixels.w) * pixels.h * 4, 0);
    font.rasterize_line(clipped_text(text, width, size), 2, size + 2,
                        &pixels.rgba, pixels.w, pixels.h, float(size), rgb, false, outline);
    blit(pixels, 0, 0, pixels.w, pixels.h, x - 2, top - 2, frame);
}

void draw_thumbnail(const dc4scene::Image& image, int dx, int dy,
                    std::vector<uint8_t>* frame) {
    if (!valid_image(image)) return;
    // The original saves 128x72 pixels but draws the crop (0,0)-(125,71).
    // Normalize foreign host preview sizes to 128x72 before the same crop.
    for (int y = 0; y < 71; ++y) {
        const int sy = int(int64_t(y) * image.h / 72);
        for (int x = 0; x < 125; ++x) {
            const int sx = int(int64_t(x) * image.w / 128);
            blend(image.rgba.data() + (size_t(sy) * image.w + sx) * 4,
                  frame->data() + (size_t(dy + y) * kWidth + dx + x) * 4);
        }
    }
}

std::u16string page_label(int page) {
    const std::string text = std::to_string(page + 1);
    return std::u16string(text.begin(), text.end());
}

std::u16string dc2dm_timestamp(const std::u16string& text) {
    if (text.size() >= 16 && (text[7] == u'-' || text[7] == u'/') && text[13] == u':')
        return text.substr(0, 4) + u" " + text.substr(5, 2) + u"/" + text.substr(8, 2) +
            u" " + text.substr(11, 5);
    return text;
}

}  // namespace

bool SaveScreen::load(const std::string& data_dir, std::string* error) {
    cancel_confirm();confirm_enabled_=true;
    confirm_dialog_.load(data_dir);confirm_dialog_.set_language(language_);
    loaded_ = false;
    save_back_ = {};
    load_back_ = {};
    save_back_later_ = {};
    load_back_later_ = {};
    chip_ = {};
    system_goto_ = {};
    common_chip_ = {};
    if (error) error->clear();
    cancel_pointer();
    dc4scene::Image save_back, load_back, chip, system_goto;
    dc4::MesFile start;
    dc2pc_ = start.load_file(data_dir+"/MES/start.mes") &&
        dc4::mes_opcode_profile(start.dialect,start.version).id==dc4::MesProfileId::DC2PC;
    dc4::MesFile main;
    dc2dm_ = main.load_file(data_dir + "/MES/main.mes") &&
        main.dialect == dc4::MesDialect::ClassicDM && main.version == 0x009D72;
    page_ = std::clamp(page_, 0, page_count() - 1);
    choosing_icon_=false;selected_icon_=-1;
    if(dc2pc_) {
        if(!read_crx(data_dir,"save_BG00.CRX",800,600,&save_back,error) ||
           !read_crx(data_dir,"load_BG00.CRX",800,600,&load_back,error) ||
           !read_crx(data_dir,"saveandload_chip00.CRX",576,288,&chip,error) ||
           !read_crx(data_dir,"charactericom.CRX",896,480,&system_goto,error) ||
           !read_crx(data_dir,"common_load_chip00.CRX",456,272,&common_chip_,error))return false;
        save_back_=std::move(save_back);load_back_=std::move(load_back);
        chip_=std::move(chip);system_goto_=std::move(system_goto);loaded_=true;return true;
    }
    if (dc2dm_) {
        dc4scene::Image save_back_later, load_back_later;
        if (!read_crx(data_dir, "SAVE_BACK.CRX", 1024, 576, &save_back, error) ||
            !read_crx(data_dir, "LOAD_BACK.CRX", 1024, 576, &load_back, error) ||
            !read_crx(data_dir, "SAVE_BACK02.CRX", 1024, 576, &save_back_later, error) ||
            !read_crx(data_dir, "LOAD_BACK02.CRX", 1024, 576, &load_back_later, error) ||
            !read_crx(data_dir, "SL_CHIP.CRX", 1340, 568, &chip, error) ||
            !read_crx(data_dir, "GOTO.CRX", 598, 150, &system_goto, error)) return false;
        save_back_ = std::move(save_back);
        load_back_ = std::move(load_back);
        save_back_later_ = std::move(save_back_later);
        load_back_later_ = std::move(load_back_later);
        chip_ = std::move(chip);
        system_goto_ = std::move(system_goto);
        loaded_ = true;
        return true;
    }
    if (!read_crx(data_dir, "SAVEBACK.CRX", 1280, 720, &save_back, error) ||
        !read_crx(data_dir, "LOADBACK.CRX", 1280, 720, &load_back, error) ||
        !read_crx(data_dir, "SLCHIP.CRX", 447, 627, &chip, error) ||
        !read_crx(data_dir, "SYSTEMGOTO.CRX", 530, 132, &system_goto, error)) return false;
    save_back_ = std::move(save_back);
    load_back_ = std::move(load_back);
    chip_ = std::move(chip);
    system_goto_ = std::move(system_goto);
    loaded_ = true;
    return true;
}

void SaveScreen::set_mode(SaveMode mode) {
    cancel_confirm();
    mode_ = mode == SaveMode::Load ? SaveMode::Load : SaveMode::Save;
    cancel_pointer();
}

void SaveScreen::set_page(int page) {
    cancel_confirm();
    page_ = std::clamp(page, 0, page_count() - 1);
    cancel_pointer();
}

void SaveScreen::change_page(int delta) {
    const int count = page_count();
    set_page((page_ + delta % count + count) % count);
}

void SaveScreen::set_shortcuts(bool latest, bool automatic, bool quick) {
    latest_ = latest;
    automatic_ = automatic;
    quick_ = quick;
    cancel_pointer();
}

void SaveScreen::set_navigation(bool config, bool title, bool quit) {
    config_ = config;
    title_ = title;
    quit_ = quit;
    cancel_pointer();
}

void SaveScreen::set_slot_occupied(int index, bool occupied) {
    if (index < 0 || index >= kSlotCount) return;
    occupied_[size_t(index)] = occupied;
    if (page_ >= page_count()) set_page(page_count() - 1);
    if (!occupied && mode_ == SaveMode::Load && pressed_.type == SaveActionType::Slot &&
        pressed_.index == index) cancel_pointer();
}

int SaveScreen::slot_index(int page, int visible_index) {
    if (page < 0 || page >= kPageCount || visible_index < 0 || visible_index >= kSlotsPerPage)
        return -1;
    return page * kSlotsPerPage + visible_index;
}

int SaveScreen::dc2dm_slot_index(int visible) const {
    // The original first page reserves Quick/Auto/New before manual saves.
    // An extra page preserves existing host saves beyond the native 97 slots.
    const int index = page_ * kSlotsPerPage + visible - 3;
    return index >= 0 && index < kSlotCount ? index : -1;
}

SaveAction SaveScreen::visible_slot_action(int visible) const {
    if (visible < 0 || visible >= kSlotsPerPage) return {};
    if ((dc2dm_ && page_ == 0 && visible < 3) || (dc2pc_ && visible < 2)) {
        if (visible == 0) {
            if (mode_ == SaveMode::Save) return {SaveActionType::QuickSave};
            return quick_ ? SaveAction{SaveActionType::QuickLoad} : SaveAction{};
        }
        if (mode_ != SaveMode::Load) return {};
        if (dc2dm_ && visible == 1)
            return automatic_ ? SaveAction{SaveActionType::AutoLoad} : SaveAction{};
        return latest_ ? SaveAction{SaveActionType::LatestLoad} : SaveAction{};
    }
    if (dc2pc_ && visible >= 5) return {};
    const int index = dc2dm_ ? dc2dm_slot_index(visible)
        : dc2pc_ ? page_ * 3 + visible - 2 : slot_index(page_, visible);
    if (index < 0 || (mode_ == SaveMode::Load && !occupied_[size_t(index)])) return {};
    return {SaveActionType::Slot, index};
}

int SaveScreen::slot_hit(int x, int y) const {
    if (dc2dm_) {
        if (!contains(x, y, 0, 0, kWidth, kHeight)) return -1;
        const int nx = x * 4 / 5, ny = y * 4 / 5;
        for (int visible = 0; visible < kSlotsPerPage; ++visible) {
            const int left = visible < 5 ? 248 : 584;
            const int top = 80 + visible % 5 * 84;
            if (contains(nx, ny, left, top, left + 336, top + 84))
                return dc2dm_slot_index(visible);
        }
        return -1;
    }
    if(dc2pc_) {
        const int nx=int(std::floor((x-160)/1.2)),ny=int(std::floor(y/1.2));
        for(int row=0;row<3;++row)if(contains(nx,ny,444,267+row*85,732,339+row*85))return page_*3+row;
        return -1;
    }
    for (int visible = 0; visible < kSlotsPerPage; ++visible) {
        const int left = visible < 5 ? 152 : 617;
        const int top = 95 + (visible % 5) * 117;
        if (contains(x, y, left, top, left + 450, top + 106))
            return slot_index(page_, visible);
    }
    return -1;
}

SaveAction SaveScreen::hit_test(int x, int y) const {
    if (!contains(x, y, 0, 0, kWidth, kHeight)) return {};
    const int slot = slot_hit(x, y);
    if (slot >= 0 && (mode_ == SaveMode::Save || occupied_[size_t(slot)]))
        return {SaveActionType::Slot, slot};
    if (dc2dm_) {
        const int nx = x * 4 / 5, ny = y * 4 / 5;
        if (page_ == 0 && contains(nx, ny, 248, 80, 584, 164) &&
            (mode_ == SaveMode::Save || quick_))
            return {mode_ == SaveMode::Save ? SaveActionType::QuickSave : SaveActionType::QuickLoad};
        if (page_ == 0 && mode_ == SaveMode::Load) {
            if (automatic_ && contains(nx, ny, 248, 164, 584, 248)) return {SaveActionType::AutoLoad};
            if (latest_ && contains(nx, ny, 248, 248, 584, 332)) return {SaveActionType::LatestLoad};
        }
        if (contains(nx, ny, 52, 460, 128, 504))
            return {SaveActionType::Page, (page_ + page_count() - 1) % page_count()};
        if (contains(nx, ny, 160, 460, 236, 504))
            return {SaveActionType::Page, (page_ + 1) % page_count()};
        if (contains(nx, ny, 924, 448, 1008, 516)) return {SaveActionType::Back};
        if (mode_ != SaveMode::Save && contains(nx, ny, 352, 522, 472, 572))
            return {SaveActionType::Mode, 0};
        if (mode_ != SaveMode::Load && contains(nx, ny, 474, 522, 594, 572))
            return {SaveActionType::Mode, 1};
        if (config_ && contains(nx, ny, 596, 522, 716, 572)) return {SaveActionType::Config};
        if (title_ && contains(nx, ny, 718, 522, 838, 572)) return {SaveActionType::Title};
        if (quit_ && contains(nx, ny, 840, 522, 960, 572)) return {SaveActionType::Quit};
        return {};
    }
    if(dc2pc_) {
        const int nx=int(std::floor((x-160)/1.2)),ny=int(std::floor(y/1.2));
        if(contains(nx,ny,171,437,243,477))return {SaveActionType::Page,(page_+9)%10};
        if(contains(nx,ny,280,437,352,477))return {SaveActionType::Page,(page_+1)%10};
        if(contains(nx,ny,233,531,345,563))return {SaveActionType::Back};
        if(contains(nx,ny,444,97,732,169) && (mode_==SaveMode::Save || quick_))return {mode_==SaveMode::Save?SaveActionType::QuickSave:SaveActionType::QuickLoad};
        if(contains(nx,ny,444,182,732,254) && mode_==SaveMode::Load && latest_)return {SaveActionType::LatestLoad};
        return {};
    }
    if (contains(x, y, 772, 28, 798, 56))
        return {SaveActionType::Page, (page_ + kPageCount - 1) % kPageCount};
    if (contains(x, y, 1234, 28, 1260, 56))
        return {SaveActionType::Page, (page_ + 1) % kPageCount};
    for (int page = 0; page < kPageCount; ++page) {
        if (page != page_ && contains(x, y, 802 + 43 * page, 20, 845 + 43 * page, 60))
            return {SaveActionType::Page, page};
    }
    if (contains(x, y, 1160, 628, 1286, 684)) return {SaveActionType::Back, -1};
    if (mode_ == SaveMode::Load) {
        if (automatic_ && contains(x, y, 348, 12, 467, 63)) return {SaveActionType::AutoLoad, -1};
        if (latest_ && contains(x, y, 476, 12, 595, 63)) return {SaveActionType::LatestLoad, -1};
        if (quick_ && contains(x, y, 604, 12, 723, 63)) return {SaveActionType::QuickLoad, -1};
    }
    if (mode_ != SaveMode::Save && contains(x, y, 724, 686, 830, 719))
        return {SaveActionType::Mode, 0};
    if (mode_ != SaveMode::Load && contains(x, y, 833, 686, 939, 719))
        return {SaveActionType::Mode, 1};
    if (config_ && contains(x, y, 942, 686, 1048, 719)) return {SaveActionType::Config, -1};
    if (title_ && contains(x, y, 1051, 686, 1157, 719)) return {SaveActionType::Title, -1};
    if (quit_ && contains(x, y, 1160, 686, 1266, 719)) return {SaveActionType::Quit, -1};
    return {};
}

int SaveScreen::control_id(int x, int y) const {
    const SaveAction action = hit_test(x, y);
    if (action.type == SaveActionType::None) return 0;
    if (action.type == SaveActionType::Slot) return 1 + action.index;
    if (action.type == SaveActionType::Page) {
        if (dc2dm_) return x < 160 ? 101 : 102;
        if (contains(x, y, 772, 28, 798, 56)) return 101;
        if (contains(x, y, 1234, 28, 1260, 56)) return 102;
        return 110 + action.index;
    }
    if (action.type == SaveActionType::Mode) return 130 + action.index;
    return 140 + int(action.type);
}

void SaveScreen::pointer_down(int x, int y) {
    if (confirm_dialog_.active()) {
        confirm_dialog_.pointer_down(x, y);
        return;
    }
    if(choosing_icon_){pressed_icon_=icon_hit(x,y);pressed_=hit_test(x,y);pointer_held_=true;return;}
    pressed_ = hit_test(x, y);
    pressed_control_ = control_id(x, y);
    pointer_held_ = true;
}

SaveAction SaveScreen::pointer_up(int x, int y) {
    if (confirm_dialog_.active()) {
        const ConfirmResult res = confirm_dialog_.pointer_up(x, y);
        if (res == ConfirmResult::Yes) {
            return finish_confirmation(true);
        } else if (res == ConfirmResult::No) {
            return finish_confirmation(false);
        }
        return {};
    }
    if(choosing_icon_) {
        const int icon=icon_hit(x,y);
        const bool accepted=pointer_held_ && icon>=0 && icon==pressed_icon_;
        const bool back=pointer_held_ && pressed_.type==SaveActionType::Back && hit_test(x,y).type==SaveActionType::Back;
        pointer_held_=false;pressed_icon_=-1;pressed_={};
        if(accepted) {
            selected_icon_=icon;
            const bool occupied=pending_action_.type==SaveActionType::QuickSave?quick_:occupied_[size_t(pending_action_.index)];
            if(!confirm_enabled_ || !confirm_dialog_.open(occupied?ConfirmKind::Overwrite:ConfirmKind::Save))
                return finish_confirmation(true);
        } else if(back) {
            cancel_confirm();
        }
        return {};
    }
    const SaveAction released = hit_test(x, y);
    const bool accepted = pointer_held_ && pressed_.type != SaveActionType::None &&
                          pressed_control_ == control_id(x, y) &&
                          same_action(pressed_, released);
    cancel_pointer();
    if (!accepted) return {};
    return activate(released);
}

SaveAction SaveScreen::answer_confirmation(bool yes) {
    if(!confirming())return {};
    confirm_dialog_.answer(yes);return finish_confirmation(yes);
}
SaveAction SaveScreen::finish_confirmation(bool yes) {
    auto action=pending_action_;pending_action_={};
    choosing_icon_=false;
    if(!yes){selected_icon_=-1;return {};}
    if(action.type==SaveActionType::Slot&&mode_==SaveMode::Load&&
       (action.index<0||action.index>=kSlotCount||!occupied_[size_t(action.index)]))return {};
    if((action.type==SaveActionType::QuickLoad&&!quick_)||
       (action.type==SaveActionType::LatestLoad&&!latest_)||
       (action.type==SaveActionType::AutoLoad&&!automatic_))return {};
    return action;
}

SaveAction SaveScreen::activate(SaveAction released) {
    if(confirming() || choosing_icon_)return {};
    if(released.type==SaveActionType::Slot &&
       (released.index<0||released.index>=kSlotCount||
        (mode_==SaveMode::Load&&!occupied_[size_t(released.index)])))return {};
    if((released.type==SaveActionType::QuickLoad&&!quick_)||
       (released.type==SaveActionType::LatestLoad&&!latest_)||
       (released.type==SaveActionType::AutoLoad&&!automatic_))return {};
    if (released.type == SaveActionType::Page) set_page(released.index);
    if (released.type == SaveActionType::Mode)
        set_mode(released.index == 1 ? SaveMode::Load : SaveMode::Save);
    if(dc2pc_ && mode_==SaveMode::Save && (released.type==SaveActionType::Slot || released.type==SaveActionType::QuickSave)) {
        cancel_pointer();pending_action_=released;choosing_icon_=true;selected_icon_=-1;return {};
    }

    ConfirmKind kind=ConfirmKind::Count;
    if(released.type==SaveActionType::Slot)kind=mode_==SaveMode::Load?ConfirmKind::Load:
        occupied_[size_t(released.index)]?ConfirmKind::Overwrite:ConfirmKind::Save;
    else if(released.type==SaveActionType::QuickLoad)kind=ConfirmKind::QuickLoad;
    else if(released.type==SaveActionType::QuickSave)kind=quick_?ConfirmKind::Overwrite:ConfirmKind::Save;
    else if(released.type==SaveActionType::LatestLoad||released.type==SaveActionType::AutoLoad)kind=ConfirmKind::Load;
    else if(released.type==SaveActionType::Title)kind=ConfirmKind::Title;
    else if(released.type==SaveActionType::Quit)kind=ConfirmKind::Quit;
    if(confirm_enabled_&&kind!=ConfirmKind::Count){
        confirm_dialog_.set_language(language_);
        if(confirm_dialog_.open(kind)){pending_action_=released;return {};}
    }
    return released;
}

void SaveScreen::cancel_pointer() {
    pressed_ = {};
    pressed_control_ = 0;
    pointer_held_ = false;
    pressed_icon_=-1;
    if (!confirm_dialog_.active() && !choosing_icon_) pending_action_ = {};
    confirm_dialog_.cancel_pointer();
}

void SaveScreen::draw(const std::vector<SaveSlot>& slots, int mouse_x, int mouse_y,
                      bool down, const dc4scene::FontRenderer& font,
                      std::vector<uint8_t>* frame) const {
    if (!frame) return;
    if (dc2dm_) { draw_dc2dm(slots, mouse_x, mouse_y, down, font, frame); return; }
    if(dc2pc_ && loaded_){draw_dc2pc(slots,mouse_x,mouse_y,down,font,frame);return;}
    if (frame->size() != kFrameBytes || !loaded_) {
        frame->resize(kFrameBytes);
        fill_rect(0, 0, kWidth, kHeight, 0x042344, frame);
    }
    if (loaded_) {
        const auto& background = mode_ == SaveMode::Save ? save_back_ : load_back_;
        blit(background, 0, 0, kWidth, kHeight, 0, 0, frame);
    } else {
        draw_text(font, dc4::translate_ui(mode_ == SaveMode::Save ? u"SAVE" : u"LOAD"), 12, 10, 300, 40, frame);
        draw_text(font, dc4::translate_ui(u"Save/Load artwork unavailable"), 22, 686, 660, 18, frame);
    }

    const SaveAction hovered = hit_test(mouse_x, mouse_y);
    const auto state = [&](SaveAction action) {
        if (action.type == SaveActionType::None || !same_action(action, hovered)) return 0;
        return down && (!pointer_held_ || same_action(pressed_, action)) ? 2 : 1;
    };

    const bool previous_hover = contains(mouse_x, mouse_y, 772, 28, 798, 56);
    const bool next_hover = contains(mouse_x, mouse_y, 1234, 28, 1260, 56);
    if (loaded_) {
        blit(chip_, previous_hover ? 28 : 2, 294, previous_hover ? 24 : 25, 30,
             previous_hover ? 773 : 772, 28, frame);
        blit(chip_, next_hover ? 78 : 53, 294, next_hover ? 24 : 25, 30, 1234, 28, frame);
    } else {
        draw_text(font, u"<", 773, 28, 24, 24, frame);
        draw_text(font, u">", 1234, 28, 24, 24, frame);
    }
    for (int page = 0; page < kPageCount; ++page) {
        const int x = 802 + page * 43;
        const bool selected = page == page_ || contains(mouse_x, mouse_y, x, 20, x + 43, 60);
        if (loaded_)
            blit(chip_, kPageSourceX[page], selected ? 246 : 206, kPageWidth[page],
                 selected ? 43 : 40, x, 20, frame);
        else {
            fill_rect(x, 20, 41, 40, selected ? 0xE68D12 : 0x12628D, frame);
            draw_text(font, page_label(page), x + 9, 24, 30, 26, frame);
        }
    }

    for (int visible = 0; visible < kSlotsPerPage; ++visible) {
        const int index = slot_index(page_, visible);
        const SaveSlot* slot = size_t(index) < slots.size() ? &slots[size_t(index)] : nullptr;
        const bool occupied = slot && slot->occupied;
        const bool active = (mode_ == SaveMode::Save || occupied) &&
                            state({SaveActionType::Slot, index}) != 0;
        const int x = visible < 5 ? 152 : 617;
        const int y = 95 + (visible % 5) * 117;
        if (loaded_) {
            // Native 0x40B6F0 uses the same (2,21)-(449,126) pink row for all
            // three states. SLCHIP is 447px wide, so clip its last two pixels.
            blit(chip_, 2, 21, 447, 105, x, y, frame);
            blit(chip_, active ? 148 : 16, 127, 129, 75, x + 14, y + 14, frame);
        } else {
            fill_rect(x, y, 450, 106, active ? 0x965F87 : 0x664361, frame);
            fill_rect(x + 14, y + 14, 129, 75, active ? 0x00D5EC : 0xC955A2, frame);
            fill_rect(x + 16, y + 16, 125, 71, 0x183045, frame);
        }
        if (occupied) draw_thumbnail(slot->thumbnail, x + 16, y + 16, frame);
        // sub_436280 doubles the native logical text coordinates (80,6)
        // and (80,25). Excerpts stand in for the original chapter-name table.
        const std::u16string timestamp = occupied && !slot->timestamp.empty()
            ? slot->timestamp : u"---- --/-- --:--:--";
        const std::u16string raw_excerpt = occupied
            ? (slot->excerpt.empty() ? u"Saved data" : slot->excerpt) : u"No Data";
        const std::u16string excerpt = dc4::translate_ui(raw_excerpt);
        // Native GDI text origin includes nine pixels before this font
        // renderer's baseline. Verified against the supplied LOAD capture.
        draw_text(font, timestamp, x + 160, y + 12 + 9, 270, occupied ? 18 : 20, frame);
        draw_text(font, excerpt, x + 160, y + 50 + 9, 270,
                  occupied ? occupied_excerpt_size(excerpt) : 20, frame);
    }

    const SaveActionType shortcut_types[] = {SaveActionType::AutoLoad,
        SaveActionType::LatestLoad, SaveActionType::QuickLoad};
    const bool shortcut_enabled[] = {automatic_, latest_, quick_};
    const char16_t* shortcut_labels[] = {u"AUTO", u"NEW", u"QLOAD"};
    for (int i = 0; i < 3; ++i) {
        const int x = 348 + i * 128;
        const bool enabled = mode_ == SaveMode::Load && shortcut_enabled[i];
        const int current = enabled ? state({shortcut_types[i], -1}) : 0;
        if (loaded_) {
            if (enabled)
                blit(chip_, 2 + i * 128, current ? 379 : 325, i == 2 ? 120 : 119, 53, x, 12, frame);
            else
                blit(chip_, i * 120, 576, 119, 51, x, 12, frame);
        } else {
            fill_rect(x, 12, 119, 51, !enabled ? 0x606975 : current ? 0xCA870F : 0x9A507E, frame);
            draw_text(font, dc4::translate_ui(std::u16string(shortcut_labels[i])), x + 12, 24, 100, 24, frame);
        }
    }

    const bool back_hover = state({SaveActionType::Back, -1}) != 0;
    if (loaded_)
        blit(chip_, back_hover ? 103 : 0, 431, 102, 38, 1160, 628, frame);
    else {
        fill_rect(1160, 628, 102, 38, back_hover ? 0xBE850A : 0x12628D, frame);
        draw_text(font, dc4::translate_ui(u"Back"), 1172, 631, 90, 24, frame);
    }

    const SaveAction footer_actions[] = {{SaveActionType::Mode, 0}, {SaveActionType::Mode, 1},
        {SaveActionType::Config, -1}, {SaveActionType::Title, -1}, {SaveActionType::Quit, -1}};
    const bool footer_enabled[] = {true, true, config_, title_, quit_};
    const char16_t* footer_labels[] = {u"SAVE", u"LOAD", u"CONFIG", u"TITLE", u"END"};
    for (int i = 0; i < 5; ++i) {
        const int x = 724 + i * 109;
        const bool selected = i == (mode_ == SaveMode::Save ? 0 : 1);
        const int current = state(footer_actions[i]);
        const int sy = !footer_enabled[i] ? 99 : selected ? 66 : current * 33;
        if (loaded_)
            blit(system_goto_, i * 106, sy, 106, 33, x, 686, frame);
        else {
            fill_rect(x, 686, 106, 33, !footer_enabled[i] ? 0x606975 :
                      selected || current == 2 ? 0x00AFCB : current ? 0x133E94 : 0x2767BF, frame);
            draw_text(font, dc4::translate_ui(std::u16string(footer_labels[i])), x + 10, 689, 96, 22, frame);
        }
    }

    if (confirm_dialog_.active()) {
        confirm_dialog_.draw(mouse_x, mouse_y, down, font, frame);
    }
}

void SaveScreen::draw_dc2dm(const std::vector<SaveSlot>& slots, int mx, int my, bool down,
                          const dc4scene::FontRenderer& font, std::vector<uint8_t>* frame) const {
    if (frame->size() != kFrameBytes || !loaded_) {
        frame->resize(kFrameBytes);
        fill_rect(0, 0, kWidth, kHeight, 0xFFEAF0, frame);
    }
    const auto scale = [](int value) { return (value * 5 + 2) / 4; };
    const auto native_blit = [&](const dc4scene::Image& image, int sx, int sy,
                                 int width, int height, int x, int y) {
        if (!valid_image(image)) return;
        const int dx = scale(x), dy = scale(y);
        for (int py = 0; py < scale(height); ++py) for (int px = 0; px < scale(width); ++px) {
            const int ix = sx + px * 4 / 5, iy = sy + py * 4 / 5;
            if (ix < image.w && iy < image.h && dx + px < kWidth && dy + py < kHeight)
                blend(image.rgba.data() + (size_t(iy) * image.w + ix) * 4,
                      frame->data() + (size_t(dy + py) * kWidth + dx + px) * 4);
        }
    };
    const auto label = [&](const std::u16string& text, int x, int y, int width, int size) {
        draw_text(font, text, scale(x), scale(y), scale(width), scale(size), frame, 0xAE244F, 0xFFFFFF);
    };
    if (loaded_) {
        const auto& background = mode_ == SaveMode::Save
            ? (page_ == 0 ? save_back_ : save_back_later_)
            : (page_ == 0 ? load_back_ : load_back_later_);
        native_blit(background, 0, 0, 1024, 576, 0, 0);
    } else {
        label(dc4::translate_ui(mode_ == SaveMode::Save ? u"SAVE" : u"LOAD"), 32, 24, 220, 32);
        label(dc4::translate_ui(u"Save/Load artwork unavailable"), 24, 546, 312, 14);
    }
    const SaveAction hovered = confirming() ? SaveAction{} : hit_test(mx, my);
    const auto state = [&](SaveAction action) {
        if (action.type == SaveActionType::None || !same_action(action, hovered)) return 0;
        return down && (!pointer_held_ || (same_action(pressed_, action) &&
            pressed_control_ == control_id(mx, my))) ? 2 : 1;
    };
    for (int visible = 0; visible < kSlotsPerPage; ++visible) {
        const bool special = page_ == 0 && visible < 3;
        const int index = dc2dm_slot_index(visible);
        if (!special && index < 0) continue;
        const int x = visible < 5 ? 248 : 584, y = 80 + visible % 5 * 84;
        const SaveSlot* slot = special ? (visible == 0 ? &quick_slot_ : visible == 2 ? &latest_slot_ : nullptr)
            : size_t(index) < slots.size() ? &slots[size_t(index)] : nullptr;
        const bool occupied = slot && slot->occupied;
        const SaveAction action = !special ? SaveAction{SaveActionType::Slot, index}
            : visible == 0 ? SaveAction{mode_ == SaveMode::Save ? SaveActionType::QuickSave : SaveActionType::QuickLoad}
            : SaveAction{visible == 1 ? SaveActionType::AutoLoad : SaveActionType::LatestLoad};
        const bool row_hover = !confirming() && contains(mx, my, scale(x), scale(y),
            scale(x + 336), scale(y + 84)) && (!special || visible == 0 || mode_ == SaveMode::Load);
        const int current = row_hover ? std::max(1, state(action)) : 0;
        if (loaded_) {
            if (special && visible == 0)
                native_blit(chip_, 1008, current * 84, 332, 84, x, y);
            else {
                const int row = special ? visible - 1 : 2;
                native_blit(chip_, current * 336, row * 84, 336, 84, x, y);
            }
        } else {
            fill_rect(scale(x + 8), scale(y + 6), scale(320), scale(72),
                      current ? 0xFFB2C8 : 0xFFFFFF, frame);
        }
        if (!special) {
            const auto number = (index < 9 ? "0" : "") + std::to_string(index + 1);
            const int left = x + 68 - int(number.size()) * 8;
            if (loaded_) {
                for (size_t digit = 0; digit < number.size(); ++digit)
                    native_blit(chip_, 336 + (number[digit] - '0') * 16, 504, 16, 28,
                                left + int(digit) * 16, y + 25);
            } else label(std::u16string(number.begin(), number.end()), left, y + 27, 48, 24);
        } else if (!loaded_) {
            const char16_t* names[] = {u"Quick", u"Auto", u"New"};
            label(names[visible], x + 24, y + 27, 78, 18);
        }
        draw_text(font, occupied && !slot->timestamp.empty() ? dc2dm_timestamp(slot->timestamp)
                  : u"---- --/-- --:--", scale(x + 112), scale(y + 9), scale(206), scale(14), frame);
        if (occupied) {
            auto excerpt = slot->excerpt.empty() ? dc4::translate_ui(u"Saved data") : slot->excerpt;
            if (language_ != dc4::Language::Japanese)
                std::replace(excerpt.begin(), excerpt.end(), u'_', u' ');
            draw_text(font, excerpt, scale(x + 112), scale(y + 41), scale(206), scale(14), frame);
        }
    }
    const int previous = state({SaveActionType::Page, (page_ + page_count() - 1) % page_count()});
    const int next = state({SaveActionType::Page, (page_ + 1) % page_count()});
    const int back = state({SaveActionType::Back});
    if (loaded_) {
        native_blit(chip_, 0, 364 + previous * 44, 76, 44, 52, 460);
        native_blit(chip_, 76, 364 + next * 44, 76, 44, 160, 460);
        native_blit(chip_, 152, 364 + back * 68, 84, 68, 924, 448);
        native_blit(chip_, ((page_ + 1) / 10) * 32, 252, 32, 64, 92, 336);
        native_blit(chip_, ((page_ + 1) % 10) * 32, 252, 32, 64, 124, 336);
        native_blit(chip_, (page_count() / 10) * 24, 316, 24, 48, 136, 416);
        native_blit(chip_, (page_count() % 10) * 24, 316, 24, 48, 160, 416);
    } else {
        label(u"<", 68, 466, 44, 28);
        label(u">", 176, 466, 44, 28);
        label(dc4::translate_ui(u"Back"), 934, 470, 72, 20);
        const auto pages = std::to_string(page_ + 1) + "/" + std::to_string(page_count());
        label(std::u16string(pages.begin(), pages.end()), 96, 356, 120, 28);
    }
    const SaveAction actions[] = {{SaveActionType::Mode, 0}, {SaveActionType::Mode, 1},
        {SaveActionType::Config}, {SaveActionType::Title}, {SaveActionType::Quit}};
    const bool enabled[] = {true, true, config_, title_, quit_};
    const char16_t* names[] = {u"SAVE", u"LOAD", u"CONFIG", u"TITLE", u"END"};
    for (int i = 0; i < 5; ++i) {
        const int x = 352 + i * 122;
        const bool selected = i == (mode_ == SaveMode::Save ? 0 : 1);
        const int current = !enabled[i] || selected ? 2 : state(actions[i]);
        if (loaded_)
            native_blit(system_goto_, i * 120, current * 50, i == 4 ? 118 : 119, 50, x, 522);
        else {
            fill_rect(scale(x), scale(522), scale(119), scale(50),
                      !enabled[i] ? 0xE7CAD3 : current ? 0xFFB2C8 : 0xFFFFFF, frame);
            label(dc4::translate_ui(names[i]), x + 10, 537, 102, 18);
        }
    }
    if (confirming()) confirm_dialog_.draw(mx, my, down, font, frame);
}

int SaveScreen::icon_hit(int x,int y) const {
    const int nx=int(std::floor((x-160)/1.2)),ny=int(std::floor(y/1.2));
    for(int i=0;i<16;++i)if(contains(nx,ny,80+(i%4)*64,191+(i/4)*64,136+(i%4)*64,247+(i/4)*64))return i;
    return -1;
}

void SaveScreen::draw_dc2pc(const std::vector<SaveSlot>& slots,int mx,int my,bool down,
                          const dc4scene::FontRenderer& font,std::vector<uint8_t>* frame) const {
    frame->resize(kFrameBytes);fill_rect(0,0,kWidth,kHeight,0,frame);
    auto native_blit=[&](const dc4scene::Image& image,int sx,int sy,int w,int h,int x,int y) {
        const int dx=160+int(std::lround(x*1.2)),dy=int(std::lround(y*1.2));
        const int dw=int(std::lround(w*1.2)),dh=int(std::lround(h*1.2));
        for(int py=0;py<dh;++py)for(int px=0;px<dw;++px) {
            const int ix=sx+std::min(w-1,int(px/1.2)),iy=sy+std::min(h-1,int(py/1.2));
            if(ix>=0 && iy>=0 && ix<image.w && iy<image.h && dx+px>=0 && dx+px<kWidth && dy+py>=0 && dy+py<kHeight)
                blend(image.rgba.data()+(size_t(iy)*image.w+ix)*4,frame->data()+(size_t(dy+py)*kWidth+dx+px)*4);
        }
    };
    native_blit(mode_==SaveMode::Save?save_back_:load_back_,0,0,800,600,0,0);
    const auto hover=confirming()||choosing_icon_?SaveAction{}:hit_test(mx,my);
    const auto active=[&](SaveAction a){return same_action(hover,a) && a.type!=SaveActionType::None;};
    for(int row=0;row<5;++row) {
        const int index=page_*3+row-2,y=97+row*85;
        const SaveSlot* slot=row==0?&quick_slot_:row==1?&latest_slot_:
            size_t(index)<slots.size()?&slots[size_t(index)]:nullptr;
        const bool occupied=slot && slot->occupied;
        SaveAction action=row==0?SaveAction{mode_==SaveMode::Save?SaveActionType::QuickSave:SaveActionType::QuickLoad}:
            row==1?SaveAction{SaveActionType::LatestLoad}:SaveAction{SaveActionType::Slot,index};
        const int sy=row==0?0:row==1?72:144;
        native_blit(chip_,active(action)?288:0,sy,288,72,444,y);
        if(occupied && slot->character_icon>=0 && slot->character_icon<16)
            native_blit(system_goto_,slot->character_icon*56,312,56,56,454,y+8);
        const std::u16string title=occupied?(slot->excerpt.empty()?dc4::translate_ui(u"Saved data"):slot->excerpt):dc4::translate_ui(u"No Data");
        draw_text(font,title,160+int(518*1.2),int((y+7)*1.2),250,19,frame,0xFFFFFF);
        if(occupied)draw_text(font,slot->timestamp,160+int(518*1.2),int((y+47)*1.2),250,17,frame,0xFFFFFF);
    }
    const int nx=int(std::floor((mx-160)/1.2)),ny=int(std::floor(my/1.2));
    const bool prev=!confirming()&&!choosing_icon_&&contains(nx,ny,171,437,243,477);
    const bool next=!confirming()&&!choosing_icon_&&contains(nx,ny,280,437,352,477);
    const bool back=!confirming()&&contains(nx,ny,233,531,345,563);
    native_blit(common_chip_,112,prev?(down?80:40):0,72,40,171,437);
    native_blit(common_chip_,184,next?(down?80:40):0,72,40,280,437);
    native_blit(common_chip_,0,back?(down?64:32):0,112,32,233,531);
    // The authored page label uses a 16x24 digit atlas.
    const std::string label=std::to_string(page_+1);
    for(size_t i=0;i<label.size();++i)native_blit(common_chip_,(label[i]-'0')*16,120,16,24,235+int(i)*16,422);
    native_blit(common_chip_,160,120,16,24,251+int(label.size()-1)*16,422);
    native_blit(common_chip_,16,120,16,24,267+int(label.size()-1)*16,422);
    native_blit(common_chip_,0,120,16,24,283+int(label.size()-1)*16,422);
    if(choosing_icon_) {
        native_blit(system_goto_,0,0,312,312,48,160);
        const int hovered=confirming()?-1:icon_hit(mx,my);
        for(int i=0;i<16;++i)native_blit(system_goto_,i*56,312+(i==hovered?(down?112:56):0),56,56,80+(i%4)*64,191+(i/4)*64);
    }
    if(confirming())confirm_dialog_.draw(mx,my,down,font,frame);
}

}  // namespace dc4ui
