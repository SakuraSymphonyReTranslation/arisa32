#include "arisa32/mesfile.h"
#include "arisa32/resource.h"

#include <cstdio>
#include <cstring>

namespace dc4 {

static uint32_t rd_u32(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}
static uint16_t rd_u16(const uint8_t* p) {
    uint16_t v;
    std::memcpy(&v, p, 2);
    return v;
}

static bool is_valid_modern_stream(const uint8_t* code, size_t len) {
    if (len < 4) return false;
    size_t p = 3;
    size_t valid_cmds = 0;
    while (p < len) {
        uint8_t op = code[p];
        if (op == 0) { p++; continue; }
        if (op == 0x2E) {
            if (p + 2 > len) return false;
            const void* f = memchr(code + p + 2, 0, len - (p + 2));
            if (!f) return false;
            p = (static_cast<const uint8_t*>(f) - code) + 1;
        } else if (op < 0x3B) {
            if (p + 3 > len) return false;
            p += 3;
        } else if (op < 0x48) {
            if (p + 2 > len) return false;
            const void* f = memchr(code + p + 2, 0, len - (p + 2));
            if (!f) return false;
            p = (static_cast<const uint8_t*>(f) - code) + 1;
        } else if (op < 0x6E) {
            const void* f = memchr(code + p + 1, 0, len - (p + 1));
            if (!f) return false;
            p = (static_cast<const uint8_t*>(f) - code) + 1;
        } else {
            if (p + 9 > len) return false;
            p += 9;
        }
        valid_cmds++;
    }
    return (p == len && valid_cmds > 0);
}

bool MesFile::load(const uint8_t* data, size_t size, std::string* err) {
    if (size < 8) {
        if (err) *err = "file too small";
        return false;
    }
    const uint32_t count = rd_u32(data);
    if (count > 0x100000) {
        if (err) *err = "bad label count";
        return false;
    }
    const size_t te4 = size_t(count) * 4 + 4;
    const size_t te6 = size_t(count) * 6 + 4;

    bool is_4byte = false;
    if (count == 0) {
        is_4byte = true;
    } else if (te6 + 4 <= size && is_valid_modern_stream(data + te6, size - te6)) {
        is_4byte = false;
    } else if (te6 + 4 > size) {
        is_4byte = true;
    } else if (te4 < size && (data[te4] == 0x69 || data[te4] == 0x63)) {
        is_4byte = true;
    }

    const size_t table_end = is_4byte ? te4 : te6;
    if (table_end > size) {
        if (err) *err = "truncated MES table";
        return false;
    }
    version = (table_end + 4 <= size) ? rd_u32(data + table_end) : 0;

    if (is_4byte) {
        dialect = MesDialect::ClassicDC2;
    } else if (table_end + 3 <= size && data[table_end] == 0x72 && data[table_end + 1] == 0x9D && data[table_end + 2] == 0x00) {
        dialect = MesDialect::ClassicDM;
    } else {
        dialect = MesDialect::Modern;
    }
    labels.resize(count);
    const uint8_t* offs = data + 4;
    const uint8_t* ids = is_4byte ? nullptr : (data + 4 + size_t(count) * 4);
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t raw = rd_u32(offs + i * 4);
        labels[i].offset = raw & 0x00FFFFFFu;
        labels[i].flags = raw >> 24;
        labels[i].id = ids ? rd_u16(ids + i * 2) : uint16_t(i);
    }
    code.assign(data + table_end, data + size);
    code_base = table_end;
    return true;
}

bool MesFile::load_file(const std::string& path, std::string* err) {
    FILE* f = fopen(resolve_asset(path).c_str(), "rb");
    if (!f) {
        if (err) *err = "cannot open " + path;
        return false;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf(size_t(sz > 0 ? sz : 0));
    bool ok = sz > 0 && std::fread(buf.data(), 1, buf.size(), f) == buf.size();
    fclose(f);
    if (!ok) {
        if (err) *err = "short read";
        return false;
    }
    return load(buf.data(), buf.size(), err);
}

const MesLabel* MesFile::find_label(uint16_t id) const {
    for (const auto& l : labels)
        if (l.id == id) return &l;
    return nullptr;
}

}  // namespace dc4
