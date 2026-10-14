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
