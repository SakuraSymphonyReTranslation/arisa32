#pragma once
#include <string>
#include <utility>

namespace dc4scene {

inline bool comu_padded_latin_text(const std::string& text) {
    bool padding = false, latin = false;
    for (size_t i = 0; i < text.size(); ++i) {
        const auto ch = static_cast<unsigned char>(text[i]);
        if ((ch >= 0x81 && ch <= 0x9F) || (ch >= 0xE0 && ch <= 0xFC)) {
            if (++i >= text.size()) return false;
            continue;
        }
        if (ch == 0x7F) return false;
        padding = padding || ch == 0xA5;
        latin = latin || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
    }
    return padding && latin;
}

inline bool comu_prose_text(const std::string& text) {
    bool del = false, space = true, ascii = true;
    for (size_t i = 0; i < text.size(); ++i) {
        const auto ch = static_cast<unsigned char>(text[i]);
        ascii = ascii && ch < 0x80;
        if ((ch >= 0x81 && ch <= 0x9F) || (ch >= 0xE0 && ch <= 0xFC)) {
            if (++i >= text.size()) return false;
            space = false;
            continue;
        }
        // Explicit padding and line breaks belong to the author's fixed layout.
        if (ch == 0xA5 || ch < 0x20) return false;
        const bool next_space = ch == 0x7F || ch == ' ';
        if (space && next_space) return false;
        space = next_space;
        del = del || ch == 0x7F;
    }
    return (del || ascii) && !space;
}

struct ComuProseLayout {
    std::u16string text;
    int size = 16;
    int lines = 1;
    bool fixed_grid = true;
    int baseline() const { return lines == 3 ? 18 : 30; }
    int line_pitch() const { return lines == 3 ? 19 : (fixed_grid ? 28 : 26); }
};

// Proportional faces can use three rows to keep lowercase letters readable.
template<class Font>
ComuProseLayout comu_prose_layout(const std::u16string& text, const Font& font) {
    constexpr int width = 228;  // Room for unpadded text beside the sender portrait.
    const bool fixed = font.text_width(u"iiii", 16) == font.text_width(u"WWWW", 16);
    int preferred_size = fixed ? 16 : 22;
    // Keep ink inside the bubble and separate at the 19px three-line pitch.
    while (!fixed && preferred_size > 16) {
        const auto bounds = font.text_vertical_bounds(text, float(preferred_size));
        if (bounds.first >= -17 && bounds.second <= 4 && bounds.second - bounds.first <= 18) break;
        --preferred_size;
    }
    for (int size = preferred_size; size > 0; --size) {
        auto result = text;
        size_t start = 0;
        for (int lines = 1; lines <= (fixed ? 2 : 3); ++lines) {
            if (font.text_width(text.substr(start), float(size), false, fixed) <= width)
                return {std::move(result), size, lines, fixed};
            size_t split = std::u16string::npos;
            for (size_t at = text.find(u' ', start); at != std::u16string::npos;
                 at = text.find(u' ', at + 1)) {
                if (font.text_width(text.substr(start, at - start), float(size), false, fixed) > width)
                    break;
                split = at;
            }
            if (split == std::u16string::npos) break;
            result[split] = u'\n';
            start = split + 1;
        }
    }
    return {text, 1, 1, fixed};
}

// DC4 0x458E09 / DC4PH 0x458BA9 insert one newline after 28 SJIS bytes.
// Keep padding intact; translation patches use it to position the second row.
inline std::string comu_line_break(std::string text, bool utf8 = false) {
    size_t at = 0;
    int units = 0;
    while (at < text.size() && units < 28) {
        const auto lead = static_cast<unsigned char>(text[at]);
        size_t length = 1;
        int cost = 1;
        if (utf8 && lead >= 0xC2 && lead <= 0xF4) {
            length = lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
            if (at + length > text.size()) length = 1;
            unsigned int cp = lead & (0x7F >> length);
            for (size_t i = 1; i < length; ++i)
                cp = (cp << 6) | (static_cast<unsigned char>(text[at + i]) & 0x3F);
            cost = cp <= 0x7F || (cp >= 0xFF61 && cp <= 0xFF9F) ? 1 : 2;
        } else if (!utf8 && ((lead >= 0x81 && lead <= 0x9F) || (lead >= 0xE0 && lead <= 0xFC)) &&
                   at + 1 < text.size()) {
            length = 2;
            cost = 2;
        }
        // A mixed Latin/Japanese boundary must not split a multibyte character.
        if (units + cost > 28) break;
        units += cost;
        at += length;
    }
    if (at < text.size()) text.insert(at, 1, '\n');
    return text;
}

}  // namespace dc4scene
