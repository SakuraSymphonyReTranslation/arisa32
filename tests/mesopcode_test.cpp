#include "arisa32/messtream.h"
#include <iostream>

namespace {
int failures = 0;
void check(bool value, int line) {
    if (!value) {
        std::cerr << "line " << line << '\n';
        ++failures;
    }
}
#define CHECK(x) check(bool(x), __LINE__)
dc4::CmdKind expected(dc4::MesDialect dialect, unsigned op) {
    using K = dc4::CmdKind;
    if (!op)
        return K::Nop;
    switch (dialect) {
    case dc4::MesDialect::ClassicDC2:
        return op < 0x29   ? K::Param2
               : op < 0x2F ? K::ParamStr
               : op < 0x4A ? K::Str
               : op < 0x4E ? K::Text
                           : K::Words4;
    case dc4::MesDialect::ClassicDM:
        return op < 0x2A   ? K::Param2
               : op < 0x32 ? K::ParamStr
               : op < 0x4D ? K::Str
               : op < 0x51 ? K::Text
                           : K::Words4;
    default:
        if (op == 0x2E)
            return K::ParamStr;
        return op < 0x3B   ? K::Param2
               : op < 0x48 ? K::ParamStr
               : op < 0x69 ? K::Str
               : op < 0x6E ? K::Text
                           : K::Words4;
    }
}
void decode_all(dc4::MesDialect dialect, uint32_t version) {
    using K = dc4::CmdKind;
    const size_t header = dialect == dc4::MesDialect::ClassicDC2 ? 2 : 3;
    for (unsigned op = 0; op < 256; ++op) {
        dc4::MesFile file;
        file.dialect = dialect;
        file.version = version;
        file.code.resize(header, 0);
        file.code.push_back(uint8_t(op));
        const auto kind = expected(dialect, op);
        switch (kind) {
        case K::Nop:
            break;
        case K::Param2:
            file.code.insert(file.code.end(), {7, 8});
            break;
        case K::Words4:
            file.code.insert(file.code.end(), {1, 2, 3, 4, 5, 6, 7, 8});
            break;
        case K::ParamStr:
            file.code.push_back(7);
            [[fallthrough]];
        default:
            file.code.insert(file.code.end(), {'A', 'B', 0});
            break;
        }
        dc4::MesStream stream(file);
        dc4::MesCommand c;
        CHECK(stream.next(&c));
        CHECK(c.op == op);
        CHECK(c.kind == kind);
        CHECK(c.dc2pc == (dialect == dc4::MesDialect::ClassicDC2 && version == 0x9969));
        CHECK(c.dc2dm == (dialect == dc4::MesDialect::ClassicDM));
        CHECK(c.pos == header);
        CHECK(stream.pos() == file.code.size());
        CHECK(c.size == file.code.size() - header);
        CHECK(!stream.next(&c));
        for (size_t end = header; end < file.code.size(); ++end) {
            auto truncated = file;
            truncated.code.resize(end);
            dc4::MesStream short_stream(truncated);
            CHECK(!short_stream.next(&c));
            CHECK(short_stream.pos() == header);
        }
    }
}

void dc2dm_menu_and_story() {
    using O = dc4::Opcode;
    dc4::MesFile file;
    file.dialect = dc4::MesDialect::ClassicDM;
    file.version = 0x9D72;
    file.code = {0x72, 0x9D, 0x00};
    auto string = [&](uint8_t op, const char* value, uint8_t param = 0) {
        file.code.push_back(op);
        if (op >= 0x2A && op < 0x32)
            file.code.push_back(param);
        for (; *value; ++value)
            file.code.push_back(uint8_t(*value) - (op >= 0x4D && op < 0x51 ? 0x20 : 0));
        file.code.push_back(0);
    };
    auto words = [&](uint8_t op, uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
        file.code.push_back(op);
        for (uint16_t value : {a, b, c, d}) {
            file.code.push_back(uint8_t(value));
            file.code.push_back(uint8_t(value >> 8));
        }
    };
    // The title uses direct atlas coordinates and three distinct button states.
    string(0x2B, "SYSTEM\\TITLE_BACK03", 1);
    string(0x33, "SYSTEM\\TITLE_BACK03");
    string(0x2B, "SYSTEM\\TITLE_CHIP", 10);
    words(0x54, 440, 216, 584, 264);
    string(0x3D, "sce_start");
    for (uint16_t state : {99, 1, 2}) {
        words(0x55, 340, 216, 0, uint16_t((state == 99 ? 0 : state) * 48));
        words(0x57, 344, 48, 10, state);
    }
    file.code.insert(file.code.end(), {0x17, 0, 0, 0x09, 0, 0});
    string(0x34, "sce_start");
    string(0x38, "SCESELECT");
    string(0x2C, "SYSTEM\\MESWIN", 0);
    words(0x52, 55, 0, 0, 0);
    string(0x33, "BG\\BG_SORA004");
    string(0x36, "VOICE_01");
    string(0x4E, "Speaker");
    string(0x4F, "Dialogue");
    words(0x53, 223, 0, 0, 0);
    string(0x3B, "DC2DM_OP01.MPG");
    words(0x52, 255, 0, 0, 0);
    file.code.push_back(0);

    dc4::MesStream stream(file);
    auto next = [&](O expected, uint8_t raw_op) {
        dc4::MesCommand raw;
        CHECK(stream.next(&raw));
        auto c = dc4::mes_engine_command(raw);
        CHECK(c.opcode() == expected);
        CHECK(c.raw_op == raw_op);
        CHECK(c.dc2dm && !c.dc2pc);
        return c;
    };
    auto c = next(O::LOAD_SLOT, 0x2B);
    CHECK(c.p1 == 1 && c.str == "SYSTEM\\TITLE_BACK03");
    CHECK(next(O::BACKGROUND, 0x33).str == "SYSTEM\\TITLE_BACK03");
    CHECK(next(O::LOAD_SLOT, 0x2B).p1 == 10);
    c = next(O::CONTROL, 0x54);
    CHECK(c.words[0] == 440 && c.words[1] == 216 && c.words[2] == 584 && c.words[3] == 264);
    CHECK(next(O::JUMP, 0x3D).str == "sce_start");
    for (uint16_t state : {99, 1, 2}) {
        c = next(O::BUTTON_POSITION, 0x55);
        CHECK(c.words[0] == 340 && c.words[1] == 216 && c.words[2] == 0);
        CHECK(c.words[3] == (state == 99 ? 0 : state) * 48);
        c = next(O::CHIP, 0x57);
        CHECK(c.words[0] == 344 && c.words[1] == 48 && c.words[2] == 10 && c.words[3] == state);
    }
    next(O::END_CONTROL, 0x17);
    next(O::WAIT_INPUT, 0x09);
    CHECK(next(O::LABEL, 0x34).str == "sce_start");
    CHECK(next(O::TRANSFER, 0x38).str == "SCESELECT");
    CHECK(next(O::LOAD_SHEET, 0x2C).str == "SYSTEM\\MESWIN");
    CHECK(next(O::PLAY_BGM_ONCE, 0x52).words[0] == 55);
    CHECK(next(O::BACKGROUND, 0x33).str == "BG\\BG_SORA004");
    CHECK(next(O::VOICE, 0x36).str == "VOICE_01");
    CHECK(next(O::SPEAKER, 0x4E).str == "Speaker");
    CHECK(next(O::DIALOGUE, 0x4F).str == "Dialogue");
    CHECK(next(O::PLAY_SE, 0x53).words[0] == 223);
    CHECK(next(O::MOVIE, 0x3B).str == "DC2DM_OP01.MPG");
    CHECK(next(O::PLAY_BGM_ONCE, 0x52).words[0] == 255);
    next(O::END, 0x00);
    CHECK(stream.pos() == file.code.size());

    dc4::MesCommand raw;
    raw.dc2dm = true;
    raw.op = 0x08;
    raw.kind = dc4::CmdKind::Param2;
    CHECK(dc4::mes_engine_command(raw).opcode() == O::WAIT_INPUT);
    raw.op = 0x33;
    raw.kind = dc4::CmdKind::Words4;
    CHECK(dc4::mes_engine_command(raw).opcode() == O::UNKNOWN);
    raw.op = 0x32;
    raw.kind = dc4::CmdKind::Str;
    CHECK(dc4::mes_engine_command(raw).opcode() == O::UNKNOWN);
    raw.op = 0x37;
    CHECK(dc4::mes_engine_command(raw).opcode() == O::VOICE);
    raw.op = 0x6B;
    raw.kind = dc4::CmdKind::Words4;
    raw.words[0] = 100; raw.words[1] = 512; raw.words[2] = 322; raw.words[3] = 16;
    auto transform = dc4::mes_engine_command(raw);
    CHECK(transform.opcode() == O::SPRITE_TRANSFORM);
    for (size_t i = 0; i < 4; ++i) CHECK(transform.words[i] == raw.words[i]);
    raw.op = 0x50;
    raw.kind = dc4::CmdKind::Text;
    CHECK(dc4::mes_engine_command(raw).opcode() == O::UNKNOWN);
    raw.op = 0x15;
    raw.kind = dc4::CmdKind::Param2;
    CHECK(dc4::mes_engine_command(raw).opcode() == O::UNKNOWN);
    raw.op = 0x57;
    raw.kind = dc4::CmdKind::Words4;
    raw.words[3] = 9;
    CHECK(dc4::mes_engine_command(raw).words[3] == 9);
    raw.dc2dm = false;
    CHECK(dc4::mes_engine_command(raw).op == 0x57);
}
} // namespace
int main() {
    decode_all(dc4::MesDialect::Modern, 0xAAB6);
    decode_all(dc4::MesDialect::ClassicDC2, 0x9869);
    decode_all(dc4::MesDialect::ClassicDC2, 0x9969);
    decode_all(dc4::MesDialect::ClassicDM, 0x9D72);
    dc2dm_menu_and_story();
    dc4::MesCommand raw;
    raw.dc2pc = true;
    raw.kind = dc4::CmdKind::Text;
    raw.op = 0x4B;
    CHECK(dc4::mes_engine_command(raw).opcode() == dc4::Opcode::SPEAKER);
    raw.op = 0x4C;
    CHECK(dc4::mes_engine_command(raw).opcode() == dc4::Opcode::DIALOGUE);
    raw.op = 0x4D;
    CHECK(dc4::mes_engine_command(raw).opcode() == dc4::Opcode::UNKNOWN);
    raw.kind = dc4::CmdKind::Str;
    raw.op = 0x44;
    CHECK(dc4::mes_engine_command(raw).opcode() == dc4::Opcode::CHOICE);
    raw.op = 0x42;
    CHECK(dc4::mes_engine_command(raw).opcode() == dc4::Opcode::LIST_TAG);
    raw.kind = dc4::CmdKind::Param2;
    raw.op = 0x08;
    CHECK(dc4::mes_engine_command(raw).opcode() == dc4::Opcode::CLEAR_CHOICES);
    raw.op = 0x09;
    CHECK(dc4::mes_engine_command(raw).opcode() == dc4::Opcode::WAIT_INPUT);
    raw.kind = dc4::CmdKind::Words4;
    raw.op = 0x50;
    raw.words[0] = 743;
    raw.words[1] = 420;
    raw.words[2] = 65535;
    raw.words[3] = 288;
    auto translated = dc4::mes_engine_command(raw);
    CHECK(translated.op == 0x73);
    CHECK(translated.words[2] == 744);
    CHECK(translated.words[3] == 132);
    raw.op = 0x52;
    raw.words[3] = 9;
    CHECK(dc4::mes_engine_command(raw).words[3] == 99);
    raw.op = 0x64;
    CHECK(dc4::mes_engine_command(raw).raw_op == 0x64);
    CHECK(dc4::mes_engine_command(raw).opcode() == dc4::Opcode::SPRITE_TRANSFORM);
    raw.op = 0x65;
    CHECK(dc4::mes_engine_command(raw).raw_op == 0x65);
    raw.op = 0x82;
    CHECK(dc4::mes_engine_command(raw).op == 0xFF);
    raw.dc2pc = false;
    CHECK(dc4::mes_engine_command(raw).op == 0x82);
    std::cout << "Decoded all 256 opcodes in four profiles, including every truncated prefix.\n";
    return failures ? 1 : 0;
}
