/*
 * Adapted from vgmstream's circus_vq_lib.c and circus_vq_data.h,
 * commit 253baa9de2a66cf2e532ebc0d47836ac1f8933bb.
 * https://github.com/vgmstream/vgmstream/tree/253baa9de2a66cf2e532ebc0d47836ac1f8933bb/src/coding/libs
 *
 * Copyright (c) 2008-2025 Adam Gashlin, Fastelbja, Ronny Elfert, bnnm,
 *                         Christopher Snowhill, NicknineTheEagle, bxaimc,
 *                         Thealexbarney, CyberBotX, EdnessP, et al
 * Portions Copyright (c) 2004-2008, Marko Kreen
 * Portions Copyright 2001-2007 jagarl / Kazunori Ueno <jagarl@creator.club.ne.jp>
 * Portions Copyright (c) 1998, Justin Frankel/Nullsoft Inc.
 * Portions Copyright (C) 2006 Nullsoft, Inc.
 * Portions Copyright (c) 2005-2007 Paul Hsieh
 * Portions Copyright (C) 2000-2004 Leshade Entis, Entis-soft.
 * Portions Public Domain originating with Sun Microsystems
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "circus_vq.h"
#include "miniz.h"

#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <stdexcept>

namespace dc4 {
namespace {

constexpr size_t kFrameBytes = 8192;
constexpr size_t kFrameSamples = 4064;
constexpr size_t kOverlapSamples = 32;
// Voice decoding stays bounded even when a corrupt header requests gigabytes.
constexpr uint32_t kMaxPcmBytes = 64u * 1024 * 1024;

constexpr int kScales[5][8] = {
    {2048, 2048, 4096, 4096, 8192, 8192, 16384, 16384},
    {2048, 2048, 2048, 4096, 4096, 4096, 8192, 16384},
    {2048, 2048, 2048, 2048, 2048, 4096, 4096, 8192},
    {1024, 1024, 1024, 2048, 2048, 2048, 2048, 2048},
    {1365, 1365, 2048, 2730, 2730, 4096, 4096, 4096},
};

// The native Q12 table is mirrored at pi/2; only its first quarter is needed.
constexpr std::array<int16_t, 1025> kSine = {
    0, 6, 12, 18, 25, 31, 37, 43, 50, 56, 62, 69, 75, 81, 87, 94,
    100, 106, 113, 119, 125, 131, 138, 144, 150, 157, 163, 169, 175, 182, 188, 194,
    200, 207, 213, 219, 226, 232, 238, 244, 251, 257, 263, 269, 276, 282, 288, 295,
    301, 307, 313, 320, 326, 332, 338, 345, 351, 357, 363, 370, 376, 382, 388, 395,
    401, 407, 413, 420, 426, 432, 438, 445, 451, 457, 463, 470, 476, 482, 488, 495,
    501, 507, 513, 520, 526, 532, 538, 545, 551, 557, 563, 569, 576, 582, 588, 594,
    601, 607, 613, 619, 625, 632, 638, 644, 650, 656, 663, 669, 675, 681, 687, 694,
    700, 706, 712, 718, 725, 731, 737, 743, 749, 755, 762, 768, 774, 780, 786, 792,
    799, 805, 811, 817, 823, 829, 836, 842, 848, 854, 860, 866, 872, 879, 885, 891,
    897, 903, 909, 915, 921, 928, 934, 940, 946, 952, 958, 964, 970, 976, 983, 989,
    995, 1001, 1007, 1013, 1019, 1025, 1031, 1037, 1043, 1050, 1056, 1062, 1068, 1074, 1080, 1086,
    1092, 1098, 1104, 1110, 1116, 1122, 1128, 1134, 1140, 1146, 1152, 1158, 1164, 1170, 1176, 1182,
    1189, 1195, 1201, 1207, 1213, 1219, 1225, 1231, 1237, 1243, 1248, 1254, 1260, 1266, 1272, 1278,
    1284, 1290, 1296, 1302, 1308, 1314, 1320, 1326, 1332, 1338, 1344, 1350, 1356, 1362, 1368, 1373,
    1379, 1385, 1391, 1397, 1403, 1409, 1415, 1421, 1427, 1433, 1438, 1444, 1450, 1456, 1462, 1468,
    1474, 1479, 1485, 1491, 1497, 1503, 1509, 1515, 1520, 1526, 1532, 1538, 1544, 1550, 1555, 1561,
    1567, 1573, 1579, 1584, 1590, 1596, 1602, 1608, 1613, 1619, 1625, 1631, 1636, 1642, 1648, 1654,
    1659, 1665, 1671, 1677, 1682, 1688, 1694, 1699, 1705, 1711, 1717, 1722, 1728, 1734, 1739, 1745,
    1751, 1756, 1762, 1768, 1773, 1779, 1785, 1790, 1796, 1802, 1807, 1813, 1819, 1824, 1830, 1835,
    1841, 1847, 1852, 1858, 1864, 1869, 1875, 1880, 1886, 1891, 1897, 1903, 1908, 1914, 1919, 1925,
    1930, 1936, 1941, 1947, 1952, 1958, 1964, 1969, 1975, 1980, 1986, 1991, 1997, 2002, 2007, 2013,
    2018, 2024, 2029, 2035, 2040, 2046, 2051, 2057, 2062, 2067, 2073, 2078, 2084, 2089, 2094, 2100,
    2105, 2111, 2116, 2121, 2127, 2132, 2138, 2143, 2148, 2154, 2159, 2164, 2170, 2175, 2180, 2186,
    2191, 2196, 2201, 2207, 2212, 2217, 2223, 2228, 2233, 2238, 2244, 2249, 2254, 2259, 2265, 2270,
    2275, 2280, 2286, 2291, 2296, 2301, 2306, 2312, 2317, 2322, 2327, 2332, 2337, 2343, 2348, 2353,
    2358, 2363, 2368, 2373, 2379, 2384, 2389, 2394, 2399, 2404, 2409, 2414, 2419, 2424, 2429, 2434,
    2439, 2445, 2450, 2455, 2460, 2465, 2470, 2475, 2480, 2485, 2490, 2495, 2500, 2505, 2510, 2515,
    2519, 2524, 2529, 2534, 2539, 2544, 2549, 2554, 2559, 2564, 2569, 2574, 2578, 2583, 2588, 2593,
    2598, 2603, 2608, 2613, 2617, 2622, 2627, 2632, 2637, 2641, 2646, 2651, 2656, 2661, 2665, 2670,
    2675, 2680, 2684, 2689, 2694, 2699, 2703, 2708, 2713, 2717, 2722, 2727, 2732, 2736, 2741, 2746,
    2750, 2755, 2760, 2764, 2769, 2773, 2778, 2783, 2787, 2792, 2796, 2801, 2806, 2810, 2815, 2819,
    2824, 2828, 2833, 2837, 2842, 2847, 2851, 2856, 2860, 2865, 2869, 2874, 2878, 2882, 2887, 2891,
    2896, 2900, 2905, 2909, 2914, 2918, 2922, 2927, 2931, 2936, 2940, 2944, 2949, 2953, 2957, 2962,
    2966, 2970, 2975, 2979, 2983, 2988, 2992, 2996, 3000, 3005, 3009, 3013, 3018, 3022, 3026, 3030,
    3034, 3039, 3043, 3047, 3051, 3055, 3060, 3064, 3068, 3072, 3076, 3080, 3085, 3089, 3093, 3097,
    3101, 3105, 3109, 3113, 3117, 3121, 3126, 3130, 3134, 3138, 3142, 3146, 3150, 3154, 3158, 3162,
    3166, 3170, 3174, 3178, 3182, 3186, 3190, 3193, 3197, 3201, 3205, 3209, 3213, 3217, 3221, 3225,
    3229, 3232, 3236, 3240, 3244, 3248, 3252, 3255, 3259, 3263, 3267, 3271, 3274, 3278, 3282, 3286,
    3289, 3293, 3297, 3301, 3304, 3308, 3312, 3315, 3319, 3323, 3326, 3330, 3334, 3337, 3341, 3345,
    3348, 3352, 3356, 3359, 3363, 3366, 3370, 3373, 3377, 3381, 3384, 3388, 3391, 3395, 3398, 3402,
    3405, 3409, 3412, 3416, 3419, 3423, 3426, 3429, 3433, 3436, 3440, 3443, 3447, 3450, 3453, 3457,
    3460, 3463, 3467, 3470, 3473, 3477, 3480, 3483, 3487, 3490, 3493, 3497, 3500, 3503, 3506, 3510,
    3513, 3516, 3519, 3522, 3526, 3529, 3532, 3535, 3538, 3541, 3545, 3548, 3551, 3554, 3557, 3560,
    3563, 3566, 3570, 3573, 3576, 3579, 3582, 3585, 3588, 3591, 3594, 3597, 3600, 3603, 3606, 3609,
    3612, 3615, 3618, 3621, 3624, 3627, 3629, 3632, 3635, 3638, 3641, 3644, 3647, 3650, 3652, 3655,
    3658, 3661, 3664, 3667, 3669, 3672, 3675, 3678, 3680, 3683, 3686, 3689, 3691, 3694, 3697, 3700,
    3702, 3705, 3708, 3710, 3713, 3716, 3718, 3721, 3723, 3726, 3729, 3731, 3734, 3736, 3739, 3742,
    3744, 3747, 3749, 3752, 3754, 3757, 3759, 3762, 3764, 3767, 3769, 3772, 3774, 3776, 3779, 3781,
    3784, 3786, 3789, 3791, 3793, 3796, 3798, 3800, 3803, 3805, 3807, 3810, 3812, 3814, 3816, 3819,
    3821, 3823, 3826, 3828, 3830, 3832, 3834, 3837, 3839, 3841, 3843, 3845, 3848, 3850, 3852, 3854,
    3856, 3858, 3860, 3862, 3864, 3867, 3869, 3871, 3873, 3875, 3877, 3879, 3881, 3883, 3885, 3887,
    3889, 3891, 3893, 3895, 3897, 3899, 3900, 3902, 3904, 3906, 3908, 3910, 3912, 3914, 3915, 3917,
    3919, 3921, 3923, 3925, 3926, 3928, 3930, 3932, 3933, 3935, 3937, 3939, 3940, 3942, 3944, 3945,
    3947, 3949, 3950, 3952, 3954, 3955, 3957, 3959, 3960, 3962, 3963, 3965, 3967, 3968, 3970, 3971,
    3973, 3974, 3976, 3977, 3979, 3980, 3982, 3983, 3985, 3986, 3988, 3989, 3990, 3992, 3993, 3995,
    3996, 3997, 3999, 4000, 4001, 4003, 4004, 4005, 4007, 4008, 4009, 4011, 4012, 4013, 4014, 4016,
    4017, 4018, 4019, 4020, 4022, 4023, 4024, 4025, 4026, 4027, 4029, 4030, 4031, 4032, 4033, 4034,
    4035, 4036, 4037, 4038, 4039, 4040, 4041, 4042, 4043, 4044, 4045, 4046, 4047, 4048, 4049, 4050,
    4051, 4052, 4053, 4054, 4055, 4056, 4057, 4057, 4058, 4059, 4060, 4061, 4062, 4062, 4063, 4064,
    4065, 4065, 4066, 4067, 4068, 4068, 4069, 4070, 4071, 4071, 4072, 4073, 4073, 4074, 4075, 4075,
    4076, 4076, 4077, 4078, 4078, 4079, 4079, 4080, 4080, 4081, 4081, 4082, 4082, 4083, 4083, 4084,
    4084, 4085, 4085, 4086, 4086, 4087, 4087, 4087, 4088, 4088, 4089, 4089, 4089, 4090, 4090, 4090,
    4091, 4091, 4091, 4091, 4092, 4092, 4092, 4092, 4093, 4093, 4093, 4093, 4094, 4094, 4094, 4094,
    4094, 4094, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095,
    4096,
};

int32_t wrap32(int64_t value) {
    const uint32_t bits = uint32_t(value);
    return bits <= uint32_t(std::numeric_limits<int32_t>::max())
        ? int32_t(bits) : int32_t(int64_t(bits) - 0x100000000LL);
}

int32_t add32(int32_t a, int32_t b) { return wrap32(int64_t(a) + b); }
int32_t sub32(int32_t a, int32_t b) { return wrap32(int64_t(a) - b); }

int64_t shift12(int64_t value) {
    // The transform uses arithmetic shifts; final sample divisions truncate.
    return value >= 0 ? value / 4096 : -((-value + 4095) / 4096);
}

int signed_code(uint16_t code) {
    if (!(code & 1)) return -int(code >> 1);
    const int value = (code >> 1) + 1;
    // DC2TY reads its lookup table as signed 16-bit, including index 65535.
    return value == 32768 ? -32768 : value;
}

void unpack_frame(const std::array<uint8_t, kFrameBytes>& encoded, const int* scales,
                  std::array<int32_t, 4096>* real, std::array<int32_t, 4096>* imaginary) {
    real->fill(0);
    imaginary->fill(0);
    for (size_t i = 0; i < 2048; ++i) {
        const uint8_t hi = encoded[4096 + i];
        const uint8_t lo = encoded[6144 + i];
        const uint16_t first = uint16_t((uint16_t(encoded[2 * i]) << 8) |
                                        (hi & 0xf0) | (lo >> 4));
        const uint16_t second = uint16_t((uint16_t(encoded[2 * i + 1]) << 8) |
                                         uint8_t((hi << 4) | (lo & 0x0f)));
        (*real)[i] = scales[i / 256] * signed_code(first);
        (*imaginary)[i] = scales[i / 256] * signed_code(second);
    }
}

void inverse_transform(std::array<int32_t, 4096>* real,
                       std::array<int32_t, 4096>* imaginary) {
    const auto butterfly = [&](int first, int second, int cosine, int sine, bool quarter) {
        const int32_t re = sub32((*real)[first], (*real)[second]);
        const int32_t im = sub32((*imaginary)[first], (*imaginary)[second]);
        (*real)[first] = add32((*real)[first], (*real)[second]);
        (*imaginary)[first] = add32((*imaginary)[first], (*imaginary)[second]);
        const int32_t rotated_re = wrap32(shift12(int64_t(re) * cosine) +
                                          shift12(int64_t(im) * sine));
        const int32_t rotated_im = wrap32(shift12(int64_t(im) * cosine) -
                                          shift12(int64_t(re) * sine));
        (*real)[second] = quarter ? rotated_im : rotated_re;
        (*imaginary)[second] = quarter ? sub32(0, rotated_re) : rotated_im;
    };

    int stride = 4096, half = 2048, quarter = 1024, angle_step = 1;
    for (int stage = 0; stage < 10; ++stage) {
        const int cosine = kSine[1024 - angle_step];
        const int sine = kSine[angle_step];
        for (int group = 0; group < 4096; group += stride) {
            butterfly(group, group + half, 4096, 0, false);
            butterfly(group + 1, group + half + 1, cosine, sine, false);
            butterfly(group + quarter, group + half + quarter, 4096, 0, true);
            butterfly(group + quarter + 1, group + half + quarter + 1, cosine, sine, true);
        }
        for (int bin = 2; bin < quarter; ++bin) {
            const int angle = bin * angle_step;
            for (int group = 0; group < 4096; group += stride) {
                butterfly(group + bin, group + half + bin,
                          kSine[1024 - angle], kSine[angle], false);
                butterfly(group + quarter + bin, group + half + quarter + bin,
                          kSine[1024 - angle], kSine[angle], true);
            }
        }
        stride = half;
        half = quarter;
        quarter /= 2;
        angle_step *= 2;
    }
    for (int i = 0; i < 4096; i += 4) {
        butterfly(i, i + 2, 4096, 0, false);
        butterfly(i + 1, i + 3, 4096, 0, true);
    }
    for (int i = 0; i < 4096; i += 2)
        butterfly(i, i + 1, 4096, 0, false);

    for (int i = 1, reversed = 0; i < 4095; ++i) {
        int bit = 2048;
        for (; bit <= reversed; bit /= 2) reversed -= bit;
        reversed += bit;
        if (i < reversed) {
            std::swap((*real)[i], (*real)[reversed]);
            std::swap((*imaginary)[i], (*imaginary)[reversed]);
        }
    }
}

struct Inflater {
    mz_stream stream{};
    bool initialized = false;
    ~Inflater() { if (initialized) mz_inflateEnd(&stream); }
};

bool decode(const uint8_t* data, size_t size, size_t sample_count, uint8_t flags,
            std::vector<int16_t>* out) {
    Inflater inflater;
    if (mz_inflateInit(&inflater.stream) != MZ_OK) return false;
    inflater.initialized = true;
    auto& stream = inflater.stream;
    stream.next_in = data;
    stream.avail_in = unsigned(size);

    std::array<uint8_t, kFrameBytes> encoded{};
    std::array<int32_t, 4096> real{}, imaginary{};
    std::array<int16_t, kOverlapSamples> overlap{};
    std::vector<int16_t> pcm(sample_count);
    int32_t hist1 = 0, hist2 = 0;
    int status = MZ_OK;
    const size_t frame_count = (sample_count + kFrameSamples - 1) / kFrameSamples;
    for (size_t frame = 0; frame < frame_count; ++frame) {
        if (status == MZ_STREAM_END) return false;
        stream.next_out = encoded.data();
        stream.avail_out = unsigned(encoded.size());
        status = mz_inflate(&stream, MZ_NO_FLUSH);
        if ((status != MZ_OK && status != MZ_STREAM_END) || stream.avail_out != 0)
            return false;

        unpack_frame(encoded, kScales[flags & 0x0f], &real, &imaginary);
        inverse_transform(&real, &imaginary);
        const size_t first_sample = frame * kFrameSamples;
        const size_t converted = std::min(real.size(), sample_count - first_sample);
        for (size_t i = 0; i < converted; ++i) {
            int32_t sample = real[i];
            if (flags & 0x10) sample = wrap32(int64_t(sample) * 3) / 2;
            sample /= 1024;
            // DC2TY's SHL EAX,11 wraps before signed division at 0x4060ff/0x40613e.
            sample = wrap32((int64_t(sample) * 27 + int64_t(hist1) * 4 + hist2) * 2048) / 65536;
            hist2 = hist1;
            hist1 = sample;
            if (i < kOverlapSamples && frame != 0)
                sample = int32_t((int64_t(i) * sample +
                                  int64_t(kOverlapSamples - i) * overlap[i]) / 32);
            const int16_t clipped = int16_t(std::clamp(sample, int32_t(-32768), int32_t(32767)));
            if (i < kFrameSamples) pcm[first_sample + i] = clipped;
            else overlap[i - kFrameSamples] = clipped;
        }
    }

    if (status != MZ_STREAM_END) {
        uint8_t extra = 0;
        stream.next_out = &extra;
        stream.avail_out = 1;
        status = mz_inflate(&stream, MZ_FINISH);
    }
    if (status != MZ_STREAM_END || stream.total_in != size ||
        stream.total_out != frame_count * kFrameBytes)
        return false;
    out->swap(pcm);
    return true;
}

}

bool decode_circus_vq(const uint8_t* data, size_t size, uint32_t decoded_pcm_size,
                      uint8_t flags, std::vector<int16_t>* out) {
    if (!out) return false;
    out->clear();
    if (!data || !size || size > std::numeric_limits<unsigned>::max() ||
        !decoded_pcm_size || (decoded_pcm_size & 1) || decoded_pcm_size > kMaxPcmBytes ||
        (flags & 0xe0) || (flags & 0x0f) >= 5)
        return false;
    try {
        return decode(data, size, decoded_pcm_size / 2, flags, out);
    } catch (const std::bad_alloc&) {
        return false;
    } catch (const std::length_error&) {
        return false;
    }
}

}
