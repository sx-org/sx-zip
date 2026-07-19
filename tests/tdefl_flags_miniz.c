#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "miniz.h"

static int put_u32(uint32_t value) {
    unsigned char bytes[4] = {
        (unsigned char)value, (unsigned char)(value >> 8),
        (unsigned char)(value >> 16), (unsigned char)(value >> 24),
    };
    return fwrite(bytes, 1, 4, stdout) == 4;
}

struct pattern_sink { unsigned char *data; size_t size, capacity; };

static mz_bool pattern_put(const void *data, int size, void *opaque) {
    struct pattern_sink *sink = (struct pattern_sink *)opaque;
    if (size < 0 || sink->size + (size_t)size > sink->capacity) return MZ_FALSE;
    memcpy(sink->data + sink->size, data, (size_t)size);
    sink->size += (size_t)size;
    return MZ_TRUE;
}

static int emit_nondeterministic(const unsigned char *input, size_t input_size) {
    tdefl_compressor compressor;
    struct pattern_sink sink;
    tdefl_status status;
    sink.data = (unsigned char *)malloc(input_size + 4096);
    if (!sink.data) return 0;
    sink.size = 0; sink.capacity = input_size + 4096;
    memset(&compressor, 0xa5, sizeof(compressor));
    status = tdefl_init(&compressor, pattern_put, &sink,
                        128 | TDEFL_NONDETERMINISTIC_PARSING_FLAG);
    if (status == TDEFL_STATUS_OKAY)
        status = tdefl_compress_buffer(&compressor, input, input_size, TDEFL_FINISH);
    if (status != TDEFL_STATUS_DONE || sink.size > UINT32_MAX ||
        !put_u32((uint32_t)sink.size) || fwrite(sink.data, 1, sink.size, stdout) != sink.size) {
        free(sink.data); return 0;
    }
    free(sink.data);
    return 1;
}

int main(void) {
    static const int flags[] = {
        7 | TDEFL_GREEDY_PARSING_FLAG,
        128,
        TDEFL_GREEDY_PARSING_FLAG,
        128 | TDEFL_RLE_MATCHES,
        128 | TDEFL_FILTER_MATCHES,
        128 | TDEFL_FORCE_ALL_STATIC_BLOCKS,
        TDEFL_FORCE_ALL_RAW_BLOCKS | TDEFL_GREEDY_PARSING_FLAG,
        128 | TDEFL_WRITE_ZLIB_HEADER,
        128 | TDEFL_COMPUTE_ADLER32,
        128 | TDEFL_RLE_MATCHES | TDEFL_FILTER_MATCHES,
    };
    unsigned char *input = (unsigned char *)malloc(100000);
    uint32_t state = 0x31415926U;
    size_t i, case_index;
    if (!input) return 1;
    for (i = 0; i < 100000; ++i) {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        input[i] = (i % 97 < 72) ? (unsigned char)('A' + (i % 7)) : (unsigned char)state;
    }
    for (case_index = 0; case_index < sizeof(flags) / sizeof(flags[0]); ++case_index) {
        size_t output_size = 0;
        void *output = tdefl_compress_mem_to_heap(input, 100000, &output_size, flags[case_index]);
        if (!output || output_size > UINT32_MAX || !put_u32((uint32_t)output_size) ||
            fwrite(output, 1, output_size, stdout) != output_size) {
            mz_free(output); free(input); return 1;
        }
        mz_free(output);
    }
    if (!emit_nondeterministic(input, 100000)) { free(input); return 1; }
    free(input);
    return 0;
}
