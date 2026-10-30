// MES command-stream walker (CIRCUS adv32, D.C.4)
// Mirrors the engine's fetcher (sub_438790). Verified against all 330 game
// scripts: 50,707/50,707 labels land on command boundaries.
#pragma once
#include "arisa32/mesopcode.h"
#include <string>
#include <vector>

namespace dc4 {

struct MesCommand {
    bool dc2pc = false; // Exact MES 0x9969 profile; retain the raw opcode.
    bool dc2dm = false;
    CmdKind kind = CmdKind::Nop;
    uint32_t pos = 0;      // offset of the opcode byte within `code`
    uint8_t op = 0;
    uint8_t raw_op = 0;    // original opcode after service translation
    uint8_t p1 = 0, p2 = 0;             // Param2 / ParamStr param
    uint16_t words[4] = {};             // Words4
    std::string str;                    // Str / ParamStr / Text (decoded)
    size_t size = 0;                    // total bytes consumed incl. opcode
    Opcode opcode() const { return static_cast<Opcode>(op); }
};

// Adapt recovered game operations to the engine's service vocabulary.
// Unknown commands cannot fall through to a same-number DC4 operation.
MesCommand mes_engine_command(const MesCommand& raw);

// Decodes stored text (real SJIS byte - 0x20) into real SJIS bytes.
std::string mes_decode_text(const uint8_t* data, size_t len);

class MesStream {
public:
    explicit MesStream(const MesFile& mes)
        : code_(mes.code), dialect_(mes.dialect), profile_(mes_opcode_profile(mes.dialect, mes.version)),
          pos_(profile_.header_size) {}

    // Reads the next command. Returns false at end of code.
    bool next(MesCommand* out);

    uint32_t pos() const { return pos_; }
    void seek(uint32_t label_offset) { pos_ = label_offset; }
    MesDialect dialect() const { return dialect_; }

private:
    const std::vector<uint8_t>& code_;
    MesDialect dialect_ = MesDialect::Modern;
    const MesOpcodeProfile& profile_;
    size_t pos_ = 0;
};

}  // namespace dc4
