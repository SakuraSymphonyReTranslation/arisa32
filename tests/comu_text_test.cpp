#include "arisa32/scene.h"
#include "arisa32/resource.h"
#include "../core/src/comu_text.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
namespace {
int failures = 0;
void check(bool ok, const char* expression, int line) {
    if (!ok) { std::cerr << line << ": " << expression << '\n'; ++failures; }
}
#define CHECK(x) check(bool(x), #x, __LINE__)

std::string patch_spaces(std::string text) {
    std::replace(text.begin(), text.end(), ' ', char(0xA5));
    return text;
}
std::string del_spaces(std::string text) {
    std::replace(text.begin(), text.end(), ' ', char(0x7F));
    return text;
}
const char* const sakura_messages[] = {
    "That's where we're at. Any ideas?",
    "I wonder if that also comes down to how much they trust us",
    "I see. If they feel we are people they can rely on, you mean",
    "Then, first of all, a list of our members",
    "ought to be made public",
    "That's true. Especially with Suginami and Shirakawa teaming up",
    "That should make quite an impression",
    "One thing we forgot,",
    "Shirakawa's matchmaking service is currently temporarily suspended",
    "Shouldn't we add a notice like that?",
    "Is \"temporarily suspended\" the right wording?",
    "You said you would not do it anymore. Shouldn't we say that?",
    "No, \"no longer accepting requests\" is too forceful.",
    "Letting this sort of thing fade out naturally",
    "would be preferable",
    "Why don't we just leave the rest to Nii-san and Mishima-san?",
    "Why would we do that?!"
};
const char* const romanized_names[] = {
    "Arisu", "Nino", "Sorane", "Hiyori", "Shiina", "Miu", "Chiyoko",
    "Arisa", "Ichito", "Suginami", "Kanata"
};
std::string sender(const std::string& name) { return "\x81\x75" + name + "\x81\x76"; }

dc4scene::ComuProseLayout prose_check(const std::string& raw, const dc4scene::FontRenderer& font) {
    CHECK(dc4scene::comu_prose_text(raw));
    const auto decoded = dc4scene::sjis_to_utf16(raw);
    const auto layout = dc4scene::comu_prose_layout(decoded, font);
    CHECK(layout.size >= (layout.fixed_grid ? 12 : 18) && layout.size <= (layout.fixed_grid ? 16 : 22));
    CHECK(layout.lines <= (layout.fixed_grid ? 2 : 3));
    CHECK(std::count(layout.text.begin(), layout.text.end(), u'\n') == layout.lines - 1);
    auto restored = layout.text;
    std::replace(restored.begin(), restored.end(), u'\n', u' ');
    CHECK(restored == decoded);
    size_t start = 0;
    for (;;) {
        const size_t split = layout.text.find(u'\n', start);
        CHECK(font.text_width(layout.text.substr(start, split == std::u16string::npos ? split : split - start),
                              float(layout.size), false, layout.fixed_grid) <= 228);
        if (split == std::u16string::npos) break;
        start = split + 1;
    }
    return layout;
}
void prose_tests(const dc4scene::FontRenderer& font) {
    for (const char* message : sakura_messages) prose_check(del_spaces(message), font);
    for (const char* message : {"  The English patch keeps   ", "Two  spaces", "Leading ", "A\nB", "", "\x82\xA0"})
        CHECK(!dc4scene::comu_prose_text(del_spaces(message)));
    CHECK(!dc4scene::comu_prose_text(patch_spaces("  The English patch keeps   ") + del_spaces("its spaces")));
    CHECK(dc4scene::comu_prose_text(std::string("Note\x7F") + "\x81\xF4"));
    CHECK(dc4scene::comu_prose_text("Understood"));
    const auto layout = dc4scene::comu_prose_layout(u"That's where we're at. Any ideas?", font);
    CHECK(layout.size == 16);
    CHECK(layout.text == u"That's where we're at. Any\nideas?");
}
void break_tests() {
    using dc4scene::comu_line_break;
    // Sender-stripped messages from dc4_miu20190319c.mes, including patch padding.
    const std::pair<const char*, const char*> messages[] = {
        {"  Begitulah situasinya, ada ", "  ide?"},
        {"  Tergantung seberapa       ", "  percaya mereka."},
        {"  Intinya kita harus        ", "  terlihat bisa diandalkan."},
        {"  Publikasikan daftar       ", "  anggota kita dulu."},
        {"  Langkah itu jauh lebih    ", "  baik."},
        {"  Benar. Apalagi Suginami & ", "  Shirakawa bersatu."},
        {" Biro jodoh Shirakawa sedang", " libur sementara."},
        {" Apa sebaiknya kita tulis   ", " pengumuman itu?"},
        {"  Apa tulisan 'Libur        ", "  sementara' sudah cukup?"},
        {"  Tulis saja tutup permanen ", "  sesuai niat awal."},
        {" Jangan, kata-kata itu      ", " terlalu kasar."},
        {" Biarkan saja masalah ini   ", " memudar alami."},
    };
    for (const auto& message : messages) {
        const auto first = patch_spaces(message.first), second = patch_spaces(message.second);
        CHECK(first.size() == 28);
        CHECK(comu_line_break(first + second) == first + '\n' + second);
    }
    for (const char* text : {"", "  Itu pasti berdampak besar.", " Ada satu hal yang terlupa.",
                             " Cara itu jauh lebih ideal."})
        CHECK(comu_line_break(patch_spaces(text)) == patch_spaces(text));
    CHECK(comu_line_break(std::string(28, 'A')) == std::string(28, 'A'));
    CHECK(comu_line_break(std::string(29, 'A')) == std::string(28, 'A') + "\nA");
    CHECK(comu_line_break(std::string(90, 'W')) == std::string(28, 'W') + '\n' + std::string(62, 'W'));
    CHECK(comu_line_break("A\r\nB") == "A\r\nB");
    std::string japanese;
    for (int i = 0; i < 14; ++i) japanese += "\x82\xA0";
    CHECK(comu_line_break(japanese) == japanese);
    CHECK(comu_line_break(japanese + "\x82\xA2") == japanese + "\n\x82\xA2");
    CHECK(comu_line_break(std::string(27, 'A') + "\x82\xA0") == std::string(27, 'A') + "\n\x82\xA0");
    CHECK(comu_line_break(std::string(28, char(0xA5)) + "X") == std::string(28, char(0xA5)) + "\nX");
    std::string utf8_japanese;
    for (int i = 0; i < 14; ++i) utf8_japanese += u8"あ";
    CHECK(comu_line_break(utf8_japanese + u8"い", true) == utf8_japanese + u8"\nい");
    CHECK(comu_line_break(std::string(27, 'A') + u8"ｱB", true) == std::string(27, 'A') + u8"ｱ\nB");
}

void write_file(const fs::path& path, const std::vector<uint8_t>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    if (!out) throw std::runtime_error("cannot write COMU fixture");
}
struct Script {
    std::vector<uint8_t> bytes{1,0,0,0,3,0,0,0,0,0,0xB6,0xAA,0};
    void command(uint16_t value) {
        bytes.push_back(0xAB); bytes.push_back(uint8_t(value)); bytes.push_back(uint8_t(value >> 8));
        bytes.insert(bytes.end(), 6, 0);
    }
    void param(uint8_t op, uint8_t a, uint8_t b) { bytes.insert(bytes.end(), {op, a, b}); }
    void str(uint8_t op, const std::string& value) {
        bytes.push_back(op); bytes.insert(bytes.end(), value.begin(), value.end()); bytes.push_back(0);
    }
    void text(uint8_t opcode, const std::string& value) {
        bytes.push_back(opcode);
        for (unsigned char ch : patch_spaces(value)) bytes.push_back(uint8_t(ch - 0x20));
        bytes.push_back(0);
    }
    void group(const std::vector<std::string>& messages) {
        command(1);
        for (const auto& message : messages) text(0x6D, message);
        command(0); text(0x6B, "STOP");
    }
};
void write_sheet(const fs::path& path, int width, int height) {
    std::vector<uint8_t> bytes{'C','R','X','G',0,0,0,0};
    for (uint16_t v : {uint16_t(width), uint16_t(height), uint16_t(1), uint16_t(0), uint16_t(0), uint16_t(0)}) {
        bytes.push_back(uint8_t(v)); bytes.push_back(uint8_t(v >> 8));
    }
    // A black sheet isolates text pixels; the stamp cell is filled separately below.
    std::vector<uint8_t> pixels(size_t((width * 3 + 3) & ~3) * height, 0);
    if (width == 640) std::fill(pixels.begin(), pixels.end(), 160);
    if (width == 740) {
        for (int y = 0; y < height; ++y) for (int x = 420; x < 484; ++x)
            pixels[(size_t(y) * width + x) * 3 + 2] = uint8_t(20 + (y / 81) * 20);
    }
    for (size_t i = 0; i < pixels.size(); i += 8) {
        bytes.push_back(255);
        bytes.insert(bytes.end(), pixels.begin() + i, pixels.begin() + std::min(i + 8, pixels.size()));
    }
    write_file(path, bytes);
}
std::vector<uint8_t> crop(const std::vector<uint8_t>& frame, int x, int y, int w, int h) {
    std::vector<uint8_t> result;
    for (int row = y; row < y + h; ++row) {
        const auto start = frame.begin() + (row * 1280 + x) * 4;
        result.insert(result.end(), start, start + w * 4);
    }
    return result;
}
bool ink(const std::vector<uint8_t>& pixels) {
    for (size_t i = 0; i < pixels.size(); i += 4) if (pixels[i] > 80) return true;
    return false;
}
std::vector<uint8_t> render(const fs::path& root, uint8_t version,
                           const std::vector<std::string>& messages,
                           const dc4scene::FontRenderer& ui, const dc4scene::FontRenderer* text,
                           int dialogue_size = 22, const fs::path& assets = {}) {
    Script script; script.bytes[11] = version; script.group(messages);
    write_file(root / "MES/render.mes", script.bytes);
    dc4scene::SceneRunner scene((assets.empty() ? root : assets).string());
    scene.set_font(&ui, text); scene.set_dialogue_font_size(dialogue_size);
    CHECK(scene.load_script((root / "MES/render.mes").string()));
    CHECK(scene.advance_to_next_text()); scene.draw(-1);
    return scene.frame();
}
void scene_tests(const fs::path& root, const dc4scene::FontRenderer& ui, const dc4scene::FontRenderer& text) {
    write_sheet(root / "GRP/SYSTEM/COMUCHIP.CRX", 740, 891);
    write_sheet(root / "GRP/SYSTEM/comustamp.CRX", 640, 512);
    for (uint8_t version : {0xAA, 0xAB}) {
        Script identity; identity.bytes[11] = version;
        write_file(root / "MES/main.mes", identity.bytes);
        write_file(root / "MES/dc4_start.mes", identity.bytes);
        const auto plain = render(root, version, {"WiMi "}, ui, &text);
        const auto padded = render(root, version, {"  WiMi"}, ui, &text);
        CHECK(ink(crop(plain, 540, 94, 50, 34)));
        CHECK(crop(plain, 538, 94, 100, 34) == crop(padded, 554, 94, 100, 34));
        CHECK(!ink(crop(padded, 538, 94, 16, 34)));
        const auto two = render(root, version, {std::string("WiMi") + std::string(24, ' ') + "WiMi"}, ui, &text);
        CHECK(crop(two, 538, 94, 100, 34) == crop(plain, 538, 94, 100, 34));
        CHECK(crop(two, 538, 102, 100, 28) == crop(two, 538, 130, 100, 28));
        CHECK(crop(two, 538, 80, 250, 400) == crop(render(root, version,
            {std::string("WiMi") + std::string(24, ' ') + "WiMi"}, ui, &text, 30), 538, 80, 250, 400));
        CHECK(crop(plain, 538, 94, 100, 62) != crop(render(root, version, {"WiMi "}, ui, nullptr), 538, 94, 100, 62));
        const auto long_line = render(root, version, {std::string(200, 'W') + ' '}, ui, &text);
        CHECK(ink(crop(long_line, 538, 122, 238, 34)));
        CHECK(!ink(crop(long_line, 778, 94, 80, 68)));
        CHECK(!ink(crop(long_line, 538, 156, 240, 100)));
        CHECK(!ink(crop(render(root, version, {"A\nB\nC"}, ui, &text), 538, 156, 240, 50)));
        const std::string sender = "\x81\x75\x93\xF1\x94\x54\x81\x76";
        CHECK(crop(render(root, version, {sender + "  WiMi"}, ui, &text), 538, 94, 240, 62) ==
              crop(padded, 538, 94, 240, 62));

        const char* japanese_names[] = {"\x97\x4c\x97\xa2\x90\xb2", "\x93\xf1\x94\x54", "\xe6\x7d\x8e\x71",
            "\x82\xd0\x82\xe6\x82\xe8", "\x8e\x8d\x96\xbc", "\x96\xa2\x89\x48", "\x82\xbf\x82\xe6\x8e\x71",
            "\x97\x4c\x97\xa2\x8d\xe7", "\x88\xea\x93\x6f", "\x90\x99\x95\xc0", "\x8a\x90\x95\xfb"};
        for (int contact = 0; contact < 11; ++contact) {
            const auto roman = render(root, version, {::sender(romanized_names[contact]) + "  WiMi"}, ui, &text);
            CHECK(roman == render(root, version, {::sender(japanese_names[contact]) + "  WiMi"}, ui, &text));
            if (contact != 1) CHECK(crop(roman, 468, 80, 64, 81) != crop(padded, 468, 80, 64, 81));
        }
        const auto prose = render(root, version, {::sender("Ichito") + del_spaces(sakura_messages[0])}, ui, &text);
        const auto reference = render(root, version, {::sender("Ichito") + "That's where we're at. Any  ideas?"}, ui, &text);
        CHECK(crop(prose, 550, 94, 228, 62) == crop(reference, 538, 94, 228, 62));
        CHECK(!ink(crop(prose, 540, 94, 10, 62)));
        dc4::set_language(dc4::Language::English);
        CHECK(render(root, version, {::sender("Ichito") + del_spaces(sakura_messages[0])}, ui, &text) == prose);
        CHECK(crop(render(root, version, {::sender("Ichito") + "  WiMi"}, ui, &text), 538, 94, 240, 62) ==
              crop(padded, 538, 94, 240, 62));
        dc4::configure_resources(root.string(), {}, "utf-8");
        const auto utf8_frame = render(root, version, {::sender("Ichito") + del_spaces(sakura_messages[0])}, ui, &text);
        CHECK(crop(utf8_frame, 440, 16, 372, 518) == crop(prose, 440, 16, 372, 518));
        dc4::configure_resources({});
        dc4::set_language(dc4::Language::Indonesian);

        Script prose_state; prose_state.bytes[11] = version;
        prose_state.group({::sender("Kanata") + del_spaces(sakura_messages[8])});
        prose_state.command(65535); prose_state.group({});
        write_file(root / "MES/prose-state.mes", prose_state.bytes);
        dc4scene::SceneRunner prose_scene(root.string()); prose_scene.set_font(&ui, &text);
        CHECK(prose_scene.load_script((root / "MES/prose-state.mes").string()));
        CHECK(prose_scene.advance_to_next_text()); prose_scene.draw(-1);
        const auto prose_frame = prose_scene.frame();
        std::vector<uint8_t> prose_saved; CHECK(prose_scene.capture_state(&prose_saved));
        CHECK(prose_scene.advance_to_next_text());
        CHECK(prose_scene.restore_state(prose_saved)); prose_scene.draw(-1);
        CHECK(prose_scene.frame() == prose_frame);
        const auto long_layout = dc4scene::comu_prose_layout(dc4scene::sjis_to_utf16(del_spaces(sakura_messages[8])), text);
        CHECK(long_layout.size < 16);
        std::vector<uint8_t> expected(240 * 62 * 4, 0);
        const size_t line_break = long_layout.text.find(u'\n');
        CHECK(line_break != std::u16string::npos);
        text.rasterize_line(long_layout.text.substr(0, line_break), 2, 30, &expected, 240, 62,
                            float(long_layout.size), 0xFFFFFF, false, 0, true);
        text.rasterize_line(long_layout.text.substr(line_break + 1), 2, 58, &expected, 240, 62,
                            float(long_layout.size), 0xFFFFFF, false, 0, true);
        const auto actual = crop(prose_frame, 550, 94, 240, 62);
        for (size_t i = 0; i < expected.size(); i += 4)
            CHECK(actual[i] == int(expected[i]) * expected[i + 3] / 255);

        Script script; script.bytes[11] = version;
        script.group({sender + "  Begitulah situasinya, ada   ide?"});
        script.command(98); script.text(0x6B, "HIDDEN");
        script.group({}); script.command(65535); script.group({}); script.group({"@d1"});
        write_file(root / "MES/state.mes", script.bytes);
        dc4scene::SceneRunner scene(root.string()); scene.set_font(&ui, &text);
        CHECK(scene.load_script((root / "MES/state.mes").string()));
        CHECK(scene.advance_to_next_text()); scene.draw(-1); const auto visible = scene.frame();
        CHECK(ink(crop(visible, 538, 122, 100, 34)));
        std::vector<uint8_t> saved; CHECK(scene.capture_state(&saved));
        CHECK(scene.advance_to_next_text()); scene.draw(-1); CHECK(!ink(crop(scene.frame(), 440, 16, 372, 518)));
        CHECK(scene.advance_to_next_text()); scene.draw(-1); CHECK(scene.frame() == visible);
        CHECK(scene.advance_to_next_text()); scene.draw(-1); CHECK(!ink(crop(scene.frame(), 440, 16, 372, 518)));
        CHECK(scene.advance_to_next_text()); scene.draw(-1); CHECK(ink(crop(scene.frame(), 480, 96, 79, 63)));
        CHECK(scene.restore_state(saved)); scene.draw(-1); CHECK(scene.frame() == visible);
        for (bool preserve : {false, true}) {
            Script commit; commit.bytes[11] = version;
            commit.group({"  WiMi"});
            commit.str(0x49, preserve ? ":n" : ":r");
            commit.bytes.push_back(0x3C); commit.str(1, "BG\\BLACK");
            commit.text(0x6B, "PRELOADED");
            commit.param(0x0F, 32, 0);
            commit.param(0x37, 0, 0); commit.str(0x49, "BG\\BLACK");
            commit.bytes.push_back(0xB4); commit.bytes.insert(commit.bytes.end(), 8, 0);
            commit.param(0x28, 13, 0); commit.param(0x0F, 0, 0); commit.param(0x15, 254, 0);
            commit.text(0x6B, "NARRATION"); commit.group({});
            write_file(root / "MES/commit.mes", commit.bytes);
            dc4scene::SceneRunner cleared(root.string()); cleared.set_font(&ui, &text);
            cleared.set_animation_enabled(true);
            CHECK(cleared.load_script((root / "MES/commit.mes").string()));
            CHECK(cleared.advance_to_next_text()); cleared.draw(-1);
            const auto phone = crop(cleared.frame(), 440, 16, 372, 518);
            CHECK(cleared.advance_to_next_text() && cleared.current_line() == "PRELOADED");
            cleared.draw(-1); CHECK(crop(cleared.frame(), 440, 16, 372, 518) == phone);
            CHECK(!cleared.advance_to_next_text() && cleared.waiting_for_time());
            cleared.draw(-1); CHECK(crop(cleared.frame(), 440, 16, 372, 518) == phone);
            cleared.update(334); CHECK(cleared.script_ready());
            CHECK(cleared.advance_to_next_text() && cleared.current_line() == "NARRATION");
            cleared.draw(-1); CHECK(!ink(crop(cleared.frame(), 440, 16, 372, 518)));
            std::vector<uint8_t> cleared_save; CHECK(cleared.capture_state(&cleared_save));
            CHECK(cleared.advance_to_next_text()); cleared.draw(-1);
            CHECK(!ink(crop(cleared.frame(), 440, 16, 372, 518)));
            CHECK(cleared.restore_state(cleared_save)); cleared.draw(-1);
            CHECK(!ink(crop(cleared.frame(), 440, 16, 372, 518)));
        }
        const auto rows = render(root, version, {"WiMi ", "WiMi ", "WiMi ", "WiMi ", "WiMi ", "WiMi "}, ui, &text);
        for (int slot = 0; slot < 5; ++slot)
            CHECK(crop(rows, 538, 94 + slot * 81, 100, 62) == crop(plain, 538, 94, 100, 62));
    }
}
void write_ppm(const fs::path& path, const std::vector<uint8_t>& frame) {
    std::ofstream out(path, std::ios::binary); out << "P6\n1280 720\n255\n";
    for (size_t i = 0; i < frame.size(); i += 4) out.write(reinterpret_cast<const char*>(&frame[i]), 3);
}

void custom_font_tests(const fs::path& root, const dc4scene::FontRenderer& ui,
                       const dc4scene::FontRenderer& font) {
    CHECK(font.text_width(u"iiii", 16) < font.text_width(u"WWWW", 16));
    for (uint8_t version : {0xAA, 0xAB}) {
        for (const char* message : {sakura_messages[0], sakura_messages[8], sakura_messages[10], "see", "Understood",
                                   "iiiiiiiiiiii WWWWWWWW iiiii WWWWWWWWW iiiiiiiii"}) {
            const auto raw = del_spaces(message);
            prose_check(raw, font);
            const auto layout = dc4scene::comu_prose_layout(dc4scene::sjis_to_utf16(raw), font);
            CHECK(!layout.fixed_grid);
            if (std::string(message) == "see") CHECK(layout.size >= 18 && layout.text == u"see");
            const auto frame = render(root, version, {sender("Ichito") + raw}, ui, &font);
            std::vector<uint8_t> expected(240 * 62 * 4, 0);
            std::vector<uint8_t> unclipped(240 * 102 * 4, 0);
            size_t start = 0;
            for (int baseline = layout.baseline(); baseline < 62; baseline += layout.line_pitch()) {
                const auto end = layout.text.find(u'\n', start);
                font.rasterize_line(layout.text.substr(start, end == std::u16string::npos ? end : end - start),
                    2, baseline, &expected, 240, 62, float(layout.size));
                font.rasterize_line(layout.text.substr(start, end == std::u16string::npos ? end : end - start),
                    2, baseline + 20, &unclipped, 240, 102, float(layout.size));
                if (end == std::u16string::npos) break;
                start = end + 1;
            }
            const auto actual = crop(frame, 550, 94, 240, 62);
            bool equal = true;
            for (size_t i = 0; i < expected.size(); i += 4)
                equal = equal && actual[i] == int(expected[i]) * expected[i + 3] / 255;
            CHECK(equal);
            bool fits = true;
            for (int y = 0; y < 102; ++y) if (y < 20 || y >= 82)
                for (int x = 0; x < 240; ++x)
                    fits = fits && unclipped[(y * 240 + x) * 4 + 3] == 0;
            CHECK(fits);
            CHECK(!ink(crop(frame, 540, 94, 10, 62)));
            CHECK(!ink(crop(frame, 782, 94, 20, 62)));
            CHECK(!ink(crop(frame, 550, 156, 240, 5)));
        }
        const auto plain = render(root, version, {"WiMi "}, ui, &font);
        const auto padded = render(root, version, {"  WiMi"}, ui, &font);
        CHECK(crop(plain, 538, 94, 100, 34) == crop(padded, 554, 94, 100, 34));
    }
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "usage: comu_text_test FONT [OUTPUT_DIR] [ORIGINAL_ADVDATA] [CORPUS_HEX] [CUSTOM_FONT]\n"; return 2; }
    const fs::path root = argc > 2 ? fs::path(argv[2]) : fs::current_path() / "comu-text-test-data";
    fs::create_directories(root);
    dc4::set_language(dc4::Language::Indonesian);
    dc4scene::FontRenderer ui, text;
    CHECK(ui.load(argv[1], 1, 22, false)); CHECK(text.load(argv[1], 0, 22, false));
    break_tests(); prose_tests(text); scene_tests(root, ui, text);
    custom_font_tests(root, ui, ui);
    dc4scene::FontRenderer custom;
    if (argc > 5) {
        CHECK(custom.load(argv[5], 0, 22, false));
        CHECK(dc4scene::comu_prose_layout(u"see", custom).size == 22);
        custom_font_tests(root, ui, custom);
    }
    if (argc > 3) {
        const std::string kazuto = "\x81\x75\x88\xEA\x93\x6F\x81\x76";
        const std::string sorako = "\x81\x75\xE6\x7D\x8E\x71\x81\x76";
        const std::string nino = "\x81\x75\x93\xF1\x94\x54\x81\x76";
        const auto frame = render(root, 0xAB, {kazuto + "  Begitulah situasinya, ada   ide?",
            sorako + "  Tergantung seberapa         percaya mereka.",
            nino + "  Intinya kita harus          terlihat bisa diandalkan."}, ui, &text, 22, argv[3]);
        write_ppm(root / "recording-messages.ppm", frame);
        if (argc > 5)
            write_ppm(root / "custom-padded.ppm", render(root, 0xAB,
                {kazuto + "  Begitulah situasinya, ada   ide?",
                 sorako + "  Tergantung seberapa         percaya mereka.",
                 nino + "  Intinya kita harus          terlihat bisa diandalkan."}, ui, &custom, 22, argv[3]));
        const int contacts[] = {8,2,1,9,9,10,10,10,10,10,1,1,9,9,9,1,8};
        for (int first : {0,5,7,10,15}) {
            std::vector<std::string> messages;
            const int end = std::min(first + 5, 17);
            for (int i = first; i < end; ++i)
                messages.push_back(sender(romanized_names[contacts[i]]) + del_spaces(sakura_messages[i]));
            write_ppm(root / ("sakura-" + std::to_string(first) + ".ppm"),
                render(root, 0xAB, messages, ui, &text, 22, argv[3]));
            if (argc > 5)
                write_ppm(root / ("custom-" + std::to_string(first) + ".ppm"),
                    render(root, 0xAB, messages, ui, &custom, 22, argv[3]));
        }
    }
    if (argc > 4) {
        std::ifstream corpus(argv[4]); CHECK(bool(corpus));
        std::string hex; int count = 0, short_messages = 0, custom_min = 22, custom_max = 0, three_lines = 0;
        while (std::getline(corpus, hex)) {
            std::string raw;
            for (size_t i = 0; i + 1 < hex.size(); i += 2)
                raw.push_back(char(std::stoi(hex.substr(i, 2), nullptr, 16)));
            if (raw.find(char(0x7F)) == std::string::npos) {
                CHECK(raw.size() <= 28);
                CHECK(dc4scene::comu_line_break(raw) == raw);
                ++short_messages;
            } else ++count;
            prose_check(raw, text);
            if (argc > 5) {
                const auto layout = prose_check(raw, custom);
                custom_min = std::min(custom_min, layout.size);
                custom_max = std::max(custom_max, layout.size);
                if (layout.lines == 3) ++three_lines;
            }
        }
        std::cout << "Corpus prose messages checked: " << count << "; short messages: " << short_messages << '\n';
        CHECK(count > 400);
        if (argc > 5) std::cout << "Custom font sizes: " << custom_min << '-' << custom_max
                               << "; three-line messages: " << three_lines << '\n';
    }
    std::cout << "COMU checks: " << failures << " failures\n";
    return failures ? 1 : 0;
}
