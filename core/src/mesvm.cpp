#include "arisa32/mesvm.h"

#include <limits>
#include <sstream>

namespace dc4 {
namespace {

std::string to_lower(const std::string& str) {
    std::string out = str;
    for (char& c : out) c = char(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

int byte_bank_size(uint16_t bank) {
    if (bank <= 10) return 100;
    switch (bank) {
    case 96: return 400;
    case 97: return 1200;
    case 98: return 650;
    case 100: return 150;
    default: return 0;
    }
}

// sub_438400 sign-extends the word. MesStream retains raw bits so graphics
// code can also recognize sentinels; VM operands must recover the signed form.
int32_t signed_word(uint16_t word) {
    return word < 0x8000 ? int32_t(word) : int32_t(word) - 0x10000;
}

int32_t wrap_add(int32_t left, int32_t right) {
    const uint32_t bits = uint32_t(left) + uint32_t(right);
    return bits <= uint32_t(std::numeric_limits<int32_t>::max())
        ? int32_t(bits) : int32_t(int64_t(bits) - 0x100000000LL);
}

bool system_read_only(uint16_t index) {
    switch (index) {
    case 1: case 3: case 4: case 6: case 20: case 21: case 22:
    case 23: case 24: case 25: case 227: case 228: case 234:
    case 235: case 236: case 242: case 244: case 255: case 223:
        return true;
    default:
        return false;
    }
}

// Stable byte-order-independent FNV-1a identity, not a security/authenticity
// primitive. Include labels as well as bytecode: checkpoint metadata belongs
// to the script even though named jumps are indexed from the bytecode itself.
uint64_t script_fingerprint(const MesFile& mes) {
    uint64_t hash = 14695981039346656037ULL;
    const auto byte = [&](uint8_t value) {
        hash = (hash ^ value) * 1099511628211ULL;
    };
    const auto integer = [&](uint64_t value, int width) {
        for (int i = 0; i < width; ++i) byte(uint8_t(value >> (i * 8)));
    };
    integer(mes.version, 4);
    integer(mes.code.size(), 8);
    for (uint8_t value : mes.code) byte(value);
    integer(mes.labels.size(), 8);
    for (const auto& label : mes.labels) {
        integer(label.offset, 4);
        integer(label.flags, 4);
        integer(label.id, 2);
    }
    return hash;
}

bool snapshot_variable_valid(uint16_t bank, uint16_t index, int32_t value) {
    const int size = byte_bank_size(bank);
    if (size) return index < size && value >= 0 && value <= 255;
    if (bank == 93) return index < 20;
    if (bank == 94) return index == 50 && value <= 100;
    if (bank == 95) return index == 50;
    return bank == 99 && !system_read_only(index) && index != 217;
}

}  // namespace

void MesVm::reset() {
    stream_.reset();
    mes_ = nullptr;
    labels_.clear();
    variables_.clear();
    choices_.clear();
    entry_label_.clear();
    error_.clear();
    waiting_for_choice_ = transferring_ = ended_ = pending_button_action_ = false;
}

bool MesVm::bind(const MesFile& mes) {
    mes_ = &mes;
    stream_ = std::make_unique<MesStream>(mes);
    labels_.clear();
    choices_.clear();
    error_.clear();
    waiting_for_choice_ = transferring_ = ended_ = pending_button_action_ = false;
    if (mes.code.size() < 3) return fail("missing MES header", 0);

    MesStream scan(mes);
    MesCommand command;
    while (scan.pos() < mes.code.size()) {
        command = MesCommand{};
        if (!scan.next(&command))
            return fail("truncated MES command", scan.pos());
        if (command.op == 0) continue;
        if (command.kind == CmdKind::Str || command.kind == CmdKind::ParamStr) {
            if (command.op == 0x4B || command.op == 0x35 || command.op == 0x34 ||
                (!command.str.empty() && command.str[0] == ':')) {
                labels_.emplace(command.str, command.pos);
                std::string lower = to_lower(command.str);
                if (lower != command.str) labels_.emplace(lower, command.pos);
                if (!command.str.empty() && command.str[0] == ':') {
                    labels_.emplace(command.str.substr(1), command.pos);
                    std::string lower_sub = to_lower(command.str.substr(1));
                    if (lower_sub != command.str.substr(1)) labels_.emplace(lower_sub, command.pos);
                }
            }
        }
    }

    for (const auto& l : mes.labels) {
        labels_.emplace(std::to_string(l.id), l.offset);
    }

    if (!entry_label_.empty()) {
        const std::string entry = std::move(entry_label_);
        entry_label_.clear();
        jump(entry);
    }
    return true;
}

uint32_t MesVm::pos() const {
    return stream_ ? stream_->pos() : 0;
}

bool MesVm::capture_snapshot(MesVmSnapshot* out, std::string* err) const {
    if (!out || !mes_ || !stream_ || !error_.empty()) {
        if (err) *err = !error_.empty() ? "cannot save faulted VM: " + error_
                                      : "cannot save an unbound VM or null destination";
        return false;
    }
    MesVmSnapshot snapshot;
    snapshot.script_fingerprint = script_fingerprint(*mes_);
    snapshot.pc = pos();
    snapshot.variables = variables_;
    snapshot.entry_label = entry_label_;
    snapshot.choices = choices_;
    snapshot.ended = ended_;
    snapshot.transferring = transferring_;
    snapshot.waiting_for_choice = waiting_for_choice_;
    *out = std::move(snapshot);
    if (err) err->clear();
    return true;
}

bool MesVm::restore_snapshot(const MesVmSnapshot& snapshot, std::string* err) {
    const auto reject = [&](const char* message) {
        if (err) *err = message;
        return false;
    };
    if (!mes_ || !stream_) return reject("bind a MES script before restoring its VM");
    if (snapshot.version != MesVmSnapshot::kVersion)
        return reject("unsupported MES VM snapshot version");
    if (snapshot.script_fingerprint != script_fingerprint(*mes_))
        return reject("MES VM snapshot belongs to a different or modified script");
    if (snapshot.pc < 3 || snapshot.pc > mes_->code.size())
        return reject("MES VM snapshot program counter is outside the script");
    const unsigned states = unsigned(snapshot.ended) + unsigned(snapshot.transferring)
                          + unsigned(snapshot.waiting_for_choice);
    if (states > 1) return reject("MES VM snapshot has conflicting stop states");
    if (snapshot.waiting_for_choice && snapshot.choices.empty())
        return reject("MES VM snapshot waits for an empty choice list");
    if (snapshot.entry_label.find('\0') != std::string::npos)
        return reject("MES VM snapshot entry label contains a NUL byte");
    for (const auto& choice : snapshot.choices) {
        if (labels_.find(choice.label) == labels_.end())
            return reject("MES VM snapshot contains an unknown choice target");
    }
    for (const auto& variable : snapshot.variables) {
        if (!snapshot_variable_valid(variable.first.first, variable.first.second, variable.second))
            return reject("MES VM snapshot contains an invalid variable bank, index, or value");
    }

    // The PC must be between decoded commands, never in a string/operand.
    // Keep the preceding command to validate suspended/finished states too.
    MesStream scan(*mes_);
    MesCommand previous;
    bool boundary = snapshot.pc == 3;
    while (!boundary && scan.pos() < mes_->code.size()) {
        if (!scan.next(&previous)) return reject("bound MES script is malformed");
        boundary = scan.pos() == snapshot.pc;
        if (previous.op == 0) break;
    }
    if (!boundary) return reject("MES VM snapshot program counter is not a command boundary");
    if (snapshot.transferring && previous.op != 0x4F && previous.op != 0x50)
        return reject("MES VM snapshot transfer is not positioned after a script-transfer command");
    if (snapshot.waiting_for_choice && (previous.op != 0x0A || previous.p1 != 0))
        return reject("MES VM snapshot choice wait is not positioned after a choice command");
    if (snapshot.ended && !(snapshot.pc > 3 && previous.op == 0)
        && snapshot.pc != mes_->code.size())
        return reject("MES VM snapshot end is not positioned at the end of playback");

    // Prepare allocations before changing live state. Do not replay variable
    // setters: aliases and future engine-service side effects must not fire.
    auto variables = snapshot.variables;
    auto choices = snapshot.choices;
    auto entry_label = snapshot.entry_label;
    variables_.swap(variables);
    choices_.swap(choices);
    entry_label_.swap(entry_label);
    stream_->seek(snapshot.pc);
    ended_ = snapshot.ended;
    transferring_ = snapshot.transferring;
    waiting_for_choice_ = snapshot.waiting_for_choice;
    error_.clear();
    if (err) err->clear();
    return true;
}

void MesVm::seek_to_pc(uint32_t target_pc) {
    if (!mes_ || !stream_) return;
    if (target_pc >= mes_->code.size()) {
        stream_->seek(uint32_t(mes_->code.size()));
        ended_ = true;
        return;
    }
    MesStream scanner(*mes_);
    MesCommand cmd;
    uint32_t best_pos = 0;
    while (scanner.next(&cmd)) {
        if (cmd.pos <= target_pc && target_pc < cmd.pos + cmd.size) {
            best_pos = cmd.pos;
            break;
        }
        if (cmd.pos > target_pc) break;
        best_pos = cmd.pos;
    }
    stream_->seek(best_pos);
    ended_ = false;
    waiting_for_choice_ = false;
    error_.clear();
}

bool MesVm::fail(const std::string& message, uint32_t at) {
    std::ostringstream text;
    text << message << " at MES offset 0x" << std::hex << at;
    error_ = text.str();
    ended_ = true;
    waiting_for_choice_ = false;
    return false;
}

bool MesVm::read_command(MesCommand* command) {
    if (!stream_ || !mes_) return false;
    if (stream_->pos() >= mes_->code.size()) {
        ended_ = true;
        return false;
    }
    *command = MesCommand{};
    if (!stream_->next(command))
        return fail("truncated MES command", stream_->pos());
    return true;
}

bool MesVm::consume_companion(MesCommand* command, const char* purpose) {
    const uint32_t at = pos();
    if (!read_command(command) || command->op == 0)
        return fail(std::string("missing ") + purpose, at);
    return true;
}

bool MesVm::jump(const std::string& label) {
    auto it = labels_.find(label);
    if (it == labels_.end() && !label.empty() && label[0] != ':') {
        it = labels_.find(":" + label);
    }
    if (it == labels_.end() && !label.empty() && label[0] == ':') {
        it = labels_.find(label.substr(1));
    }
    if (it == labels_.end()) {
        std::string lower = to_lower(label);
        it = labels_.find(lower);
    }
    if (it == labels_.end()) {
        char* end = nullptr;
        unsigned long id = std::strtoul(label.c_str(), &end, 10);
        if (end != label.c_str() && *end == '\0' && mes_) {
            const MesLabel* ml = mes_->find_label(uint16_t(id));
            if (ml) {
                stream_->seek(ml->offset);
                ended_ = false;
                return true;
            }
        }
        return fail("unknown MES label '" + label + "'", pos());
    }
    stream_->seek(it->second);
    ended_ = false;
    return true;
}

bool MesVm::has_label(const std::string& label) const {
    if (labels_.count(label)) return true;
    if (!label.empty() && label[0] != ':' && labels_.count(":" + label)) return true;
    if (!label.empty() && label[0] == ':' && labels_.count(label.substr(1))) return true;
    std::string lower = to_lower(label);
    if (labels_.count(lower)) return true;
    return false;
}

bool MesVm::choose(size_t index) {
    if (!waiting_for_choice_ || index >= choices_.size()) return false;
    const std::string target = choices_[index].label;
    if (!jump(target)) return false;
    waiting_for_choice_ = false;
    choices_.clear();
    return true;
}

int32_t MesVm::read_variable(uint16_t bank, uint16_t index) const {
    // Route unlock flags (100..119) default to 1 so choices are never skipped or locked out.
    if (bank == 100 && index >= 100 && index <= 119) {
        const auto found = variables_.find({bank, index});
        if (found == variables_.end() || found->second == 0) return 1;
        return found->second;
    }
    const int size = byte_bank_size(bank);
    if (size && index >= size) return 0;
    if (!size && bank != 93 && bank != 94 && bank != 95 && bank != 99)
        return 0;
    if (bank == 93 && index >= 20) return 0;
    // 94/95 address audio-track records; only the special record 50 is
    // independent of the original engine's track registry.
    if ((bank == 94 || bank == 95) && index != 50) return -1;
    if (bank == 99 && index == 217) {
        const int32_t selected = read_variable(99, 218);
        return selected >= 0 && selected < 20
            ? read_variable(93, uint16_t(selected)) : 0;
    }
    const auto found = variables_.find({bank, index});
    return found == variables_.end() ? 0 : found->second;
}

void MesVm::write_variable(uint16_t bank, uint16_t index, int32_t value) {
    const int size = byte_bank_size(bank);
    if (size) {
        if (index >= size) return;
        variables_[{bank, index}] = uint8_t(value);
        if (bank == 100 && index == 49)
            write_variable(99, 11, value);
        return;
    }
    if (bank == 93) {
        if (index < 20) variables_[{bank, index}] = value;
        return;
    }
    if (bank == 94 || bank == 95) {
        if (index == 50)
            variables_[{bank, index}] = bank == 94 && value > 100 ? 100 : value;
        return;
    }
    if (bank != 99 || system_read_only(index)) return;
    if (index == 217) {
        const int32_t selected = read_variable(99, 218);
        if (selected >= 0 && selected < 20)
            write_variable(93, uint16_t(selected), value);
        return;
    }
    if (index == 254 || index == 253 || index == 246 || index == 245 || index == 243) {
        if (value == 255) value = -1;
    }
    if (index == 224) value = value > 0;
    if (index == 225 && value > 100) value = 100;
    variables_[{bank, index}] = value;
    // The original falls through and mirrors this text-speed setting.
    if (index == 10) variables_[{uint16_t(99), uint16_t(214)}] = value == 2;
}

bool MesVm::skip_scope(uint8_t scope) {
    MesCommand command;
    while (read_command(&command)) {
        if (command.op == 0)
            return fail("unterminated conditional scope", command.pos);
        // sub_4385A0 matches the encoded scope ID, not nesting depth or p1.
        if (command.op == 0x20 && command.p2 == scope) return true;
    }
    return fail("unterminated conditional scope", pos());
}

bool MesVm::execute_compare(const MesCommand& command) {
    const int32_t left = read_variable(command.words[0], command.words[1]);
    int32_t right = signed_word(command.words[2]);
    uint16_t operation = command.words[3];
    if (operation & 0x80) {
        right = read_variable(uint16_t(command.words[2] >> 8),
                              uint16_t(command.words[2] & 0xFF));
        operation &= 0x7F;
    }
    bool condition;
    switch (operation) {
    case 0: condition = left == right; break;
    case 1: condition = left != right; break;
    case 2: condition = left > right; break;
    case 3: condition = left < right; break;
    case 4: condition = left >= right; break;
    case 5: condition = left <= right; break;
    default: return fail("unsupported comparison operator", command.pos);
    }
    MesCommand marker;
    if (!consume_companion(&marker, "conditional scope marker")) return false;
    if (marker.op != 0x02)
        return fail("expected 0x02 conditional scope marker", marker.pos);
    return condition || skip_scope(marker.p1);
}

bool MesVm::execute_sum(const MesCommand& command) {
    int32_t sum = 0;
    MesCommand operand;
    for (;;) {
        if (!consume_companion(&operand, "sum-list operand or terminator"))
            return false;
        if (operand.op == 0x02) break;
        if (operand.kind != CmdKind::Words4)
            return fail("expected word operands in sum list", operand.pos);
        sum = wrap_add(sum, read_variable(operand.words[0], operand.words[1]));
    }
    write_variable(command.words[0], command.words[1], sum);
    return true;
}

bool MesVm::next(MesCommand* out) {
    if (!out || ended_ || transferring_ || waiting_for_choice_ || !error_.empty())
        return false;
    // A bad jump must not hang the UI. Normal scripts yield to their host
    // well before this budget; this is an emulator guard, not engine timing.
    for (size_t executed = 0; executed < 100000; ++executed) {
        MesCommand command;
        if (!read_command(&command)) return false;
        if (pending_button_action_) {
            pending_button_action_ = false;
            *out = command;
            return true;
        }
        if (command.kind == CmdKind::Words4 &&
            (command.op == 0x72 || command.op == 0x54 || command.op == 0x4F)) {
            pending_button_action_ = true;
            *out = command;
            return true;
        }
        switch (command.op) {
        case 0:
            if (!stream_ || stream_->pos() >= mes_->code.size()) {
                ended_ = true;
                return false;
            }
            continue;
        case 0x02: // companion marker; harmless if reached linearly
        case 0x4B: // named-label declaration (Modern)
        case 0x34: // named-label declaration (ClassicDM)
            continue;
        case 0x35: // named-label declaration (Classic) or voice
            if (!command.str.empty()) {
                std::string s = to_lower(command.str);
                if (s.rfind("voice\\", 0) == 0 || s.rfind("lvoice\\", 0) == 0 ||
                    s.rfind("mvoice\\", 0) == 0 || s.rfind("svoice\\", 0) == 0) {
                    break;
                }
            }
            continue;
        case 0x09:
            choices_.clear();
            break;  // host may set up the choice window here
        case 0x0A:
        case 0x0D:
            if (!choices_.empty()) {
                waiting_for_choice_ = true;
                return false;
            }
            break;
        case 0x1C: {
            // Saves a resume checkpoint and consumes its following jump tag.
            // The host still sees 0x1C so save support can capture scene state.
            MesCommand companion;
            if (!consume_companion(&companion, "checkpoint target")) return false;
            if (companion.kind != CmdKind::Str)
                return fail("expected checkpoint target string", companion.pos);
            break;
        }
        case 0x1D:
            return fail("checkpoint restore needs a saved scene", command.pos);
        case 0x20:
            if (!command.p1 && !skip_scope(command.p2)) return false;
            continue;
        case 0x2E:
            transferring_ = true;
            break;
        case 0x4F:
        case 0x50:
            if (command.kind == CmdKind::Str && !command.str.empty() &&
                static_cast<unsigned char>(command.str[0]) >= 0x20 &&
                static_cast<unsigned char>(command.str[0]) < 0x7F &&
                command.str[0] != ':') {
                transferring_ = true;
            }
            break;
        case 0x3B:
        case 0x51:
            if (command.kind == CmdKind::Str) {
                entry_label_ = command.str;
                continue;
            }
            break;
        case 0x3A:
        case 0x54:
        case 0x3D:
            if (command.kind == CmdKind::Str && !command.str.empty()) {
                if (!jump(command.str)) return false;
                continue;
            }
            break;
        case 0x62: {
            if (command.kind == CmdKind::Words4) {
                int32_t value = signed_word(command.words[2]);
                const int32_t old = read_variable(command.words[0], command.words[1]);
                if (command.words[3] == 1) value = wrap_add(old, value);
                else if (command.words[3] == 2) value = wrap_add(old, -value);
                else if (command.words[3] != 0)
                    return fail("unsupported assignment operator", command.pos);
                write_variable(command.words[0], command.words[1], value);
                continue;
            }
            break;
        }
        case 0x66: {
            if (command.kind == CmdKind::Words4) {
                if (!execute_compare(command)) return false;
                continue;
            }
            break;
        }
        case 0x5C: {
            if (command.kind == CmdKind::Str) {
                MesCommand caption;
                if (!consume_companion(&caption, "choice text")) return false;
                if (caption.kind != CmdKind::Text && caption.kind != CmdKind::Str && caption.kind != CmdKind::ParamStr)
                    return fail("expected choice text", caption.pos);
                choices_.push_back({command.str, caption.str});
                continue;
            }
            // ClassicDC2 Words4: 0x5C is assignment (equivalent to 0x82)
            int32_t value = signed_word(command.words[2]);
            const int32_t old = read_variable(command.words[0], command.words[1]);
            if (command.words[3] == 1) value = wrap_add(old, value);
            else if (command.words[3] == 2) value = wrap_add(old, -value);
            else if (command.words[3] != 0)
                return fail("unsupported assignment operator", command.pos);
            write_variable(command.words[0], command.words[1], value);
            continue;
        }
        case 0x5F: {
            if (command.kind == CmdKind::Str) {
                MesCommand companion;
                if (!consume_companion(&companion, "string companion")) return false;
                if (companion.kind != CmdKind::Str && companion.kind != CmdKind::Text)
                    return fail("expected string companion", companion.pos);
                continue;
            }
            // ClassicDC2 Words4: 0x5F is variable compare
            continue;
        }
        case 0x82: {
            int32_t value = signed_word(command.words[2]);
            const int32_t old = read_variable(command.words[0], command.words[1]);
            if (command.words[3] == 1) value = wrap_add(old, value);
            else if (command.words[3] == 2) value = wrap_add(old, -value);
            else if (command.words[3] != 0)
                return fail("unsupported assignment operator", command.pos);
            write_variable(command.words[0], command.words[1], value);
            continue;
        }
        case 0x83:
            write_variable(command.words[0], command.words[1],
                           read_variable(command.words[2], command.words[3]));
            continue;
        case 0x84:
            if (!execute_sum(command)) return false;
            continue;
        case 0x85:
            write_variable(command.words[0], command.words[1],
                wrap_add(read_variable(command.words[0], command.words[1]),
                         read_variable(command.words[2], command.words[3])));
            continue;
        case 0x86:
            if (!execute_compare(command)) return false;
            continue;
        default:
            break; // the host owns rendering/audio/other engine services
        }
        *out = std::move(command);
        return true;
    }
    return fail("MES execution budget exceeded (possible jump loop)", pos());
}

}  // namespace dc4
