#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"

static int put_u32(uint32_t value) {
    unsigned char bytes[4] = {
        (unsigned char)value, (unsigned char)(value >> 8),
        (unsigned char)(value >> 16), (unsigned char)(value >> 24),
    };
    return fwrite(bytes, 1, 4, stdout) == 4;
}

typedef struct bit_writer_tag {
    unsigned char bytes[128];
    size_t bit_count;
} bit_writer;

static void bits_init(bit_writer *writer) {
    memset(writer, 0, sizeof(*writer));
}

static void bits_put(bit_writer *writer, unsigned value, unsigned count) {
    unsigned i;
    for (i = 0; i < count; ++i) {
        if (value & (1U << i))
            writer->bytes[writer->bit_count >> 3] |= (unsigned char)(1U << (writer->bit_count & 7));
        writer->bit_count++;
    }
}

static unsigned reverse_bits(unsigned value, unsigned count) {
    unsigned result = 0, i;
    for (i = 0; i < count; ++i) result = (result << 1) | ((value >> i) & 1U);
    return result;
}

static void fixed_symbol(bit_writer *writer, unsigned symbol) {
    if (symbol <= 143) bits_put(writer, reverse_bits(0x30 + symbol, 8), 8);
    else if (symbol <= 255) bits_put(writer, reverse_bits(0x190 + symbol - 144, 9), 9);
    else if (symbol <= 279) bits_put(writer, reverse_bits(symbol - 256, 7), 7);
    else bits_put(writer, reverse_bits(0xc0 + symbol - 280, 8), 8);
}

static int malformed_case(const bit_writer *writer) {
    tinfl_decompressor state;
    unsigned char output[64];
    size_t input_size = (writer->bit_count + 7) / 8, output_size = sizeof(output);
    tinfl_status status;
    tinfl_init(&state);
    status = tinfl_decompress(&state, writer->bytes, &input_size, output, output,
                              &output_size, TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF |
                                            TINFL_FLAG_COMPUTE_ADLER32);
    return put_u32((uint32_t)(int32_t)status) && put_u32((uint32_t)input_size) &&
           put_u32((uint32_t)output_size) && put_u32(tinfl_get_adler32(&state));
}

static int run_case(const unsigned char *packed, size_t packed_size,
                    const unsigned char *plain, size_t plain_size, int base_flags,
                    tinfl_status expected_status) {
    tinfl_decompressor state;
    unsigned char output[8192];
    size_t input_at = 0, output_at = 0;
    int calls = 0;
    tinfl_status status;
    tinfl_init(&state);
    memset(output, 0, sizeof(output));
    do {
        size_t input_size = packed_size - input_at;
        size_t output_size = sizeof(output) - output_at;
        int flags = base_flags;
        if (input_size > 7) input_size = 7;
        if ((base_flags & TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF) && output_size > 13) output_size = 13;
        if (input_at + input_size < packed_size) flags |= TINFL_FLAG_HAS_MORE_INPUT;
        status = tinfl_decompress(&state, packed + input_at, &input_size,
                                  output, output + output_at, &output_size, flags);
        if (!put_u32((uint32_t)(int32_t)status) || !put_u32((uint32_t)input_size) ||
            !put_u32((uint32_t)output_size)) return 0;
        input_at += input_size;
        output_at += output_size;
        if (++calls > 100000) return 0;
    } while (status > 0);
    if (!put_u32(0x7fffffffU) || !put_u32((uint32_t)output_at) ||
        !put_u32(tinfl_get_adler32(&state)) ||
        fwrite(output, 1, output_at, stdout) != output_at) return 0;
    if (status != expected_status ||
        (status == TINFL_STATUS_DONE && (output_at != plain_size || memcmp(output, plain, plain_size) != 0))) {
        fprintf(stderr, "tinfl case failed status=%d input=%zu/%zu output=%zu/%zu flags=%d\n",
                (int)status, input_at, packed_size, output_at, plain_size, base_flags);
        return 0;
    }
    return 1;
}

int main(void) {
    unsigned char plain[4096];
    size_t raw_size = 0, zlib_size = 0, i;
    unsigned char *raw, *zlib;
    for (i = 0; i < sizeof(plain); ++i)
        plain[i] = (i % 29 < 21) ? (unsigned char)('a' + i % 5) : (unsigned char)(i * 73 + 11);
    raw = (unsigned char *)tdefl_compress_mem_to_heap(plain, sizeof(plain), &raw_size, 128);
    zlib = (unsigned char *)tdefl_compress_mem_to_heap(plain, sizeof(plain), &zlib_size,
                                                       128 | TDEFL_WRITE_ZLIB_HEADER);
    if (!raw || !zlib) { mz_free(raw); mz_free(zlib); return 1; }
    if (!run_case(raw, raw_size, plain, sizeof(plain), TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF, TINFL_STATUS_DONE) ||
        !run_case(raw, raw_size, plain, sizeof(plain), TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF | TINFL_FLAG_COMPUTE_ADLER32, TINFL_STATUS_DONE) ||
        !run_case(raw, raw_size, plain, sizeof(plain), 0, TINFL_STATUS_DONE) ||
        !run_case(zlib, zlib_size, plain, sizeof(plain), TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF | TINFL_FLAG_PARSE_ZLIB_HEADER, TINFL_STATUS_DONE)) {
        mz_free(raw); mz_free(zlib); return 1;
    }
    zlib[zlib_size - 1] ^= 1;
    if (!run_case(zlib, zlib_size, plain, sizeof(plain), TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF | TINFL_FLAG_PARSE_ZLIB_HEADER, TINFL_STATUS_ADLER32_MISMATCH) ||
        !run_case(raw, raw_size - 1, plain, sizeof(plain), TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF, TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS)) {
        mz_free(raw); mz_free(zlib); return 1;
    }
    {
        bit_writer writer;
        /* Reserved BTYPE=3. */
        bits_init(&writer); bits_put(&writer, 7, 3);
        if (!malformed_case(&writer)) { mz_free(raw); mz_free(zlib); return 1; }
        /* Stored block with a LEN/NLEN mismatch. */
        bits_init(&writer); bits_put(&writer, 1, 3); bits_put(&writer, 0, 5);
        bits_put(&writer, 1, 16); bits_put(&writer, 0, 16);
        if (!malformed_case(&writer)) { mz_free(raw); mz_free(zlib); return 1; }
        /* A match before any literal makes distance 1 impossible. */
        bits_init(&writer); bits_put(&writer, 3, 3); fixed_symbol(&writer, 257); bits_put(&writer, 0, 5);
        if (!malformed_case(&writer)) { mz_free(raw); mz_free(zlib); return 1; }
        /* Fixed trees contain bit patterns for reserved literal 286. */
        bits_init(&writer); bits_put(&writer, 3, 3); fixed_symbol(&writer, 286);
        if (!malformed_case(&writer)) { mz_free(raw); mz_free(zlib); return 1; }
        /* Fixed distance patterns 30 and 31 are reserved. */
        bits_init(&writer); bits_put(&writer, 3, 3); fixed_symbol(&writer, 'A');
        fixed_symbol(&writer, 257); bits_put(&writer, reverse_bits(30, 5), 5);
        if (!malformed_case(&writer)) { mz_free(raw); mz_free(zlib); return 1; }
        /* Dynamic code-length alphabet with four one-bit symbols is oversubscribed. */
        bits_init(&writer); bits_put(&writer, 1, 1); bits_put(&writer, 2, 2);
        bits_put(&writer, 0, 5); bits_put(&writer, 0, 5); bits_put(&writer, 0, 4);
        bits_put(&writer, 1, 3); bits_put(&writer, 1, 3); bits_put(&writer, 1, 3); bits_put(&writer, 1, 3);
        if (!malformed_case(&writer)) { mz_free(raw); mz_free(zlib); return 1; }
        /* Truncated stored header reaches cannot-make-progress, not generic failure. */
        bits_init(&writer); bits_put(&writer, 1, 3);
        if (!malformed_case(&writer)) { mz_free(raw); mz_free(zlib); return 1; }
    }
    mz_free(raw); mz_free(zlib);
    return 0;
}
