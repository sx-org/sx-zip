#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "miniz.h"

static int put_u32(uint32_t value) {
    unsigned char bytes[4] = {
        (unsigned char)value, (unsigned char)(value >> 8),
        (unsigned char)(value >> 16), (unsigned char)(value >> 24),
    };
    return fwrite(bytes, 1, sizeof(bytes), stdout) == sizeof(bytes);
}

static int put_string(const char *value) {
    size_t length;
    if (!value) return put_u32(UINT32_MAX);
    length = strlen(value);
    return length <= UINT32_MAX && put_u32((uint32_t)length) &&
           fwrite(value, 1, length, stdout) == length;
}

int main(void) {
    static const int statuses[] = {
        MZ_OK, MZ_STREAM_END, MZ_NEED_DICT, MZ_ERRNO, MZ_STREAM_ERROR,
        MZ_DATA_ERROR, MZ_MEM_ERROR, MZ_BUF_ERROR, MZ_VERSION_ERROR,
        MZ_PARAM_ERROR, 12345,
    };
    static const size_t lengths[] = { 0, 1, 3, 4, 7, 8, 5551, 5552, 5553, 11999, 12000 };
    static const mz_ulong bounds[] = { 0, 1, 31743, 31744, 31745, 65535, 1000000 };
    unsigned char data[12000];
    uint32_t state = 0x6d2b79f5U;
    mz_ulong adler, crc;
    size_t i;

    if (!put_string(mz_version())) return 1;
    for (i = 0; i < sizeof(statuses) / sizeof(statuses[0]); ++i)
        if (!put_string(mz_error(statuses[i]))) return 1;

    for (i = 0; i < sizeof(data); ++i) {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        data[i] = (unsigned char)(state + (uint32_t)(i * 37));
    }

    if (!put_u32((uint32_t)mz_adler32(0x12345678U, NULL, 0)) ||
        !put_u32((uint32_t)mz_crc32(0x89abcdefU, NULL, 0))) return 1;
    for (i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        if (!put_u32((uint32_t)mz_adler32(MZ_ADLER32_INIT, data, lengths[i])) ||
            !put_u32((uint32_t)mz_crc32(MZ_CRC32_INIT, data, lengths[i]))) return 1;
    }

    adler = MZ_ADLER32_INIT;
    crc = MZ_CRC32_INIT;
    for (i = 0; i < sizeof(data);) {
        size_t amount = ((i * 17) % 101) + 1;
        if (amount > sizeof(data) - i) amount = sizeof(data) - i;
        adler = mz_adler32(adler, data + i, amount);
        crc = mz_crc32(crc, data + i, amount);
        i += amount;
    }
    if (!put_u32((uint32_t)adler) || !put_u32((uint32_t)crc)) return 1;

    for (i = 0; i < sizeof(bounds) / sizeof(bounds[0]); ++i)
        if (!put_u32((uint32_t)mz_compressBound(bounds[i])) ||
            !put_u32((uint32_t)mz_deflateBound(NULL, bounds[i]))) return 1;
    return 0;
}
