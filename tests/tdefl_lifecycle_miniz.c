#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"

struct sink {
    unsigned char *data;
    size_t size, capacity, calls;
    long fail_after;
};

static mz_bool put(const void *data, int size, void *opaque) {
    struct sink *sink = (struct sink *)opaque;
    sink->calls++;
    if ((sink->fail_after >= 0) && ((long)(sink->calls - 1) >= sink->fail_after)) return MZ_FALSE;
    if ((size < 0) || (sink->size + (size_t)size > sink->capacity)) return MZ_FALSE;
    memcpy(sink->data + sink->size, data, (size_t)size);
    sink->size += (size_t)size;
    return MZ_TRUE;
}

static uint32_t checksum(const unsigned char *data, size_t size) {
    return (uint32_t)mz_crc32(MZ_CRC32_INIT, data, size);
}

int main(void) {
    const int flags = 128 | TDEFL_COMPUTE_ADLER32;
    unsigned char *input = (unsigned char *)malloc(100000);
    unsigned char *packed = (unsigned char *)malloc(120000);
    tdefl_compressor compressor;
    tdefl_status status;
    size_t in_size, out_size, total, calls;
    uint32_t state = 0x31415926U;
    size_t i;
    if (!input || !packed) return 1;
    for (i = 0; i < 100000; ++i) {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        input[i] = (i % 97 < 72) ? (unsigned char)('A' + i % 7) : (unsigned char)state;
    }

    status = tdefl_init(&compressor, NULL, NULL, flags);
    printf("caller-init %d %d %u\n", status, tdefl_get_prev_return_status(&compressor), tdefl_get_adler32(&compressor));
    in_size = 100000; out_size = 0;
    status = tdefl_compress(&compressor, input, &in_size, NULL, &out_size, TDEFL_FINISH);
    printf("caller-first %d %zu %zu %d %u\n", status, in_size, out_size,
           tdefl_get_prev_return_status(&compressor), tdefl_get_adler32(&compressor));
    total = calls = 0;
    while (status == TDEFL_STATUS_OKAY) {
        in_size = 0; out_size = 17;
        status = tdefl_compress(&compressor, NULL, &in_size, packed + total, &out_size, TDEFL_FINISH);
        total += out_size; calls++;
        if (total > 120000 || calls > 120000) return 1;
    }
    printf("caller-drain %zu %zu %d %d %u %u\n", calls, total, status,
           tdefl_get_prev_return_status(&compressor), tdefl_get_adler32(&compressor), checksum(packed, total));
    in_size = 0; out_size = 17;
    status = tdefl_compress(&compressor, NULL, &in_size, packed, &out_size, TDEFL_FINISH);
    printf("caller-post %d %zu %zu %d\n", status, in_size, out_size, tdefl_get_prev_return_status(&compressor));

    {
        struct sink sink = { packed, 0, 120000, 0, -1 };
        status = tdefl_init(&compressor, put, &sink, flags);
        status = tdefl_compress_buffer(&compressor, input, 100000, TDEFL_FINISH);
        printf("callback-ok %d %d %u %zu %zu %u\n", status,
               tdefl_get_prev_return_status(&compressor), tdefl_get_adler32(&compressor),
               sink.calls, sink.size, checksum(sink.data, sink.size));
    }

    {
        struct sink sink = { packed, 0, 120000, 0, 0 };
        tdefl_status follow;
        status = tdefl_init(&compressor, put, &sink, flags);
        status = tdefl_compress_buffer(&compressor, input, 100000, TDEFL_FINISH);
        printf("callback-fail %d %d %u %zu %zu\n", status,
               tdefl_get_prev_return_status(&compressor), tdefl_get_adler32(&compressor), sink.calls, sink.size);
        follow = tdefl_compress_buffer(&compressor, NULL, 0, TDEFL_FINISH);
        printf("callback-follow %d %d\n", follow, tdefl_get_prev_return_status(&compressor));
    }

    status = tdefl_init(&compressor, NULL, NULL, flags);
    in_size = 100000; out_size = 0;
    status = tdefl_compress(&compressor, input, &in_size, NULL, &out_size, TDEFL_FINISH);
    printf("finish-request %d %zu %zu %d %u\n", status, in_size, out_size,
           tdefl_get_prev_return_status(&compressor), tdefl_get_adler32(&compressor));
    in_size = 0; out_size = 0;
    status = tdefl_compress(&compressor, NULL, &in_size, NULL, &out_size, TDEFL_NO_FLUSH);
    printf("finish-mismatch %d %zu %zu %d\n", status, in_size, out_size, tdefl_get_prev_return_status(&compressor));

    free(packed);
    free(input);
    return 0;
}
