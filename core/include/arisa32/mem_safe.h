// Safe unaligned and bounds-checked memory access helpers.
// Prevents SIGBUS and undefined behavior on ARM / Android architectures.
#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>

namespace dc4 {

// Checks whether [offset, offset + count) fits inside total_size without integer overflow.
inline bool check_bounds(size_t offset, size_t count, size_t total_size) {
    return offset <= total_size && count <= (total_size - offset);
}

// Safely loads a trivially-copyable value from unaligned memory using std::memcpy.
// Avoids reinterpret_cast dereferences that cause SIGBUS on ARMv7 / ARM64.
template <typename T>
inline T read_unaligned(const void* ptr) {
    static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
    T val;
    std::memcpy(&val, ptr, sizeof(T));
    return val;
}

// Safely stores a trivially-copyable value to unaligned memory using std::memcpy.
template <typename T>
inline void write_unaligned(void* dst, const T& val) {
    static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
    std::memcpy(dst, &val, sizeof(T));
}

// Little-endian integer readers with unaligned safety
inline uint16_t read_u16_le(const void* ptr) {
    const auto* p = static_cast<const uint8_t*>(ptr);
    return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}

inline uint32_t read_u32_le(const void* ptr) {
    const auto* p = static_cast<const uint8_t*>(ptr);
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

inline uint64_t read_u64_le(const void* ptr) {
    const auto* p = static_cast<const uint8_t*>(ptr);
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= uint64_t(p[i]) << (i * 8);
    }
    return v;
}

inline int16_t read_i16_le(const void* ptr) {
    return static_cast<int16_t>(read_u16_le(ptr));
}

inline int32_t read_i32_le(const void* ptr) {
    return static_cast<int32_t>(read_u32_le(ptr));
}

inline float read_f32_le(const void* ptr) {
    uint32_t u = read_u32_le(ptr);
    float f;
    std::memcpy(&f, &u, sizeof(f));
    return f;
}

inline double read_f64_le(const void* ptr) {
    uint64_t u = read_u64_le(ptr);
    double d;
    std::memcpy(&d, &u, sizeof(d));
    return d;
}

// Bounds-checked little-endian readers
inline bool safe_read_u16_le(const uint8_t* buf, size_t len, size_t offset, uint16_t* out) {
    if (!check_bounds(offset, 2, len)) return false;
    if (out) *out = read_u16_le(buf + offset);
    return true;
}

inline bool safe_read_u32_le(const uint8_t* buf, size_t len, size_t offset, uint32_t* out) {
    if (!check_bounds(offset, 4, len)) return false;
    if (out) *out = read_u32_le(buf + offset);
    return true;
}

// Safely extracts a null-terminated or fixed-width string from a byte buffer.
// Guarantees no reads beyond the buffer boundaries.
inline std::string safe_string(const uint8_t* buf, size_t len, size_t offset, size_t max_len) {
    if (offset >= len) return {};
    size_t limit = std::min(max_len, len - offset);
    const char* start = reinterpret_cast<const char*>(buf + offset);
    const void* null_pos = std::memchr(start, 0, limit);
    if (null_pos) {
        limit = static_cast<const char*>(null_pos) - start;
    }
    return std::string(start, limit);
}

}  // namespace dc4
