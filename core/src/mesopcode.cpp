#include "arisa32/mesopcode.h"
#include <cstring>

namespace dc4 {
namespace {
MesOpcodeProfile make_profile(MesProfileId id, const char* name, size_t header, uint8_t param_str,
                              uint8_t str, uint8_t text, uint8_t words) {
    MesOpcodeProfile profile{id, name, header, {}};
    const struct {
        unsigned first, end;
        CmdKind kind;
    } ranges[] = {{0, 1, CmdKind::Nop},
                  {1, param_str, CmdKind::Param2},
                  {param_str, str, CmdKind::ParamStr},
                  {str, text, CmdKind::Str},
                  {text, words, CmdKind::Text},
                  {words, 256, CmdKind::Words4}};
    for (const auto& range : ranges)
        for (unsigned op = range.first; op < range.end; ++op)
            profile.opcodes[op] = {range.kind, static_cast<Opcode>(op), range.kind};
    return profile;
}

const auto modern = [] {
    auto p = make_profile(MesProfileId::Modern, "modern", 3, 0x3B, 0x48, 0x69, 0x6E);
    // Modern 0x2E carries a string despite lying in the two-byte parameter range.
    p.opcodes[0x2E] = {CmdKind::ParamStr, Opcode::TRANSFER_WITH_PARAM, CmdKind::ParamStr};
    return p;
}();
const auto classic =
    make_profile(MesProfileId::ClassicDC2, "classic-dc2", 2, 0x29, 0x2F, 0x4A, 0x4E);
const auto marriage = [] {
    auto p = make_profile(MesProfileId::ClassicDM, "classic-dm", 3, 0x2A, 0x32, 0x4D, 0x51);
    for (auto& op : p.opcodes)
        op.operation = Opcode::UNKNOWN;
    // DC2DM main/start/scenario scripts use different service bytes from DC4.
    const struct {
        uint8_t raw;
        Opcode operation;
    } aliases[] = {{0x00, Opcode::END},
                   {0x02, Opcode::SCOPE_MARKER},
                   {0x07, Opcode::CLEAR_CHOICES},
                   {0x08, Opcode::WAIT_INPUT},
                   {0x09, Opcode::WAIT_INPUT},
                   {0x0A, Opcode::WAIT_INPUT_ALT},
                   {0x0D, Opcode::TRANSITION},
                   {0x12, Opcode::WAIT},
                   {0x17, Opcode::END_CONTROL},
                   {0x19, Opcode::CHECKPOINT},
                   {0x1B, Opcode::SYSTEM_SERVICE},
                   {0x1D, Opcode::SCOPE},
                   {0x21, Opcode::CLEAR_LAYOUT},
                   {0x2B, Opcode::LOAD_SLOT},
                   {0x2C, Opcode::LOAD_SHEET},
                   {0x2E, Opcode::CHARACTER_LAYER},
                   {0x2F, Opcode::FACE_LAYER},
                   {0x33, Opcode::BACKGROUND},
                   {0x34, Opcode::LABEL},
                   {0x35, Opcode::VOICE},
                   {0x36, Opcode::VOICE},
                   {0x37, Opcode::VOICE},
                   {0x38, Opcode::TRANSFER},
                   {0x39, Opcode::TRANSFER_ALT},
                   {0x3A, Opcode::ENTRY},
                   {0x3B, Opcode::MOVIE},
                   {0x3D, Opcode::JUMP},
                   {0x43, Opcode::CHOICE},
                   {0x4E, Opcode::SPEAKER},
                   {0x4F, Opcode::DIALOGUE},
                   {0x52, Opcode::PLAY_BGM_ONCE},
                   {0x53, Opcode::PLAY_SE},
                   {0x54, Opcode::CONTROL},
                   {0x55, Opcode::BUTTON_POSITION},
                   {0x56, Opcode::CONTROL_CAPTION},
                   {0x57, Opcode::CHIP},
                   {0x5A, Opcode::CHIP_POSITION},
                   {0x62, Opcode::ASSIGN},
                   {0x63, Opcode::COPY},
                   {0x64, Opcode::SUM},
                   {0x66, Opcode::COMPARE},
                   {0x67, Opcode::CAPTION},
                   {0x68, Opcode::CHIP_ORIGIN},
                   {0x6B, Opcode::SPRITE_TRANSFORM}};
    for (const auto& alias : aliases)
        p.opcodes[alias.raw].operation = alias.operation;
    return p;
}();
const auto dc2pc = [] {
    auto p = classic;
    p.id = MesProfileId::DC2PC;
    p.name = "dc2pc";
    // Only recovered DC2PC services may enter the shared engine vocabulary.
    for (auto& op : p.opcodes)
        op.operation = Opcode::UNKNOWN;
    const struct {
        uint8_t raw;
        Opcode operation;
    } aliases[] = {{0x00, Opcode::END},
                   {0x05, Opcode::PLAY_BGM_ONCE},
                   {0x02, Opcode::SCOPE_MARKER},
                   {0x08, Opcode::CLEAR_CHOICES},
                   {0x09, Opcode::WAIT_INPUT},
                   {0x0A, Opcode::WAIT_INPUT},
                   {0x0B, Opcode::WAIT_INPUT},
                   {0x0D, Opcode::TRANSITION},
                   {0x12, Opcode::WAIT},
                   {0x1E, Opcode::SCOPE},
                   {0x21, Opcode::CLEAR_LAYOUT},
                   {0x17, Opcode::END_CONTROL},
                   {0x2E, Opcode::TRANSFER},
                   {0x2A, Opcode::LOAD_SHEET},
                   {0x2B, Opcode::CHARACTER_LAYER},
                   {0x2C, Opcode::FACE_LAYER},
                   {0x30, Opcode::BACKGROUND},
                   {0x31, Opcode::LEGACY_SHEET_1},
                   {0x32, Opcode::LEGACY_SHEET_2},
                   {0x33, Opcode::LEGACY_SHEET_3},
                   {0x35, Opcode::LABEL},
                   {0x36, Opcode::VOICE},
                   {0x38, Opcode::MOVIE},
                   {0x3D, Opcode::VOICE},
                   {0x3A, Opcode::JUMP},
                   {0x3B, Opcode::ENTRY},
                   {0x37, Opcode::ENTRY},
                   {0x42, Opcode::LIST_TAG},
                   {0x44, Opcode::CHOICE},
                   {0x4A, Opcode::SPEAKER},
                   {0x4B, Opcode::SPEAKER},
                   {0x4C, Opcode::DIALOGUE},
                   {0x4F, Opcode::CONTROL},
                   {0x50, Opcode::BUTTON_POSITION},
                   {0x53, Opcode::CHIP_POSITION},
                   {0x52, Opcode::CHIP},
                   {0x55, Opcode::LIST_RECT},
                   {0x56, Opcode::CONTROL},
                   {0x57, Opcode::LEGACY_LIST_ITEM},
                   {0x58, Opcode::CONTROL_CAPTION},
                   {0x60, Opcode::CAPTION},
                   {0x64, Opcode::SPRITE_TRANSFORM},
                   {0x65, Opcode::SPRITE_TRANSFORM},
                   {0x5C, Opcode::ASSIGN},
                   {0x5D, Opcode::COPY},
                   {0x5E, Opcode::ADD},
                   {0x5F, Opcode::COMPARE}};
    for (const auto& alias : aliases)
        p.opcodes[alias.raw].operation = alias.operation;
    p.opcodes[0x2E].engine_kind = CmdKind::Str;
    p.opcodes[0x05].engine_kind = CmdKind::Words4;
    p.opcodes[0x05].transform = OperandTransform::ByteAudio;
    p.opcodes[0x50].transform = OperandTransform::ButtonSource;
    p.opcodes[0x52].transform = OperandTransform::StaticState;
    return p;
}();
const auto dc2ty = [] {
    auto p = dc2pc;
    p.id = MesProfileId::DC2TY;
    p.name = "dc2ty";
    return p;
}();
const auto dalsegno = [] {
    auto p = make_profile(MesProfileId::DalSegno, "dal-segno", 3, 0x39, 0x41, 0x5F, 0x63);
    for (auto& op : p.opcodes)
        op.operation = Opcode::UNKNOWN;
    const struct {
        uint8_t raw;
        Opcode operation;
    } aliases[] = {
        {0x00, Opcode::END},
        {0x02, Opcode::SCOPE_MARKER},
        {0x05, Opcode::BGM_VOLUME},
        {0x08, Opcode::CLEAR_CHOICES},
        {0x09, Opcode::WAIT_INPUT},
        {0x0A, Opcode::WAIT_INPUT},
        {0x0B, Opcode::RETURN},
        {0x0E, Opcode::TRANSITION},
        {0x10, Opcode::STOP_BGM},
        {0x12, Opcode::WAIT},
        {0x13, Opcode::WAIT_INPUT},
        {0x18, Opcode::END_CONTROL},
        {0x1A, Opcode::CHECKPOINT},
        {0x1C, Opcode::SYSTEM_SERVICE},
        {0x1E, Opcode::SCOPE},
        {0x22, Opcode::CLEAR_LAYOUT},

        {0x3A, Opcode::LOAD_SLOT},
        {0x3B, Opcode::LOAD_SHEET},
        {0x3D, Opcode::CHARACTER_LAYER},
        {0x3E, Opcode::FACE_LAYER},

        {0x42, Opcode::BACKGROUND},
        {0x43, Opcode::LABEL},
        {0x44, Opcode::VOICE},
        {0x45, Opcode::VOICE_ALT},
        {0x46, Opcode::VOICE_EXTRA},
        {0x47, Opcode::TRANSFER},
        {0x48, Opcode::TRANSFER_ALT},
        {0x4A, Opcode::MOVIE},
        {0x4C, Opcode::JUMP},
        {0x51, Opcode::ENTRY},
        {0x53, Opcode::CHOICE},
        {0x54, Opcode::CAPTION},
        {0x56, Opcode::SPEAKER},
        {0x5C, Opcode::VOICE_EXTRA},

        {0x5F, Opcode::TEXT},
        {0x60, Opcode::SPEAKER},
        {0x61, Opcode::DIALOGUE},
        {0x62, Opcode::TEXT},

        {0x64, Opcode::PLAY_BGM},
        {0x65, Opcode::PLAY_SE},
        {0x66, Opcode::CONTROL},
        {0x67, Opcode::CHIP_POSITION},
        {0x69, Opcode::CONTROL},
        {0x6A, Opcode::CHIP},
        {0x6D, Opcode::CHIP_POSITION},
        {0x75, Opcode::ASSIGN},
        {0x76, Opcode::COPY},
        {0x77, Opcode::SUM},
        {0x78, Opcode::ADD},
        {0x79, Opcode::COMPARE},
        {0x7A, Opcode::CONTROL_CAPTION},
        {0x7B, Opcode::CHIP_ORIGIN},
        {0x7C, Opcode::SPRITE_TRANSFORM},
        {0x7E, Opcode::SPRITE_EFFECT},
        {0x80, Opcode::SPRITE_SLIDE},
        {0x8E, Opcode::SPRITE_SLIDE},
        {0x90, Opcode::CAMERA_DURATION},
        {0x91, Opcode::CAMERA},
        {0x95, Opcode::PLAY_EFFECT},
        {0x96, Opcode::CAMERA_DURATION},
    };
    for (const auto& alias : aliases)
        p.opcodes[alias.raw].operation = alias.operation;
    return p;
}();
} // namespace

const MesOpcodeProfile& mes_opcode_profile(MesDialect dialect, uint32_t version) {
    switch (dialect) {
    case MesDialect::ClassicDC2:
        if ((version & 0xFFFF) == 0x5F69) return dc2ty;
        return (version & 0xFFFF) == 0x9969 ? dc2pc : classic;
    case MesDialect::ClassicDM:
        return marriage;
    default:
        if ((version & 0xFFFF) == 0x9F9A) return dalsegno;
        return modern;
    }
}

size_t mes_command_size(const uint8_t* code, size_t size, size_t pos, CmdKind kind) {
    if (!code || pos >= size)
        return 0;
    switch (kind) {
    case CmdKind::Nop:
        return 1;
    case CmdKind::Param2:
        return size - pos >= 3 ? 3 : 0;
    case CmdKind::Words4:
        return size - pos >= 9 ? 9 : 0;
    default: {
        const size_t prefix = kind == CmdKind::ParamStr ? 2 : 1;
        if (size - pos <= prefix)
            return 0;
        const auto* start = code + pos + prefix;
        const auto* end = static_cast<const uint8_t*>(std::memchr(start, 0, size - pos - prefix));
        return end ? size_t(end - (code + pos)) + 1 : 0;
    }
    }
}
} // namespace dc4
