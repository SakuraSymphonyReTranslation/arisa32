#include "arisa32/scene.h"
#include "arisa32/resource.h"
#include "arisa32/bgm.h"
#include "arisa32/mem_safe.h"

#include <cmath>
#include <algorithm>
#include <ctime>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace dc4scene {
namespace {
constexpr size_t kMaxSave = 2 * 1024 * 1024;
constexpr char kMagic[8] = {'D','C','4','S','A','V','E','\0'};
constexpr char kCompanionMagic[8] = {'A','R','I','S','A','P','C','\0'};
uint64_t checksum(const std::vector<uint8_t>& bytes) {
    uint64_t value = 14695981039346656037ull;
    for (uint8_t b : bytes) { value ^= b; value *= 1099511628211ull; }
    return value;
}
struct Writer {
    std::vector<uint8_t> bytes;
    void u64(uint64_t value) { for (int i = 0; i < 8; ++i) bytes.push_back(uint8_t(value >> (i * 8))); }
    void u32(uint32_t value) { for (int i = 0; i < 4; ++i) bytes.push_back(uint8_t(value >> (i * 8))); }
    void number(double value) { uint64_t bits; std::memcpy(&bits, &value, 8); u64(bits); }
    void flag(bool value) { bytes.push_back(value ? 1 : 0); }
    void str(const std::string& value) {
        if (value.size() > 65536) throw std::runtime_error("save string too large");
        u32(uint32_t(value.size())); bytes.insert(bytes.end(), value.begin(), value.end());
    }
};
struct Reader {
    const std::vector<uint8_t>& bytes;
    size_t at = 0;
    void need(size_t count) const { if (at > bytes.size() || count > bytes.size() - at) throw std::runtime_error("truncated save"); }
    uint64_t u64() { need(8); uint64_t value = 0; for (int i = 0; i < 8; ++i) value |= uint64_t(bytes[at++]) << (i * 8); return value; }
    uint32_t u32() { need(4); uint32_t value = 0; for (int i = 0; i < 4; ++i) value |= uint32_t(bytes[at++]) << (i * 8); return value; }
    int32_t integer() { uint32_t bits = u32(); int32_t result; std::memcpy(&result, &bits, 4); return result; }
    double number() { uint64_t bits = u64(); double value; std::memcpy(&value, &bits, 8); if (!std::isfinite(value)) throw std::runtime_error("invalid save number"); return value; }
    bool flag() { need(1); const auto value = bytes[at++]; if (value > 1) throw std::runtime_error("invalid save flag"); return value != 0; }
    uint32_t count(uint32_t limit) { const auto value = u32(); if (value > limit) throw std::runtime_error("save collection too large"); return value; }
    std::string str(size_t limit = 65536) {
        size_t size = count(uint32_t(limit)); need(size);
        std::string value(reinterpret_cast<const char*>(bytes.data() + at), size); at += size;
        if (value.find('\0') != std::string::npos) throw std::runtime_error("embedded NUL in save string");
        return value;
    }
};
bool resource_name(const std::string& name, bool allow_separator = true) {
    if (name.empty() || name.size() > 255 || name.find(':') != std::string::npos ||
        name.find("..") != std::string::npos || name[0] == '/' || name[0] == '\\') return false;
    if (!allow_separator && name.find_first_of("/\\") != std::string::npos) return false;
    return true;
}
void atomic_write(const std::filesystem::path& target, const std::vector<uint8_t>& bytes) {
    const std::filesystem::path temporary = target.string() + ".tmp";
    if (!target.parent_path().empty()) std::filesystem::create_directories(target.parent_path());
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    if (!file) throw std::runtime_error("cannot create save: " + temporary.string());
    file.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    file.flush();
    if (!file) throw std::runtime_error("cannot write complete save");
    file.close();
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("cannot replace save (Windows error " + std::to_string(GetLastError()) + ")");
#else
    std::filesystem::rename(temporary, target);
#endif
}
bool read_bytes(const std::string& path, std::vector<uint8_t>* bytes) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    const auto size = file.tellg();
    if (size < 8 || size > std::streamoff(kMaxSave + 64)) return false;
    bytes->assign(size_t(size), 0); file.seekg(0);
    return bool(file.read(reinterpret_cast<char*>(bytes->data()), size));
}
void write_vm(Writer& out, const dc4::MesVmSnapshot& vm) {
    out.u32(vm.version); out.u64(vm.script_fingerprint); out.u32(vm.pc);
    out.u32(uint32_t(vm.variables.size()));
    for (const auto& item : vm.variables) { out.u32(item.first.first); out.u32(item.first.second); out.u32(uint32_t(item.second)); }
    out.str(vm.entry_label); out.u32(uint32_t(vm.choices.size()));
    for (const auto& choice : vm.choices) { out.str(choice.label); out.str(choice.text); }
    out.flag(vm.ended); out.flag(vm.transferring); out.flag(vm.waiting_for_choice);
}
dc4::MesVmSnapshot read_vm(Reader& in) {
    dc4::MesVmSnapshot vm;
    vm.version = in.u32(); vm.script_fingerprint = in.u64(); vm.pc = in.u32();
    const auto count = in.count(8192);
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t bank = in.u32(), index = in.u32(); const int32_t value = in.integer();
        if (bank > 65535 || index > 65535 || !vm.variables.emplace(std::make_pair(uint16_t(bank), uint16_t(index)), value).second)
            throw std::runtime_error("invalid or duplicate saved variable");
    }
    vm.entry_label = in.str(256);
    const auto choices = in.count(64);
    for (uint32_t i = 0; i < choices; ++i) { dc4::MesChoice choice; choice.label = in.str(256); choice.text = in.str(); vm.choices.push_back(std::move(choice)); }
    vm.ended = in.flag(); vm.transferring = in.flag(); vm.waiting_for_choice = in.flag();
    return vm;
}
void write_audio(Writer& out, const dc4::SceneAudioSnapshot& audio) {
    out.u32(audio.version); out.u32(uint32_t(audio.bgm_id)); out.u32(uint32_t(audio.bgm_mode));
    out.flag(audio.bgm_alternate); out.flag(audio.bgm_playing); out.number(audio.bgm_position_frames);
    out.u32(uint32_t(audio.looped_se.size()));
    for (const auto& se : audio.looped_se) { out.u32(uint32_t(se.id)); out.u32(uint32_t(se.channel)); out.number(se.position_frames); }
    out.str(audio.last_voice_name); out.u32(uint32_t(audio.bgm_volume)); out.u32(uint32_t(audio.saved_bgm_volume));
    for (const auto& group : audio.groups) {
        out.number(group.volume_db); out.number(group.volume_target_db); out.u64(group.volume_remaining_frames);
        out.number(group.pan); out.number(group.pan_target); out.u64(group.pan_remaining_frames);
    }
}
dc4::SceneAudioSnapshot read_audio(Reader& in) {
    dc4::SceneAudioSnapshot audio;
    audio.version = in.u32(); audio.bgm_id = in.integer(); audio.bgm_mode = in.integer();
    audio.bgm_alternate = in.flag(); audio.bgm_playing = in.flag(); audio.bgm_position_frames = in.number();
    auto count = in.count(3);
    for (uint32_t i = 0; i < count; ++i) { dc4::SceneLoopedSeSnapshot se; se.id = in.integer(); se.channel = in.integer(); se.position_frames = in.number(); audio.looped_se.push_back(se); }
    audio.last_voice_name = in.str(260); audio.bgm_volume = in.integer(); audio.saved_bgm_volume = in.integer();
    for (auto& group : audio.groups) {
        group.volume_db = in.number(); group.volume_target_db = in.number(); group.volume_remaining_frames = in.u64();
        group.pan = in.number(); group.pan_target = in.number(); group.pan_remaining_frames = in.u64();
    }
    return audio;
}
} // namespace

bool SceneRunner::capture_state(std::vector<uint8_t>* bytes, std::string* capture_error) {
    try {
        if (!bytes) throw std::runtime_error("missing snapshot destination");
        if (!pending_movie_.empty() || script_wait_pending_)
            throw std::runtime_error("wait for the current movie or timed sequence to finish before saving");
        dc4::MesVmSnapshot snapshot;
        std::string error;
        if (!vm_.capture_snapshot(&snapshot, &error)) throw std::runtime_error(error);
        if (!resource_name(script_name_, false)) throw std::runtime_error("no scenario is loaded");
        Writer out;
        out.str(script_name_); write_vm(out, snapshot);
        out.str(current_line_); out.str(speaker_); out.flag(scene_end_); out.flag(message_visible_); out.flag(preserve_scene_);
        out.u32(uint32_t(transition_type_)); out.number(camera_scale_); out.number(camera_x_); out.number(camera_y_);
        out.u32(uint32_t(layers_.size()));
        for (const auto& layer : layers_) { out.u32(uint32_t(layer.first)); out.str(layer.second.name); }
        out.u32(uint32_t(sprites_.size()));
        for (const auto& sprite : sprites_) {
            out.str(sprite.name); out.str(sprite.image); out.number(sprite.scale);
            out.u32(uint32_t(sprite.x)); out.u32(uint32_t(sprite.y)); out.u32(uint32_t(sprite.alpha)); out.u32(uint32_t(sprite.z)); out.flag(sprite.visible);
            out.u32(uint32_t(sprite.cur_x)); out.u32(uint32_t(sprite.cur_y));
        }
        write_audio(out, scene_audio_.capture_snapshot());
        out.u32(camera_duration_); out.u32(uint32_t(camera_motion_.size()));
        for (const auto& motion : camera_motion_) {
            out.number(motion.from_scale); out.number(motion.to_scale);
            out.number(motion.from_x); out.number(motion.from_y);
            out.number(motion.to_x); out.number(motion.to_y);
            out.number(motion.elapsed_ms); out.number(motion.duration_ms);
        }
        // v5: face overlays retain their clean body asset across Save/Load.
        for (const auto& sprite : sprites_) out.str(sprite.expression);
        // v4: COMU phone chat state (visibility + timeline in scene order).
        out.flag(comu_visible_);
        out.u32(uint32_t(comu_items_.size()));
        for (const auto& item : comu_items_) {
            out.u32(uint32_t(item.contact));
            out.u32(uint32_t(item.stamp));
            out.str(item.text);
        }
        if (out.bytes.size() > kMaxSave) throw std::runtime_error("save exceeds size limit");
        Writer header;
        header.bytes.insert(header.bytes.end(), kMagic, kMagic + 8);
        header.u32(5); header.u32(uint32_t(out.bytes.size())); header.u64(checksum(out.bytes));
        header.bytes.insert(header.bytes.end(), out.bytes.begin(), out.bytes.end());
        *bytes = std::move(header.bytes);
        if (capture_error) capture_error->clear();
        return true;
    } catch (const std::exception& error) {
        if (capture_error) *capture_error = error.what();
        return false;
    }
}

bool SceneRunner::capture_pc_save(std::vector<uint8_t>* destination, std::string* capture_error) {
    try {
        if (!destination) throw std::runtime_error("missing native save destination");
        const bool classic = mes_.version == 0x9969;
        if (!classic && mes_.version != 0xAAB6 && mes_.version != 0xABB6)
            throw std::runtime_error("native saves are not supported for this game format");
        if (current_line_.empty() || scene_end_ || comu_visible_ || !pending_movie_.empty() || script_wait_pending_)
            throw std::runtime_error("save at a dialogue or choice after the current sequence finishes");
        std::vector<dc4::MesCommand> commands;
        dc4::MesStream stream(mes_); dc4::MesCommand c;
        size_t target = size_t(-1);
        while (stream.next(&c)) {
            if (c.pos >= vm_.pos()) break;
            if (c.op == 3 || (vm_.waiting_for_choice() && c.op == 4)) target = commands.size();
            commands.push_back(c);
        }
        if (target == size_t(-1)) throw std::runtime_error("no native scenario checkpoint at this position");
        const auto& marker = commands[target];
        const uint16_t id = uint16_t(marker.p1 | uint16_t(marker.p2) << 8);
        // Read the PC script directly, bypassing the selected translation overlay.
        // Native byte offsets must point into the script used by the PC game.
        dc4::MesFile original;
        std::vector<uint8_t> source;
        std::string original_path = dc4::find_file_ci(data_ + "/MES", script_name_);
        if (original_path.empty()) original_path = data_ + "/MES/" + script_name_;
        else original_path = data_ + "/MES/" + original_path;
        if (!read_bytes(original_path, &source) || !original.load(source.data(), source.size()) || original.version != mes_.version)
            throw std::runtime_error("original PC scenario is unavailable");
        dc4::MesStream pc_stream(original);
        uint32_t pc = 0; size_t matches = 0;
        while (pc_stream.next(&c)) if (c.op == marker.op && uint16_t(c.p1 | uint16_t(c.p2) << 8) == id) { pc = c.pos; ++matches; }
        if (matches != 1) throw std::runtime_error("native checkpoint is missing or ambiguous in the PC scenario");
        std::vector<uint8_t> out(classic ? 0x2B20 : 0x56FE4, 0);
        auto u32 = [&](size_t at, uint32_t value) { for(int i=0;i<4;++i) out.at(at+i)=uint8_t(value>>(8*i)); };
        auto u16 = [&](size_t at, uint16_t value) { out.at(at)=uint8_t(value);out.at(at+1)=uint8_t(value>>8); };
        auto number = [&](size_t at, float value) { uint32_t bits; std::memcpy(&bits,&value,4);u32(at,bits); };
        auto name = [&](size_t at, size_t capacity, std::string value) {
            if (value.empty()) return;
            if (!resource_name(value) || value.size() >= capacity) throw std::runtime_error("resource name cannot fit the native save: " + value);
            std::replace(value.begin(),value.end(),'/','\\');
            std::copy(value.begin(),value.end(),out.begin()+at);
        };
        std::string script = script_name_;
        if (script.size()>4 && script.substr(script.size()-4)==".mes") script.resize(script.size()-4);
        name(classic ? 4 : 8, classic ? 128 : 160, script);
        const auto audio = scene_audio_.capture_snapshot();
        const auto now=std::time(nullptr);
        std::tm local{};
#ifdef _WIN32
        localtime_s(&local,&now);
#else
        localtime_r(&now,&local);
#endif
        if (classic) {
            for(const auto& layer:layers_)if(layer.first!=1)throw std::runtime_error("native DC2PC export does not support this image layer");
            for(size_t i=5;i<sprites_.size();++i)if(sprites_[i].visible)throw std::runtime_error("native DC2PC export does not support this sprite layer");
            if(!audio.looped_se.empty())throw std::runtime_error("native DC2PC export does not support looped sound effects");
            u32(0,1);u32(0x804,audio.bgm_playing ? uint32_t(audio.bgm_id) : uint32_t(-1));
            u32(0x808,1);u32(0x810,pc);u32(0x814,id);u32(0x81C,vm_.waiting_for_choice()?9:7);
            // The PC loader redraws the last dialogue from this marker history.
            std::vector<uint16_t> history;
            for (const auto& raw : commands) if(raw.op==3) history.push_back(uint16_t(raw.p1 | uint16_t(raw.p2)<<8));
            if(history.size()>1000) history.erase(history.begin(),history.end()-1000);
            if(history.empty()) throw std::runtime_error("native dialogue history is unavailable");
            u32(0x818,uint32_t(history.size()));
            for(size_t i=0;i<history.size();++i) u16(0x820+i*2,history[i]);
            if(layers_.count(1)) name(0x84,128,layers_.at(1).name);
            for(size_t i=0;i<5;++i) {
                const auto& sprite=sprites_[i];
                if(sprite.visible) {name(0x104+i*128,128,sprite.image);name(0x384+i*128,128,sprite.expression);}
                const size_t at=0x14D0+i*52;
                u32(at,uint32_t(sprite.cur_x));u32(at+4,uint32_t(sprite.cur_y));u32(at+8,uint32_t(std::lround(sprite.scale*100)));
                u32(at+12,uint32_t(sprite.cur_x));u32(at+16,uint32_t(sprite.cur_y));u32(at+20,uint32_t(std::lround(sprite.scale*100)));
                u32(at+24,uint32_t(sprite.x));u32(at+28,uint32_t(sprite.y));
                u32(at+32,uint32_t(std::lround(sprite.frame_cx-sprite.img.w/2.f)));
                u32(at+36,uint32_t(std::lround(sprite.frame_cy-sprite.img.h/2.f)));
                u32(at+40,uint32_t(sprite.canvas_w));u32(at+44,uint32_t(sprite.canvas_h));
            }
            for(int bank=0;bank<10;++bank) for(int i=0;i<100;++i) out[0xFF0+bank*100+i]=uint8_t(variable(bank,i));
            // Native bank 9 fields 21..25 hold the slot date, populated after
            // copying the local variables just as the PC save routine does.
            out[0x1389]=uint8_t(local.tm_mon+1);out[0x138A]=uint8_t(local.tm_mday);
            out[0x138B]=uint8_t(local.tm_hour);out[0x138C]=uint8_t(local.tm_min);out[0x138D]=uint8_t((local.tm_year+1900)%100);
        } else {
            u32(0xAF4,pc);u32(0xAFC,id);u32(0xB00,pc);
            if(id>=original.labels.size()) throw std::runtime_error("native checkpoint metadata is unavailable");
            u32(0xAF8,original.labels[id].id);
            for(int bank=0;bank<=10;++bank) for(int i=0;i<100;++i) out[0x184EC+bank*100+i]=uint8_t(variable(bank,i));
            out[0x1873A]=uint8_t(local.tm_mon+1);out[0x1873B]=uint8_t(local.tm_mday);
            out[0x1873C]=uint8_t(local.tm_hour);out[0x1873D]=uint8_t(local.tm_min);
            out[0x1873E]=uint8_t((local.tm_year+1900)%100);out[0x1873F]=uint8_t(local.tm_sec);
            // Renderer records are copied verbatim by the PC loader (960 bytes).
            // Scalar offsets below are relative to the serialized record, not
            // the C++ renderer object whose first eight bytes are not saved.
            for(int i=0;i<150;++i) {
                const size_t at=0x18E20+i*960;
                for(int offset : {24,28,32,104,108,112}) number(at+offset,1);
                for(int offset : {120,124}) number(at+offset,100);
                u32(at+80,1280);u32(at+84,720);number(at+152,1280);number(at+156,720);u32(at+176,1);
            }
            // The background renderer also carries the scene camera when no
            // background asset is visible. Keep it for subsequent sprite moves.
            number(0x18E20+28,camera_scale_);number(0x18E20+32,camera_scale_);
            number(0x18E20+36,camera_x_);number(0x18E20+40,camera_y_);u32(0x18E20+44,1);
            auto renderer = [&](int index, const Image& image, float x, float y, float scale, float alpha, float pivot_x, float pivot_y, bool camera) {
                const size_t at=0x18E20+index*960;
                // Native zero pivots select the image centre. Compensate the
                // stored origin so x/y still describe the scaled top-left.
                if(pivot_x==0) pivot_x=image.w/2.f;
                if(pivot_y==0) pivot_y=image.h/2.f;
                x-=pivot_x*(1-scale);y-=pivot_y*(1-scale);
                number(at,x);number(at+4,y);number(at+48,x);number(at+52,y);
                number(at+64,float(image.w));number(at+68,float(image.h));number(at+96,float(image.w));number(at+100,float(image.h));
                const float native_scale=scale*(scale>=10 ? 10000.f : 100.f);
                number(at+104,alpha);number(at+120,native_scale);number(at+124,native_scale);
                number(at+144,pivot_x);number(at+148,pivot_y);
                if(camera) {number(at+28,camera_scale_);number(at+32,camera_scale_);number(at+36,camera_x_);number(at+40,camera_y_);u32(at+44,1);}
                // Texture handles and animation pointers stay zero on disk.
                u32(at+176,0);
            };
            for(const auto& layer : layers_) {
                if(layer.first<1 || layer.first>3) throw std::runtime_error("native export does not support this image layer");
                name(0xA8+layer.first*48,48,layer.second.name);
                const float scale=std::max(1280.f/layer.second.w,720.f/layer.second.h);
                renderer(layer.first-1,layer.second,(1280-layer.second.w*scale)/2,
                         (720-layer.second.h*scale)/2,scale,1,0,0,true);
            }
            for(size_t i=0;i<sprites_.size();++i) if(sprites_[i].visible && sprites_[i].img.valid()) {
                const auto& sprite=sprites_[i];
                int slot=-1,record=-1;
                if(i<5 && sprite.z==0) {slot=4+int(i);record=10+int(i)*10;}
                else if(i==12) {slot=25;record=63;}
                else if(i==13) {slot=26;record=65;}
                else throw std::runtime_error("native export does not support this sprite layer");
                name(0xA8+slot*48,48,sprite.image);
                // The PC reconstructs a full BST registration canvas, including
                // transparent margins around the cropped CRX. Both renderers
                // use 1280x720 screen coordinates; no 1.25 conversion applies.
                Image canvas;
                canvas.w=sprite.canvas_w>0?sprite.canvas_w:sprite.img.w;
                canvas.h=sprite.canvas_h>0?sprite.canvas_h:sprite.img.h;
                const float left=sprite.cur_x+sprite.x*(1-sprite.scale);
                const float top=sprite.cur_y+sprite.y*(1-sprite.scale);
                renderer(record,canvas,left,top,sprite.scale,sprite.alpha/255.f,float(sprite.x),float(sprite.y),true);
                const size_t at=0x18E20+record*960;
                number(at+36,512+int((camera_x_-512)*80/100));
                number(at+40,288+int((camera_y_-288)*80/100));
            }
            scene_audio_.load_names();
            if(audio.bgm_playing) {
                if(audio.bgm_id<0 || size_t(audio.bgm_id)>=scene_audio_.bgm_names().size()) throw std::runtime_error("native BGM resource is unavailable");
                name(0xA38,40,scene_audio_.bgm_names()[audio.bgm_id]);
            }
            if(audio.looped_se.size()>3)throw std::runtime_error("too many native looping effects");
            for(size_t i=0;i<audio.looped_se.size();++i) {
                const auto& se=audio.looped_se[i];
                if(se.id<0 || size_t(se.id)>=scene_audio_.se_names().size()) throw std::runtime_error("native SE resource is unavailable");
                name(0xA60+i*32,32,scene_audio_.se_names()[se.id]);u32(0xAE0+i*4,1);
            }
        }
        *destination=std::move(out);
        if(capture_error) capture_error->clear();
        return true;
    } catch(const std::exception& error) {if(capture_error)*capture_error=error.what();return false;}
}

bool SceneRunner::save_load_allowed() const {
    if (dc2pc_ui_ || dc4::mes_opcode_profile(mes_.dialect, mes_.version).id == dc4::MesProfileId::DC2PC)
        return variable(9, 7) != 1;
    // DC4 and DC4PH win_size.mes disable persistence for any nonzero replay
    // flag. Other modern games can assign this variable a different meaning.
    return !dc4_replay_profile_ || variable(9, 7) == 0;
}

void SceneRunner::finish_replay() {
    if (save_load_allowed()) return;
    set_variable(9, 7, 0);
    if (dc2pc_ui_ || dc4::mes_opcode_profile(mes_.dialect, mes_.version).id == dc4::MesProfileId::DC2PC)
        set_variable(3, 30, 0);
}

bool SceneRunner::save_state(const std::string& filename) {
    try {
        if (!save_load_allowed()) throw std::runtime_error("Save/Load unavailable during scene replay");
        std::vector<uint8_t> snapshot;
        std::string error;
        if (!capture_state(&snapshot, &error)) throw std::runtime_error(error);
        const std::filesystem::path target(filename);
        std::string extension=target.extension().string();
        std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return char(std::tolower(c));});
        if(extension==".dat") {
            std::vector<uint8_t> native;
            if(!capture_pc_save(&native,&error)) throw std::runtime_error(error);
            Writer companion;
            companion.bytes.insert(companion.bytes.end(),kCompanionMagic,kCompanionMagic+8);
            companion.u64(checksum(native));companion.bytes.insert(companion.bytes.end(),snapshot.begin(),snapshot.end());
            // A crash between writes leaves an unmatched companion, safely ignored.
            atomic_write(target.string()+".arisa",companion.bytes);
            atomic_write(target,native);
        } else atomic_write(target,snapshot);
        last_error_.clear();
        return true;
    } catch (const std::exception& error) { last_error_ = error.what(); return false; }
}

bool SceneRunner::load_state(const std::string& filename) {
    try {
        if (!save_load_allowed()) throw std::runtime_error("Save/Load unavailable during scene replay");
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file) throw std::runtime_error("save not found: " + filename);
        const auto size = file.tellg();
        if (size < 24 || size > std::streamoff(kMaxSave + 24)) throw std::runtime_error("invalid save size");
        file.seekg(0);
        std::vector<uint8_t> bytes(size_t(size), 0);
        if (!file.read(reinterpret_cast<char*>(bytes.data()), size)) throw std::runtime_error("cannot read complete save");
        std::string error;
        bool ok = false;
        if (bytes.size() >= 8 && !std::memcmp(bytes.data(), kMagic, 8)) {
            ok = restore_state(bytes, &error);
        } else {
            std::vector<uint8_t> companion;
            if(read_bytes(filename+".arisa",&companion) && companion.size()>=40 &&
               !std::memcmp(companion.data(),kCompanionMagic,8)) {
                Reader binding{companion,8};
                if(binding.u64()==checksum(bytes)) {
                    std::vector<uint8_t> snapshot(companion.begin()+16,companion.end());
                    ok=restore_state(snapshot,&error);
                }
            }
            if(!ok) ok = restore_pc_save(bytes, &error);
        }
        if (!ok) throw std::runtime_error(error);
        last_error_.clear();
        return true;
    } catch (const std::exception& error) { last_error_ = error.what(); return false; }
}

bool SceneRunner::restore_pc_save(const std::vector<uint8_t>& bytes, std::string* restore_error) {
    dc4::MesFile identity;
    if (mes_.version == 0x9969 ||
        (identity.load_file(dc4::resolve_asset(data_ + "/MES/main.mes")) && identity.version == 0x9969))
        return restore_dc2pc_pc_save(bytes, restore_error);
    try {
        if(bytes.size()!=0x56FE4) throw std::runtime_error("truncated or unsupported DC4 PC save");
        auto integer=[&](size_t at){Reader reader{bytes,at};return reader.u32();};
        auto number=[&](size_t at){const auto bits=integer(at);float value;std::memcpy(&value,&bits,4);if(!std::isfinite(value)||std::abs(value)>100000)throw std::runtime_error("invalid native renderer scalar");return value;};
        auto name=[&](size_t at,size_t capacity){
            size_t end=at;while(end<at+capacity && bytes[end])++end;
            if(end==at+capacity)throw std::runtime_error("unterminated PC save resource");
            std::string value(reinterpret_cast<const char*>(bytes.data()+at),end-at);
            if(!value.empty()&&!resource_name(value))throw std::runtime_error("unsafe PC save resource");
            return value;
        };
        std::string script=name(8,160);
        if(!resource_name(script,false))throw std::runtime_error("invalid PC scenario");
        if(script.find('.')==std::string::npos)script+=".mes";
        SceneRunner candidate(data_);
        if(!candidate.load_script(data_+"/MES/"+script))throw std::runtime_error(candidate.last_error_);
        if(candidate.mes_.version!=0xAAB6 && candidate.mes_.version!=0xABB6)throw std::runtime_error("PC save scenario has an unsupported format");
        const auto checkpoint=integer(0xAFC);
        if(checkpoint>65535)throw std::runtime_error("invalid PC save checkpoint");
        std::vector<dc4::MesCommand> commands;
        dc4::MesStream stream(candidate.mes_);dc4::MesCommand c;
        size_t target=size_t(-1);
        while(stream.next(&c)) {
            if((c.op==3 || c.op==4) && (uint32_t(c.p1)|uint32_t(c.p2)<<8)==checkpoint) {
                if(target!=size_t(-1))throw std::runtime_error("ambiguous PC save checkpoint");
                target=commands.size();
            }
            commands.push_back(c);
        }
        if(target==size_t(-1))throw std::runtime_error("saved checkpoint is unavailable in this scenario");
        // Preserve system/route progress. Native slot records contain only the
        // local story banks; importing a slot must never unlock every route.
        for(int bank : {96,97,98,100}) {
            const int count=bank==97?1200:bank==98?650:bank==100?150:400;
            for(int i=0;i<count;++i)candidate.set_variable(bank,i,variable(bank,i));
        }
        for(int bank=0;bank<=10;++bank)for(int i=0;i<100;++i)candidate.set_variable(bank,i,bytes[0x184EC+bank*100+i]);
        // Reconstruct dialogue and placement only, avoiding assignments, flow,
        // audio and achievement side effects from the preceding scenario.
        for(size_t i=0;i<target;++i) {
            if(commands[i].op==3)candidate.speaker_.clear();
            if(commands[i].op==0x8B || commands[i].op==0x99 || commands[i].op==0x9D)
                candidate.dispatch_sprite_command(dc4::mes_engine_command(commands[i]));
            candidate.dispatch_text_command(dc4::mes_engine_command(commands[i]));
        }
        uint32_t resume=commands[target].pos+uint32_t(commands[target].size);
        if(commands[target].op==4) {
            candidate.vm_.seek_to_pc(resume);
            size_t budget=0;
            while(candidate.vm_.next(&c))if(++budget>256 || c.op==0x2E || c.op==0x4C)throw std::runtime_error("invalid native choice block");
            if(!candidate.vm_.waiting_for_choice())throw std::runtime_error("native choice is unavailable");
        } else {
            candidate.speaker_.clear();candidate.current_line_.clear();
            for(size_t i=target+1;i<commands.size();++i) {
                const auto& raw=commands[i];
                if(raw.op==3 || raw.op==4 || raw.op==0)break;
                candidate.dispatch_text_command(dc4::mes_engine_command(raw));
                resume=raw.pos+uint32_t(raw.size);
                if(!candidate.current_line_.empty())break;
            }
            if(candidate.current_line_.empty())throw std::runtime_error("saved native dialogue is unavailable");
            candidate.vm_.seek_to_pc(resume);
        }
        for(int slot=1;slot<=3;++slot) {
            const auto resource=name(0xA8+slot*48,48);if(resource.empty())continue;
            candidate.load_layer(slot,resource);
            if(!candidate.layers_.count(slot))throw std::runtime_error("saved background is unavailable: "+resource);
        }
        const size_t background=0x18E20;
        const float camera=number(background+28);
        if(integer(background+44) && camera>=1 && camera<=655.35f) {
            candidate.camera_scale_=camera;candidate.camera_x_=number(background+36);candidate.camera_y_=number(background+40);
        }
        const float scene_pivot_x=512+int((candidate.camera_x_-512)*80/100);
        const float scene_pivot_y=288+int((candidate.camera_y_-288)*80/100);
        const int slots[]={4,5,6,7,8,25,26};
        const int indices[]={0,1,2,3,4,12,13};
        const int records[]={10,20,30,40,50,63,65};
        for(int i=0;i<7;++i) {
            const auto resource=name(0xA8+slots[i]*48,48);if(resource.empty())continue;
            candidate.load_bst(indices[i],resource,slots[i]);
            auto& sprite=candidate.sprites_[indices[i]];
            if(!sprite.img.valid())throw std::runtime_error("saved sprite is unavailable: "+resource);
            const size_t at=0x18E20+records[i]*960;
            const float raw_scale=number(at+120);
            const float scale=raw_scale/(raw_scale>=1000?10000.f:100.f);
            if(scale<=0 || scale>100)throw std::runtime_error("invalid saved sprite scale");
            // Native origins refer to the full registration canvas, not the
            // cropped CRX's top-left. A zero pivot selects the source centre.
            float pivot_x=number(at+144),pivot_y=number(at+148);
            if(pivot_x==0)pivot_x=(number(at+96)-number(at+88))/2;
            if(pivot_y==0)pivot_y=(number(at+100)-number(at+92))/2;
            const float crop_x=sprite.frame_cx-sprite.img.w/2.f;
            const float crop_y=sprite.frame_cy-sprite.img.h/2.f;
            float native_x=number(at)+number(at+12)+pivot_x*(1-scale)+crop_x*scale;
            float native_y=number(at+4)+number(at+16)+pivot_y*(1-scale)+crop_y*scale;
            const float layer_camera=integer(at+44)?number(at+28):1;
            if(layer_camera<=0 || layer_camera>655.35f)throw std::runtime_error("invalid saved sprite camera");
            if(integer(at+44)) {
                native_x=number(at+36)+(native_x-number(at+36))*layer_camera;
                native_y=number(at+40)+(native_y-number(at+40))*layer_camera;
            }
            sprite.scale=scale*layer_camera/candidate.camera_scale_;
            sprite.x=int(std::lround(pivot_x));sprite.y=int(std::lround(pivot_y));
            const float base_x=scene_pivot_x+(native_x-scene_pivot_x)/candidate.camera_scale_;
            const float base_y=scene_pivot_y+(native_y-scene_pivot_y)/candidate.camera_scale_;
            sprite.cur_x=int(std::lround(base_x-sprite.x*(1-sprite.scale)-crop_x*sprite.scale));
            sprite.cur_y=int(std::lround(base_y-sprite.y*(1-sprite.scale)-crop_y*sprite.scale));
            sprite.alpha=int(std::lround(std::clamp(number(at+104),0.f,1.f)*255));
        }
        auto audio=candidate.scene_audio_.capture_snapshot();
        auto normal=[](std::string value){std::replace(value.begin(),value.end(),'\\','/');std::transform(value.begin(),value.end(),value.begin(),[](unsigned char ch){return char(std::tolower(ch));});if(value.size()>4&&value.substr(value.size()-4)==".pcm")value.resize(value.size()-4);return value;};
        const auto bgm=name(0xA38,40);
        if(!bgm.empty()) {
            candidate.scene_audio_.load_names();
            const auto& names=candidate.scene_audio_.bgm_names();
            auto found=std::find_if(names.begin(),names.end(),[&](const std::string& item){return normal(item)==normal(bgm);});
            if(found==names.end())throw std::runtime_error("saved BGM is unavailable: "+bgm);
            audio.bgm_id=int(found-names.begin());audio.bgm_playing=true;audio.bgm_mode=1;
        }
        for(int i=0;i<4;++i) {
            const auto se=name(0xA60+i*32,32);
            if(se.empty() || !integer(0xAE0+i*4))continue;
            candidate.scene_audio_.load_names();
            const auto& names=candidate.scene_audio_.se_names();
            auto found=std::find_if(names.begin(),names.end(),[&](const std::string& item){return normal(item)==normal(se);});
            if(found==names.end())throw std::runtime_error("saved SE is unavailable: "+se);
            const auto resource=normal(se);
            const int channel=resource.rfind("se/201",0)==0?7:
                resource.rfind("se/200",0)==0 || resource.rfind("se/202",0)==0 || resource.rfind("se/473",0)==0?6:2;
            audio.looped_se.push_back({int(found-names.begin()),channel,0});
        }
        std::string error;dc4::MesVmSnapshot vm;
        if(!candidate.vm_.capture_snapshot(&vm,&error))throw std::runtime_error(error);
        if(!scene_audio_.restore_snapshot(audio,false,&error))throw std::runtime_error(error);
        mes_=std::move(candidate.mes_);vm_.reset();vm_.bind(mes_);vm_.restore_snapshot(vm);
        layers_.swap(candidate.layers_);sprites_.swap(candidate.sprites_);
        script_name_=script;current_line_=std::move(candidate.current_line_);speaker_=std::move(candidate.speaker_);
        camera_scale_=candidate.camera_scale_;camera_x_=candidate.camera_x_;camera_y_=candidate.camera_y_;camera_motion_.clear();
        transition_type_=0;scene_dirty_=true;scene_end_=false;message_visible_=true;preserve_scene_=false;
        comu_visible_=comu_group_=false;comu_items_.clear();reset_restore_transients();
        load_ui_script(data_+"/MES/win_size.mes");taskbar_visible_=true;
        if(restore_error)restore_error->clear();return true;
    } catch(const std::exception& error){if(restore_error)*restore_error=error.what();return false;}
}

bool SceneRunner::restore_dc2pc_pc_save(const std::vector<uint8_t>& bytes, std::string* restore_error) {
    try {
        if (bytes.size() != 0x2B20)
            throw std::runtime_error("truncated or unsupported DC2PC PC save");
        const auto integer = [&](size_t offset) { Reader in{bytes, offset}; return in.u32(); };
        const auto name = [&](size_t offset) {
            size_t end = offset;
            while (end < offset + 128 && bytes[end]) ++end;
            if (end == offset + 128) throw std::runtime_error("unterminated DC2PC save resource");
            std::string value(reinterpret_cast<const char*>(bytes.data()+offset), end-offset);
            for (unsigned char c : value) if (c < 32 || c > 126)
                throw std::runtime_error("invalid DC2PC save resource");
            if (!value.empty() && !resource_name(value)) throw std::runtime_error("unsafe DC2PC save resource");
            return value;
        };
        if (integer(0) != 1) throw std::runtime_error("unsupported DC2PC PC save version");
        std::string script = name(4);
        if (!resource_name(script, false)) throw std::runtime_error("invalid DC2PC saved scenario");
        if (script.find('.') == std::string::npos) script += ".mes";
        SceneRunner candidate(data_);
        if (!candidate.load_script(data_ + "/MES/" + script)) throw std::runtime_error(candidate.last_error_);
        if (candidate.mes_.version != 0x9969) throw std::runtime_error("PC save scenario is not DC2PC");
        // Native PC offsets differ between translations. The raw03/raw04 line
        // identifiers survive those changes, including saves paused at choice.
        const uint32_t checkpoint = integer(0x814), stop = integer(0x81C);
        if (checkpoint > 65535 || (stop != 7 && stop != 9))
            throw std::runtime_error("unsupported DC2PC save checkpoint");
        std::vector<dc4::MesCommand> commands;
        dc4::MesStream scan(candidate.mes_);
        dc4::MesCommand c;
        size_t target = size_t(-1);
        while (scan.next(&c)) {
            if (c.op == (stop == 9 ? 4 : 3) && (uint32_t(c.p1) | uint32_t(c.p2)<<8) == checkpoint) {
                if (target != size_t(-1)) throw std::runtime_error("ambiguous DC2PC save checkpoint");
                target = commands.size();
            }
            commands.push_back(c);
        }
        if (target == size_t(-1)) throw std::runtime_error("saved DC2PC checkpoint is unavailable in this scenario");
        // Only placement operations are reconstructed, never past story flow,
        // assignments or achievement side effects. Saved assets are authoritative.
        for (size_t i = 0; i < target; ++i)
            if (commands[i].op == 0x64 || commands[i].op == 0x65)
                candidate.dispatch_sprite_command(dc4::mes_engine_command(commands[i]));
        for (int bank : {95,96,97,98,100}) {
            const int count = bank == 97 || bank == 98 ? 1500 : bank == 100 ? 100 : 400;
            for (int i = 0; i < count; ++i) candidate.set_variable(uint16_t(bank), uint16_t(i), variable(uint16_t(bank), uint16_t(i)));
        }
        for (int bank = 0; bank < 10; ++bank) for (int i = 0; i < 100; ++i)
            candidate.set_variable(uint16_t(bank), uint16_t(i), bytes[0xFF0+bank*100+i]);
        candidate.set_variable(9, 7, 0);
        candidate.set_variable(3, 30, 0);
        uint32_t resume = commands[target].pos + uint32_t(commands[target].size);
        if (stop == 7) {
            for (size_t i = target+1; i < commands.size(); ++i) {
                const auto& raw = commands[i];
                if (raw.op == 3 || raw.op == 4 || raw.op == 0) break;
                if (raw.op == 0x64 || raw.op == 0x65)
                    candidate.dispatch_sprite_command(dc4::mes_engine_command(raw));
                candidate.dispatch_text_command(dc4::mes_engine_command(raw));
                resume = raw.pos + uint32_t(raw.size);
                if (raw.op == 7) break;
            }
            if (candidate.current_line_.empty()) throw std::runtime_error("DC2PC saved dialogue is unavailable");
            candidate.vm_.seek_to_pc(resume);
        } else {
            // Retain the dialogue behind the choice without running its branch.
            for (size_t i = 0; i < target; ++i) {
                if (commands[i].op == 3) candidate.speaker_.clear();
                candidate.dispatch_text_command(dc4::mes_engine_command(commands[i]));
            }
            candidate.vm_.seek_to_pc(resume);
            size_t budget = 0;
            while (candidate.vm_.next(&c)) {
                if (++budget > 256 || c.op == 0x2E || c.op == 0x4C)
                    throw std::runtime_error("invalid DC2PC saved choice block");
            }
            if (!candidate.vm_.waiting_for_choice()) throw std::runtime_error("DC2PC saved choice is unavailable");
        }
        const std::string bg = name(0x84);
        if (!bg.empty()) {
            candidate.load_layer(1, bg);
            if (!candidate.layers_.count(1)) throw std::runtime_error("saved DC2PC background is unavailable: " + bg);
        }
        for (int i = 0; i < 5; ++i) {
            // Native hide clears the first byte but leaves stale bytes behind.
            // Never recover that stale tail as a visible character.
            const std::string body = name(0x104+i*128);
            if (body.empty()) continue;
            candidate.load_bst(i, body, i+4);
            if (!candidate.sprites_[i].img.valid()) throw std::runtime_error("saved DC2PC sprite is unavailable: " + body);
            const auto expression=name(0x384+i*128);
            if(!expression.empty() && !candidate.load_bst_expression(i,expression))
                throw std::runtime_error("saved DC2PC expression is unavailable: "+expression);
            const size_t at=0x14D0+i*52;
            const auto scale=integer(at+8);
            if(scale) {
                if(scale>10000)throw std::runtime_error("invalid saved DC2PC sprite scale");
                auto& sprite=candidate.sprites_[i];
                sprite.cur_x=int32_t(integer(at));sprite.cur_y=int32_t(integer(at+4));
                sprite.scale=scale/100.f;sprite.x=int32_t(integer(at+24));sprite.y=int32_t(integer(at+28));
            }
        }
        auto audio = candidate.scene_audio_.capture_snapshot();
        const uint32_t bgm = integer(0x804);
        if (bgm != uint32_t(-1) && bgm != 255) {
            if (bgm > 254) throw std::runtime_error("invalid DC2PC saved BGM");
            audio.bgm_id = int(bgm); audio.bgm_playing = true;
            audio.bgm_mode = 1;
        }
        std::string error;
        dc4::MesVmSnapshot vm;
        if (!candidate.vm_.capture_snapshot(&vm, &error)) throw std::runtime_error(error);
        // All assets and the checkpoint are validated in the detached runner.
        // Commit them directly: serializing then restoring a second runner used
        // to decode every background, sprite and music track twice on PC import.
        if (!scene_audio_.restore_snapshot(audio, false, &error)) throw std::runtime_error(error);
        mes_ = std::move(candidate.mes_);
        vm_.reset(); vm_.bind(mes_); vm_.restore_snapshot(vm);
        layers_.swap(candidate.layers_); sprites_.swap(candidate.sprites_);
        script_name_ = script; current_line_ = std::move(candidate.current_line_);
        speaker_ = std::move(candidate.speaker_);
        transition_type_ = 0;
        reset_camera();
        scene_dirty_ = true; scene_end_ = false; message_visible_ = true; preserve_scene_ = false;
        comu_visible_ = comu_group_ = false; comu_items_.clear();
        reset_restore_transients();
        if (restore_error) restore_error->clear();
        return true;
    } catch (const std::exception& error) {
        if (restore_error) *restore_error = error.what();
        return false;
    }
}

bool SceneRunner::restore_state(const std::vector<uint8_t>& bytes, std::string* restore_error) {
    try {
        if (bytes.size() < 24 || bytes.size() > kMaxSave + 24) throw std::runtime_error("invalid save size");
        if (std::memcmp(bytes.data(), kMagic, 8)) throw std::runtime_error("not a dc4engine save");
        Reader header{bytes, 8};
        const auto version = header.u32();
        if (version < 1 || version > 5)
            throw std::runtime_error("unsupported save version");
        if (header.u32() != bytes.size() - 24) throw std::runtime_error("invalid save payload length");
        const uint64_t expected_checksum = header.u64();
        std::vector<uint8_t> payload(bytes.begin() + 24, bytes.end());
        if (checksum(payload) != expected_checksum) throw std::runtime_error("save checksum mismatch");
        Reader in{payload};
        const std::string script = in.str(255);
        if (!resource_name(script, false)) throw std::runtime_error("invalid saved scenario path");
        const auto vm = read_vm(in);
        const std::string line = in.str(), speaker = in.str(1024);
        const bool ended = in.flag(), message_visible = in.flag(), preserve_scene = in.flag();
        const int transition = in.integer(); const double camera_scale = in.number();
        const double camera_x = version == 1 ? in.integer() : in.number();
        const double camera_y = version == 1 ? in.integer() : in.number();
        if (transition < 0 || transition > 255 || camera_scale < 1 || camera_scale > 655.35 ||
            camera_x < -127 || camera_x > 1151 || camera_y < -71 || camera_y > 647)
            throw std::runtime_error("invalid saved camera or transition");
        // Rehydrate and validate in an independent runner. A broken save never
        // destroys the currently running scene, its VM, or its resource state.
        SceneRunner candidate(data_);
        if (!candidate.load_script(data_ + "/MES/" + script)) throw std::runtime_error(candidate.last_error_);
        std::string error;
        if (!candidate.vm_.restore_snapshot(vm, &error)) throw std::runtime_error(error);
        const auto layers = in.count(64);
        for (uint32_t i = 0; i < layers; ++i) {
            const int index = in.integer(); const std::string name = in.str(255);
            if (index < 0 || index > 255 || !resource_name(name) || candidate.layers_.count(index))
                throw std::runtime_error("invalid saved image layer");
            candidate.load_layer(index, name);
            if (!candidate.layers_.count(index)) throw std::runtime_error("saved image is unavailable: " + name);
        }
        if (version <= 4 && candidate.mes_.version == 0x9969) {
            // Early DC2PC snapshots stored default placement even for hidden
            // layers. Later body loads rely on that placement remaining intact.
            // Recover transforms only; never replay story or progress changes.
            dc4::MesStream scan(candidate.mes_);
            dc4::MesCommand command;
            while (scan.next(&command) && command.pos < vm.pc)
                if (command.op == 0x64 || command.op == 0x65)
                    candidate.dispatch_sprite_command(dc4::mes_engine_command(command));
        }
        if (in.count(16) != 16) throw std::runtime_error("invalid saved sprite count");
        for (size_t i = 0; i < 16; ++i) {
            const std::string name = in.str(255), image = in.str(255);
            const double scale = in.number();
            const int x = in.integer(), y = in.integer(), alpha = in.integer(), z = in.integer();
            const bool visible = in.flag();
            const int cur_x = version >= 3 ? in.integer() : 0;
            const int cur_y = version >= 3 ? in.integer() : 0;
            if ((!name.empty() && !resource_name(name)) || (!image.empty() && !resource_name(image)) ||
                scale <= 0 || scale > 655.35 || x < -32768 || x > 65535 || y < -32768 || y > 65535 || alpha < 0 || alpha > 255 || z < 0 || z > 2)
                throw std::runtime_error("invalid saved sprite");
            if (!image.empty()) {
                candidate.load_bst(int(i), image, z == 2 ? 25 : z == 1 ? 19 : 4);
                if (!candidate.sprites_[i].img.valid()) throw std::runtime_error("saved sprite is unavailable: " + image);
            }
            auto& sprite = candidate.sprites_[i];
            sprite.name = name; sprite.image = image; sprite.scale = float(scale); sprite.x = x; sprite.y = y; sprite.alpha = alpha; sprite.z = z; sprite.visible = visible;
            if (version > 4 || candidate.mes_.version != 0x9969) {
                sprite.cur_x = cur_x; sprite.cur_y = cur_y;
            }
        }
        const auto audio = read_audio(in);
        uint16_t camera_duration = 5;
        std::vector<CameraMotion> camera_motion;
        if (version >= 2) {
            const auto duration = in.u32();
            if (duration > 65535) throw std::runtime_error("invalid saved camera duration");
            camera_duration = uint16_t(duration);
            const auto count = in.count(10); // native queue limit
            for (uint32_t i = 0; i < count; ++i) {
                CameraMotion motion;
                motion.from_scale = float(in.number()); motion.to_scale = float(in.number());
                motion.from_x = float(in.number()); motion.from_y = float(in.number());
                motion.to_x = float(in.number()); motion.to_y = float(in.number());
                motion.elapsed_ms = in.number(); motion.duration_ms = in.number();
                auto valid_scale = [](float scale) { return scale >= 1 && scale <= 655.35f; };
                auto valid_pivot = [](float x, float y) { return x >= -127 && x <= 1151 && y >= -71 && y <= 647; };
                if (!valid_scale(motion.from_scale) || !valid_scale(motion.to_scale) ||
                    !valid_pivot(motion.from_x, motion.from_y) || !valid_pivot(motion.to_x, motion.to_y) ||
                    motion.duration_ms <= 0 || motion.duration_ms > 65535.0 * 200 ||
                    motion.elapsed_ms < 0 || motion.elapsed_ms >= motion.duration_ms ||
                    (i && motion.elapsed_ms != 0))
                    throw std::runtime_error("invalid saved camera animation");
                camera_motion.push_back(motion);
            }
        }
        if (version >= 5) {
            for (size_t i = 0; i < candidate.sprites_.size(); ++i) {
                const auto expression = in.str(255);
                if (expression.empty()) continue;
                if (!resource_name(expression) ||
                    dc4::mes_opcode_profile(candidate.mes_.dialect, candidate.mes_.version).id != dc4::MesProfileId::DC2PC ||
                    !candidate.load_bst_expression(int(i), expression))
                    throw std::runtime_error("saved expression is unavailable: " + expression);
            }
        }
        bool restored_comu_visible = false;
        std::vector<ComuItem> restored_comu;
        if (version >= 4) {
            const bool comu_visible = in.flag();
            const auto items = in.count(200);
            std::vector<ComuItem> restored;
            restored.reserve(items);
            for (uint32_t i = 0; i < items; ++i) {
                ComuItem item;
                item.contact = in.integer();
                item.stamp = in.integer();
                item.text = in.str(255);
                if (item.contact < -1 || item.contact > 10 || item.stamp < -1 || item.stamp > 63)
                    throw std::runtime_error("invalid saved chat entry");
                restored.push_back(std::move(item));
            }
            restored_comu_visible = comu_visible;
            restored_comu = std::move(restored);
        }
        if (in.at != payload.size()) throw std::runtime_error("unexpected trailing save data");
        if (!scene_audio_.restore_snapshot(audio, false, &error)) throw std::runtime_error(error);
        comu_visible_ = restored_comu_visible;
        comu_group_ = false;
        comu_items_ = std::move(restored_comu);
        mes_ = std::move(candidate.mes_);
        vm_.reset(); vm_.bind(mes_); vm_.restore_snapshot(vm);
        layers_.swap(candidate.layers_); sprites_.swap(candidate.sprites_);
        scene_dirty_ = true;
        script_name_ = script; current_line_ = line; speaker_ = speaker; transition_type_ = transition;
        camera_scale_ = float(camera_scale); camera_x_ = float(camera_x); camera_y_ = float(camera_y);
        camera_duration_ = camera_duration; camera_motion_ = std::move(camera_motion);
        if (!animation_enabled_) finish_animations();
        scene_end_ = ended; message_visible_ = message_visible; preserve_scene_ = preserve_scene;
        reset_restore_transients();
        if (restore_error) restore_error->clear();
        return true;
    } catch (const std::exception& error) {
        if (restore_error) *restore_error = error.what();
        return false;
    }
}

void SceneRunner::reset_restore_transients() {
    next_script_.clear(); last_error_.clear(); pointer_x_ = pointer_y_ = -1; pointer_down_ = false;
    pending_movie_.clear(); pending_movie_is_effect_ = false; input_skip_label_.clear();
    script_wait_pending_ = false; script_wait_ms_ = 0;
    script_wait_skippable_ = true;
    dc2pc_clock_ms_ = dc2pc_wait_reference_ms_ = 0; dc2pc_wait_reference_valid_ = false;
    credits_active_ = false; credits_elapsed_ms_ = credits_duration_ms_ = 0;
    credits_archive_.clear(); credits_entries_.clear(); credits_pages_.clear();
    effect_pck_.clear();
    transition_active_ = transition_pending_ = false; transition_snap_.clear();
    dc2pc_transition_active_ = dc2pc_present_pending_ = dc2pc_black_ = false;
    dc2pc_displayed_frame_.clear(); dc2pc_transition_target_.clear();
    dc2pc_transition_elapsed_ms_ = dc2pc_transition_duration_ms_ = 0;
    scene_chips_.clear(); pending_scene_blit_ = false;
    sidebar_visible_ = false;
    if (mes_.version == 0x9969) {
        // Interview/other menu controls are ephemeral and never belong to a
        // story save. Reload the authored story toolbar even if sheet37 exists.
        load_ui_script(data_ + "/MES/start.mes");
        show_message_window("SYSTEM/MESWIN.CRX");
        taskbar_visible_ = true;
    }
    reset_dialogue_history();
}
} // namespace dc4scene
