#include "arisa32/messtream.h"

namespace dc4 {
std::string mes_decode_text(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve(len);
    for (size_t i = 0; i < len; ++i) out.push_back(char(data[i] + 0x20));
    return out;
}

bool MesStream::next(MesCommand* out) {
    if (!out) return false;
    *out = MesCommand{};
    if (pos_ >= code_.size()) return false;
    out->dc2pc = profile_.id == MesProfileId::DC2PC;
    out->pos = uint32_t(pos_);
    out->op = code_[pos_];
    out->kind = profile_.opcodes[out->op].kind;
    out->size = mes_command_size(code_.data(), code_.size(), pos_, out->kind);
    if (!out->size) return false;
    const auto* operands = code_.data() + pos_ + 1;
    switch (out->kind) {
    case CmdKind::Param2:
        out->p1 = operands[0]; out->p2 = operands[1]; break;
    case CmdKind::ParamStr:
        out->p1 = operands[0];
        out->str.assign(reinterpret_cast<const char*>(operands + 1), out->size - 3);
        break;
    case CmdKind::Str:
        out->str.assign(reinterpret_cast<const char*>(operands), out->size - 2);
        break;
    case CmdKind::Text:
        out->str = mes_decode_text(operands, out->size - 2); break;
    case CmdKind::Words4:
        for (int i = 0; i < 4; ++i)
            out->words[i] = uint16_t(operands[2*i] | (operands[2*i+1] << 8));
        break;
    default: break;
    }
    pos_ += out->size;
    return true;
}

MesCommand mes_engine_command(const MesCommand& raw) {
    if (!raw.dc2pc) return raw;
    MesCommand c = raw;
    c.raw_op = raw.op;
    const auto& opcode = mes_opcode_profile(MesDialect::ClassicDC2, 0x9969).opcodes[raw.op];
    c.op = opcode_byte(Opcode::UNKNOWN);
    if (raw.kind != opcode.kind) return c;
    c.op = opcode_byte(opcode.operation);
    c.kind = opcode.engine_kind;
    switch (opcode.transform) {
    case OperandTransform::ByteAudio:
        c.words[0] = raw.p1;
        c.words[1] = raw.p2;
        break;
    case OperandTransform::ButtonSource:
        c.words[2] = uint16_t(raw.words[0] - raw.words[2]);
        c.words[3] = uint16_t(raw.words[1] - raw.words[3]);
        break;
    case OperandTransform::StaticState:
        if (c.words[3] == 9) c.words[3] = 99;
        break;
    default: break;
    }
    return c;
}
}
