#include "arisa32/audio.h"
#include "arisa32/xpcm.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

namespace {
int checks = 0, failures = 0;
void check(bool result, const char* expression, int line) {
    ++checks;
    if (!result) { ++failures; std::cerr << line << ": " << expression << '\n'; }
}
#define CHECK(x) check(bool(x), #x, __LINE__)
void put16(std::vector<uint8_t>& bytes, size_t at, uint16_t value) {
    bytes[at] = uint8_t(value); bytes[at + 1] = uint8_t(value >> 8);
}
void put32(std::vector<uint8_t>& bytes, size_t at, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[at + i] = uint8_t(value >> (i * 8));
}
std::vector<uint8_t> stored_zlib(const std::vector<uint8_t>& raw) {
    CHECK(raw.size() < 65536);
    std::vector<uint8_t> bytes{0x78,0x01,0x01,0,0,0,0};
    put16(bytes,3,uint16_t(raw.size())); put16(bytes,5,uint16_t(~raw.size()));
    bytes.insert(bytes.end(),raw.begin(),raw.end());
    uint32_t a = 1, b = 0;
    for (uint8_t value : raw) { a = (a + value) % 65521; b = (b + a) % 65521; }
    const uint32_t checksum = (b << 16) | a;
    for (int shift : {24,16,8,0}) bytes.push_back(uint8_t(checksum >> shift));
    return bytes;
}
std::vector<uint8_t> voice(const std::vector<uint8_t>& raw, uint32_t decoded_bytes) {
    const auto packed = stored_zlib(raw);
    std::vector<uint8_t> bytes(32);
    bytes[0] = 'X'; bytes[1] = 'P'; bytes[2] = 'C'; bytes[3] = 'M';
    put32(bytes,4,decoded_bytes); put32(bytes,8,0x0303);
    put16(bytes,12,1); put16(bytes,14,1); put32(bytes,16,44100);
    put32(bytes,20,88200); put16(bytes,24,2); put16(bytes,26,16);
    put32(bytes,28,uint32_t(packed.size()));
    bytes.insert(bytes.end(),packed.begin(),packed.end());
    return bytes;
}
std::vector<uint8_t> spectrum() {
    std::vector<uint8_t> raw(16384);
    for (int frame = 0; frame < 2; ++frame)
        for (const auto [offset,value] : {std::pair<int,int>{0,0x80},{1,0x20},{2,0x40},
                                         {8,0x70},{1024,0x40},{2048,0x20},{4096,0x10},{6144,0x30}})
            raw[size_t(frame) * 8192 + offset] = uint8_t(value + frame * 3);
    return raw;
}
uint64_t pcm_hash(const std::vector<int16_t>& pcm) {
    uint64_t hash = 14695981039346656037ULL;
    for (int16_t sample : pcm) for (unsigned shift : {0u,8u}) {
        hash ^= uint8_t(uint16_t(sample) >> shift);
        hash *= 1099511628211ULL;
    }
    return hash;
}
void reject(const std::vector<uint8_t>& bytes) {
    std::vector<int16_t> pcm{123}; uint32_t rate = 1; uint16_t channels = 1;
    CHECK(!dc4::decode_xpcm_pcm(bytes.data(),bytes.size(),&pcm,&rate,&channels));
    CHECK(pcm.empty() && rate == 0 && channels == 0);
}
}

int main() {
    const auto raw = spectrum();
    const auto bytes = voice(raw,16256);
    dc4::XpcmInfo info;
    CHECK(dc4::parse_xpcm(bytes.data(),bytes.size(),&info));
    CHECK(info.codec == dc4::XpcmCodec::CircusVq && info.has_wavefmt);
    CHECK(info.header_size == 32 && info.data_size == bytes.size() - 32);
    CHECK(info.decoded_pcm_size == 16256 && info.compression_flags == 3);
    CHECK(info.format_tag == 1 && info.channels == 1 && info.sample_rate == 44100);
    CHECK(info.avg_bytes_sec == 88200 && info.block_align == 2 && info.bits == 16);
    std::vector<uint8_t> payload;
    CHECK(dc4::xpcm_payload(bytes.data(),bytes.size(),&payload));
    CHECK(payload == std::vector<uint8_t>(bytes.begin() + 32,bytes.end()));

    std::vector<int16_t> pcm; uint32_t rate = 0; uint16_t channels = 0;
    CHECK(dc4::decode_xpcm_pcm(bytes.data(),bytes.size(),&pcm,&rate,&channels));
    CHECK(rate == 44100 && channels == 1 && pcm.size() == 8128);
    // Synthetic coefficients decoded by the original DC2TY.exe, including signed wrap and overlap.
    CHECK(pcm_hash(pcm) == 0x3fed63aa67ad9a0bULL);
    const auto expected = pcm;
    auto partial = bytes; put32(partial,4,8130);
    CHECK(dc4::decode_xpcm_pcm(partial.data(),partial.size(),&pcm,&rate,&channels));
    CHECK(pcm.size() == 4065 && pcm_hash(pcm) == 0x0f6f0da66a0b316dULL);
    auto boosted = bytes; put32(boosted,8,0x1303);
    CHECK(dc4::decode_xpcm_pcm(boosted.data(),boosted.size(),&pcm,&rate,&channels));
    CHECK(pcm.size() == 8128 && pcm_hash(pcm) == 0x91405154337394b1ULL);
    auto padded = bytes; padded.resize(padded.size() + 2048);
    CHECK(dc4::decode_xpcm_pcm(padded.data(),padded.size(),&pcm,&rate,&channels));
    CHECK(pcm == expected);
    auto silence = voice(std::vector<uint8_t>(8192),8128);
    CHECK(dc4::decode_xpcm_pcm(silence.data(),silence.size(),&pcm,&rate,&channels));
    CHECK(pcm.size() == 4064 && std::all_of(pcm.begin(),pcm.end(),[](int16_t value){ return value == 0; }));

    for (size_t size : {size_t(0),size_t(15),size_t(27),size_t(28),size_t(31),bytes.size() - 1})
        reject(std::vector<uint8_t>(bytes.begin(),bytes.begin() + size));
    auto bad = bytes; bad.back() ^= 1; reject(bad);
    bad = bytes; bad[32] ^= 1; reject(bad);
    bad = bytes; bad.push_back(0); put32(bad,28,uint32_t(bad.size() - 32)); reject(bad);
    for (uint32_t decoded : {0u,1u,8128u,24384u,0xfffffff0u}) {
        bad = bytes; put32(bad,4,decoded); reject(bad);
    }
    for (uint32_t codec : {0x0503u,0x2003u,0x010303u}) {
        bad = bytes; put32(bad,8,codec); reject(bad);
    }
    bad = bytes; put32(bad,28,0); reject(bad);
    bad = bytes; put32(bad,28,0xffffffffu); reject(bad);
    bad = bytes; put16(bad,12,2); reject(bad);
    bad = bytes; put16(bad,14,0); reject(bad);
    bad = bytes; put16(bad,14,3); reject(bad);
    bad = bytes; put32(bad,16,0); reject(bad);
    bad = bytes; put16(bad,24,4); reject(bad);
    bad = bytes; put16(bad,26,8); reject(bad);
    reject(voice({},2));
    CHECK(!dc4::parse_xpcm(nullptr,32,&info));
    CHECK(!dc4::parse_xpcm(bytes.data(),bytes.size(),nullptr));
    CHECK(!dc4::xpcm_payload(bytes.data(),31,&payload));
    CHECK(!dc4::xpcm_payload(bytes.data(),bytes.size(),nullptr));
    std::cout << "XPCM: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
