#pragma once
#include "arisa32/mesfile.h"
#include <array>

namespace dc4 {

enum class CmdKind { Nop, Param2, ParamStr, Str, Text, Words4 };
enum class MesProfileId { Modern, ClassicDC2, ClassicDM, DC2PC };
enum class OperandTransform { None, ButtonSource, StaticState, ByteAudio };

// Values are shared engine service IDs. Operand kind still matters for legacy
// dialects, where the same byte can name a different service.
enum class Opcode : uint8_t {
    END = 0x00,
    SCOPE_MARKER = 0x02,
    BGM_VOLUME = 0x05,
    CLEAR_CHOICES = 0x09,
    WAIT_INPUT = 0x0A,
    RETURN = 0x0B,
    EXIT = 0x0C,
    WAIT_INPUT_ALT = 0x0D,
    TRANSITION = 0x0F,
    STOP_BGM = 0x13,
    WAIT = 0x15,
    END_CONTROL = 0x1A,
    CHECKPOINT = 0x1C,
    RESTORE_CHECKPOINT = 0x1D,
    SYSTEM_SERVICE = 0x1E,
    SCOPE = 0x20,
    CLEAR_LAYOUT = 0x24,
    LEGACY_LOAD_SHEET = 0x2A,
    LEGACY_LOAD_SLOT = 0x2B,
    TRANSFER_WITH_PARAM = 0x2E,

    LEGACY_BACKGROUND = 0x30,
    LEGACY_SHEET_1 = 0x31,
    LEGACY_SHEET_2 = 0x32,
    LEGACY_SHEET_3 = 0x33,
    LEGACY_LABEL_DM = 0x34,
    LEGACY_LABEL_OR_VOICE = 0x35,
    COMMIT_TRANSITION = 0x36,
    RESET_TRANSITION = 0x37,
    LEGACY_JUMP = 0x3A,
    LEGACY_ENTRY = 0x3B,
    LOAD_SLOT = 0x3C,
    LOAD_SHEET = 0x3D,
    CHARACTER_LAYER = 0x3F,
    EFFECT_LAYER = 0x40,
    FACE_LAYER = 0x41,
    INPUT_SKIP_TARGET = 0x43,
    LEGACY_BUTTON_TARGET = 0x48,
    BACKGROUND = 0x49,
    LEGACY_SPEAKER = 0x4A,
    LABEL = 0x4B,
    VOICE = 0x4C,
    VOICE_ALT = 0x4D,
    VOICE_EXTRA = 0x4E,
    TRANSFER = 0x4F,
    TRANSFER_ALT = 0x50,
    ENTRY = 0x51,
    MOVIE = 0x52,
    LEGACY_CHIP_POSITION = 0x53,
    JUMP = 0x54,
    LEGACY_CHIP_POSITION_ALT = 0x55,
    LEGACY_CHIP = 0x57,
    CHOICE = 0x5C,
    STRING_COMPANION = 0x5F,
    LEGACY_ASSIGN = 0x62,
    LEGACY_COMPARE = 0x66,

    TEXT = 0x69,
    SPEAKER = 0x6A,
    DIALOGUE = 0x6B,
    TEXT_ALT = 0x6C,
    CHAT_TEXT = 0x6D,
    PLAY_BGM = 0x6F,
    PLAY_BGM_ONCE = 0x70,
    PLAY_SE = 0x71,
    CONTROL = 0x72,
    BUTTON_POSITION = 0x73,
    LIST_BACKGROUND_POSITION = 0x74,
    LIST_TITLE_POSITION = 0x75,
    CONTROL_CAPTION = 0x76,
    CHIP = 0x77,
    CHIP_POSITION = 0x7A,
    LIST_RECT = 0x7E,
    LIST_ID = 0x7F,
    LEGACY_LIST_ITEM = 0x80,
    LIST_TAG = 0x81,

    ASSIGN = 0x82,
    COPY = 0x83,
    SUM = 0x84,
    ADD = 0x85,
    COMPARE = 0x86,
    CAPTION = 0x87,
    SPRITE_TRANSFORM = 0x8B,
    SPRITE_EFFECT = 0x8C,
    SPRITE_SLIDE = 0x8E,
    PLAY_EFFECT = 0x95,
    CAMERA_DURATION = 0x99,
    CAMERA = 0x9D,
    PHONE_CHAT = 0xAB,
    AUDIO_PAN = 0xAF,
    COMMIT_LAYERS = 0xB4,
    UNKNOWN = 0xFF
};

constexpr uint8_t opcode_byte(Opcode opcode) { return static_cast<uint8_t>(opcode); }

struct OpcodeLayout {
    CmdKind kind = CmdKind::Nop;
    Opcode operation = Opcode::END;
    CmdKind engine_kind = CmdKind::Nop;
    OperandTransform transform = OperandTransform::None;
};

struct MesOpcodeProfile {
    MesProfileId id;
    const char* name;
    size_t header_size;
    std::array<OpcodeLayout, 256> opcodes;
};

const MesOpcodeProfile& mes_opcode_profile(MesDialect dialect, uint32_t version = 0);
// Returns zero for an incomplete command; used by loading and decoding.
size_t mes_command_size(const uint8_t* code, size_t size, size_t pos, CmdKind kind);

} // namespace dc4
