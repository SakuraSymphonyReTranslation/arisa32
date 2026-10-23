#include "arisa32/system_menu.h"
#include "arisa32/resource.h"
#include "arisa32/mem_safe.h"
#include "arisa32/names.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <random>
#include <sstream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace dc4ui {
namespace {
using Action = SystemMenuAction;
using Type = SystemMenuActionType;
using Page = SystemMenuPage;
using Command = dc4::MesCommand;
using Opcode = dc4::Opcode;
constexpr int W = 1280, H = 720;
constexpr size_t FrameBytes = size_t(W) * H * 4;
int sw(uint16_t word) { return int(int16_t(word)); }
std::string key(std::string name) {
    for (char& c : name) c = c == '\\' ? '/' : char(std::tolower(static_cast<unsigned char>(c)));
    for (const char* ext : {".mes", ".crx", ".pcm", ".mpg"})
        if (name.size() >= 4 && name.compare(name.size() - 4, 4, ext) == 0) name.resize(name.size() - 4);
    return name;
}
bool safe_name(const std::string& name) {
    return !name.empty() && name.size() < 512 && name.front() != '/' &&
           name.find(':') == std::string::npos && name.find("..") == std::string::npos &&
           name.find('\0') == std::string::npos;
}
std::string resolve(const std::string& root, const std::string& name) {
    std::filesystem::path path(root);
    for (const auto& part : std::filesystem::path(name)) {
        std::string found = dc4::find_file_ci(path.string(), part.string());
        path /= found.empty() ? part.string() : found;
    }
    return path.string();
}
bool read_bytes(const std::string& path, std::vector<uint8_t>* out) {
    std::ifstream in(dc4::resolve_asset(path), std::ios::binary);
    if (!in) return false;
    out->assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}
struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
    bool has(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};
struct Chip { std::string image; Rect src, dst; int state = 99, control = -1; };
struct Widget {
    Rect hit;
    std::string tag;
    bool enabled = true;
    int slider_bank = -1, slider_index = 0, slider_count = 0, slider_left = 0, slider_right = 0;
    std::vector<int> slider_stops;
};
struct Caption { int x, y, size, control = -1, state = 99; std::string text; int max_width = 0; };
struct MenuProfile {
    dc4::MesProfileId id;
    int canvas_width, canvas_height;
    int progress_first, progress_end;
    const char* title_entry;
    const char* extras_entry;
    const char* config_script;
    const char* config_entry;
};
constexpr MenuProfile ModernMenu{
    dc4::MesProfileId::Modern, 1280, 720, 100, 150, "main", "ex_main", "config", "menu1"};
constexpr MenuProfile DC2PCMenu{
    dc4::MesProfileId::DC2PC, 800, 600, 0, 23, "", "extra", "option", "init_menu"};
struct Script {
    bool dc2pc = false;
    std::vector<Command> code;
    std::map<std::string, size_t> labels;
};
void blit(const dc4scene::Image& image, const Rect& src, const Rect& dst, std::vector<uint8_t>* frame, int alpha = 255) {
    if (!image.valid() || src.w <= 0 || src.h <= 0 || dst.w <= 0 || dst.h <= 0) return;
    for (int y = std::max(dst.y, 0); y < std::min(dst.y + dst.h, H); ++y) {
        int sy = src.y + int(int64_t(y - dst.y) * src.h / dst.h);
        if (sy < 0 || sy >= image.h) continue;
        for (int x = std::max(dst.x, 0); x < std::min(dst.x + dst.w, W); ++x) {
            int sx = src.x + int(int64_t(x - dst.x) * src.w / dst.w);
            if (sx < 0 || sx >= image.w) continue;
            const uint8_t* s = &image.rgba[(size_t(sy) * image.w + sx) * 4];
            uint8_t* d = &(*frame)[(size_t(y) * W + x) * 4];
            int a = s[3] * alpha / 255;
            for (int c = 0; c < 3; ++c) d[c] = uint8_t((s[c] * a + d[c] * (255 - a)) / 255);
            d[3] = 255;
        }
    }
}
}

struct SystemMenu::Impl {
    bool dc2pc = false;
    const MenuProfile* profile = &ModernMenu;
    std::string title_script = "title_00";
    Rect destination(Rect r) const {
        const int width = profile->canvas_width, height = profile->canvas_height;
        const int scaled_width = width * H / height;
        return {(W-scaled_width)/2 + r.x*H/height, r.y*H/height, r.w*H/height, r.h*H/height};
    }
    int input_x(int x) const { return (x-(W-profile->canvas_width*H/profile->canvas_height)/2)*profile->canvas_height/H; }
    int input_y(int y) const { return y*profile->canvas_height/H; }
    std::string data, last_error, script_name, background;
    std::string replay_menu = "sce_asa";
    int replay_page = 0, replay_route = 0;
    ConfirmDialog confirm;
    std::string pending_tag;
    Page page = Page::Title, config_return = Page::Title;
    bool ready = false, in_game = false, latest = false, quick = false, automatic = false;
    std::map<std::pair<int, int>, int> vars;
    std::set<std::string> seen_cg, seen_music, seen_scene;
    std::map<std::string, Script> scripts;
    mutable std::map<std::string, dc4scene::Image> images;
    std::map<int, std::string> sheets;
    std::vector<std::string> bgm_names, cg_names, scene_names;
    std::vector<Widget> widgets;
    std::vector<Chip> chips;
    std::vector<Caption> captions;
    std::vector<Chip> music_underlay;
    std::array<Chip, 3> list_back;
    std::vector<Rect> list_rects;
    std::map<int, std::array<Chip, 3>> list_titles;
    struct LegacyItem { Chip chip; std::string tag; std::vector<Caption> captions; };
    std::map<int, LegacyItem> legacy_items;
    std::vector<int> legacy_rows;
    std::set<int> playlist;
    int music_top = 0, music_selected = -1;
    int pressed = -1;
    bool held = false, gallery_view = false, movie_menu = false;
    std::vector<int> gallery_variants;
    size_t gallery_variant = 0;
    std::mt19937 random{std::random_device{}()};

    Impl() { defaults(); }
    void defaults() {
        const std::pair<int, int> defaults[] = {
            {5,1},{7,80},{8,80},{9,100},{10,1},{11,80},{12,5},{14,1},{15,1},
            {26,8},{27,8},{28,1},{29,12},{30,0},{180,1},{181,1},{190,1},{194,6},
            {200,1},{203,1},{206,50},{207,100},{208,0},{209,1},{210,1},{213,1},{225,100},
            {245,-1},{246,-1},{253,-1},{254,-1}
        };
        for (auto v : defaults) vars[{99, v.first}] = v.second;
        vars[{100,49}] = 80;
        for (int i = 0; i < 51; ++i) { vars[{94,i}] = 100; vars[{95,i}] = 1; }
        for (int i = 100; i <= 119; ++i) vars[{100, i}] = 1;
    }
    int get(int b, int i) const {
        if (dc2pc && b >= 200 && b < 210) {
            if (i < 0 || i >= 97) return 0;
            uint32_t bits = 0;
            for (int n = 0; n < 4; ++n) bits = (bits << 8) | uint8_t(get(b-200,i+n));
            return bits <= INT32_MAX ? int(bits) : int(int64_t(bits)-0x100000000LL);
        }
        if (dc2pc && b == 100 && i >= 100) return 0;
        if (b == 99 && i == 4) return get(99,1);
        if (b == 99 && i == 20) return latest;
        if (b == 99 && i == 21) return quick;
        if (b == 99 && i == 22) return automatic;
        auto at = vars.find({b,i});
        if (at == vars.end()) {
            if (b == 94) return 100;
            if (b == 95) return 1;
            return 0;
        }
        return at->second;
    }
    void put(int b, int i, int value) {
        if (dc2pc && b >= 200 && b < 210) {
            if (i < 0 || i >= 97) return;
            for (int n=0;n<4;++n) put(b-200,i+n,(uint32_t(value) >> (24-8*n)) & 255);
            return;
        }
        if (b < 0 || b > 100 || i < 0 || i > 4095) return;
        if (dc2pc && b < 10) {
            if (i >= 100) return;
            value=std::clamp(value,0,255);
        }
        if (dc2pc && b == 100 && i >= 100) return;
        vars[{b,i}] = value;
        if ((b==94 || b==95) && i==50) for(int character=0;character<18;++character)vars[{b,character}]=value;
        if (b == 100 && i == 49) vars[{99,11}] = value;
        if (b == 99 && i == 11) vars[{100,49}] = value;
    }
    dc4scene::Image* image(const std::string& raw) const {
        std::string name = key(raw);
        if (!name.empty() && name.front() == '*') {
            int id = -1; try { id = std::stoi(name.substr(1)); } catch (...) {}
            if (id < 0 || size_t(id) >= cg_names.size()) return nullptr;
            name = key(cg_names[size_t(id)]);
        }
        if (!safe_name(name)) return nullptr;
        auto found = images.find(name);
        if (found != images.end()) return &found->second;
        dc4scene::Image result;
        std::vector<uint8_t> bytes;
        dc4::CrxInfo info;
        if (read_bytes(resolve(data + "/GRP", name + ".crx"), &bytes) &&
            dc4::crx_decode(bytes.data(), bytes.size(), &result.rgba, &info)) {
            result.w = info.width; result.h = info.height; result.name = name;
        }
        return &images.emplace(name, std::move(result)).first->second;
    }
    Script* script(const std::string& raw) {
        std::string name = key(raw);
        auto found = scripts.find(name);
        if (found != scripts.end()) return &found->second;
        if (!safe_name(name)) return nullptr;
        dc4::MesFile mes;
        if (!mes.load_file(resolve(data + "/MES", name + ".mes"), &last_error)) return nullptr;
        Script result;
        result.dc2pc = mes.dialect == dc4::MesDialect::ClassicDC2 && mes.version == 0x9969;
        dc4::MesStream stream(mes); Command c;
        while (stream.next(&c) && c.op) {
            c = dc4::mes_engine_command(c);
            if (c.opcode() == Opcode::LABEL) result.labels.emplace(c.str, result.code.size());
            result.code.push_back(c);
        }
        return &scripts.emplace(name, std::move(result)).first->second;
    }
    bool names() {
        std::vector<uint8_t> b;
        std::string path = resolve(data, "GRP/NAMES.DAT");
        if (!read_bytes(path, &b)) path = resolve(data, "DAT/names.dat");
        if (!read_bytes(path, &b)) path = resolve(data, "DAT/NAMES.DAT");
        if (!read_bytes(path, &b) || b.size() < 4) return false;
        dc4::GameNames result;
        if (!dc4::decode_game_names(b,&result)) return false;
        bgm_names=std::move(result.bgm); cg_names=std::move(result.cg); scene_names=std::move(result.scenes);
        return true;
    }
    void progress_vars() {
        for (size_t i = 0; i < cg_names.size(); ++i) {
            std::string k = key(cg_names[i]);
            if (get(97, int(i)) > 0) seen_cg.insert(k);
            if (seen_cg.count(k)) put(97, int(i), 1);
        }
        for (size_t i = 0; i < scene_names.size(); ++i) {
            std::string k = key(scene_names[i]);
            if (get(98, int(i)) > 0) seen_scene.insert(k);
            if (seen_scene.count(k)) put(98, int(i), 1);
        }
        for (size_t i = 0; i < bgm_names.size(); ++i) {
            std::string k = key(bgm_names[i]);
            if (get(96, int(i)) > 0) seen_music.insert(k);
            if (seen_music.count(k)) put(96, int(i), 1);
        }
    }
    void clear_layout() { widgets.clear(); chips.clear(); captions.clear(); list_rects.clear(); list_titles.clear(); legacy_items.clear(); legacy_rows.clear(); }
    int hit(int x,int y) const {
        for (int i = int(widgets.size()) - 1; i >= 0; --i)
            if (widgets[size_t(i)].enabled && widgets[size_t(i)].hit.has(x, y))
                return i;
        return -1;
    }
    struct RunState {
        Script* source;
        size_t pc = 0;
        bool action;
        int transfer_depth;
        int control = -1, list_id = -1;
        bool expect_tag = false, running = true;
        std::string entry;
        Command pending;
        Action output;
        void finish(Action value) {
            output = std::move(value);
            running = false;
        }
    };
    void skip_scope(RunState& context, int scope) {
        while (context.pc < context.source->code.size()) {
            const auto& marker = context.source->code[context.pc++];
            if (marker.opcode() == Opcode::SCOPE && marker.p2 == scope)
                return;
        }
    }
    void menu_stop(RunState& context, const Command&) { context.finish(context.output); }

    void menu_yield_menu(RunState& context, const Command& c) {
        if (context.source->dc2pc) {
            context.finish(context.output);
            return;
        }
    }

    void menu_scope(RunState& context, const Command& c) {
        if (c.p1 == 0)
            skip_scope(context, c.p2);
    }

    void menu_compare(RunState& context, const Command& c) {
        const auto& w = c.words;
        int left = get(w[0], w[1]), right = sw(w[2]), op = w[3];
        if (op & 0x80) {
            right = get(w[2] >> 8, w[2] & 255);
            op &= 127;
        }
        bool yes = false;
        switch (op) {
        case 0:
            yes = left == right;
            break;
        case 1:
            yes = left != right;
            break;
        case 2:
            yes = left > right;
            break;
        case 3:
            yes = left < right;
            break;
        case 4:
            yes = left >= right;
            break;
        case 5:
            yes = left <= right;
            break;
        }
        if (context.pc < context.source->code.size() &&
            context.source->code[context.pc].opcode() == Opcode::SCOPE_MARKER) {
            int scope = context.source->code[context.pc++].p1;
            if (!yes)
                skip_scope(context, scope);
        }
    }

    void menu_assign(RunState& context, const Command& c) {
        const auto& w = c.words;
        int value = sw(w[2]);
        if (w[3] == 1)
            value += get(w[0], w[1]);
        else if (w[3] == 2)
            value = get(w[0], w[1]) - value;
        put(w[0], w[1], value);
    }

    void menu_copy(RunState& context, const Command& c) {
        const auto& w = c.words;
        put(w[0], w[1], get(w[2], w[3]));
    }

    void menu_sum(RunState& context, const Command& c) {
        const auto& w = c.words;
        int sum = 0;
        while (context.pc < context.source->code.size()) {
            const auto& term = context.source->code[context.pc++];
            if (term.opcode() == Opcode::SCOPE_MARKER)
                break;
            sum += get(term.words[0], term.words[1]);
        }
        put(w[0], w[1], sum);
    }

    void menu_add(RunState& context, const Command& c) {
        const auto& w = c.words;
        put(w[0], w[1], get(w[0], w[1]) + get(w[2], w[3]));
    }

    void menu_sheet(RunState& context, const Command& c) {
        if (c.p1 != 0 && c.p1 != 28) {
            sheets[c.p1 + (c.opcode() == Opcode::LOAD_SHEET ? 37 : 0)] = c.str;
            image(c.str);
        }
    }

    void menu_background(RunState& context, const Command& c) {
        if (context.source->dc2pc)
            chips.clear();
        background = c.str;
        image(background);
        if (!c.str.empty() && c.str.front() == '*') {
            gallery_view = true;
            context.finish(context.output);
        }
    }

    void menu_legacy_sheet(RunState& context, const Command& c) {
        if (context.source->dc2pc) {
            sheets[dc4::opcode_byte(c.opcode()) - dc4::opcode_byte(Opcode::LEGACY_SHEET_1) + 6] =
                c.str;
            image(c.str);
        }
    }

    void menu_clear(RunState& context, const Command& c) {
        if (c.p1 == 0) {
            if (context.source->dc2pc) {
                legacy_items.clear(); legacy_rows.clear();
                widgets.clear();
                chips.erase(std::remove_if(chips.begin(), chips.end(),
                                           [](const Chip& chip) { return chip.control >= 0; }),
                            chips.end());
                captions.erase(std::remove_if(captions.begin(), captions.end(),
                                             [](const Caption& caption) { return caption.control >= 0; }),
                               captions.end());
            } else
                clear_layout();
            context.control = -1;
            context.expect_tag = false;
        }
    }

    void menu_end_control(RunState& context, const Command& c) {
        if (c.p1 == 0) {
            context.control = -1;
            context.expect_tag = false;
        }
    }

    void menu_control(RunState& context, const Command& c) {
        const auto& w = c.words;
        widgets.push_back(
            {{sw(w[0]), sw(w[1]), sw(w[2]) - sw(w[0]), sw(w[3]) - sw(w[1])}, "", true});
        context.control = int(widgets.size()) - 1;
        context.expect_tag = true;
        if (context.source->dc2pc &&
            (page==Page::Music || page==Page::Scenes)) {
            // List slots have no immediate jump target; regular buttons do.
            if (context.pc >= context.source->code.size() || context.source->code[context.pc].opcode()!=Opcode::JUMP)
                legacy_rows.push_back(context.control);
        }
    }

    void menu_target(RunState& context, const Command& c) {
        if (context.expect_tag && context.control >= 0) {
            widgets[size_t(context.control)].tag = c.str;
            widgets[size_t(context.control)].enabled = !c.str.empty();
            context.expect_tag = false;
        } else {
            auto at = context.source->labels.find(c.str);
            if (at != context.source->labels.end()) {
                context.pc = at->second;
                context.control = -1;
            }
        }
    }

    void menu_chip_position(RunState& context, const Command& c) { context.pending = c; }

    void menu_chip(RunState& context, const Command& c) {
        const auto& w = c.words;
        if (context.pending.opcode() == Opcode::END)
            return;
        int chip_state = sw(w[3]);
        if(context.source->dc2pc && context.control>=0 && context.pending.opcode()==Opcode::BUTTON_POSITION && chip_state==20) {
            auto& widget=widgets[size_t(context.control)];
            widget.slider_bank=uint16_t(context.pending.words[1]-context.pending.words[3]); widget.slider_index=w[0];
            widget.slider_count=uint16_t(context.pending.words[0]-context.pending.words[2]);
            widget.slider_left=sw(context.pending.words[0]); widget.slider_right=sw(context.pending.words[1]);
            context.pending = Command{};
            return;
        }
        if(context.source->dc2pc && context.control>=0 && context.pending.opcode()==Opcode::BUTTON_POSITION && chip_state==21) {
            auto& widget=widgets[size_t(context.control)];
            widget.slider_stops={widget.slider_left,sw(context.pending.words[0]),sw(context.pending.words[1]),
                sw(uint16_t(context.pending.words[0]-context.pending.words[2])),
                sw(uint16_t(context.pending.words[1]-context.pending.words[3]))};
            context.pending = Command{};
            return;
        }
        if (context.pending.opcode() == Opcode::LEGACY_LIST_ITEM) {
            const auto& p = context.pending.words;
            legacy_items[context.list_id].chip = {sheets[w[2]],
                {sw(p[2]), sw(p[3]), sw(w[0])-sw(p[2]), sw(w[1])-sw(p[3])}, {}, 99, -1};
            context.pending = Command{};
            return;
        }
        Chip chip{sheets[w[2]],
                  {sw(context.pending.words[2]), sw(context.pending.words[3]), w[0], w[1]},
                  {sw(context.pending.words[0]), sw(context.pending.words[1]), w[0], w[1]},
                  chip_state,
                  context.control};
        if (context.source->dc2pc && context.pending.opcode() == Opcode::CHIP_POSITION) {
            chip.src.w -= chip.src.x;
            chip.src.h -= chip.src.y;
            chip.dst.w = chip.src.w;
            chip.dst.h = chip.src.h;
        }
        if (context.pending.opcode() == Opcode::BUTTON_POSITION && context.control >= 0) {
            auto& widget = widgets[size_t(context.control)];
            if (chip_state == 5) {
                widget.slider_bank = context.pending.words[3];
                widget.slider_index = w[0];
                widget.slider_count = context.pending.words[2];
                widget.slider_left = sw(context.pending.words[0]);
                widget.slider_right = sw(context.pending.words[1]);
            } else if (context.source->dc2pc && chip_state>=22 && chip_state<=26)
                chips.push_back(chip);
            else if (chip_state == 6 || chip_state == 8 || chip_state == 10)
                chips.push_back(chip);
            else if (chip_state == 99 || chip_state == 1 || chip_state == 2)
                chips.push_back(chip);
        } else if (context.pending.opcode() == Opcode::CHIP_POSITION) {
            chip.control = -1;
            chips.push_back(chip);
        } else if (context.pending.opcode() == Opcode::LIST_BACKGROUND_POSITION && chip_state >= 0)
            list_back[chip_state == 99 ? 0 : std::min(chip_state, 2)] = chip;
        else if (context.pending.opcode() == Opcode::LIST_TITLE_POSITION && context.list_id >= 0 &&
                 (chip_state == 99 || chip_state == 1 || chip_state == 2))
            list_titles[context.list_id][chip_state == 99 ? 0 : chip_state] = chip;
        context.pending = Command{};
    }

    void menu_list_rect(RunState& context, const Command& c) {
        const auto& w = c.words;
        list_rects.push_back({sw(w[0]), sw(w[1]), sw(w[2]) - sw(w[0]), sw(w[3]) - sw(w[1])});
    }

    void menu_list_id(RunState& context, const Command& c) {
        const auto& w = c.words;
        context.list_id = w[0];
        context.pending = Command{};
    }

    void menu_legacy_list_item(RunState& context, const Command& c) {
        context.list_id = c.words[0];
        context.pending = c;
        context.expect_tag = false;
    }
    void menu_choice(RunState& context, const Command& c) {
        if (context.source->dc2pc && context.list_id >= 0)
            legacy_items[context.list_id].tag = c.str;
    }
    void menu_bgm(RunState& context, const Command& c) {
        if (!context.action || !context.source->dc2pc) return;
        const int id = c.words[0];
        if (id == 255) context.output = {Type::StopMusic};
        else if (id >= 0 && size_t(id) < bgm_names.size()) {
            put(100,26,c.words[1] < 2 ? 0 : 1);
            context.output = {Type::PlayMusic,id,bgm_names[size_t(id)]};
        }
    }

    void menu_caption(RunState& context, const Command& c) {
        const auto& w = c.words;
        if (context.pc < context.source->code.size()) {
            const auto& text = context.source->code[context.pc++];
            if(context.source->dc2pc && c.opcode()==Opcode::CONTROL_CAPTION && context.list_id>=0) {
                legacy_items[context.list_id].captions.push_back(
                    {sw(w[0]),sw(w[1]),16,-1,-1,text.str});
                return;
            }
            captions.push_back({sw(w[0]), sw(w[1]), c.opcode() == Opcode::CAPTION ? int(w[2]) : 20,
                                c.opcode() == Opcode::CONTROL_CAPTION ? context.control : -1,
                                c.opcode() == Opcode::CONTROL_CAPTION ? int(w[3]) : 99, text.str});
        }
    }

    void menu_entry(RunState& context, const Command& c) { context.entry = c.str; }

    void menu_transfer(RunState& context, const Command& c) {
        const std::string dest = key(c.str);
        if (context.source->dc2pc) {
            if (dest.rfind("title_", 0) == 0) {
                page = context.entry == "rextra" ? Page::Extras : Page::Title;
                title_script = dest;
                context.finish(
                    run(dest, context.entry, context.action, context.transfer_depth + 1));
                return;
            }
            if (dest == "option") {
                if (page != Page::Config) config_return = page;
                page = Page::Config;
                context.finish(
                    run(dest, context.entry, context.action, context.transfer_depth + 1));
                return;
            }
            if (dest.rfind("cg_",0)==0 || dest.rfind("sce_",0)==0 || dest=="music") {
                page = dest=="music" ? Page::Music : dest.rfind("cg_",0)==0 ? Page::Gallery : Page::Scenes;
                context.finish(run(dest,context.entry,false,context.transfer_depth+1));
                return;
            }
            if (dest == "load") {
                context.finish({Type::Load});
                return;
            }
            if (dest == "save") {
                context.finish({Type::Save});
                return;
            }
            context.finish({page == Page::Scenes ? Type::StartScene : Type::NewGame,
                            -1, c.str, context.entry});
            return;
        }
        if (dest.rfind("cg_", 0) == 0 || dest.rfind("sce_", 0) == 0) {
            page = dest.rfind("cg_", 0) == 0 ? Page::Gallery : Page::Scenes;
            context.finish(run(dest, context.entry, context.action, context.transfer_depth + 1));
            return;
        }
        if (dest == "main") {
            page = Page::Extras;
            context.finish(run("main", "ex_main", false, context.transfer_depth + 1));
            return;
        }
        if (dest.rfind("dc4_", 0) == 0) {
            context.finish({Type::StartScene, -1, c.str, context.entry});
            return;
        }
        if (dest == "save") {
            context.finish({Type::Save});
            return;
        }
        if (dest == "load") {
            context.finish({Type::Load});
            return;
        }
        context.finish(context.output);
    }

    void menu_voice(RunState& context, const Command& c) {
        if (context.action)
            context.output = {Type::PlayVoice, -1, c.str, {}};
    }

    void menu_playlist(RunState& context, const Command& c) {
        if (c.p1 == 202)
            put(3, 0, playlist.count(music_top + c.p2) ? 1 : 0);
    }

    void dispatch_menu_opcode(Opcode opcode, RunState& context, const Command& c) {
        using Handler = void (Impl::*)(RunState&, const Command&);
        static const auto handlers = [] {
            std::array<Handler, 256> table{};
            const struct {
                Opcode opcode;
                Handler handler;
            } operations[] = {
                {Opcode::END, &Impl::menu_stop},
                {Opcode::RETURN, &Impl::menu_stop},
                {Opcode::EXIT, &Impl::menu_stop},
                {Opcode::WAIT_INPUT, &Impl::menu_yield_menu},
                {Opcode::SCOPE, &Impl::menu_scope},
                {Opcode::COMPARE, &Impl::menu_compare},
                {Opcode::ASSIGN, &Impl::menu_assign},
                {Opcode::COPY, &Impl::menu_copy},
                {Opcode::SUM, &Impl::menu_sum},
                {Opcode::ADD, &Impl::menu_add},
                {Opcode::LOAD_SLOT, &Impl::menu_sheet},
                {Opcode::LOAD_SHEET, &Impl::menu_sheet},
                {Opcode::BACKGROUND, &Impl::menu_background},
                {Opcode::LEGACY_SHEET_1, &Impl::menu_legacy_sheet},
                {Opcode::LEGACY_SHEET_2, &Impl::menu_legacy_sheet},
                {Opcode::LEGACY_SHEET_3, &Impl::menu_legacy_sheet},
                {Opcode::CLEAR_LAYOUT, &Impl::menu_clear},
                {Opcode::END_CONTROL, &Impl::menu_end_control},
                {Opcode::CONTROL, &Impl::menu_control},
                {Opcode::JUMP, &Impl::menu_target},
                {Opcode::BUTTON_POSITION, &Impl::menu_chip_position},
                {Opcode::LIST_BACKGROUND_POSITION, &Impl::menu_chip_position},
                {Opcode::LIST_TITLE_POSITION, &Impl::menu_chip_position},
                {Opcode::CHIP_POSITION, &Impl::menu_chip_position},
                {Opcode::CHIP, &Impl::menu_chip},
                {Opcode::LIST_RECT, &Impl::menu_list_rect},
                {Opcode::LIST_ID, &Impl::menu_list_id},
                {Opcode::LEGACY_LIST_ITEM, &Impl::menu_legacy_list_item},
                {Opcode::CHOICE, &Impl::menu_choice},
                {Opcode::PLAY_BGM_ONCE, &Impl::menu_bgm},
                {Opcode::CONTROL_CAPTION, &Impl::menu_caption},
                {Opcode::CAPTION, &Impl::menu_caption},
                {Opcode::ENTRY, &Impl::menu_entry},
                {Opcode::TRANSFER, &Impl::menu_transfer},
                {Opcode::TRANSFER_ALT, &Impl::menu_transfer},
                {Opcode::VOICE, &Impl::menu_voice},
                {Opcode::SYSTEM_SERVICE, &Impl::menu_playlist},
            };
            for (const auto& operation : operations)
                table[dc4::opcode_byte(operation.opcode)] = operation.handler;
            return table;
        }();
        if (const auto handler = handlers[dc4::opcode_byte(opcode)])
            (this->*handler)(context, c);
    }
    Action run(std::string name, std::string label, bool action, int transfer_depth = 0) {
        if (transfer_depth > 8) {
            last_error = "Menu transfer depth exceeded";
            return {};
        }
        Script* source = script(name);
        if (!source)
            return {};
        script_name = key(name);
        RunState context{source, 0, action, transfer_depth};
        if (!label.empty()) {
            const auto at = source->labels.find(label);
            if (at == source->labels.end()) {
                last_error = "Missing menu label: " + label;
                return {};
            }
            context.pc = at->second;
        }
        std::map<size_t, int> visits;
        for (size_t budget = 0;
             context.running && context.pc < source->code.size() && budget < 30000; ++budget) {
            const Command c = source->code[context.pc++];
            if (c.opcode() == Opcode::LABEL && ++visits[context.pc] > 3) {
                last_error = "Menu loop without event yield: " + c.str;
                break;
            }
            dispatch_menu_opcode(c.opcode(), context, c);
        }
        if (source->dc2pc && script_name == key(name)) legacy_list_rows();
        return context.output;
    }
    void legacy_list_rows() {
        if (legacy_items.empty() || legacy_rows.empty()) return;
        const int top = std::clamp(page==Page::Music?get(3,25):get(206,80),0,
            std::max(0,int(legacy_items.size())-int(legacy_rows.size())));
        if(page==Page::Music)put(3,25,top);else put(206,80,top);
        put(99,246,top);
        auto item=legacy_items.begin();
        std::advance(item,top);
        for (size_t row=0;row<legacy_rows.size();++row) {
            const int control=legacy_rows[row];
            if (item==legacy_items.end()) break;
            auto& widget=widgets[size_t(control)];
            widget.tag=item->second.tag;
            widget.enabled=!widget.tag.empty();
            Chip chip=item->second.chip;
            chip.dst=widget.hit; chip.dst.w=chip.src.w; chip.dst.h=chip.src.h;
            chips.push_back(chip);
            for(auto caption:item->second.captions) {
                caption.max_width=std::max(1,widget.hit.w-caption.x-4);
                caption.x+=widget.hit.x;caption.y+=widget.hit.y;
                caption.control=control;
                captions.push_back(std::move(caption));
            }
            ++item;
        }
    }
    void music_rows() {
        if (page != Page::Music || movie_menu)
            return;
        for (size_t i = 0; i < list_rects.size(); ++i) {
            int id = music_top + int(i);
            if (list_titles.find(id) == list_titles.end())
                continue;
            bool unlocked =
                size_t(id) < bgm_names.size() && seen_music.count(key(bgm_names[size_t(id)]));
            const Rect r = list_rects[i];
            int control = int(widgets.size());
            widgets.push_back({r, "#music:" + std::to_string(id), unlocked});
            for (int s = 0; s < 3; ++s) {
                Chip back = list_back[size_t(s)];
                back.control = control;
                back.state = s == 0 ? 99 : s;
                back.dst.x += r.x;
                back.dst.y += r.y;
                chips.push_back(back);
                if (unlocked) {
                    Chip title = list_titles[id][size_t(s)];
                    title.control = control;
                    title.state = s == 0 ? 99 : s;
                    title.dst.x += r.x;
                    title.dst.y += r.y;
                    chips.push_back(title);
                }
            }
        }
        widgets.push_back({{1072, 216, 40, 384}, "#music:scroll", true, -2, 0, 42, 216, 600});
    }
    void rebuild() {
        if (!ready || gallery_view)
            return;
        held = false;
        pressed = -1;
        put(9, 7, in_game ? 0 : 1);
        put(9, 87, latest ? 0 : 1);
        put(9, 91, 1);
        clear_layout();
        switch (page) {
        case Page::Title:
            // Main selects the authored DC2PC title variant from progression flags.
            run("main", profile->title_entry, false);
            break;
        case Page::Extras:
            run(dc2pc ? title_script : "main", profile->extras_entry, false);
            break;
        case Page::Config:
            if (dc2pc) {
                run(profile->config_script, profile->config_entry, false);
            } else {
                put(3, 97, 2);
                run(script(profile->config_script) ? profile->config_script : "option",
                    profile->config_entry, false);
            }
            break;
        case Page::Gallery:
        case Page::Scenes:
            run(script_name, "menu", false);
            break;
        case Page::Music:
            if (dc2pc) run("music", "menu", false);
            else {
                run("music", movie_menu ? "qmovie" : "menu", false);
                if (!movie_menu)
                    music_rows();
            }
            break;
        }
    }
    Action play_music(int selected, int direction = 1) {
        int count = int(list_titles.size());
        if (!count)
            count = std::min(42, int(bgm_names.size()));
        for (int n = 0; n < count; ++n) {
            int id = (selected + direction * n + count * 2) % count;
            if (size_t(id) < bgm_names.size() && seen_music.count(key(bgm_names[size_t(id)])) &&
                (!get(100, 21) || playlist.count(id))) {
                music_selected = id;
                put(3, 11, 0);
                put(99, 187, id);
                put(99, 189, 0);
                rebuild();
                return {Type::PlayMusic, id, bgm_names[size_t(id)], {}};
            }
        }
        return {};
    }
    Action slider(int id, int x, int y) {
        if (id < 0 || size_t(id) >= widgets.size())
            return {};
        const auto widget = widgets[size_t(id)];
        if (widget.slider_bank == -2) {
            music_top = std::clamp((y - 216) * 36 / 384, 0, 36);
            rebuild();
            return {};
        }
        if (widget.slider_bank < 0 || widget.slider_right <= widget.slider_left) return {};
        if(!widget.slider_stops.empty()) {
            int value=0;
            while(value+1<widget.slider_count && size_t(value+1)<widget.slider_stops.size() &&
                  x>=widget.slider_stops[size_t(value+1)])++value;
            put(widget.slider_bank,widget.slider_index,value);
            return {Type::SettingsChanged};
        }
        int max = widget.slider_count - 1;
        int value = std::clamp(int(std::lround(double(x-widget.slider_left)*max /
                        (widget.slider_right-widget.slider_left))),0,std::max(max,0));
        put(widget.slider_bank,widget.slider_index,value);
        if (widget.slider_bank == 3 && widget.slider_index == 41) put(94,get(100,55),value);
        if (page == Page::Music && widget.slider_bank == 99 && widget.slider_index == 189)
            return {Type::SeekMusic,value};
        return {Type::SettingsChanged};
    }
};

SystemMenu::SystemMenu() : p_(std::make_unique<Impl>()) {}
SystemMenu::~SystemMenu() = default;
SystemMenu::SystemMenu(SystemMenu&&) noexcept = default;
SystemMenu& SystemMenu::operator=(SystemMenu&&) noexcept = default;
bool SystemMenu::load(const std::string& data_dir,std::string* error) {
    p_->data = data_dir; p_->scripts.clear(); p_->images.clear(); p_->last_error.clear(); p_->ready = false;
    p_->confirm.load(data_dir);
    p_->dc2pc = false;
    p_->profile = &ModernMenu;
    auto* main = p_->script("main");
    if (main && main->dc2pc && p_->script("title_00")) {
        p_->dc2pc = true;
        p_->profile = &DC2PCMenu;
        for(const auto& v : {std::pair<int,int>{7,25},{8,25},{9,29},{11,8},{12,1},{13,0},{16,8},{17,0},{20,1}})
            p_->vars[{99,v.first}]=v.second;
        if (!p_->names()) { if(error)*error="Invalid DC2PC DAT/names.dat"; return false; }
        p_->progress_vars();
        p_->ready = true; p_->last_error.clear();
        open(Page::Title);
        if (error) *error = p_->last_error;
        return p_->last_error.empty();
    }
    for (const char* name : {"main","config","music","cg_asa","sce_asa"}) {
        if (std::string(name) == "config" && !p_->script(name)) {
            if (p_->script("option")) continue;
        }
        if (!p_->script(name)) {
            if (std::string(name) == "cg_asa" || std::string(name) == "sce_asa") continue;
            if (error) *error = p_->last_error; return false;
        }
    }
    if (!p_->names()) { p_->last_error = "Invalid or missing version 7 GRP/NAMES.DAT"; if (error) *error = p_->last_error; return false; }
    for (const char* name : {"system/titleback02","system/titlechip01","system/configback01","system/configchip01","system/exchip"}) {
        auto* img = p_->image(name);
        if (!img || !img->valid()) {
            p_->last_error = std::string("Missing menu artwork: ") + name; if(error)*error=p_->last_error; return false;
        }
    }
    p_->ready = true; p_->progress_vars(); open(Page::Title); if(error)error->clear(); return true;
}
bool SystemMenu::loaded() const { return p_->ready; }
SystemMenuPage SystemMenu::page() const { return p_->page; }
void SystemMenu::resume_scenes() {
    cancel_pointer();
    p_->page = Page::Scenes; p_->gallery_view = p_->movie_menu = false;
    p_->script_name = p_->replay_menu;
    if (p_->dc2pc) {p_->put(3,3,p_->replay_route);p_->put(206,80,p_->replay_page);}
    else {p_->put(3,5,p_->replay_route); p_->put(3,6,p_->replay_page);}
    p_->rebuild();
}
void SystemMenu::open(SystemMenuPage page) {
    p_->confirm.close();p_->pending_tag.clear();cancel_pointer();
    if (page == Page::Title) p_->in_game = false;
    if (page == Page::Config && p_->page != Page::Config) p_->config_return = p_->page;
    p_->page = page; p_->gallery_view = false; p_->movie_menu = false;
    if (page == Page::Gallery || page == Page::Scenes) {
        if(p_->dc2pc){p_->put(3,3,0);p_->put(206,80,0);}
        p_->put(3,5,0); p_->put(3,6,0); p_->script_name = page == Page::Gallery ? (p_->dc2pc?"cg_otome":"cg_asa") : (p_->dc2pc?"sce_otome":"sce_asa");
    }
    if (page == Page::Music) { p_->put(3,11,p_->music_selected < 0); p_->put(99,179,1); }
    p_->rebuild();
}
void SystemMenu::set_in_game(bool value) { p_->in_game = value; p_->rebuild(); }
void SystemMenu::set_save_available(bool latest,bool quick,bool automatic) {
    p_->latest=latest; p_->quick=quick; p_->automatic=automatic; p_->rebuild();
}
void SystemMenu::set_config_page(int n) { if(confirming())return;p_->put(100,54,std::clamp(n,0,6)); if(p_->page==Page::Config)p_->rebuild(); }
int SystemMenu::config_page() const { return p_->get(100,54); }
void SystemMenu::pointer_down(int x,int y) { if(confirming()){p_->confirm.pointer_down(x,y);return;}p_->held=true; p_->pressed=p_->hit(p_->input_x(x),p_->input_y(y)); }
Action SystemMenu::pointer_move(int x,int y) {
    if(confirming())return {};
    if (!p_->held || p_->pressed < 0) return {};
    return p_->slider(p_->pressed,p_->input_x(x),p_->input_y(y));
}
Action SystemMenu::pointer_up(int x,int y) {
    if(confirming()){
        const auto result=p_->confirm.pointer_up(x,y);
        if(result==ConfirmResult::None)return {};
        auto tag=std::move(p_->pending_tag);p_->pending_tag.clear();
        return result==ConfirmResult::Yes?activate_confirmed(tag):Action{};
    }
    x=p_->input_x(x); y=p_->input_y(y);
    if (p_->gallery_view) { cancel_pointer(); scroll(1); return {}; }
    int pressed=p_->pressed, hit=p_->hit(x,y); bool held=p_->held;
    if (held && pressed >= 0 && size_t(pressed)<p_->widgets.size() && p_->widgets[size_t(pressed)].slider_bank != -1) {
        auto out=p_->slider(pressed,x,y); cancel_pointer(); return out;
    }
    std::string tag = held && pressed == hit && hit >= 0 ? p_->widgets[size_t(hit)].tag : "";
    cancel_pointer();
    if (!tag.empty()) return activate(tag);
    if (held && p_->page == Page::Config && p_->get(100,54) == 3 && y >= 127 && y <= 195) {
        static const struct { int left, right, id; } kCards[] = {
            {34,126,0},{128,220,1},{222,314,2},{316,408,3},{410,502,4},
            {504,596,5},{598,690,6},{692,784,7},{786,878,8},{880,972,10},{974,1066,11}
        };
        for (const auto& card : kCards) {
            if (x >= card.left && x <= card.right && p_->get(100,56) == card.id) {
                return {Type::PlayVoice, -1, system_voice(18), {}};
            }
        }
    }
    return {};
}
void SystemMenu::cancel_pointer() { p_->held=false; p_->pressed=-1;p_->confirm.cancel_pointer(); }
void SystemMenu::scroll(int delta) {
    if (!delta||confirming()) return;
    if (p_->gallery_view && !p_->gallery_variants.empty()) {
        int n=int(p_->gallery_variants.size());
        p_->gallery_variant=size_t((int(p_->gallery_variant)+delta%n+n)%n);
        p_->background="*"+std::to_string(p_->gallery_variants[p_->gallery_variant]); p_->image(p_->background);
    } else if (p_->page==Page::Music && !p_->movie_menu) {
        if(p_->dc2pc) {
            p_->put(3,25,std::clamp(p_->get(3,25)+delta*2,0,std::max(0,int(p_->legacy_items.size())-int(p_->legacy_rows.size()))));
            p_->rebuild(); return;
        }
        p_->music_top=std::clamp(p_->music_top+delta,0,std::max(0,int(p_->list_titles.size())-6)); p_->rebuild();
    } else if (p_->page==Page::Gallery || p_->page==Page::Scenes) {
        if(p_->dc2pc && p_->page==Page::Scenes) {
            p_->put(206,80,std::clamp(p_->get(206,80)+delta*2,0,std::max(0,int(p_->legacy_items.size())-int(p_->legacy_rows.size()))));
            p_->rebuild();return;
        }
        if(p_->dc2pc && p_->page==Page::Gallery) {
            p_->run(p_->script_name,delta>0?"page++":"page--",true);return;
        }
        const int count=p_->get(3,2)+1;
        p_->put(3,6,(p_->get(3,6)+delta%count+count)%count); p_->rebuild();
    }
}
Action SystemMenu::back() {
    if(confirming())return answer_confirmation(false);
    cancel_pointer();
    if(p_->gallery_view) {p_->gallery_view=false; p_->rebuild(); return {};}
    if(p_->movie_menu) {p_->movie_menu=false; p_->rebuild(); return {};}
    if(p_->page==Page::Config) {
        if(p_->in_game) return {Type::Back};
        open(p_->config_return); return {};
    }
    if(p_->page==Page::Title) return activate("end");
    if(p_->page==Page::Extras) open(Page::Title); else open(Page::Extras);
    return {};
}
Action SystemMenu::activate(const std::string& tag) {
    if(confirming())return {};
    ConfirmKind kind=ConfirmKind::Count;
    if(tag=="prologue"||tag=="sce_start"||tag=="start"||(tag=="dc"&&p_->get(100,100)))kind=ConfirmKind::NewGame;
    else if(tag=="end"||tag=="gomenu05")kind=ConfirmKind::Quit;
    else if((tag=="gomenu04"||tag=="menutitle")&&p_->in_game)kind=ConfirmKind::Title;
    else if(tag=="qload"&&p_->latest)kind=ConfirmKind::Load;
    else if(p_->page==Page::Config&&(tag=="defaultparts"||tag=="init"))kind=ConfirmKind::ResetSettings;
    else if(p_->page==Page::Music&&(tag=="movie01"||tag=="movie02"||tag=="movie03"))kind=ConfirmKind::Movie;
    else if(p_->page==Page::Scenes&&tag.size()>3&&tag.rfind("sce",0)==0&&std::isdigit(static_cast<unsigned char>(tag[3]))){
        if(p_->dc2pc) {
            // Authored list items use global unlock IDs and local action tags.
            bool unlocked=false;
            for(const auto& item:p_->legacy_items)
                if(item.second.tag==tag && p_->get(98,item.first)>0)unlocked=true;
            if(!unlocked)return {};
        } else {
            int id=-1;try{id=std::stoi(tag.substr(3));}catch(...){}
            if(id<0||p_->get(98,id)<=0)return {};
        }
        kind=ConfirmKind::SceneReplay;
    }
    if(kind!=ConfirmKind::Count){
        p_->confirm.set_language(dc4::get_language());
        if(p_->confirm.open(kind)){cancel_pointer();p_->pending_tag=tag;return {};}
    }
    return activate_confirmed(tag);
}
bool SystemMenu::confirming()const{return p_->confirm.active();}
Action SystemMenu::answer_confirmation(bool yes){
    if(!confirming())return {};
    p_->confirm.answer(yes);auto tag=std::move(p_->pending_tag);p_->pending_tag.clear();
    return yes?activate_confirmed(tag):Action{};
}
Action SystemMenu::activate_confirmed(const std::string& tag) {
    if(tag.empty())return {};
    if(p_->dc2pc) {
        if(tag=="load")return {Type::Load};
        if(tag=="end")return {Type::Quit};
        if(tag=="config" || tag==":option"){open(Page::Config);return {};}
        if(tag=="rmain"){open(Page::Title);return {};}
        if(tag=="return" && p_->page==Page::Config)return back();
        if(tag=="extra"){open(Page::Extras);return {};}
        if(p_->page==Page::Config && (tag=="full" || tag=="win")) {
            p_->put(99,1,tag=="full");p_->rebuild();return {Type::SettingsChanged};
        }
        if(p_->page==Page::Gallery && tag.size()>2 && tag.rfind("cg",0)==0 &&
           std::isdigit(static_cast<unsigned char>(tag[2]))) {
            int first=-1;try{first=std::stoi(tag.substr(2));}catch(...){}
            auto* source=p_->script(p_->script_name);
            if(first<0 || !source)return {};
            int end=int(p_->cg_names.size());
            for(size_t i=1;i<source->code.size();++i) {
                const auto& c=source->code[i];
                if(source->code[i-1].opcode()!=Opcode::CONTROL || c.opcode()!=Opcode::JUMP ||
                   c.str.size()<3 || c.str.rfind("cg",0)!=0 || !std::isdigit(static_cast<unsigned char>(c.str[2])))continue;
                int next=std::stoi(c.str.substr(2));if(next>first)end=std::min(end,next);
            }
            p_->gallery_variants.clear();
            for(int i=first;i<end;++i)if(p_->get(97,i)>0)p_->gallery_variants.push_back(i);
            if(p_->gallery_variants.empty())return {};
            p_->gallery_variant=0;p_->background="*"+std::to_string(p_->gallery_variants.front());
            p_->gallery_view=true;p_->image(p_->background);cancel_pointer();return {};
        }
        const auto before=p_->vars;
        if (p_->page==Page::Music && tag.rfind("music",0)==0 && tag.size()>5) {
            int id=-1;try{id=std::stoi(tag.substr(5));}catch(...){}
            if(id<0)return {};
            p_->music_selected=id; p_->put(99,253,id); p_->put(3,5,id);
            return p_->run("music","play_list",true);
        }
        if(p_->page==Page::Music && (tag=="play" || tag=="rplay" || tag=="all_play_mode"))
            p_->put(3,5,std::max(0,p_->music_selected));
        if(p_->page==Page::Music && (tag=="all_play_mode" || tag=="next")) {
            if(tag=="next")p_->music_selected=(std::max(0,p_->music_selected)+1)%std::max(1,int(p_->legacy_items.size()));
            p_->put(3,5,std::max(0,p_->music_selected));p_->put(99,253,p_->get(3,5));
            p_->put(3,6,3);return p_->run("music","play_list",true);
        }
        if(p_->page==Page::Scenes && tag.rfind("sce",0)==0) {
            p_->replay_menu=p_->script_name;
            p_->replay_page=p_->get(206,80);p_->replay_route=p_->get(3,3);
        }
        auto out=p_->run(p_->script_name,tag,true);
        if(p_->page==Page::Config && before!=p_->vars && out.type==Type::None)out.type=Type::SettingsChanged;
        return out;
    }
    if(tag=="return" || tag=="rmain" || tag=="retchk")return back();
    if(tag=="prologue")return {Type::NewGame,-1,"DC4_SEN20181213B",{}};
    if(tag=="sce_start" || tag=="start")return {Type::NewGame,-1,"DC4_KYO20190204A1",{}};
    if(tag=="dc" && p_->get(100,100))return {Type::NewGame,-1,"DC4_DC99990101A",{}};
    if(tag=="load" || tag=="gomenu02")return {Type::Load};
    if(tag=="qload")return p_->latest ? Action{Type::LatestLoad} : Action{};
    if(tag=="gomenu01")return p_->in_game ? Action{Type::Save} : Action{};
    if(tag=="end" || tag=="gomenu05")return {Type::Quit};
    if(tag=="extra") {open(Page::Extras);return {};}
    if(tag=="config" || tag=="gomenu03") {open(Page::Config);return {};}
    if(tag=="gomenu04" || tag=="menutitle") {p_->in_game = false; open(Page::Title); return {Type::Title};}
    if(tag=="cgmode") {open(Page::Gallery);return {};}
    if(tag=="scemode") {open(Page::Scenes);return {};}
    if(tag=="musmode") {open(Page::Music);return {};}
    if(p_->page==Page::Music) {
        if(tag=="selmovie") {p_->music_underlay=p_->chips;p_->movie_menu=true;p_->rebuild();return {};}
        if(tag=="ret_main") {p_->movie_menu=false;p_->rebuild();return {};}
        if(tag=="movie01")return {Type::PlayMovie,0,"DC4_OP1.MPG",{}};
        if(tag=="movie02")return {Type::PlayMovie,1,"DC4_OP2.MPG",{}};
        if(tag=="movie03")return {Type::PlayMovie,2,"DC4_OP3.MPG",{}};
        if(tag.rfind("#music:",0)==0) {
            int id=-1;try{id=std::stoi(tag.substr(7));}catch(...){} return id>=0?p_->play_music(id):Action{};
        }
        if(tag=="play")return p_->play_music(std::max(0,p_->music_selected));
        if(tag=="next")return p_->play_music(p_->get(100,26)==2 ? int(p_->random()%42) : p_->music_selected+1);
        if(tag=="back")return p_->play_music(p_->music_selected-1,-1);
        if(tag=="stop") {p_->put(3,11,1);p_->rebuild();return {Type::StopMusic};}
        if(tag=="allon") {for(int i=0;i<42;++i)p_->playlist.insert(i);p_->put(100,21,1);p_->rebuild();return {};}
        if(tag=="alloff") {p_->playlist.clear();p_->put(100,21,0);p_->rebuild();return {};}
        if(tag.rfind("flag",0)==0) {
            int id=-1;try{id=p_->music_top+std::stoi(tag.substr(4))-1;}catch(...){}
            if(id>=0){if(p_->playlist.count(id))p_->playlist.erase(id);else p_->playlist.insert(id);p_->rebuild();}return {};
        }
    }
    if(p_->page==Page::Gallery && tag.rfind("cg",0)==0 && tag.size()>2 && std::isdigit(static_cast<unsigned char>(tag[2]))) {
        int first=-1;try{first=std::stoi(tag.substr(2));}catch(...){}
        auto* s=p_->script(p_->script_name); if(first<0 || !s)return {};
        int end=int(p_->cg_names.size());
        // A thumbnail starts at cgN; its sum block lists every variant until
        // the next thumbnail's cgN target in the same script.
        for(const auto& c:s->code) if(c.opcode() == Opcode::JUMP && c.str.rfind("cg",0)==0 && c.str.size()>2 && std::isdigit(static_cast<unsigned char>(c.str[2]))) {
            int next=std::stoi(c.str.substr(2));if(next>first)end=std::min(end,next);
        }
        // Restrict to the exact sum preceding this thumbnail, when present.
        for(size_t i=0;i<s->code.size();++i) if(s->code[i].opcode()==Opcode::JUMP && s->code[i].str==tag && i && s->code[i-1].opcode()==Opcode::CONTROL) {
            std::vector<int> indexes;
            size_t begin=i>60?i-60:0;
            for(size_t j=i;j>begin;--j) {const auto& c=s->code[j-1];if(c.opcode() == Opcode::SUM&&c.words[0]==3){break;}if(c.opcode() == Opcode::SUM&&c.words[0]==97)indexes.push_back(c.words[1]);}
            if(!indexes.empty())end=*std::max_element(indexes.begin(),indexes.end())+1;
            break;
        }
        p_->gallery_variants.clear();
        for(int i=first;i<end&&size_t(i)<p_->cg_names.size();++i)if(p_->get(97,i)>0)p_->gallery_variants.push_back(i);
        if(p_->gallery_variants.empty())return {};
        p_->gallery_variant=0;p_->background="*"+std::to_string(p_->gallery_variants[0]);
        p_->gallery_view=true;p_->image(p_->background);cancel_pointer();return {};
    }
    const auto before=p_->vars;
    std::string target=tag;
    if(p_->page==Page::Scenes && tag.rfind("sce",0)==0 && tag.size()>3 && std::isdigit(static_cast<unsigned char>(tag[3]))) {
        int id=std::stoi(tag.substr(3)); if(p_->get(98,id)<=0)return {};
        // Execute the authored sceN label: unlock IDs are global, but that
        // label translates them to the route-local selector used by sce_set.
        p_->replay_menu=p_->script_name;
        p_->replay_page=p_->get(3,6);p_->replay_route=p_->get(3,5);
    }
    if(p_->page==Page::Config && (tag=="defaultparts" || tag=="init")) {
        int pg=p_->get(100,54);p_->defaults();p_->put(100,54,pg);p_->rebuild();return {Type::SettingsChanged};
    }
    if(p_->page==Page::Config && tag=="win_set") {p_->put(99,1,!p_->get(99,1));p_->rebuild();return {Type::SettingsChanged};}
    p_->put(9,91,1);
    auto out=p_->run(p_->script_name,target,true);
    if(p_->page==Page::Music)p_->music_rows();
    if(out.type==Type::StartScene)return out;
    if(p_->page==Page::Config && p_->vars!=before) {
        if(out.type==Type::None)out.type=Type::SettingsChanged;
    }
    return out;
}

void SystemMenu::draw(int mx,int my,bool down,const dc4scene::FontRenderer& font,std::vector<uint8_t>* frame) const {
    if(!frame)return;
    frame->assign(FrameBytes,0);for(size_t i=3;i<frame->size();i+=4)(*frame)[i]=255;
    if(auto* back=p_->image(p_->background)) {
        Rect src{0,0,back->w,back->h};
        Rect dst=p_->destination({0,0,p_->profile->canvas_width,p_->profile->canvas_height});
        if(p_->gallery_view && back->h>H) {dst={0,0,back->w,back->h};}
        blit(*back,src,dst,frame);
    }
    if(p_->gallery_view)return;
    int hover=confirming()?-1:p_->hit(p_->input_x(mx),p_->input_y(my));
    auto state_for=[&](int control) {
        if(control<0)return 99;
        if(control==hover) return down && (!p_->held || p_->pressed==control) ? 2 : 1;
        return 99;
    };
    auto render_chip=[&](Chip chip,bool underlay) {
        if(chip.control>=0) {
            if(underlay) { if(chip.state!=99)return;chip.control=-1; }
        }
        if(chip.control>=0) {
            if(size_t(chip.control)>=p_->widgets.size())return;
            int state=state_for(chip.control);
            const auto& widget=p_->widgets[size_t(chip.control)];
            if(p_->dc2pc && chip.state>=22 && chip.state<=26) {
                if(widget.slider_bank<0 || widget.slider_count<2)return;
                const int val=std::clamp(p_->get(widget.slider_bank,widget.slider_index),0,widget.slider_count-1);
                if(chip.state==22) {
                    chip.src.w=chip.dst.w=std::clamp(widget.slider_left + val*(widget.slider_right-widget.slider_left)/(widget.slider_count-1)-chip.dst.x,0,chip.src.w);
                } else if(chip.state==23) {
                    chip.dst.x+=val*(widget.slider_right-widget.slider_left)/(widget.slider_count-1);
                } else {
                    if(widget.slider_stops.size()<size_t(widget.slider_count+1))return;
                    const bool active=hover==chip.control;
                    if((chip.state==25&&!active)||(chip.state==26&&active))return;
                    const int left=chip.state==24?widget.slider_stops.front():widget.slider_stops[size_t(val)];
                    const int right=chip.state==24?widget.slider_stops.back():widget.slider_stops[size_t(val+1)];
                    chip.src.x+=left-chip.dst.x;chip.dst.x=left;
                    chip.src.w=chip.dst.w=right-left;
                }
            } else if(chip.state==6 || chip.state==8 || chip.state==10) {
                if(widget.slider_bank<0)return;
                bool active=hover==chip.control;
                if((chip.state==6&&active)||(chip.state==8&&!active)||chip.state==10)return;
                int count=std::max(widget.slider_count-1,1);
                int val=std::clamp(p_->get(widget.slider_bank,widget.slider_index),0,count);
                chip.dst.x=widget.slider_left + val*(widget.slider_right-widget.slider_left)/count - chip.dst.w/2;
            } else if(chip.state!=state)return;
        }
        if(auto* img=p_->image(chip.image))blit(*img,chip.src,p_->destination(chip.dst),frame);
    };
    if(p_->movie_menu)for(const auto& chip:p_->music_underlay)render_chip(chip,true);
    for(const auto& chip:p_->chips)render_chip(chip,false);
    for(const auto& c:p_->captions) {
        if(c.control>=0 && c.state>=0 && state_for(c.control)!=c.state)continue;
        std::string line=c.text;
        for(size_t at=line.find("@s");at!=std::string::npos;at=line.find("@s"))line.erase(at,2);
        line = dc4::translate_ui(line);
        std::istringstream lines(line);int y=c.y+c.size;
        while(std::getline(lines,line)) {
            line = dc4::translate_ui(line);
            const auto point=p_->destination({c.x,y,0,0});
            auto text=dc4scene::sjis_to_utf16(line);
            float size=float(c.size)*H/p_->profile->canvas_height;
            if(c.max_width>0) {
                const int width=c.max_width*H/p_->profile->canvas_height;
                while(size>16 && font.text_width(text,size)>width)--size;
                if(font.text_width(text,size)>width) {
                    while(!text.empty() && font.text_width(text+u"...",size)>width)text.pop_back();
                    text+=u"...";
                }
            }
            font.rasterize_line(text,point.x,point.y,frame,W,H,size);
            y+=c.size+4;
        }
    }
    if(p_->page==Page::Music && !p_->movie_menu && !p_->dc2pc) {
        // Scroll thumb supplied by MUSICCHIP (0x74 state -1).
        if(auto* img=p_->image("system/musicchip"))blit(*img,{902,30,24,23},{1072,266+p_->music_top*267/36,24,23},frame);
    }
    if(!p_->ready) font.rasterize_line(dc4::translate_ui(u"Menu data could not be loaded"),50,80,frame,W,H,28);
    p_->confirm.draw(mx,my,down,font,frame);
}

SystemConfig SystemMenu::config() const {
    SystemConfig c;auto v=[&](int i){return p_->get(99,i);};
    c.master_volume=std::clamp(v(225),0,100);c.music_volume=std::clamp(v(7),0,100);
    c.se_volume=std::clamp(v(8),0,100);c.voice_volume=std::clamp(v(9),0,100);c.movie_volume=std::clamp(v(207),0,100);
    c.master_enabled=v(203)!=0;c.music_enabled=v(14)!=0;c.se_enabled=v(15)!=0;c.voice_enabled=v(5)!=0;
    c.movie_enabled=v(200)!=0;c.duck_music_for_voice=v(190)!=0;
    c.text_speed=v(26);c.auto_speed=v(27);c.auto_wait=v(29);c.window_opacity=std::clamp(p_->get(100,49),0,100);
    c.skip_unread=v(30)!=0;c.stop_skip_at_choice=v(210)!=0;c.stop_auto_at_choice=v(209)!=0;
    c.wait_for_voice=v(28)!=0;c.stop_voice_on_advance=v(208)!=0;c.fullscreen=v(1)!=0;c.effects=v(10);c.effect_speed=v(12);
    for(int i=0;i<51;++i){c.character_volume[size_t(i)]=std::clamp(p_->get(94,i),0,100);c.character_muted[size_t(i)]=(p_->get(95,i)==2);}
    if(p_->dc2pc) {
        c.music_volume=std::clamp(v(7)*100/29,0,100);c.se_volume=std::clamp(v(8)*100/29,0,100);
        c.voice_volume=std::clamp(v(9)*100/29,0,100);c.window_opacity=std::clamp(v(11)*100/15,0,100);
        c.text_speed=(v(13)==1 || v(13)==3)?11:std::clamp(v(12)*11/3,0,11);c.effect_speed=v(16);
    }
    return c;
}
std::string SystemMenu::system_voice(int event_id) const {
    if(p_->dc2pc) {
        static const char* actors[]={"otm","yme","mnt","kko","nnk","anz","akn","mya","myk","erk","ais","mhr","wtr","sgn","skr","hrm"};
        int id=p_->get(100,56);if(id<0 || id>=16)id=int(p_->random()%16);
        // Native host events to the authored DC2PC system-voice events.
        int event=0;
        switch(event_id){case 2:event=2;break;case 7:event=11;break;case 8:event=12;break;case 14:event=5;break;case 15:event=6;break;case 16:event=7;break;case 17:event=8;break;}
        if(!event)return {};
        char buffer[64];std::snprintf(buffer,sizeof(buffer),"VOICE/sys/sys_%s_%02d",actors[id],event);
        return buffer;
    }
    int actor_idx = p_->get(100, 56);
    static const char* kActors[] = {
        "ASA", "NIN", "SOR", "HYR", "SII", "MIU", "TIY", "SGN", "KAN", "", "KTR", "CHE"
    };
    if (actor_idx == 50 || actor_idx < 0 || actor_idx >= 12 || strlen(kActors[actor_idx]) == 0) {
        static const int kValid[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11};
        actor_idx = kValid[size_t(p_->random() % 11)];
    }
    const char* actor = kActors[actor_idx];
    char buf[64];
    std::snprintf(buf, sizeof(buf), "VOICE\\SYSTEM\\SYSTEM_%s%03d", actor, event_id);
    return std::string(buf);
}
std::string SystemMenu::title_music() const {
    if(p_->dc2pc) {
        auto* source=p_->script(p_->title_script);
        if(source)for(const auto& c:source->code)
            if(c.opcode()==Opcode::PLAY_BGM_ONCE && c.words[0]<p_->bgm_names.size())
                return key(p_->bgm_names[c.words[0]])+".pcm";
        return {};
    }
    return "music/001.pcm";
}
std::string SystemMenu::scene_ui_script() const { return p_->dc2pc?"start.mes":"win_size.mes"; }
int SystemMenu::variable(int b,int i)const{return p_->get(b,i);}
void SystemMenu::set_variable(int b,int i,int value){p_->put(b,i,value);p_->rebuild();}
void SystemMenu::initialize_scene(dc4scene::SceneRunner& scene,bool replay) const {
    for(int bank:{97,98,100}) {
        const int count=bank==97?int(p_->cg_names.size()):bank==98?int(p_->scene_names.size()):150;
        for(int i=0;i<count;++i)scene.set_variable(uint16_t(bank),uint16_t(i),p_->get(bank,i));
    }
    // scemode.mes sets this before selecting a chapter. Story scripts use it
    // to bypass normal-story setup and return through SCE_END after a replay.
    scene.set_variable(9,7,replay?1:0);
    if(p_->dc2pc)scene.set_variable(3,30,replay?1:0);
}
bool SystemMenu::save_profile(const std::string& path,std::string* error)const {
    const std::string temporary=path+".tmp";
    std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
    if(!out){if(error)*error="Cannot create emulator profile";return false;}
    out<<"DC4ENGINE_PROFILE 1\n";
    for(const auto& pair:p_->vars) {
        int b=pair.first.first,i=pair.first.second;
        if(b==93||b==94||b==95||b==99||b==100)out<<"v "<<b<<' '<<i<<' '<<pair.second<<'\n';
    }
    for(const auto& s:p_->seen_cg)out<<"c "<<std::quoted(s)<<'\n';
    for(const auto& s:p_->seen_music)out<<"m "<<std::quoted(s)<<'\n';
    for(const auto& s:p_->seen_scene)out<<"s "<<std::quoted(s)<<'\n';
    for(int id:p_->playlist)out<<"p "<<id<<'\n';
    out.flush();if(!out){if(error)*error="Cannot write emulator profile";return false;}out.close();
    bool replaced=false;
#ifdef _WIN32
    replaced=MoveFileExA(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    std::error_code ec;std::filesystem::rename(temporary,path,ec);replaced=!ec;
#endif
    if(!replaced){if(error)*error="Cannot replace emulator profile";return false;}if(error)error->clear();return true;
}
bool SystemMenu::load_system_dat(const std::string& path, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) *error = "Cannot open system.dat";
        return false;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (data.size() < 0x4B80) {
        if (error) *error = "system.dat too small";
        return false;
    }
    // Bank 100: offset 0x00D8, size 150
    if (data.size() >= 0x00D8 + 150) {
        for (int i = 0; i < 150; ++i) {
            uint8_t val = data[0x00D8 + i];
            if (val > p_->vars[{100, i}]) p_->vars[{100, i}] = val;
        }
    }
    // Bank 96 (BGM): offset 0x0180, size 400
    if (data.size() >= 0x0180 + 400) {
        for (int i = 0; i < 400; ++i) {
            uint8_t val = data[0x0180 + i];
            if (val > p_->vars[{96, i}]) p_->vars[{96, i}] = val;
            if (val > 0 && size_t(i) < p_->bgm_names.size()) {
                p_->seen_music.insert(key(p_->bgm_names[i]));
            }
        }
    }
    // Bank 97 (CG gallery): offset 0x0310, size 1200
    if (data.size() >= 0x0310 + 1200) {
        for (int i = 0; i < 1200; ++i) {
            uint8_t val = data[0x0310 + i];
            if (val > p_->vars[{97, i}]) p_->vars[{97, i}] = val;
            if (val > 0 && size_t(i) < p_->cg_names.size()) {
                p_->seen_cg.insert(key(p_->cg_names[i]));
            }
        }
    }
    // Bank 98 (Scenario mode): offset 0x07C0, size 650
    if (data.size() >= 0x07C0 + 650) {
        for (int i = 0; i < 650; ++i) {
            uint8_t val = data[0x07C0 + i];
            if (val > p_->vars[{98, i}]) p_->vars[{98, i}] = val;
            if (val > 0 && size_t(i) < p_->scene_names.size()) {
                p_->seen_scene.insert(key(p_->scene_names[i]));
            }
        }
    }
    // Bank 93: offset 0x44E8, 20 32-bit ints
    if (data.size() >= 0x44E8 + 20 * 4) {
        for (int i = 0; i < 20; ++i) {
            uint32_t val = uint32_t(data[0x44E8 + i*4]) |
                          (uint32_t(data[0x44E8 + i*4 + 1]) << 8) |
                          (uint32_t(data[0x44E8 + i*4 + 2]) << 16) |
                          (uint32_t(data[0x44E8 + i*4 + 3]) << 24);
            p_->vars[{93, i}] = int32_t(val);
        }
    }
    for (int i = 0; i < 51; ++i) {
        if (p_->vars.find({94, i}) == p_->vars.end()) p_->vars[{94, i}] = 100;
        if (p_->vars.find({95, i}) == p_->vars.end() || p_->vars[{95, i}] == 0) p_->vars[{95, i}] = 1;
    }
    p_->progress_vars();
    p_->rebuild();
    if (error) error->clear();
    return true;
}

bool SystemMenu::load_profile(const std::string& path,std::string* error) {
    std::error_code size_error;
    const auto bytes=std::filesystem::file_size(path,size_error);
    if(!size_error && bytes>32*1024*1024){if(error)*error="Emulator profile exceeds byte limit";return false;}
    std::ifstream in(path,std::ios::binary);if(!in){if(error)*error="Cannot open emulator profile";return false;}
    std::string header;std::getline(in,header);
    if(header!="DC4ENGINE_PROFILE 1"){
        in.close();
        if (load_system_dat(path, error)) return true;
        if(error)*error="Unsupported emulator profile";
        return false;
    }
    auto vars=p_->vars;std::set<std::string> cg=p_->seen_cg,music=p_->seen_music,scene=p_->seen_scene;std::set<int> playlist=p_->playlist;
    std::string line;size_t lines=0;
    while(std::getline(in,line)) {
        if(++lines>120000||line.size()>4096){if(error)*error="Emulator profile exceeds limits";return false;}
        std::istringstream row(line);char type=0;row>>type;
        if(type=='v'){int b,i,value;if(!(row>>b>>i>>value)||b<93||b>100||i<0||i>4095||std::abs(int64_t(value))>1000000){if(error)*error="Invalid profile variable";return false;}vars[{b,i}]=value;}
        else if(type=='c'||type=='m'||type=='s'){std::string s;if(!(row>>std::quoted(s))||!safe_name(s)){if(error)*error="Invalid profile asset";return false;}(type=='c'?cg:type=='m'?music:scene).insert(key(s));}
        else if(type=='p'){int id;if(!(row>>id)||id<0||id>1000){if(error)*error="Invalid playlist entry";return false;}playlist.insert(id);}
        else if(type){if(error)*error="Unknown profile record";return false;}
    }
    for (int i = 0; i < 51; ++i) {
        if (vars.find({94, i}) == vars.end()) vars[{94, i}] = 100;
        if (vars.find({95, i}) == vars.end() || vars[{95, i}] == 0) vars[{95, i}] = 1;
    }
    for (int i = 100; !p_->dc2pc && i <= 119; ++i) {
        if (vars.find({100, i}) == vars.end() || vars[{100, i}] == 0) vars[{100, i}] = 1;
    }
    p_->vars=std::move(vars);p_->seen_cg=std::move(cg);p_->seen_music=std::move(music);p_->seen_scene=std::move(scene);p_->playlist=std::move(playlist);
    p_->progress_vars();p_->rebuild();if(error)error->clear();return true;
}
void SystemMenu::mark_cg_seen(const std::string& asset){if(safe_name(key(asset))&&p_->seen_cg.insert(key(asset)).second)p_->progress_vars();}
void SystemMenu::mark_music_seen(const std::string& asset){if(safe_name(key(asset))&&p_->seen_music.insert(key(asset)).second)p_->progress_vars();}
void SystemMenu::mark_scene_seen(const std::string& script,const std::string& label){
    bool changed=false;
    if(safe_name(key(script))){changed=p_->seen_scene.insert(key(script)).second;if(!label.empty())changed=p_->seen_scene.insert(key(script)+"#"+label).second||changed;}
    if(changed)p_->progress_vars();
}
bool SystemMenu::cg_seen(const std::string& asset)const{return p_->seen_cg.count(key(asset))!=0;}
bool SystemMenu::music_seen(const std::string& asset)const{return p_->seen_music.count(key(asset))!=0;}
bool SystemMenu::scene_seen(const std::string& script,const std::string& label)const{return p_->seen_scene.count(key(script)+(label.empty()?"":"#"+label))!=0;}
void SystemMenu::observe(const Command& command,const std::string& script){
    const auto c=dc4::mes_engine_command(command);
    const auto opcode=c.opcode();
    if(opcode == Opcode::BACKGROUND){std::string name=key(c.str);if(std::find_if(p_->cg_names.begin(),p_->cg_names.end(),[&](const std::string& s){return key(s)==name;})!=p_->cg_names.end())mark_cg_seen(name);}
    if((opcode == Opcode::PLAY_BGM_ONCE || opcode == Opcode::PLAY_BGM) && c.words[0]<p_->bgm_names.size())mark_music_seen(p_->bgm_names[c.words[0]]);
    if(!script.empty()&&((c.kind==dc4::CmdKind::Text && opcode!=Opcode::SPEAKER) || opcode==Opcode::CHECKPOINT))mark_scene_seen(script);
}
bool SystemMenu::sync_progress(const dc4scene::SceneRunner& scene){
    bool changed=false;
    for(size_t i=0;i<p_->cg_names.size();++i)if(scene.variable(97,uint16_t(i))>0)changed=p_->seen_cg.insert(key(p_->cg_names[i])).second||changed;
    for(size_t i=0;i<p_->scene_names.size();++i)if(scene.variable(98,uint16_t(i))>0)changed=p_->seen_scene.insert(key(p_->scene_names[i])).second||changed;
    for(int i=p_->profile->progress_first;i<p_->profile->progress_end;++i){int value=scene.variable(100,uint16_t(i));if(value>p_->get(100,i)){p_->put(100,i,value);changed=true;}}
    if(changed)p_->progress_vars();return changed;
}
void SystemMenu::set_music_position(int percent){p_->put(99,189,std::clamp(percent,0,100));}
std::vector<SystemMenu::Control> SystemMenu::controls()const {
    std::vector<Control> result;for(const auto& w:p_->widgets){const auto r=p_->destination(w.hit);result.push_back({r.x,r.y,r.w,r.h,w.tag,w.enabled});}return result;
}
std::string SystemMenu::error()const{return p_->last_error;}
} // namespace dc4ui
