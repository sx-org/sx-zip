#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "miniz.h"

#ifndef ORACLE_PREFIX
#define ORACLE_PREFIX "c"
#endif

static int emit_case(const unsigned char *data, size_t size, const char *name,
                     int level, int strategy) {
    char path[128];
    size_t packed_size = 0;
    mz_uint flags = tdefl_create_comp_flags_from_zip_params(level, 15, strategy);
    void *packed = tdefl_compress_mem_to_heap(data, size, &packed_size, (int)flags);
    FILE *file;
    int ok;
    if (!packed) return 0;
    snprintf(path, sizeof(path), "/tmp/sx-miniz-strategy-%s-%s.zlib", ORACLE_PREFIX, name);
    file = fopen(path, "wb");
    if (!file) { mz_free(packed); return 0; }
    ok = fwrite(packed, 1, packed_size, file) == packed_size;
    ok = fclose(file) == 0 && ok;
    mz_free(packed);
    return ok;
}

int main(void) {
    unsigned char *data = (unsigned char *)malloc(65536);
    uint32_t state = 0x12345678u;
    size_t i;
    int ok = 1;
    if (!data) return 1;
    for (i = 0; i < 65536; ++i) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        data[i] = (i % 251 < 190) ? (unsigned char)('a' + ((i / 7) % 11)) : (unsigned char)state;
    }
    ok = emit_case(data, 65536, "default-neg1", -1, MZ_DEFAULT_STRATEGY) && ok;
    ok = emit_case(data, 65536, "default-neg2", -2, MZ_DEFAULT_STRATEGY) && ok;
    ok = emit_case(data, 65536, "default-0", 0, MZ_DEFAULT_STRATEGY) && ok;
    ok = emit_case(data, 65536, "default-1", 1, MZ_DEFAULT_STRATEGY) && ok;
    ok = emit_case(data, 65536, "default-6", 6, MZ_DEFAULT_STRATEGY) && ok;
    ok = emit_case(data, 65536, "default-10", 10, MZ_DEFAULT_STRATEGY) && ok;
    ok = emit_case(data, 65536, "default-11", 11, MZ_DEFAULT_STRATEGY) && ok;
    ok = emit_case(data, 65536, "default-99", 99, MZ_DEFAULT_STRATEGY) && ok;
    ok = emit_case(data, 65536, "filtered-6", 6, MZ_FILTERED) && ok;
    ok = emit_case(data, 65536, "huffman-6", 6, MZ_HUFFMAN_ONLY) && ok;
    ok = emit_case(data, 65536, "rle-6", 6, MZ_RLE) && ok;
    ok = emit_case(data, 65536, "fixed-6", 6, MZ_FIXED) && ok;
    free(data);
    return ok ? 0 : 1;
}
