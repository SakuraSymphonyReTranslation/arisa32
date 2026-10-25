#include "arisa32/voice.h"
#include "arisa32/resource.h"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <limits>

namespace dc4 {

bool VoiceArchive::open(const std::string& path, std::string* err) {
    entries_.clear();
    path_.clear();
    std::string resolved = resolve_asset(path);
    FILE* f = fopen(resolved.c_str(), "rb");
    if (!f) {
        if (err) *err = "cannot open " + path;
        return false;
    }
    uint32_t count = 0;
    if (fread(&count, 4, 1, f) != 1 || count == 0 || count > 1000000) {
        fclose(f);
        if (err) *err = "bad entry count";
        return false;
    }
    auto seek = [f](uint64_t at) {
#ifdef _WIN32
        return _fseeki64(f, int64_t(at), SEEK_SET) == 0;
#else
        return fseeko(f, off_t(at), SEEK_SET) == 0;
#endif
    };
#ifdef _WIN32
    _fseeki64(f, 0, SEEK_END);
    const int64_t file_size = _ftelli64(f);
#else
    fseeko(f, 0, SEEK_END);
    const int64_t file_size = ftello(f);
#endif
    auto fail = [&](const char* message) {
        fclose(f); if (err) *err = message; return false;
    };
    const uint64_t toc_size = uint64_t(count) * 64;
    if (file_size < 0 || uint64_t(file_size) < 4 + toc_size || !seek(4))
        return fail("short voice archive");
    uint8_t first[64];
    if (fread(first, 1, 64, f) != 64) return fail("short TOC read");
    uint32_t first_offset;
    memcpy(&first_offset, first + 60, 4);
    // DC2PC: name[60], absolute offset, directly after the count. DC4 has
    // a count*8 index before its name[56], offset, size records.
    bool dc2pc = first[0] != 0 && first_offset >= 4 + toc_size && first_offset < uint64_t(file_size);
    for (size_t i = 0; i < 60 && first[i]; ++i)
        if (first[i] < 0x20 || first[i] > 0x7e) dc2pc = false;
    const uint64_t toc_at = dc2pc ? 4 : 4 + uint64_t(count) * 8;
    if (toc_at + toc_size > uint64_t(file_size) || !seek(toc_at)) return fail("short TOC read");
    std::vector<uint8_t> toc(size_t(toc_size), 0);
    if (fread(toc.data(), 1, toc.size(), f) != toc.size()) return fail("short TOC read");
    std::vector<VoiceEntry> entries(count);
    std::vector<uint32_t> offsets;
    for (size_t i = 0; i < count; ++i) {
        const uint8_t* record = toc.data() + i * 64;
        auto& entry = entries[i];
        memcpy(entry.name, record, dc2pc ? 60 : 56);
        memcpy(&entry.offset, record + (dc2pc ? 60 : 56), 4);
        if (!dc2pc) memcpy(&entry.size, record + 60, 4);
        if (entry.offset < toc_at + toc_size || entry.offset >= uint64_t(file_size))
            return fail("invalid voice offset");
        if (dc2pc) offsets.push_back(entry.offset);
        else if (!entry.size || uint64_t(entry.offset) + entry.size > uint64_t(file_size))
            return fail("invalid voice size");
    }
    if (dc2pc) {
        std::sort(offsets.begin(), offsets.end());
        offsets.erase(std::unique(offsets.begin(), offsets.end()), offsets.end());
        for (auto& entry : entries) {
            const auto next = std::upper_bound(offsets.begin(), offsets.end(), entry.offset);
            const uint64_t end = next == offsets.end() ? uint64_t(file_size) : *next;
            const uint64_t size = end - entry.offset;
            if (!size || size > std::numeric_limits<uint32_t>::max()) return fail("invalid voice size");
            entry.size = uint32_t(size);
        }
    }
    fclose(f);
    entries_ = std::move(entries);
    path_ = resolved;
    if (err) err->clear();
    return true;
}

static char lower(char c) { return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c; }

static bool ieq(const std::string& a, const char* b) {
    size_t i = 0;
    for (; i < a.size(); ++i) {
        if (!b[i] || lower(a[i]) != lower(b[i])) return false;
    }
    return b[i] == 0;
}

const VoiceEntry* VoiceArchive::find(const std::string& name) const {
    if (name.empty()) return nullptr;

    // Normalize slashes: convert '/' to '\' and strip leading slashes
    std::string key = name;
    for (char& c : key) {
        if (c == '/') c = '\\';
    }
    while (!key.empty() && key.front() == '\\') {
        key.erase(key.begin());
    }

    // Strip leading virtual mount prefixes: "VOICE\", "LVOICE\", "MVOICE\", "SVOICE\"
    static const char* kPrefixes[] = {"VOICE\\", "LVOICE\\", "MVOICE\\", "SVOICE\\"};
    for (const char* pfx : kPrefixes) {
        size_t len = strlen(pfx);
        if (key.size() >= len) {
            bool match = true;
            for (size_t i = 0; i < len; ++i) {
                if (lower(key[i]) != lower(pfx[i])) { match = false; break; }
            }
            if (match) {
                key = key.substr(len);
                while (!key.empty() && key.front() == '\\') key.erase(key.begin());
                break;
            }
        }
    }

    // Append ".pcm" if not already present
    if (key.size() < 4 || !ieq(key.substr(key.size() - 4), ".pcm")) {
        key += ".pcm";
    }

    auto normalize_entry_name = [](const char* raw) {
        std::string s = raw;
        for (char& c : s) if (c == '/') c = '\\';
        while (!s.empty() && s.front() == '\\') s.erase(s.begin());
        // Handle 48-byte struct truncation (.pcm truncated to .pc)
        if (s.size() >= 3 && ieq(s.substr(s.size() - 3), ".pc")) {
            s += "m";
        }
        return s;
    };

    // 1. Exact match against entry names (with normalized slashes)
    for (const auto& e : entries_) {
        std::string ename = normalize_entry_name(e.name);
        if (ieq(key, ename.c_str())) return &e;
    }

    // 2. Basename matching: extract the filename component of key
    std::string base_key = key;
    size_t last_slash = base_key.rfind('\\');
    if (last_slash != std::string::npos) {
        base_key = base_key.substr(last_slash + 1);
    }
    for (const auto& e : entries_) {
        std::string ename = normalize_entry_name(e.name);
        size_t e_slash = ename.rfind('\\');
        const char* e_base = (e_slash != std::string::npos) ? (ename.c_str() + e_slash + 1) : ename.c_str();
        if (ieq(base_key, e_base)) return &e;
    }

    return nullptr;
}

bool VoiceArchive::read_entry(const VoiceEntry& e, std::vector<uint8_t>* out, std::string* err) const {
    FILE* f = fopen(path_.c_str(), "rb");
    if (!f) {
        if (err) *err = "cannot reopen " + path_;
        return false;
    }
#ifdef _WIN32
    if (_fseeki64(f, e.offset, SEEK_SET) != 0) {
#else
    if (fseeko(f, off_t(e.offset), SEEK_SET) != 0) {
#endif
        fclose(f);
        if (err) *err = "seek failed";
        return false;
    }
    out->resize(e.size);
    bool ok = fread(out->data(), 1, e.size, f) == e.size;
    fclose(f);
    if (!ok && err) *err = "short data read";
    return ok;
}

}  // namespace dc4
