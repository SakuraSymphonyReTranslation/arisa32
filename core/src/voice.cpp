#include "arisa32/voice.h"
#include "arisa32/resource.h"

#include <cstdio>
#include <cstring>

namespace dc4 {

bool VoiceArchive::open(const std::string& path, std::string* err) {
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
    // Legacy index: count*(offset,size) pairs — skipped by the engine loader.
    if (fseek(f, long(count) * 8, SEEK_CUR) != 0) {
        fclose(f);
        if (err) *err = "seek over index failed";
        return false;
    }
    entries_.resize(count);
    if (fread(entries_.data(), 64, count, f) != count) {
        fclose(f);
        if (err) *err = "short TOC read";
        return false;
    }
    fclose(f);
    path_ = resolved;
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
