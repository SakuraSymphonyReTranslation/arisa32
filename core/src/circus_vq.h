#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dc4 {

// The input is one complete zlib stream from an XPCM mode-3 payload.
bool decode_circus_vq(const uint8_t* data, size_t size, uint32_t decoded_pcm_size,
                      uint8_t flags, std::vector<int16_t>* out);

}
