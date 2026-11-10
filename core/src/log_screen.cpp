#include "arisa32/log_screen.h"
#include "arisa32/resource.h"

#include <algorithm>
#include <cstdio>
#include <cctype>
#include <fstream>

#include "arisa32/crm.h"
#include "arisa32/crx.h"
#include "arisa32/messtream.h"

namespace dc4ui {

namespace {

bool read_crx(const std::string& data_dir, const char* rel, dc4scene::Image* out) {
    const std::string dir = data_dir + "/GRP/SYSTEM";
    const std::string resolved = dc4::find_file_ci(dir, rel);
    std::vector<uint8_t> buf;
    std::ifstream in(dc4::resolve_asset(resolved.empty() ? dir + "/" + rel : dir + "/" + resolved),
                     std::ios::binary);
    if (!in) return false;
    buf.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    dc4::CrxInfo info;
    std::vector<uint8_t> rgba;
    std::string err;
    if (!dc4::crx_decode(buf.data(), buf.size(), &rgba, &info, &err)) return false;
    out->name = rel;
    out->w = info.width;
    out->h = info.height;
    out->rgba = std::move(rgba);
    return true;
}

// SJIS speaker names in face-grid order (BKLOGCHIP cells, 8 columns from
// y=370). Cell 29 is the blank/silhouette face used for speakers not on the
// grid (unknown characters, nicknames); 30/31 are empty.
// Names verified against 0x6A speaker commands across all 512 MES scripts.
const struct { const char* sjis; int id; } kFaceNames[] = {
    {"\x88\xea\x93\x6f", 0},             // 0 一登
    {"\x97\x4c\x97\xa2\x90\xb2", 1},     // 1 有里栖
    {"\x97\x4c\x97\xa2\x8d\xe7", 2},     // 2 有里咲
    {"\x93\xf1\x94\x54", 3},             // 3 二乃
    {"\xe6\x7d\x8e\x71", 4},             // 4 諳子
    {"\x82\xd0\x82\xe6\x82\xe8", 5},     // 5 ひより
    {"\x8e\x8d\x96\xbc", 6},             // 6 詩名
    {"\x96\xa2\x89\x48", 7},             // 7 未羽
    {"\x82\xbf\x82\xe6\x8e\x71", 8},     // 8 ちよ子 (0x82E6, not 0x82E5)
    {"\x43\x48\x4f\x43\x4f", 9},         // 9 CHOCO (ASCII in scripts)
    {"\x90\x99\x95\xc0", 10},            // 10 杉並
    {"\x8a\x90\x95\xfb", 11},            // 11 叶方
    {"\x8c\xb3", 12},                    // 12 元
    {"\x90\xf2", 13},                    // 13 泉
    {"\x82\x6a\x82\x8f\x82\x94\x82\x8f\x82\x71\x82\x68", 14},  // 14 ＫｏｔｏＲＩ (fullwidth)
    {"\x8b\xd5\x97\xa2", 15},            // 15 琴里
    {"\x83\x41\x83\x8a\x83\x58", 16},    // 16 アリス
    {"\x82\xbf\x82\xa5\x82\xb5", 17},    // 17 ちぇし (script form)
    {"\x88\xb2", 18},                    // 18 梓
    {"\x83\x89\x83\x89", 19},            // 19 ララ
    {"\x83\x8a\x83\x8a", 20},            // 20 リリ
    {"\x83\x8b\x83\x8b", 21},            // 21 ルル
    {"\x8e\xd1\x8a\xf3", 22},            // 22 紗希
    {"\x8f\xae\x8f\x83", 23},            // 23 尚純 (script form)
    {"\x97\x4a\x8e\x9f", 24},            // 24 憂次
    {"\x8f\x5c\x98\x61\x8e\x71", 25},    // 25 十和子
    {"\x8e\x4f\x94\xfc", 26},            // 26 三美
    {"\x90\xb3\x89\xc0", 27},            // 27 正佳
    {"\x89\xc2\x94\xbf", 28},            // 28 可帆
};
// Extra script speaker names mapped onto grid cells (verified off the art
// where possible; 29 is the silhouette placeholder for anything unknown).
const struct { const char* sjis; int id; } kFaceAliases[] = {
    {"\x94\xfc\x8f\x74", 26},            // 美春 (Miu) -> 三美 cell
    {"\x89\xc2\x8f\x83", 28},            // 可純 -> 可帆 cell
    {"\x82\x67\x82\x68\x82\x6e\x82\x6e", 9},   // ＣＨＯＣＯ fullwidth -> CHOCO cell
};

int text_width(const std::u16string& text, int size) {
    int w = 0;
    for (char16_t ch : text) w += ch < 0x100 || (ch >= 0xFF61 && ch <= 0xFF9F) ? size / 2 : size;
    return w;
}

const struct { const char16_t* name; int id; } kDC2DMFaceNames[] = {
    {u"音姫",0}, {u"Otome",0}, {u"由夢",1}, {u"Yume",1},
    {u"小恋",2}, {u"Koko",2}, {u"ななか",3}, {u"Nanaka",3},
    {u"杏",4}, {u"Anzu",4}, {u"美夏",5}, {u"Minatsu",5},
    {u"エリカ",6}, {u"Erika",6}, {u"渉",7}, {u"Wataru",7},
    {u"茜",8}, {u"Akane",8}, {u"麻耶",9}, {u"Maya",9},
    {u"まゆき",10}, {u"Mayuki",10}, {u"杉並",11}, {u"Suginami",11},
    {u"さくら",12}, {u"Sakura",12}, {u"水越",13}, {u"舞佳",13},
    {u"Mizukoshi",13}, {u"Maika",13}, {u"Yuki",14}, {u"由姫",14},
    {u"Yuto",15}, {u"勇斗",15}, {u"イベール",16}, {u"Hiver",16},
    {u"Yver",16}, {u"Yvel",16}, {u"Yve",16}, {u"Ivair",16},
    {u"Ybel",16}, {u"Yvers",16}, {u"Yoshiyuki",17}, {u"義之",17},
    {u"立夏",18}, {u"森園",18}, {u"Rikka",18}, {u"Morizono",18},
    {u"桜姫",20}, {u"Yuuki",20}, {u"はりまお",24}, {u"Harimao",24}
};

bool dm_hit(int x, int y, int left, int top, int width, int height) {
    return x >= left * 5 / 4 && x < (left + width) * 5 / 4 &&
           y >= top * 5 / 4 && y < (top + height) * 5 / 4;
}

int dm_row_at(int x, int y, int scroll, size_t count) {
    for (int slot = 0; slot < 4 && scroll + slot < int(count); ++slot)
        if (scroll + slot >= 0 && dm_hit(x, y, 200, 25 + slot * 133, 680, 120))
            return scroll + slot;
    return -1;
}

}  // namespace

LogRow capture_log_row(dc4scene::SceneRunner& scene, int line) {
    LogRow row;
    row.name = dc4scene::sjis_to_utf16(scene.speaker());
    row.text = scene.visible_text();
    row.voice = scene.last_voice();
    row.script = scene.script_name();
    row.line = line;
    scene.capture_state(&row.snapshot);
    row.thumb = scene.capture_thumbnail(LogRow::kThumbW, LogRow::kThumbH);
    return row;
}

std::vector<std::u16string> LogScreen::text_lines(const std::u16string& text, bool latin, bool dc2dm,
                                              const dc4scene::FontRenderer* font) {
    const int size = dc2dm ? 22 : (latin ? 18 : 22);
    const bool proportional = dc2dm && font && font->is_pretendard_jp_bold();
    // The Japanese MES already lays out its lines. Only wrap unusually long
    // lines at the row's right edge; 545px incorrectly split a 25-cell line.
    const int wrap = dc2dm ? 506 : (latin ? 545 : 720);
    auto space = [](char16_t ch) { return ch == u' ' || ch == u'\t' || ch == u'\u3000'; };
    auto cjk = [](char16_t ch) {
        return (ch >= 0x2E80 && ch <= 0x9FFF) || (ch >= 0xAC00 && ch <= 0xD7AF) ||
               (ch >= 0xFF00 && ch <= 0xFFEF);
    };
    std::u16string normalized;
    for (size_t i = 0; i < text.size(); ++i) {
        const char16_t ch = text[i];
        if (ch == u'\r') continue;
        if (ch == u'\n' && latin) {
            if (!normalized.empty() && !space(normalized.back()) &&
                i + 1 < text.size() && !space(text[i + 1]) &&
                text[i + 1] != u'\n' && text[i + 1] != u'\r') normalized.push_back(u' ');
        } else normalized.push_back(ch);
    }
    std::vector<std::u16string> lines;
    size_t begin = 0;
    while (begin < normalized.size()) {
        const size_t newline = normalized.find(u'\n', begin);
        const size_t end = newline == std::u16string::npos ? normalized.size() : newline;
        if (begin == end) lines.emplace_back();
        while (begin < end) {
            size_t take = begin, last_break = begin;
            int width = 0;
            while (take < end) {
                const char16_t ch = normalized[take];
                const int cell = proportional ? font->glyph_advance(ch,float(size)) :
                    (ch < 0x100 || (ch >= 0xFF61 && ch <= 0xFF9F) ? size / 2 : size) - (dc2dm ? 1 : 0);
                if (width + cell > wrap && take > begin) {
                    if (last_break > begin) take = last_break;
                    break;
                }
                width += cell;
                if (space(ch)) {
                    size_t next = take + 1;
                    while (next < end && space(normalized[next])) ++next;
                    last_break = next;
                } else if (cjk(ch) || ch == u'-' || ch == u'/' || ch == u'\\') last_break = take + 1;
                ++take;
            }
            lines.push_back(normalized.substr(begin, take - begin));
            begin = take;
        }
        if (newline == std::u16string::npos) break;
        begin = newline + 1;
    }
    return lines;
}

bool LogScreen::load(const std::string& data_dir) {
    back_={};chip_={};
    dc4::MesFile main;
    dc2dm_=main.load_file(data_dir+"/MES/main.mes") && main.dialect==dc4::MesDialect::ClassicDM;
    dc2dm_text_rgb_ = 0xC8A0DC;
    dc2dm_selected_rgb_ = 0xFFFFFF;
    dc2dm_selected_reverse_ = 0;
    if (dc2dm_) {
        dc4::MesFile log;
        if (log.load_file(data_dir + "/MES/LOG.mes")) {
            dc4::MesStream stream(log);
            dc4::MesCommand command;
            while (stream.next(&command)) {
                if (command.op != 0x6E || command.kind != dc4::CmdKind::Words4) continue;
                const unsigned index = command.words[0], red = command.words[1],
                               green = command.words[2], blue = command.words[3];
                if (red > 255 || green > 255 || blue > 255) continue;
                const uint32_t rgb = (red << 16) | (green << 8) | blue;
                if (index == 15) dc2dm_text_rgb_ = rgb;
                if (index == 7) dc2dm_selected_rgb_ = rgb;
            }
        }
    }
    if (!read_crx(data_dir, dc2dm_ ? "LOG_BACK.CRX" : "BKLOGBACK.CRX", &back_)) return false;
    if (!read_crx(data_dir, dc2dm_ ? "LOG_CHIP.CRX" : "BKLOGCHIP.CRX", &chip_)) {back_={};return false;}
    face_names_.clear();
    for (const auto& entry : kFaceNames)
        face_names_.push_back(dc4scene::sjis_to_utf16(entry.sjis));
    alias_names_.clear();
    for (const auto& entry : kFaceAliases)
        alias_names_.push_back({dc4scene::sjis_to_utf16(entry.sjis), entry.id});
    return true;
}

int LogScreen::face_id(const std::u16string& name, const std::string& script) const {
    if (name.empty()) return -1;  // narration: no face at all
    if (dc2dm_) {
        int id = -2;
        for (const auto& entry : kDC2DMFaceNames)
            if (name == entry.name) { id = entry.id; break; }
        std::string basename = script.substr(script.find_last_of("/\\") + 1);
        const auto dot = basename.find_last_of('.');
        if (dot != std::string::npos) basename.resize(dot);
        std::string lower = basename;
        for (char& ch : lower) ch = char(std::tolower(static_cast<unsigned char>(ch)));
        const auto prefix = [&](const char* value) { return lower.rfind(value, 0) == 0; };
        if (id == 0) {
            for (const char* value : {"suc61","suc65","suc66","suc68","suc69","suc71","suc74","suc75"})
                if (prefix(value)) return 19;
            for (const char* value : {"suc60_1110_f1_otm","suc60_1121_a1_otm","suc60_1205_b1_otm",
                                     "suc60_1224_a1_otm","suc60_1231_f1_otm"})
                if (basename == value) return 19;
        } else if (id == 20) {
            for (const char* value : {"suc65","suc66","suc68","suc69"})
                if (prefix(value)) return 21;
            for (const char* value : {"suc71","suc74_0131","suc74_0201","suc74_0203",
                                     "suc74_0214","suc74_0325"})
                if (prefix(value)) return 22;
            if (prefix("suc74") || prefix("suc75")) return 23;
        }
        return id;
    }
    for (size_t i = 0; i < face_names_.size(); ++i)
        if (face_names_[i] == name) return int(i);
    for (const auto& alias : alias_names_)
        if (alias.first == name) return alias.second;
    int rom = dc4::romanized_face_id(name);
    if (rom >= 0) return rom;
    return -2;  // unknown speaker: silhouette cell 29 + dynamic name text
}

void LogScreen::blit(const dc4scene::Image& img, int sx, int sy, int sw, int sh,
                     int dx, int dy, std::vector<uint8_t>* frame) const {
    for (int y = 0; y < sh; ++y) {
        const int fy = dy + y;
        if (fy < 0 || fy >= dc4scene::kScreenH) continue;
        const int iy = sy + y;
        if (iy < 0 || iy >= img.h) continue;
        for (int x = 0; x < sw; ++x) {
            const int fx = dx + x;
            if (fx < 0 || fx >= dc4scene::kScreenW) continue;
            const int ix = sx + x;
            if (ix < 0 || ix >= img.w) continue;
            const uint8_t* src = &img.rgba[(size_t(iy) * img.w + ix) * 4];
            if (!src[3]) continue;
            uint8_t* dst = &(*frame)[(size_t(fy) * dc4scene::kScreenW + fx) * 4];
            const int a = src[3];
            for (int c = 0; c < 3; ++c)
                dst[c] = uint8_t((int(src[c]) * a + int(dst[c]) * (255 - a)) / 255);
            dst[3] = 255;
        }
    }
}

// White glyphs with the font's built-in dark outline, matching the message
// window. Rasterizes a tight bounds buffer like the shell's overlay_text.
void LogScreen::draw_text(const dc4scene::FontRenderer& font, const std::u16string& text,
                          int x, int baseline, std::vector<uint8_t>* frame, int size) const {
    if (text.empty()) return;
    const int width = std::min(text_width(text, size) + 8, dc4scene::kScreenW - x + 2);
    if (width <= 0) return;
    const int height = size + 16;
    std::vector<uint8_t> pixels(size_t(width) * height * 4, 0);
    font.rasterize_line(text, 2, size + 8, &pixels, width, height, float(size));
    const int top = baseline - size - 8;
    for (int py = 0; py < height; ++py) {
        for (int px = 0; px < width; ++px) {
            const int dx = x - 2 + px, dy = top + py;
            if (dx < 0 || dy < 0 || dx >= dc4scene::kScreenW || dy >= dc4scene::kScreenH) continue;
            const uint8_t* src = &pixels[(size_t(py) * width + px) * 4];
            if (!src[3]) continue;
            uint8_t* dst = &(*frame)[(size_t(dy) * dc4scene::kScreenW + dx) * 4];
            for (int c = 0; c < 3; ++c)
                dst[c] = uint8_t((int(src[c]) * src[3] + int(dst[c]) * (255 - src[3])) / 255);
            dst[3] = 255;
        }
    }
}

// Blit a raw RGBA buffer (the captured scene thumbnail) into the frame.
void LogScreen::blit_image(const std::vector<uint8_t>& rgba, int w, int h,
                           int dx, int dy, std::vector<uint8_t>* frame) const {
    if (rgba.size() != size_t(w) * h * 4) return;
    for (int y = 0; y < h; ++y) {
        const int fy = dy + y;
        if (fy < 0 || fy >= dc4scene::kScreenH) continue;
        for (int x = 0; x < w; ++x) {
            const int fx = dx + x;
            if (fx < 0 || fx >= dc4scene::kScreenW) continue;
            const uint8_t* src = &rgba[(size_t(y) * w + x) * 4];
            if (!src[3]) continue;
            uint8_t* dst = &(*frame)[(size_t(fy) * dc4scene::kScreenW + fx) * 4];
            const int a = src[3];
            for (int c = 0; c < 3; ++c)
                dst[c] = uint8_t((int(src[c]) * a + int(dst[c]) * (255 - a)) / 255);
            dst[3] = 255;
        }
    }
}

void LogScreen::draw(const std::vector<LogRow>& rows, int scroll, bool thumbnails,
                     int mouse_x, int mouse_y, const dc4scene::FontRenderer& font,
                     std::vector<uint8_t>* frame) {
    if (!loaded() || !frame || frame->size()!=size_t(dc4scene::kScreenW)*dc4scene::kScreenH*4) return;
    if(dc2dm_) {draw_dc2dm(rows,scroll,mouse_x,mouse_y,font,frame);return;}
    // Opaque full-screen background: title, rules, track and arrows are baked in.
    blit(back_, 0, 0, back_.w, back_.h, 0, 0, frame);

    const int count = int(rows.size());
    for (int slot = 0; slot < kVisibleRows; ++slot) {
        const int index = scroll + slot;
        if (index < 0 || index >= count) continue;
        const LogRow& row = rows[size_t(index)];
        const int top = kTopY + slot * kRowPitch;

        // Row bar (sub_404EE0 0x4057F4: chip (0,220)-(1203,343) at (20, top+14)).
        blit(chip_, 0, 220, 1203, 123, 20, top + 14, frame);

        // Speaker face at (20, top+18); the pink name plate is baked into
        // each grid cell. Speakers not on the grid (unknown characters,
        // nicknames) use the silhouette cell 29 plus a dynamic name text.
        // Narration (empty name) shows no face, matching the original.
        const int id = face_id(row.name);
        if (id >= 0) {
            const int cx = 151 * (id % 8);
            const int cy = 121 * (id / 8) + 370;
            blit(chip_, cx, cy, 150, 120, 20, top + 18, frame);
        } else if (id == -2) {
            // Unknown speaker: silhouette placeholder (cell 29) + name text.
            blit(chip_, 151 * (29 % 8), 121 * (29 / 8) + 370, 150, 120, 20, top + 18, frame);
            int bytes = 0;
            for (char16_t ch : row.name) bytes += ch < 0x100 ? 1 : 2;
            const int x = bytes >= 20 ? 75 - 3 * bytes : 48 - 4 * bytes;
            draw_text(font, row.name, x, top + 36, frame);
        }

        // The original keeps the speaker portrait/name visible and draws a
        // separate 16:9 scene preview between it and the scene-jump button.
        if (thumbnails && jump_hit(mouse_x, mouse_y, scroll, rows.size()) == index)
            blit_image(row.thumb, LogRow::kThumbW, LogRow::kThumbH, 184, top + 38, frame);

        // Keep the Japanese script's newlines and indentation. The supplied
        // original row has 36px line spacing; translations keep their reflow.
        {
            const int font_size = latin_mode_ ? 18 : 22;
            const int line_pitch = latin_mode_ ? 24 : 36;
            int baseline = top + (latin_mode_ ? 44 : 49);
            for (const auto& line : text_lines(row.text, latin_mode_)) {
                if (baseline >= top + kRowPitch) break;
                draw_text(font, line, 480, baseline, frame, font_size);
                baseline += line_pitch;
            }
        }

        // Scene-jump button (0x405ABD): region (328,top+28)-(488,top+62),
        // art (362,52)-(488,87), hover (492,52)-(618,87).
        const bool jump_hover = jump_hit(mouse_x, mouse_y, scroll, rows.size()) == index;
        blit(chip_, jump_hover ? 492 : 362, 52, 126, 35, 328, top + 28, frame);
        // Voice-replay button (0x405B7E), only when the row has a voice.
        if (!row.voice.empty()) {
            const bool voice_hover = voice_hit(mouse_x, mouse_y, scroll, rows.size()) == index;
            blit(chip_, voice_hover ? 492 : 362, 12, 126, 34, 328, top + 68, frame);
        }
    }

    // Thumbnail checkbox (0x4058C9): box (0,6)-(29,37) at (327,44), hover
    // (31,6)-(61,37), label (76,0)-(310,38) at (356,38), check (310,0)-(341,32).
    const bool check_hover = checkbox_hit(mouse_x, mouse_y);
    blit(chip_, check_hover ? 31 : 0, 6, 29, 31, 327, 44, frame);
    blit(chip_, 76, 0, 234, 38, 356, 38, frame);
    if (thumbnails) blit(chip_, 310, 0, 31, 32, 327, 44, frame);

    // Back button (sub_4010F0): art (640,8)-(744,47) at (1100,672),
    // hover (640,47)-(744,89); region (1100,672)-(1216,712).
    blit(chip_, 640, back_hit(mouse_x, mouse_y) ? 47 : 8, 104, 39, 1100, 672, frame);

    // Scroll thumb (Backlog_InitGraphics 0x406061): art (1200,38)-(1224,68)
    // at x=1232, y = 82 + 582 * scrolled fraction over the TOTAL row count
    // (native: 582 * current / total), hidden with <= 4 rows.
    if (rows.size() > kVisibleRows)
        blit(chip_, 1200, 38, 24, 30, 1232, thumb_y(rows.size(), scroll), frame);
}

bool LogScreen::back_hit(int x, int y) const {
    if (dc2dm_) return dm_hit(x, y, 936, 504, 84, 68);
    return x >= 1100 && x < 1216 && y >= 672 && y < 712;
}

bool LogScreen::checkbox_hit(int x, int y) const {
    if(dc2dm_)return false;
    return x >= 327 && x < 666 && y >= 38 && y < 76;
}

static int row_at(int x, int y, int scroll, size_t row_count, int dy0, int dy1) {
    if (x < 328 || x >= 488) return -1;
    for (int slot = 0; slot < LogScreen::kVisibleRows; ++slot) {
        const int index = scroll + slot;
        if (index < 0 || index >= int(row_count)) break;
        const int top = LogScreen::kTopY + slot * LogScreen::kRowPitch;
        if (y >= top + dy0 && y < top + dy1) return index;
    }
    return -1;
}

int LogScreen::jump_hit(int x, int y, int scroll, size_t row_count) const {
    if(dc2dm_) {
        return row_count && dm_hit(x, y, 28, 66, 160, 48)
            ? std::clamp(scroll, 0, last_scroll(row_count)) : -1;
    }
    return row_at(x, y, scroll, row_count, 28, 62);
}

int LogScreen::voice_hit(int x, int y, int scroll, size_t row_count) const {
    if(dc2dm_) {
        return dm_row_at(x, y, scroll, row_count);
    }
    return row_at(x, y, scroll, row_count, 68, 101);
}

int LogScreen::thumb_y(size_t row_count, int scroll) {
    if (row_count <= kVisibleRows) return kTopY;
    const int y = kTopY + 582 * std::clamp(scroll, 0, int(row_count)) / int(row_count);
    return std::clamp(y, kTopY, 634);
}

bool LogScreen::thumb_hit(int x, int y, int scroll, size_t row_count) const {
    if (dc2dm_) return row_count > 4 && dm_hit(x, y, 944, 114, 40, 330);
    if (row_count <= kVisibleRows || x < 1222 || x >= 1266) return false;
    const int ty = thumb_y(row_count, scroll);
    return y >= ty - 4 && y < ty + 34;
}

int LogScreen::thumb_scroll(int y, size_t row_count) const {
    if (row_count <= kVisibleRows) return 0;
    if (dc2dm_) return std::clamp((y * 4 / 5 - 114) * (int(row_count) - 1) / 330, 0, last_scroll(row_count));
    const int offset = (y - kTopY) * int(row_count) / 582;
    return std::clamp(offset, 0, max_scroll(row_count));
}

int LogScreen::scroll_button(int x, int y, int scroll, size_t row_count) const {
    if (!dc2dm_ || row_count <= 4) return 0;
    if (scroll > 0) {
        if (dm_hit(x, y, 120, 136, 72, 40)) return -4;
        if (dm_hit(x, y, 944, 88, 40, 24)) return -1;
    }
    if (scroll < last_scroll(row_count)) {
        if (dm_hit(x, y, 120, 512, 72, 40)) return 4;
        if (dm_hit(x, y, 944, 444, 40, 24)) return 1;
    }
    return 0;
}

void LogScreen::select_at(int x, int y, int scroll, size_t row_count) {
    if (!dc2dm_) return;
    scroll = std::clamp(scroll, 0, last_scroll(row_count));
    const int row = dm_row_at(x, y, scroll, row_count);
    if (row >= 0)
        dc2dm_selected_reverse_ = std::min(visible_rows(), int(row_count) - scroll) - 1 - (row - scroll);
}

void LogScreen::select_top(int scroll, size_t row_count) {
    if (!dc2dm_) return;
    scroll = std::clamp(scroll, 0, last_scroll(row_count));
    dc2dm_selected_reverse_ = std::max(0, std::min(visible_rows(), int(row_count) - scroll) - 1);
}

void LogScreen::draw_dc2dm(const std::vector<LogRow>& rows, int scroll,
                          int mouse_x, int mouse_y, const dc4scene::FontRenderer& font,
                          std::vector<uint8_t>* frame) {
    constexpr int width = 1024, height = 576;
    const bool proportional = font.is_pretendard_jp_bold();
    std::vector<uint8_t> native(size_t(width) * height * 4, 0);
    struct OutputText {
        std::u16string value;
        int x, baseline, size, raster_size, top, bottom;
        uint32_t rgb;
        bool proportional = false;
        bool voice_after = false;
        int left = 0, right = 1024;
    };
    std::vector<OutputText> output_text;
    const auto native_blit = [&](const dc4scene::Image& image, int sx, int sy,
                                 int w, int h, int x, int y) {
        for (int py = 0; py < h; ++py) for (int px = 0; px < w; ++px) {
            const int ix = sx + px, iy = sy + py, dx = x + px, dy = y + py;
            if (ix < 0 || ix >= image.w || iy < 0 || iy >= image.h ||
                dx < 0 || dx >= width || dy < 0 || dy >= height) continue;
            const auto* src = &image.rgba[(size_t(iy) * image.w + ix) * 4];
            auto* dst = &native[(size_t(dy) * width + dx) * 4];
            for (int c = 0; c < 3; ++c)
                dst[c] = uint8_t((int(src[c]) * src[3] + int(dst[c]) * (255 - src[3])) / 255);
            dst[3] = 255;
        }
    };
    const auto latin_text = [&](const std::u16string& text) {
        if (latin_mode_) return true;
        bool ascii = false;
        for (char16_t ch : text) {
            if ((ch >= 0x3040 && ch <= 0x30FF) || (ch >= 0x3400 && ch <= 0x9FFF) ||
                (ch >= 0xFF61 && ch <= 0xFF9F)) return false;
            ascii |= (ch >= u'A' && ch <= u'Z') || (ch >= u'a' && ch <= u'z');
        }
        return ascii;
    };
    const auto render_text = [&](const std::u16string& value, int x, int baseline,
                                 int size, int raster_size, bool latin, uint32_t rgb,
                                 int top, int bottom) {
        if (proportional || (latin && font.is_ms_gothic())) {
            output_text.push_back({value,x,baseline,size,proportional ? size : raster_size,top,bottom,rgb,proportional});
        } else {
            font.rasterize_line(value,x,baseline,&native,width,height,
                                float(size),rgb,false,0,true,-1,float(raster_size));
        }
    };
    scroll = std::clamp(scroll, 0, last_scroll(rows.size()));
    const int visible = std::min(visible_rows(), int(rows.size()) - scroll);
    select_at(mouse_x, mouse_y, scroll, rows.size());
    native_blit(back_, 0, 0, back_.w, back_.h, 0, 0);
    if (rows.empty())
        render_text(latin_mode_ ? u"No dialogue history yet." : u"履歴はありません。",
                    364,41+(latin_mode_ ? 18 : 22),22,latin_mode_ ? 17 : 22,
                    latin_mode_,dc2dm_text_rgb_,29,145);
    for (int slot = 0; slot < visible_rows(); ++slot) {
        const int index = scroll + slot, top = 29 + slot * 133;
        if (index >= int(rows.size())) break;
        const auto& row = rows[size_t(index)];
        const uint32_t rgb = visible - 1 - slot == dc2dm_selected_reverse_
            ? dc2dm_selected_rgb_ : dc2dm_text_rgb_;
        native_blit(chip_, 228, 0, 680, 124, 200, top - 4);
        const int id = face_id(row.name, row.script);
        bool deferred_voice = false;
        if (id >= 0)
            native_blit(chip_, id == 24 ? 864 : 168 + id % 6 * 116,
                        id == 24 ? 480 : 132 + id / 6 * 116, 116, 116, 240, top);
        else if (!row.name.empty()) {
            int bytes = 0;
            for (char16_t ch : row.name)
                bytes += ch < 0x100 || (ch >= 0xFF61 && ch <= 0xFF9F) ? 1 : 2;
            const bool latin = latin_text(row.name);
            const int x = (bytes >= 16 ? 304 : 288) -
                (proportional ? font.text_width(row.name,16)/2 : 4 * bytes);
            render_text(row.name,x,top+15+(latin ? 13 : 16),16,latin ? 12 : 16,
                        latin,rgb,top,top+116);
            if (proportional) {
                output_text.back().left = 200;
                output_text.back().right = 364;
            }
            deferred_voice = (proportional || (latin && font.is_ms_gothic())) && !row.voice.empty();
            if (deferred_voice) output_text.back().voice_after = true;
        }
        const int body_top = top + (row.name.empty() ? 4 : 12);
        if (!row.voice.empty() && !deferred_voice)
            native_blit(chip_, 84, 120, 24, 24, 208, body_top - 4);
        const bool latin = latin_text(row.text);
        int y = body_top;
        for (const auto& line : text_lines(row.text, latin, true, &font)) {
            if (y + 22 > top + 116) break;
            render_text(line,364,y+(latin ? 18 : 22),22,latin ? 17 : 22,
                        latin,rgb,top,top+116);
            y += latin ? 24 : 32;
        }
    }
    if (!rows.empty())
        native_blit(chip_, 0, jump_hit(mouse_x, mouse_y, scroll, rows.size()) >= 0 ? 252 : 204,
                    160, 48, 28, 66);
    native_blit(chip_, 0, back_hit(mouse_x, mouse_y) ? 68 : 0, 84, 68, 936, 504);
    const int direction = scroll_button(mouse_x, mouse_y, scroll, rows.size());
    if (scroll > 0)
        native_blit(chip_, 156, direction == -4 ? 40 : 0, 71, 40, 120, 136);
    if (scroll < last_scroll(rows.size()))
        native_blit(chip_, 84, direction == 4 ? 40 : 0, 71, 40, 120, 512);
    if (rows.size() > 4) {
        native_blit(chip_, 908, direction == -1 ? 24 : 0, 40, 24, 944, 88);
        native_blit(chip_, 948, direction == 1 ? 24 : 0, 40, 24, 944, 444);
        native_blit(chip_, 988, 0, 32, 332, 948, 112);
        native_blit(chip_, 908, 72, 32, 32, 948, 112 + scroll * 300 / last_scroll(rows.size()));
    }
    for (int y = 0; y < dc4scene::kScreenH; ++y) for (int x = 0; x < dc4scene::kScreenW; ++x) {
        const auto src = (size_t(y * 4 / 5) * width + x * 4 / 5) * 4;
        const auto dst = (size_t(y) * dc4scene::kScreenW + x) * 4;
        std::copy_n(native.data() + src, 4, frame->data() + dst);
    }
    for (const auto& run : output_text) {
        // Align the strip to four native pixels so rounding keeps the global
        // baseline, including rows whose native origins are not multiples of four.
        const int top = std::max(0,run.top/4*4);
        const int y = top*5/4;
        const int bottom = std::min(dc4scene::kScreenH,(run.bottom*5+3)/4);
        const auto begin = frame->begin()+size_t(y)*dc4scene::kScreenW*4;
        const auto end = frame->begin()+size_t(bottom)*dc4scene::kScreenW*4;
        std::vector<uint8_t> strip(begin,end);
        font.rasterize_line(run.value,run.x,run.baseline-top,&strip,dc4scene::kScreenW,
                            bottom-y,float(run.size),run.rgb,false,0,!run.proportional,
                            run.proportional ? 0 : -1,float(run.raster_size),1.25f);
        if (run.voice_after) {
            const int icon_top = run.top+8;
            for (int py = (icon_top*5+3)/4; py < ((icon_top+24)*5+3)/4; ++py)
                for (int px = 260; px < 290; ++px) {
                    const int sx = 84+px*4/5-208, sy = 120+py*4/5-icon_top;
                    if (sx < 0 || sx >= chip_.w || sy < 0 || sy >= chip_.h) continue;
                    const auto* src = &chip_.rgba[(size_t(sy)*chip_.w+sx)*4];
                    auto* dst = &strip[(size_t(py-y)*dc4scene::kScreenW+px)*4];
                    for (int c = 0; c < 3; ++c)
                        dst[c] = uint8_t((int(src[c])*src[3]+int(dst[c])*(255-src[3]))/255);
                    dst[3] = 255;
                }
        }
        if (run.left == 0 && run.right == width) {
            std::copy(strip.begin(),strip.end(),begin);
        } else {
            const int left = run.left*5/4, right = run.right*5/4;
            for (int row = 0; row < bottom-y; ++row) {
                const size_t offset = (size_t(row)*dc4scene::kScreenW+left)*4;
                std::copy_n(strip.begin()+offset,size_t(right-left)*4,begin+offset);
            }
        }
    }
}

}  // namespace dc4ui
