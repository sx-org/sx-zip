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

static int record_compressed(const unsigned char *plain, size_t plain_size, int level) {
    unsigned char packed[8192];
    mz_ulong packed_size = sizeof(packed);
    int status = mz_compress2(packed, &packed_size, plain, (mz_ulong)plain_size, level);
    return put_u32((uint32_t)status) && put_u32((uint32_t)packed_size) &&
           fwrite(packed, 1, packed_size, stdout) == packed_size;
}

int main(void) {
    static const int levels[] = { -2, -1, 0, 1, 6, 9, 10, 11, 99 };
    unsigned char plain[4096], packed[8192], combined[8200], output[8192];
    mz_ulong packed_size, output_size, source_size;
    uint32_t state = 0x13579bdfU;
    size_t i;
    int status;

    for (i = 0; i < sizeof(plain); ++i) {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        plain[i] = (i % 47 < 35) ? (unsigned char)('a' + i % 9) : (unsigned char)state;
    }
    for (i = 0; i < sizeof(levels) / sizeof(levels[0]); ++i)
        if (!record_compressed(plain, sizeof(plain), levels[i])) return 1;

    packed_size = sizeof(packed);
    status = mz_compress(packed, &packed_size, plain, sizeof(plain));
    if (!put_u32((uint32_t)status) || !put_u32((uint32_t)packed_size) ||
        fwrite(packed, 1, packed_size, stdout) != packed_size) return 1;

    output_size = 2;
    status = mz_compress2(output, &output_size, plain, sizeof(plain), 6);
    if (!put_u32((uint32_t)status) || !put_u32((uint32_t)output_size)) return 1;

    output_size = sizeof(output);
    status = mz_uncompress(output, &output_size, packed, packed_size);
    if (!put_u32((uint32_t)status) || !put_u32((uint32_t)output_size) ||
        fwrite(output, 1, output_size, stdout) != output_size) return 1;

    memcpy(combined, packed, packed_size);
    combined[packed_size + 0] = 0xde; combined[packed_size + 1] = 0xad;
    combined[packed_size + 2] = 0xbe; combined[packed_size + 3] = 0xef;
    output_size = sizeof(output);
    source_size = packed_size + 4;
    status = mz_uncompress2(output, &output_size, combined, &source_size);
    if (!put_u32((uint32_t)status) || !put_u32((uint32_t)output_size) ||
        !put_u32((uint32_t)source_size) ||
        fwrite(output, 1, output_size, stdout) != output_size) return 1;

    output_size = 13;
    source_size = packed_size;
    status = mz_uncompress2(output, &output_size, packed, &source_size);
    if (!put_u32((uint32_t)status) || !put_u32((uint32_t)output_size) ||
        !put_u32((uint32_t)source_size)) return 1;

    output_size = sizeof(output);
    source_size = packed_size - 1;
    status = mz_uncompress2(output, &output_size, packed, &source_size);
    if (!put_u32((uint32_t)status) || !put_u32((uint32_t)output_size) ||
        !put_u32((uint32_t)source_size)) return 1;

    combined[0] = 0;
    output_size = sizeof(output);
    source_size = packed_size;
    status = mz_uncompress2(output, &output_size, combined, &source_size);
    if (!put_u32((uint32_t)status) || !put_u32((uint32_t)output_size) ||
        !put_u32((uint32_t)source_size)) return 1;
    return 0;
}
