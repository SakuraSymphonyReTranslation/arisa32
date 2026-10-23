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
} // namespace
int main() {
    decode_all(dc4::MesDialect::Modern, 0xAAB6);
    decode_all(dc4::MesDialect::ClassicDC2, 0x9869);
    decode_all(dc4::MesDialect::ClassicDC2, 0x9969);
    decode_all(dc4::MesDialect::ClassicDM, 0x9D72);
    dc4::MesCommand raw;
    raw.dc2pc = true;
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
    raw.op = 0x82;
    CHECK(dc4::mes_engine_command(raw).op == 0xFF);
    raw.dc2pc = false;
    CHECK(dc4::mes_engine_command(raw).op == 0x82);
    std::cout << "Decoded all 256 opcodes in four profiles, including every truncated prefix.\n";
    return failures ? 1 : 0;
}
